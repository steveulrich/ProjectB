#include "Development/BwayRelicFlightProbe.h"
#include "Development/BwayRelicLaunchObservation.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PBDRigidsSolver.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRelicFlightProbe)

namespace BwayRelicFlight
{
enum EStage { Waiting, Ready, Input, Released, Cancelled, GroundFlight, Grounded, Unprepared, GroundPickup, AirFlight, AirCaught, Restored, Complete, Failed };
const TCHAR* ThrowPath = TEXT("/Script/BreakawayCoreRuntime.BwayGameplayAbility_RelicThrow");

struct FPlayer
{
	TWeakObjectPtr<ABwayPlayerState> Player;
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	TWeakObjectPtr<APlayerController> Controller;
	FTransform Transform;
	FRotator Rotation;
	EMovementMode Mode = MOVE_Walking;
	uint8 CustomMode = 0;
	int32 Interceptions = 0;
	int32 PreparingCount = 0;
};

ARelicActor* RelicFor(UWorld* World)
{
	auto* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	auto* Manager = GameState ? GameState->FindComponentByClass<UBwayRelicManagerComponent>() : nullptr;
	return Manager ? Manager->GetRelicActor() : nullptr;
}

ABwayPlayerState* PlayerFor(UWorld* World, int32 PlayerId)
{
	if (auto* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr)
		for (APlayerState* Player : GameState->PlayerArray)
			if (Player && Player->GetPlayerId() == PlayerId) return Cast<ABwayPlayerState>(Player);
	return nullptr;
}

int32 ThrowSpecs(ULyraAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle& Handle, FGameplayTag& InputTag)
{
	UClass* Family = FindObject<UClass>(nullptr, ThrowPath);
	int32 Count = 0;
	if (ASC && Family)
		for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			if (Spec.Ability && Spec.Ability->IsA(Family))
			{
				++Count;
				Handle = Spec.Handle;
				const FGameplayTag Expected = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Relic.Throw"), false);
				InputTag = Spec.GetDynamicSpecSourceTags().HasTagExact(Expected) ? Expected : FGameplayTag();
			}
	return Count;
}

float Speed(ARelicActor* Relic)
{
	auto* Mesh = Relic ? Cast<UPrimitiveComponent>(Relic->GetRootComponent()) : nullptr;
	return Mesh ? Mesh->GetPhysicsLinearVelocity().Size() : -1.0f;
}

bool SendMouseInput(APlayerController* Controller, EInputEvent Event, int32 Scenario, const TCHAR* Purpose)
{
	if (!Controller || !Controller->IsLocalController()) return false;
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	const FPlatformUserId User = Controller->GetPlatformUserId();
	const FInputDeviceId Device = Mapper.GetPrimaryInputDeviceForUser(User);
	const FPlatformUserId MappedUser = Mapper.GetUserForInputDevice(Device);
	const int32 Source = Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
	if (!User.IsValid() || !Device.IsValid() || MappedUser != User)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRelicFlight: InputRouteRejected source=%d scenario=%d platformUser=%d device=%d mappedUser=%d purpose=%s"),
			Source, Scenario, User.GetInternalId(), Device.GetId(), MappedUser.GetInternalId(), Purpose);
		return false;
	}
	const bool bHandled = Controller->InputKey(FInputKeyEventArgs::CreateSimulated(
		EKeys::LeftMouseButton, Event, Event == IE_Pressed ? 1.0f : 0.0f, -1, Device));
	UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: InputRoute source=%d scenario=%d event=%s platformUser=%d device=%d mappedUser=%d inputKeyResult=%d purpose=%s"),
		Source, Scenario, Event == IE_Pressed ? TEXT("Pressed") : TEXT("Released"), User.GetInternalId(), Device.GetId(), MappedUser.GetInternalId(), bHandled, Purpose);
	return true;
}
}

struct FBwayRelicFlightState
{
	bool bCoordinator = false;
	bool bCleaned = false;
	bool bDone = false;
	bool bPressed = false;
	bool bKeyDown = false;
	bool bSawActive = false;
	bool bGroundVerified = false;
	bool bSavedLocalRotation = false;
	bool bCancelledInput = false;
	bool bObservedRegrant = false;
	bool bAuthoritySawActive = false;
	bool bCancelRegranted = false;
	bool bOwnershipDropped = false;
	bool bObservedThrown = false;
	int32 SeenSerial = 0;
	int32 ReportedSerial = 0;
	int32 SourceIndex = INDEX_NONE;
	int32 EnemyIndex = INDEX_NONE;
	double Started = FPlatformTime::Seconds();
	double StageAt = FPlatformTime::Seconds();
	double StableAt = 0.0;
	double PressAt = 0.0;
	double OwnershipDropAt = 0.0;
	double RegrantObservedHeld = 0.0;
	float LaunchSpeed = 0.0f;
	BwayRelicFlight::FLaunchObservation LaunchObservation;
	FVector LaunchAim = FVector::ZeroVector;
	TWeakObjectPtr<ARelicActor> LaunchRelic;
	TWeakObjectPtr<UPrimitiveComponent> LaunchMesh;
	double LaunchSolverTime = 0.0;
	int32 LaunchSolverFrame = INDEX_NONE;
	FDelegateHandle PhysicsStepHandle;
	uint64 LastLaunchPhysicsFrame = MAX_uint64;
	FGameplayAbilitySpecHandle InputHandle;
	FGameplayAbilitySpecHandle RegrantHandle;
	FGameplayTag InputTag;
	FVector Floor = FVector::ZeroVector;
	FTransform RelicTransform;
	FRotator LocalRotation;
	TArray<BwayRelicFlight::FPlayer> Players;
	TArray<TWeakObjectPtr<ABwayRelicFlightProbe>> Witnesses;
	TMap<int32, int32> Acks;
};

ABwayRelicFlightProbe::ABwayRelicFlightProbe()
{
#if !UE_BUILD_SHIPPING
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	NetUpdateFrequency = 30.0f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
#endif
}

void ABwayRelicFlightProbe::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	State = MakeShared<FBwayRelicFlightState>();
#endif
}

void ABwayRelicFlightProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayRelicFlightProbe, Frame);
}

bool ABwayRelicFlightProbe::Setup()
{
	using namespace BwayRelicFlight;
	auto* GameState = GetWorld()->GetGameState<ABwayGameState>();
	auto* Relic = RelicFor(GetWorld());
	if (!GameState || !Relic || !Relic->GetRelicSettings() || Relic->CurrentCarrier || !GameState->GetRoundManagement()
		|| GameState->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return false;
	TArray<ABwayPlayerState*> Humans;
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		auto* Player = Cast<ABwayPlayerState>(Entry);
		if (!Player || Player->IsABot()) continue;
		auto* Pawn = Cast<ABwayCharacterWithAbilities>(Player->GetPawn());
		auto* Controller = Cast<APlayerController>(Player->GetOwner());
		const auto* Health = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
		if (!Pawn || !Controller || Controller->GetPawn() != Pawn || !Player->GetLyraAbilitySystemComponent()
			|| !Pawn->GetCharacterMovement() || GameState->GetPlayerTeam(Player) == INDEX_NONE
			|| !Health || Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f
			|| Player->GetLyraAbilitySystemComponent()->GetTagCount(BwayGameplayTags::State_Relic_PreparingCatch) != 0) return false;
		Humans.Add(Player);
	}
	if (Humans.Num() != 4 || GameState->PlayerArray.Num() != 4) return false;
	if ((Relic->GetCurrentState() != ERelicState::Neutral && Relic->GetCurrentState() != ERelicState::Dropped) || Speed(Relic) > 50.0f) return false;
	Humans.Sort([](const ABwayPlayerState& Left, const ABwayPlayerState& Right) { return Left.GetPlayerId() < Right.GetPlayerId(); });
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BwayRelicFlightFloor), false);
	Query.AddIgnoredActor(Relic);
	for (ABwayPlayerState* Player : Humans) Query.AddIgnoredActor(Player->GetPawn());
	FHitResult FloorHit;
	const FVector RelicLocation = Relic->GetActorLocation();
	if (!GetWorld()->LineTraceSingleByChannel(FloorHit, RelicLocation + FVector(0, 0, 1000),
		RelicLocation - FVector(0, 0, 3000), ECC_WorldStatic, Query) || FloorHit.ImpactNormal.Z < 0.9f)
	{
		Fail(TEXT("no-walkable-world-floor")); return false;
	}
	State->Floor = FloorHit.ImpactPoint;
	FHitResult LaneHit;
	if (GetWorld()->SweepSingleByChannel(LaneHit, State->Floor + FVector(-600, 0, 500), State->Floor + FVector(300, 0, 500),
		FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(100.0f), Query))
	{ Fail(TEXT("air-pass-lane-obstructed")); return false; }
	State->RelicTransform = Relic->GetActorTransform();
	for (ABwayPlayerState* Player : Humans)
	{
		FPlayer Saved;
		Saved.Player = Player;
		Saved.Pawn = Cast<ABwayCharacterWithAbilities>(Player->GetPawn());
		Saved.Controller = Cast<APlayerController>(Player->GetOwner());
		Saved.Transform = Saved.Pawn->GetActorTransform();
		Saved.Rotation = Saved.Controller->GetControlRotation();
		Saved.Mode = Saved.Pawn->GetCharacterMovement()->MovementMode;
		Saved.CustomMode = Saved.Pawn->GetCharacterMovement()->CustomMovementMode;
		Saved.Interceptions = Player->GetInterceptions();
		Saved.PreparingCount = Player->GetLyraAbilitySystemComponent()->GetTagCount(BwayGameplayTags::State_Relic_PreparingCatch);
		State->Players.Add(Saved);
	}
	State->SourceIndex = State->Players.IndexOfByPredicate([](const FPlayer& Player) { return !Player.Controller->IsLocalController(); });
	if (State->SourceIndex == INDEX_NONE) { Fail(TEXT("remote-owner-required")); return false; }
	const int32 SourceTeam = GameState->GetPlayerTeam(State->Players[State->SourceIndex].Player.Get());
	State->EnemyIndex = State->Players.IndexOfByPredicate([GameState, SourceTeam](const FPlayer& Player)
		{ return GameState->GetPlayerTeam(Player.Player.Get()) != SourceTeam; });
	if (State->EnemyIndex == INDEX_NONE) { Fail(TEXT("enemy-required")); return false; }
	State->bCoordinator = true;
	Coordinator = this;
	FPhysScene* PhysicsScene = GetWorld()->GetPhysicsScene();
	if (!PhysicsScene) { Fail(TEXT("launch-physics-scene-missing")); return false; }
	State->PhysicsStepHandle = PhysicsScene->OnPhysScenePreTick.AddWeakLambda(this,
		[this](FPhysScene* Scene, float StepSeconds)
		{
			if (!State || State->bDone || State->bCleaned || State->LaunchSpeed > 0.0f || Frame.Stage != BwayRelicFlight::Input || Frame.Scenario >= 3) return;
			auto* Relic = BwayRelicFlight::RelicFor(GetWorld());
			if (!Relic || Relic->GetCurrentState() != ERelicState::Thrown || Relic->CurrentCarrier) return;
			auto* Mesh = Cast<UPrimitiveComponent>(Relic->GetRootComponent());
			const FBodyInstance* Body = Mesh ? Mesh->GetBodyInstance() : nullptr;
			const UPhysicsSettings* Physics = UPhysicsSettings::Get();
			const IConsoleVariable* DragOverride = IConsoleManager::Get().FindConsoleVariable(TEXT("p.LinearEtherDragOverride"));
			const IConsoleVariable* SpeedOverride = IConsoleManager::Get().FindConsoleVariable(TEXT("p.HackMaxVelocity2"));
			if (!Mesh || !Body || !Mesh->IsSimulatingPhysics() || !Scene || !Scene->GetSolver()
				|| !Body->bNotifyRigidBodyCollision || Body->GravityGroupIndex != 0 || !BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body)
				|| Mesh->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap
				|| Scene->GetSolver()->IsPaused_External() || static_cast<const Chaos::FPhysicsSolverBase*>(Scene->GetSolver())->IsResimming()
				|| Scene->GetSolver()->IsUsingAsyncResults() || Physics->bSubstepping || Physics->bTickPhysicsAsync
				|| Scene->GetSolver()->GetSolverSubstep_External() || Scene->GetSolver()->GetMaxSubSteps_External() != 1
				|| StepSeconds <= Scene->GetSolver()->GetMinDeltaTime_External()
				|| (Scene->GetSolver()->GetMaxDeltaTime_External() > 0.0f && StepSeconds > Scene->GetSolver()->GetMaxDeltaTime_External())
				|| !DragOverride || !FMath::IsFinite(DragOverride->GetFloat()) || DragOverride->GetFloat() >= 0.0f || State->LastLaunchPhysicsFrame == GFrameCounter)
			{ State->LaunchObservation.bInvalid = true; return; }
			double MaxBodySpeed = 0.0;
			FPhysicsCommand::ExecuteRead(Body->ActorHandle, [&MaxBodySpeed](const FPhysicsActorHandle& Actor)
			{ MaxBodySpeed = FPhysicsInterface::GetMaxLinearVelocity_AssumesLocked(Actor); });
			if (!SpeedOverride || !FMath::IsFinite(SpeedOverride->GetFloat()) || SpeedOverride->GetFloat() >= 0.0f || !FMath::IsFinite(MaxBodySpeed)
				|| MaxBodySpeed <= Relic->GetRelicSettings()->MaxThrowSpeed + 180.0f)
			{ State->LaunchObservation.bInvalid = true; return; }
			State->LastLaunchPhysicsFrame = GFrameCounter;
			if (State->LaunchObservation.Steps.IsEmpty())
			{
				State->LaunchRelic = Relic; State->LaunchMesh = Mesh;
				State->LaunchSolverTime = Scene->GetSolver()->GetSolverTime();
				State->LaunchSolverFrame = Scene->GetSolver()->GetCurrentFrame();
				const auto* Controller = State->Players[State->SourceIndex].Controller.Get();
				State->LaunchAim = Controller ? Controller->GetControlRotation().Vector().GetSafeNormal() : FVector::ZeroVector;
			}
			if (State->LaunchRelic.Get() != Relic || State->LaunchMesh.Get() != Mesh
				|| Scene->GetSolver()->GetCurrentFrame() - State->LaunchSolverFrame != State->LaunchObservation.Steps.Num()
				|| !FMath::IsNearlyEqual(double(Scene->GetSolver()->GetSolverTime()) - State->LaunchSolverTime, State->LaunchObservation.Seconds, 0.00001))
			{ State->LaunchObservation.bInvalid = true; return; }
			const FVector Gravity(0.0, 0.0, Mesh->IsGravityEnabled() ? GetWorld()->GetGravityZ() : 0.0);
			State->LaunchObservation.AddStep(StepSeconds, Mesh->GetLinearDamping(), Gravity);
		});
	Frame.Source = State->Players[State->SourceIndex].Player->GetPlayerId();
	for (int32 Index = 0; Index < State->Players.Num(); ++Index)
	{
		FPlayer& Player = State->Players[Index];
		Player.Pawn->GetCharacterMovement()->StopMovementImmediately();
		Player.Pawn->GetCharacterMovement()->DisableMovement();
		Player.Pawn->SetActorLocation(State->Floor + FVector(-2000, 1000 + Index * 500, 120), false, nullptr, ETeleportType::TeleportPhysics);
		Player.Player->GetLyraAbilitySystemComponent()->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);
		ABwayRelicFlightProbe* Witness = this;
		if (Index > 0)
		{
			FActorSpawnParameters Spawn;
			Spawn.Owner = Player.Controller.Get();
			Witness = GetWorld()->SpawnActor<ABwayRelicFlightProbe>(StaticClass(), FTransform::Identity, Spawn);
			if (!Witness) { Fail(TEXT("witness-spawn-failed")); return false; }
		}
		Witness->SetOwner(Player.Controller.Get());
		Witness->Frame.Witness = Player.Player->GetPlayerId();
		Witness->Coordinator = this;
		State->Witnesses.Add(Witness);
	}
	UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: Setup source=%d enemy=%d humans=4 remoteOwner=1 floor=%s scope=synthetic-mouse-enhanced-input-and-real-physics"),
		Frame.Source, State->Players[State->EnemyIndex].Player->GetPlayerId(), *State->Floor.ToCompactString());
	return AcquireSource();
}

bool ABwayRelicFlightProbe::AcquireSource()
{
	using namespace BwayRelicFlight;
	auto* Relic = RelicFor(GetWorld());
	FPlayer& Source = State->Players[State->SourceIndex];
	Relic->OnDropped();
	Relic->ClearScoringFlag();
	Source.Pawn->SetActorLocation(State->Floor + FVector(-400, 0, 120), false, nullptr, ETeleportType::TeleportPhysics);
	Source.Controller->SetControlRotation(FRotator(60, 0, 0));
	Relic->OnPickedUp(Source.Pawn.Get());
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Tag;
	const int32 Count = ThrowSpecs(Source.Player->GetLyraAbilitySystemComponent(), Handle, Tag);
	if (Relic->CurrentCarrier != Source.Pawn.Get() || Count != 1 || !Tag.IsValid())
	{
		Fail(TEXT("source-acquire-or-unique-native-input-grant")); return false;
	}
	UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: Grant scenario=%d source=%d count=%d input=%s handle=%s"),
		Frame.Scenario, Frame.Source, Count, *Tag.ToString(), *Handle.ToString());
	Publish(Ready);
	return true;
}

void ABwayRelicFlightProbe::Publish(int32 Stage)
{
	if (!State || !State->bCoordinator) return;
	using namespace BwayRelicFlight;
	auto* Relic = RelicFor(GetWorld());
	++Frame.Serial;
	Frame.Stage = Stage;
	Frame.Carrier = Relic && Relic->CurrentCarrier && Relic->CurrentCarrier->GetPlayerState()
		? Relic->CurrentCarrier->GetPlayerState()->GetPlayerId() : INDEX_NONE;
	Frame.RelicState = Relic ? int32(Relic->GetCurrentState()) : INDEX_NONE;
	Frame.bPassInFlight = Relic && Relic->bIsPassInFlight;
	Frame.PassingTeam = Relic ? Relic->PassingTeamIndex : INDEX_NONE;
	Frame.PossessingTeam = Relic ? Relic->LastPossessingTeam : INDEX_NONE;
	Frame.Players.Reset(); Frame.Interceptions.Reset();
	for (const FPlayer& Player : State->Players)
	{
		Frame.Players.Add(Player.Player->GetPlayerId());
		Frame.Interceptions.Add(Player.Player->GetInterceptions());
	}
	State->StageAt = FPlatformTime::Seconds();
	for (const auto& Weak : State->Witnesses)
		if (auto* Witness = Weak.Get())
		{
			const int32 PlayerId = Witness->Frame.Witness;
			Witness->Frame = Frame;
			Witness->Frame.Witness = PlayerId;
			Witness->ForceNetUpdate();
		}
	UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: Stage serial=%d stage=%d scenario=%d state=%d carrier=%d pass=%d passingTeam=%d possession=%d"),
		Frame.Serial, Stage, Frame.Scenario, Frame.RelicState, Frame.Carrier, Frame.bPassInFlight, Frame.PassingTeam, Frame.PossessingTeam);
}

bool ABwayRelicFlightProbe::Converged(FString& Summary) const
{
	using namespace BwayRelicFlight;
	auto* Relic = RelicFor(GetWorld());
	if (!Relic || Frame.Players.Num() != 4 || Frame.Interceptions.Num() != 4) return false;
	const int32 Carrier = Relic->CurrentCarrier && Relic->CurrentCarrier->GetPlayerState() ? Relic->CurrentCarrier->GetPlayerState()->GetPlayerId() : INDEX_NONE;
	bool bStats = true;
	for (int32 Index = 0; Index < Frame.Players.Num(); ++Index)
	{
		auto* Player = PlayerFor(GetWorld(), Frame.Players[Index]);
		bStats &= Player && Player->GetInterceptions() == Frame.Interceptions[Index];
	}
	bool bGrant = true;
	int32 GrantCount = INDEX_NONE;
	if (Frame.Witness == Frame.Source)
	{
		auto* Player = PlayerFor(GetWorld(), Frame.Source);
		FGameplayAbilitySpecHandle Handle;
		FGameplayTag Tag;
		GrantCount = ThrowSpecs(Player ? Player->GetLyraAbilitySystemComponent() : nullptr, Handle, Tag);
		bGrant = GrantCount == (Frame.Carrier == Frame.Source ? 1 : 0);
		if (GrantCount == 1) bGrant &= Tag.IsValid();
	}
	Summary = FString::Printf(TEXT("state=%d carrier=%d pass=%d passingTeam=%d possession=%d stats=%d grantCount=%d"),
		int32(Relic->GetCurrentState()), Carrier, Relic->bIsPassInFlight, Relic->PassingTeamIndex, Relic->LastPossessingTeam, bStats, GrantCount);
	return int32(Relic->GetCurrentState()) == Frame.RelicState && Carrier == Frame.Carrier
		&& Relic->bIsPassInFlight == Frame.bPassInFlight && Relic->PassingTeamIndex == Frame.PassingTeam
		&& Relic->LastPossessingTeam == Frame.PossessingTeam && bStats && bGrant;
}

void ABwayRelicFlightProbe::LocalTick()
{
	using namespace BwayRelicFlight;
	auto* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController() || Frame.Serial <= 0 || State->ReportedSerial == Frame.Serial) return;
	if (State->SeenSerial != Frame.Serial)
	{
		if (State->bKeyDown && Frame.Stage != Input)
		{
			State->bKeyDown = !SendMouseInput(Controller, IE_Released, Frame.Scenario, TEXT("stage-cleanup"));
		}
		if (State->bSavedLocalRotation && (Frame.Stage == Restored || Frame.Stage == Complete || Frame.Stage == Failed))
		{
			Controller->SetControlRotation(State->LocalRotation);
			State->bSavedLocalRotation = false;
		}
		State->SeenSerial = Frame.Serial;
		State->StableAt = 0.0;
		State->bPressed = false;
		State->bSawActive = false;
		State->bCancelledInput = false;
		State->bObservedRegrant = false;
	}
	if (Frame.Stage == Failed) { State->bDone = true; return; }
	if (Frame.Stage == Input)
	{
		if (Frame.Witness != Frame.Source) { Report(true, TEXT("input-witness")); return; }
		auto* Player = Cast<ABwayPlayerState>(Controller->PlayerState);
		auto* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
		if (!ASC || HasAuthority() || !Controller->GetPawn() || Controller->GetPawn()->GetLocalRole() != ROLE_AutonomousProxy)
		{ Report(false, TEXT("source-not-autonomous")); return; }
		if (!State->bPressed)
		{
			if (ThrowSpecs(ASC, State->InputHandle, State->InputTag) != 1 || !State->InputTag.IsValid())
			{ Report(false, TEXT("source-input-grant")); return; }
			if (!State->bSavedLocalRotation)
			{
				State->LocalRotation = Controller->GetControlRotation();
				State->bSavedLocalRotation = true;
			}
			Controller->SetControlRotation(FRotator(60, 0, 0));
			if (!SendMouseInput(Controller, IE_Pressed, Frame.Scenario, TEXT("scenario")))
			{ Report(false, TEXT("unmapped-local-input-device")); return; }
			State->bPressed = true;
			State->bKeyDown = true;
			State->PressAt = GetWorld()->GetTimeSeconds();
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: InputPress source=%d scenario=%d net=%d role=%d tag=%s hold=%.3f route=PlayerController.InputKey key=LeftMouseButton"),
				Frame.Source, Frame.Scenario, int32(GetNetMode()), int32(Controller->GetPawn()->GetLocalRole()), *State->InputTag.ToString(), Frame.HoldSeconds);
			return;
		}
		const double Held = GetWorld()->GetTimeSeconds() - State->PressAt;
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(State->InputHandle);
		if (Frame.Scenario == 4 && State->bSawActive && !State->bCancelledInput && (!Spec || !Spec->IsActive()))
		{
			State->bCancelledInput = true;
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: OwnershipLossObserved source=%d keyDown=1 held=%.6f"), Frame.Source, Held);
		}
		if (!State->bCancelledInput && Spec && Spec->IsActive()) State->bSawActive = true;
		else if (!State->bCancelledInput && (State->bSawActive || Held > 0.5))
		{ Report(false, TEXT("input-hold-not-active")); return; }
		if (Frame.Scenario == 3 && Held >= 0.25 && !State->bCancelledInput && State->bSawActive)
		{
			ASC->CancelAbilityHandle(State->InputHandle);
			State->bCancelledInput = true;
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: CancelHeld source=%d keyDown=1 held=%.6f"), Frame.Source, Held);
		}
		if (State->bCancelledInput)
		{
			FGameplayAbilitySpecHandle CurrentHandle; FGameplayTag CurrentTag;
			const int32 Count = ThrowSpecs(ASC, CurrentHandle, CurrentTag);
			if (Count > 1 || (Count == 1 && ASC->FindAbilitySpecFromHandle(CurrentHandle)->IsActive()))
			{ Report(false, TEXT("held-key-reactivated-cancelled-or-regranted-throw")); return; }
			if (State->bObservedRegrant && (Count != 1 || CurrentHandle != State->RegrantHandle))
			{ Report(false, TEXT("held-key-regrant-did-not-remain-unique")); return; }
			if (!State->bObservedRegrant && Count == 1 && CurrentHandle != State->InputHandle && CurrentTag.IsValid())
			{
				State->bObservedRegrant = true;
				State->RegrantObservedHeld = Held;
				State->RegrantHandle = CurrentHandle;
				UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: RegrantObserved source=%d scenario=%d net=%d held=%.6f count=1 active=0 keyDown=1 oldHandle=%s newHandle=%s"),
					Frame.Source, Frame.Scenario, int32(GetNetMode()), Held, *State->InputHandle.ToString(), *CurrentHandle.ToString());
			}
		}
		if (Held < Frame.HoldSeconds) return;
		if (!State->bSawActive) { Report(false, TEXT("mapped-input-never-activated")); return; }
		if (Frame.Scenario >= 3)
		{
			if (!State->bObservedRegrant || Held - State->RegrantObservedHeld < 0.25)
			{
				if (Held >= Frame.HoldSeconds + 2.0) Report(false, TEXT("remote-regrant-hold-not-observed"));
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: HeldAfterRegrant source=%d scenario=%d held=%.6f elapsed=%.6f count=1 active=0 keyDown=1"),
				Frame.Source, Frame.Scenario, Held, Held - State->RegrantObservedHeld);
		}
		if (!SendMouseInput(Controller, IE_Released, Frame.Scenario, TEXT("scenario")))
		{ Report(false, TEXT("unmapped-local-input-device")); return; }
		State->bKeyDown = false;
		UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: InputRelease source=%d scenario=%d held=%.6f cancelled=%d"), Frame.Source, Frame.Scenario, Held, Frame.Scenario >= 3);
		Report(true, TEXT("input-sent"));
		return;
	}
	if (Frame.Stage == GroundFlight || Frame.Stage == AirFlight) { Report(true, TEXT("physics-in-progress")); return; }
	FString Summary;
	if (!Converged(Summary)) { State->StableAt = 0.0; return; }
	if (State->StableAt == 0.0) State->StableAt = FPlatformTime::Seconds();
	if (FPlatformTime::Seconds() - State->StableAt < 0.5) return;
	UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: Peer PASS witness=%d source=%d serial=%d stage=%d scenario=%d net=%d %s"),
		Frame.Witness, Frame.Source, Frame.Serial, Frame.Stage, Frame.Scenario, int32(GetNetMode()), *Summary);
	Report(true, TEXT("settled"));
	if (Frame.Stage == Complete)
	{
		if (!State->bCoordinator) State->bDone = true;
	}
}

void ABwayRelicFlightProbe::Report(bool bSuccess, const TCHAR* Reason)
{
	if (State->ReportedSerial == Frame.Serial) return;
	State->ReportedSerial = Frame.Serial;
	ServerReport(Frame.Serial, bSuccess, Reason);
}

void ABwayRelicFlightProbe::ServerReport_Implementation(int32 Serial, bool bSuccess, const FString& Reason)
{
#if !UE_BUILD_SHIPPING
	auto* OwnerController = Cast<APlayerController>(GetOwner());
	auto* Leader = Coordinator.Get();
	if (!Leader || !Leader->State || !Leader->State->bCoordinator || Serial != Frame.Serial || Serial != Leader->Frame.Serial
		|| !OwnerController || !OwnerController->PlayerState || OwnerController->PlayerState->GetPlayerId() != Frame.Witness
		|| !Leader->State->Witnesses.Contains(this)) return;
	if (!bSuccess) { Leader->Fail(*Reason.Left(100)); return; }
	Leader->State->Acks.Add(Frame.Witness, Serial);
	UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: Ack witness=%d serial=%d stage=%d"), Frame.Witness, Serial, Frame.Stage);
#endif
}

void ABwayRelicFlightProbe::AuthorityTick()
{
	using namespace BwayRelicFlight;
	auto* Relic = RelicFor(GetWorld());
	if (!Relic || !State->Players.IsValidIndex(State->SourceIndex)) { Fail(TEXT("lost-fixture-world")); return; }
	for (const FPlayer& Player : State->Players)
		if (!Player.Player.IsValid() || !Player.Pawn.IsValid() || !Player.Controller.IsValid()
			|| Player.Controller->PlayerState != Player.Player.Get() || Player.Controller->GetPawn() != Player.Pawn.Get()
			|| Player.Pawn->GetPlayerState() != Player.Player.Get())
		{ Fail(TEXT("participant-identity-changed")); return; }
	const double Age = FPlatformTime::Seconds() - State->StageAt;
	if (Age > 15.0) { Fail(TEXT("stage-timeout")); return; }
	FPlayer& Source = State->Players[State->SourceIndex];
	FPlayer& Enemy = State->Players[State->EnemyIndex];
	auto* SourceASC = Source.Player->GetLyraAbilitySystemComponent();
	int32 Acknowledged = 0;
	for (const auto& Weak : State->Witnesses)
		if (auto* Witness = Weak.Get(); Witness && State->Acks.FindRef(Witness->Frame.Witness) == Frame.Serial) ++Acknowledged;
	const bool bAllAcknowledged = Acknowledged == 4;
	if (Frame.Stage == Input)
	{
		if (Frame.Scenario < 3 && State->LaunchSpeed == 0.0f
			&& (State->LaunchObservation.bInvalid || (!State->LaunchObservation.Steps.IsEmpty()
				&& (Relic != State->LaunchRelic.Get() || Relic->GetRootComponent() != State->LaunchMesh.Get()
					|| Relic->GetCurrentState() != ERelicState::Thrown || Relic->CurrentCarrier))))
		{ Fail(TEXT("launch-physics-state-changed-or-unsupported")); return; }
		FGameplayAbilitySpecHandle CurrentHandle; FGameplayTag CurrentTag;
		const int32 Count = ThrowSpecs(SourceASC, CurrentHandle, CurrentTag);
		const bool bActive = Count == 1 && SourceASC->FindAbilitySpecFromHandle(CurrentHandle)->IsActive();
		State->bAuthoritySawActive |= bActive;
		if (Frame.Scenario == 4 && bActive && !State->bOwnershipDropped && Age >= 0.75)
		{
			if (Relic->CurrentCarrier != Source.Pawn.Get()) { Fail(TEXT("ownership-loss-no-carrier")); return; }
			Relic->OnDropped();
			State->bOwnershipDropped = true;
			State->OwnershipDropAt = GetWorld()->GetTimeSeconds();
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: OwnershipDrop source=%d wasActive=1 keyStillHeld=1"), Frame.Source);
		}
		if (Frame.Scenario == 4 && State->bOwnershipDropped && !State->bCancelRegranted
			&& GetWorld()->GetTimeSeconds() - State->OwnershipDropAt >= Relic->GetRelicSettings()->PickupCooldownAfterDrop + 0.2f)
		{
			Relic->OnPickedUp(Source.Pawn.Get());
			if (Relic->CurrentCarrier != Source.Pawn.Get() || ThrowSpecs(SourceASC, CurrentHandle, CurrentTag) != 1)
			{ Fail(TEXT("ownership-loss-regrant-count")); return; }
			State->bCancelRegranted = true;
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: OwnershipRegrant source=%d count=1 keyStillHeld=1 cooldownElapsed=1"), Frame.Source);
		}
		if (Frame.Scenario == 3 && State->bAuthoritySawActive && !bActive && !State->bCancelRegranted && Age >= 0.75)
		{
			if (Relic->CurrentCarrier != Source.Pawn.Get()) { Fail(TEXT("cancel-lost-possession")); return; }
			Relic->OnDropped(); Relic->ClearScoringFlag(); Relic->OnPickedUp(Source.Pawn.Get());
			if (Relic->CurrentCarrier != Source.Pawn.Get() || ThrowSpecs(SourceASC, CurrentHandle, CurrentTag) != 1)
			{ Fail(TEXT("cancel-regrant-count")); return; }
			State->bCancelRegranted = true;
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: CancelRegrant source=%d count=1 keyStillHeld=1"), Frame.Source);
		}
		if (Frame.Scenario < 3 && Relic->GetCurrentState() == ERelicState::Thrown && !Relic->CurrentCarrier)
		{
			if (!State->bObservedThrown)
			{
				State->bObservedThrown = true;
				return;
			}
			if (State->LaunchSpeed == 0.0f)
			{
				auto* Mesh = Cast<UPrimitiveComponent>(Relic->GetRootComponent());
				auto* Scene = GetWorld()->GetPhysicsScene();
				auto* Solver = Scene ? Scene->GetSolver() : nullptr;
				const FVector Observed = Mesh ? Mesh->GetPhysicsLinearVelocity() : FVector::ZeroVector;
				FVector Initial;
				if (!Solver || State->LaunchRelic.Get() != Relic || State->LaunchMesh.Get() != Mesh
					|| Solver->GetCurrentFrame() - State->LaunchSolverFrame != State->LaunchObservation.Steps.Num()
					|| !FMath::IsNearlyEqual(double(Solver->GetSolverTime()) - State->LaunchSolverTime, State->LaunchObservation.Seconds, 0.00001)
					|| State->LastLaunchPhysicsFrame != GFrameCounter || !State->LaunchObservation.Reconstruct(Observed, Initial)
					|| State->LaunchAim.ContainsNaN() || State->LaunchAim.IsNearlyZero())
				{ Fail(TEXT("launch-physics-observation-invalid")); return; }
				State->LaunchSpeed = Initial.Size();
				const URelicSettings* Settings = Relic->GetRelicSettings();
				const float Expected = Settings->ComputeChargedThrowSpeed(Frame.HoldSeconds);
				const double VectorError = (Initial - State->LaunchAim * Expected).Size();
				const bool bSpeed = FMath::Abs(State->LaunchSpeed - Expected) <= 180.0f && VectorError <= 180.0
					&& (Frame.Scenario > 0 || State->LaunchSpeed < Settings->MaxThrowSpeed - 200.0f)
					&& (Frame.Scenario != 1 || (State->LaunchSpeed > Settings->MinThrowSpeed + 200.0f && State->LaunchSpeed < Settings->MaxThrowSpeed - 200.0f));
				// Keep raw physical speed explicit; the companion record proves reconstruction.
				TSharedRef<FJsonObject> Evidence = MakeShared<FJsonObject>();
				Evidence->SetNumberField(TEXT("schema"), 1);
				Evidence->SetNumberField(TEXT("scenario"), Frame.Scenario);
				Evidence->SetNumberField(TEXT("source"), Frame.Source);
				Evidence->SetNumberField(TEXT("multiplier"), State->LaunchObservation.Multiplier);
				Evidence->SetNumberField(TEXT("seconds"), State->LaunchObservation.Seconds);
				Evidence->SetNumberField(TEXT("expectedSpeed"), Expected);
				Evidence->SetNumberField(TEXT("minThrowSpeed"), Settings->MinThrowSpeed);
				Evidence->SetNumberField(TEXT("maxThrowSpeed"), Settings->MaxThrowSpeed);
				Evidence->SetNumberField(TEXT("maxChargeTime"), Settings->MaxChargeTime);
				Evidence->SetNumberField(TEXT("holdSeconds"), Frame.HoldSeconds);
				Evidence->SetNumberField(TEXT("lastPhysicsFrame"), double(State->LastLaunchPhysicsFrame));
				Evidence->SetNumberField(TEXT("sampleFrame"), double(GFrameCounter));
				Evidence->SetNumberField(TEXT("startSolverFrame"), State->LaunchSolverFrame);
				Evidence->SetNumberField(TEXT("sampleSolverFrame"), Solver->GetCurrentFrame());
				Evidence->SetNumberField(TEXT("solverElapsed"), double(Solver->GetSolverTime()) - State->LaunchSolverTime);
				Evidence->SetStringField(TEXT("solverMode"), TEXT("synchronous-single-step"));
				Evidence->SetNumberField(TEXT("vectorError"), VectorError);
				Evidence->SetBoolField(TEXT("pass"), bSpeed);
				const auto VectorJson = [](const FVector& V)
				{ return TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)}; };
				Evidence->SetArrayField(TEXT("observed"), VectorJson(Observed));
				Evidence->SetArrayField(TEXT("initial"), VectorJson(Initial));
				Evidence->SetArrayField(TEXT("aim"), VectorJson(State->LaunchAim));
				Evidence->SetArrayField(TEXT("gravityVelocity"), VectorJson(State->LaunchObservation.GravityVelocity));
				TArray<TSharedPtr<FJsonValue>> Steps;
				for (const auto& Step : State->LaunchObservation.Steps)
				{
					TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
					Row->SetNumberField(TEXT("seconds"), Step.Seconds); Row->SetNumberField(TEXT("damping"), Step.Damping);
					Row->SetArrayField(TEXT("gravity"), VectorJson(Step.Gravity));
					Steps.Add(MakeShared<FJsonValueObject>(Row));
				}
				Evidence->SetArrayField(TEXT("steps"), Steps);
				FString Json;
				FJsonSerializer::Serialize(Evidence, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
				UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: LaunchPhysics %s"), *Json);
				UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: Launch scenario=%d source=%d observedSpeed=%.3f expectedSpeed=%.3f physicsTolerance=180 sample=post-physics-next-frame pass=%d"),
					Frame.Scenario, Frame.Source, Observed.Size(), Expected, bSpeed);
				if (!bSpeed) { Fail(TEXT("launch-speed-outside-bound")); return; }
			}
			if (bAllAcknowledged) Publish(Released);
		}
		else if (Frame.Scenario >= 3 && bAllAcknowledged && Age >= Frame.HoldSeconds + 0.8f)
		{
			FGameplayAbilitySpecHandle Handle;
			FGameplayTag Tag;
			if (ThrowSpecs(SourceASC, Handle, Tag) != 1 || SourceASC->FindAbilitySpecFromHandle(Handle)->IsActive()
				|| !State->bCancelRegranted || Relic->CurrentCarrier != Source.Pawn.Get() || Relic->GetCurrentState() != ERelicState::Carried)
			{ Fail(TEXT("cancel-did-not-retain-carrier")); return; }
			Publish(Cancelled);
		}
		return;
	}
	if (Frame.Stage == GroundFlight)
	{
		if (Relic->GetCurrentState() == ERelicState::Dropped && !Relic->CurrentCarrier && !Relic->bIsPassInFlight
			&& Relic->PassingTeamIndex == INDEX_NONE && Relic->LastPossessingTeam == Frame.PossessingTeam)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: WorldLanding pass=1 state=%d location=%s speed=%.3f"), int32(Relic->GetCurrentState()), *Relic->GetActorLocation().ToCompactString(), Speed(Relic));
			Publish(Grounded);
		}
		return;
	}
	if (Frame.Stage == AirFlight)
	{
		if (Relic->CurrentCarrier == Enemy.Pawn.Get())
		{
			if (Enemy.Player->GetInterceptions() != Enemy.Interceptions + 1) { Fail(TEXT("air-catch-credit")); return; }
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: AirCatch pass=1 enemy=%d delta=1"), Enemy.Player->GetPlayerId());
			Publish(AirCaught);
		}
		return;
	}
	if (Frame.Stage == GroundPickup)
	{
		if (Relic->CurrentCarrier != Enemy.Pawn.Get()) return;
		if (Enemy.Player->GetInterceptions() != Enemy.Interceptions) { Fail(TEXT("ground-pickup-awarded-interception")); return; }
		if (!State->bGroundVerified)
		{
			State->bGroundVerified = true;
			UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: GroundRecovery pass=1 enemy=%d delta=0"), Enemy.Player->GetPlayerId());
			Publish(GroundPickup); return;
		}
	}
	if (!bAllAcknowledged) return;
	if (Frame.Stage == Ready)
	{
		const float Maximum = Relic->GetRelicSettings()->MaxChargeTime;
		Frame.HoldSeconds = Frame.Scenario == 0 ? 0.1f : Frame.Scenario == 1 ? Maximum * 0.5f : Frame.Scenario == 2 ? Maximum + 0.35f
			: Frame.Scenario == 3 ? 1.7f : 2.5f + Relic->GetRelicSettings()->PickupCooldownAfterDrop;
		State->LaunchSpeed = 0.0f;
		State->LaunchObservation = {};
		State->LaunchAim = FVector::ZeroVector;
		State->LastLaunchPhysicsFrame = MAX_uint64;
		State->bAuthoritySawActive = false;
		State->bCancelRegranted = false;
		State->bOwnershipDropped = false;
		State->bObservedThrown = false;
		Publish(Input);
	}
	else if (Frame.Stage == Released)
	{
		++Frame.Scenario;
		AcquireSource();
	}
	else if (Frame.Stage == Cancelled)
	{
		if (Frame.Scenario == 3) { ++Frame.Scenario; AcquireSource(); return; }
		Source.Pawn->SetActorLocation(State->Floor + FVector(0, 0, 500), false, nullptr, ETeleportType::TeleportPhysics);
		Relic->Server_PassRelic(FVector(0, 0, -Relic->GetRelicSettings()->PassSpeed));
		if (!Relic->bIsPassInFlight || Relic->GetCurrentState() != ERelicState::BeingPassed) { Fail(TEXT("ground-pass-start")); return; }
		Publish(GroundFlight);
	}
	else if (Frame.Stage == Grounded)
	{
		Enemy.Pawn->SetActorLocation(Relic->GetActorLocation() + FVector(0, 0, 75), false, nullptr, ETeleportType::TeleportPhysics);
		Publish(Unprepared);
	}
	else if (Frame.Stage == Unprepared)
	{
		if (Relic->CurrentCarrier || Enemy.Player->GetInterceptions() != Enemy.Interceptions) { Fail(TEXT("unprepared-ground-auto-pickup")); return; }
		Enemy.Pawn->SetActorLocation(State->Floor + FVector(2000, 0, 120), false, nullptr, ETeleportType::TeleportPhysics);
		Enemy.Player->GetLyraAbilitySystemComponent()->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
		Enemy.Pawn->SetActorLocation(Relic->GetActorLocation() + FVector(0, 0, 75), false, nullptr, ETeleportType::TeleportPhysics);
		Publish(GroundPickup);
	}
	else if (Frame.Stage == GroundPickup)
	{
		Relic->OnDropped(); Relic->ClearScoringFlag();
		Source.Pawn->SetActorLocation(State->Floor + FVector(-500, 0, 500), false, nullptr, ETeleportType::TeleportPhysics);
		Enemy.Pawn->SetActorLocation(State->Floor + FVector(200, 0, 500), false, nullptr, ETeleportType::TeleportPhysics);
		Relic->OnPickedUp(Source.Pawn.Get());
		Relic->Server_PassRelic(FVector(Relic->GetRelicSettings()->PassSpeed, 0, 0));
		if (!Relic->bIsPassInFlight || Relic->CurrentCarrier) { Fail(TEXT("air-pass-start")); return; }
		Publish(AirFlight);
	}
	else if (Frame.Stage == AirCaught)
	{
		Cleanup();
		Publish(Restored);
	}
	else if (Frame.Stage == Restored) Publish(Complete);
	else if (Frame.Stage == Complete)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: COMPLETE pass=1 peers=4 scenarios=5 remoteOwner=%d groundedDelta=0 airborneDelta=1 cleanup=1"), Frame.Source);
		State->bDone = true;
		for (const auto& Weak : State->Witnesses) if (auto* Witness = Weak.Get()) Witness->SetLifeSpan(3.0f);
	}
}

void ABwayRelicFlightProbe::Cleanup()
{
	if (!State || !State->bCoordinator || State->bCleaned) return;
	State->bCleaned = true;
	if (State->PhysicsStepHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
			if (FPhysScene* Scene = World->GetPhysicsScene()) Scene->OnPhysScenePreTick.Remove(State->PhysicsStepHandle);
		State->PhysicsStepHandle.Reset();
	}
	for (const auto& Player : State->Players)
	{
		if (auto* ASC = Player.Player.IsValid() ? Player.Player->GetLyraAbilitySystemComponent() : nullptr)
		{
			FGameplayAbilitySpecHandle Handle; FGameplayTag Tag;
			if (BwayRelicFlight::ThrowSpecs(ASC, Handle, Tag) > 0) ASC->CancelAbilityHandle(Handle);
			ASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, Player.PreparingCount);
		}
		if (Player.Pawn.IsValid())
		{
			Player.Pawn->SetActorTransform(Player.Transform, false, nullptr, ETeleportType::TeleportPhysics);
			Player.Pawn->GetCharacterMovement()->SetMovementMode(Player.Mode, Player.CustomMode);
		}
		if (Player.Controller.IsValid()) Player.Controller->SetControlRotation(Player.Rotation);
		if (Player.Player.IsValid())
		{
			if (auto* Property = FindFProperty<FIntProperty>(ABwayPlayerState::StaticClass(), TEXT("Interceptions")))
				Property->SetPropertyValue_InContainer(Player.Player.Get(), Player.Interceptions);
			Player.Player->ForceNetUpdate();
			Player.Player->OnMatchStatsChanged.Broadcast();
		}
	}
	if (auto* Relic = BwayRelicFlight::RelicFor(GetWorld()))
	{
		Relic->BeginResettingState(); Relic->OnDropped();
		Relic->SetActorTransform(State->RelicTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (auto* Mesh = Cast<UPrimitiveComponent>(Relic->GetRootComponent())) Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Relic->ForceNetUpdate();
	}
	UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: Cleanup restoredPawns=%d callbacks=0"), State->Players.Num());
}

void ABwayRelicFlightProbe::Fail(const TCHAR* Reason)
{
	if (!State || State->bDone) return;
	UE_LOG(LogTemp, Error, TEXT("BwayRelicFlight: FAIL stage=%d scenario=%d reason=%s"), Frame.Stage, Frame.Scenario, Reason);
	Cleanup();
	if (State->bCoordinator) Publish(BwayRelicFlight::Failed);
	State->bDone = true;
}

void ABwayRelicFlightProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	if (!State || State->bDone) return;
	if (FPlatformTime::Seconds() - State->Started > 120.0) { Fail(TEXT("overall-timeout")); return; }
	if (HasAuthority() && !GetOwner())
	{
		Setup();
		return;
	}
	LocalTick();
	if (State->bCoordinator && !State->bDone) AuthorityTick();
#endif
}

void ABwayRelicFlightProbe::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
	if (State && State->bKeyDown)
		if (auto* Controller = Cast<APlayerController>(GetOwner()); Controller && Controller->IsLocalController())
			BwayRelicFlight::SendMouseInput(Controller, IE_Released, Frame.Scenario, TEXT("end-play"));
	if (State && State->bSavedLocalRotation)
		if (auto* Controller = Cast<APlayerController>(GetOwner()); Controller && Controller->IsLocalController())
			Controller->SetControlRotation(State->LocalRotation);
	Cleanup();
	if (!HasAuthority() && State && State->bDone && Frame.Stage == BwayRelicFlight::Complete
		&& FParse::Param(FCommandLine::Get(), TEXT("RelicFlightAutoExit"))) FPlatformMisc::RequestExit(false);
#endif
	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
namespace BwayRelicFlight
{
FTSTicker::FDelegateHandle StartupHandle;
FAutoConsoleCommand Command(TEXT("bway.Test.RelicFlight"), TEXT("Bounded four-player remote relic input and physical landing fixture; run on listen authority."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (StartupHandle.IsValid()) return;
		const double Started = FPlatformTime::Seconds();
		StartupHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Started, bSelected = false](float) mutable
		{
			if (GEngine)
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* World = Context.World();
					if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_ListenServer) continue;
					auto* GameState = World->GetGameState<ABwayGameState>();
					if (World->GetMapName().Contains(TEXT("HeroSelect_Staging")))
					{
						if (!bSelected && GameState && GameState->PlayerArray.Num() == 4 && GameState->HeroSelectionManager
							&& GameState->HeroSelectionManager->IsSelectionActive())
						{
							TArray<ABwayPlayerState*> Players;
							for (APlayerState* Entry : GameState->PlayerArray)
								if (auto* Player = Cast<ABwayPlayerState>(Entry); Player && !Player->IsABot()) Players.Add(Player);
							if (Players.Num() != 4) continue;
							Players.Sort([](const ABwayPlayerState& Left, const ABwayPlayerState& Right) { return Left.GetPlayerId() < Right.GetPlayerId(); });
							const TCHAR* Heroes[] = { TEXT("Argus"), TEXT("Rawlins"), TEXT("Alona"), TEXT("Korryn") };
							bSelected = true;
							for (int32 Index = 0; Index < Players.Num(); ++Index)
								if (!GameState->HeroSelectionManager->AssignHeroToPlayer(Players[Index], UBwayHeroRegistry::ResolveHeroIdByName(Heroes[Index]), true))
								{
									UE_LOG(LogTemp, Error, TEXT("BwayRelicFlight: FAIL reason=staging-selection"));
									StartupHandle.Reset(); return false;
								}
							UE_LOG(LogTemp, Display, TEXT("BwayRelicFlight: StagingLocked humans=4"));
							return true;
						}
						continue;
					}
					if (!World->GetMapName().Contains(TEXT("Dorado"))) continue;
					for (TActorIterator<ABwayRelicFlightProbe> Existing(World); Existing; ++Existing)
					{
						UE_LOG(LogTemp, Error, TEXT("BwayRelicFlight: FAIL reason=existing-probe"));
						StartupHandle.Reset(); return false;
					}
					World->SpawnActor<ABwayRelicFlightProbe>();
					StartupHandle.Reset(); return false;
				}
			if (FPlatformTime::Seconds() - Started > 120.0)
			{
				UE_LOG(LogTemp, Error, TEXT("BwayRelicFlight: FAIL reason=listen-world-timeout"));
				StartupHandle.Reset(); return false;
			}
			return true;
		}), 0.1f);
	}));
}
#endif
