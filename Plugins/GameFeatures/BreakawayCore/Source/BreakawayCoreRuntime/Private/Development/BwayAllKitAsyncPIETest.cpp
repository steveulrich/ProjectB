// Opt-in same-process PIE proof of real death/reset, replacement, and fresh owner use.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayHeroProjectileBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "LyraGameplayTags.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/ObjectKey.h"
#include "UObject/UnrealType.h"

namespace BwayAllKitAsyncPIE
{
struct FCase { const TCHAR* Family; const TCHAR* Duration; };
const FCase Cases[] = {
	{TEXT("BwayGameplayAbility_ArgusSlide"), TEXT("SlideDashDuration")},
	{TEXT("BwayGameplayAbility_NoRetreat"), TEXT("NoRetreatChargeDuration")},
	{TEXT("BwayGameplayAbility_Retribution"), TEXT("RetributionDownSmashDelay")},
	{TEXT("BwayGameplayAbility_AlonaSunsGrace"), TEXT("SunsGraceInvulnerabilityDuration")},
	{TEXT("BwayGameplayAbility_AlonaRadiance"), TEXT("RadianceActiveDuration")},
	{TEXT("BwayGameplayAbility_KorrynFlock"), TEXT("FlockDuration")},
	{TEXT("BwayGameplayAbility_RawlinsPrimary"), TEXT("PrimaryShotInterval")},
	{TEXT("BwayGameplayAbility_RawlinsDoubleDown"), TEXT("DoubleDownDashDuration")},
	{TEXT("BwayGameplayAbility_RawlinsSlideShot"), TEXT("SlideShotSlideDuration")}
};
FString Identity(const UObject* Object)
{
	FObjectKey Key(Object); TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); Writer << Key;
	return BytesToHex(Bytes.GetData(), Bytes.Num());
}
bool Family(const UClass* Class, int32 Case)
{
	for (; Class; Class = Class->GetSuperClass()) if (Class->GetFName() == FName(Cases[Case].Family)) return true;
	return false;
}
ABwayCharacterWithAbilities* Pawn(ABwayPlayerState* PS) { return PS ? Cast<ABwayCharacterWithAbilities>(PS->GetPawn()) : nullptr; }
ULyraAbilitySystemComponent* ASC(ABwayPlayerState* PS) { return PS ? PS->GetLyraAbilitySystemComponent() : nullptr; }
float Health(ABwayPlayerState* PS) { return ASC(PS) ? ASC(PS)->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.f; }
bool ClearTags(ULyraAbilitySystemComponent* A)
{
	return A && !A->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity)
		&& !A->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)
		&& !A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal)
		&& !A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced);
}
FGameplayAbilitySpec* UniqueSpec(ULyraAbilitySystemComponent* A, int32 Case)
{
	FGameplayAbilitySpec* Result = nullptr;
	if (A) for (auto& Spec : A->GetActivatableAbilities()) if (Spec.Ability && Family(Spec.Ability->GetClass(), Case))
	{
		if (Result) return nullptr;
		Result = &Spec;
	}
	return Result;
}
struct FPerson
{
	int32 Id = INDEX_NONE, Team = INDEX_NONE;
	TWeakObjectPtr<ABwayPlayerState> PS;
	TWeakObjectPtr<ULyraAbilitySystemComponent> Component;
	TWeakObjectPtr<ABwayCharacterWithAbilities> OldPawn;
	TWeakObjectPtr<AController> Controller;
	FObjectKey OldKey;
	FString OldIdentity, OldPath, NewIdentity;
	FTransform Transform;
	FVector Velocity, Pending;
	EMovementMode Mode = MOVE_Walking;
	uint8 Custom = 0;
	float Fly = 0, Walk = 0, Gravity = 0, InitialHealth = 0;
	bool bOrientToMovement = false;
	ECollisionResponse Collision = ECR_Block;
};
struct FControl
{
	TWeakObjectPtr<APlayerController> PC;
	FRotator Aim;
	bool bAddedMoveHold = false, bAddedLookHold = false;
};
struct FPeer
{
	TWeakObjectPtr<UWorld> World;
	TArray<FPerson> People;
	TWeakObjectPtr<AActor> Floor;
	int32 Source = INDEX_NONE, Target = INDEX_NONE;
	FGameplayAbilitySpecHandle OldHandle, NewHandle;
	TWeakObjectPtr<UClass> AbilityClass;
	FDelegateHandle Activated, Ended;
	int32 Activations[2] = {0, 0}, Ends[2] = {0, 0};
	int32 Keys[2] = {0, 0};
	FPredictionKey Prediction[2];
	TWeakObjectPtr<UGameplayAbility> Instance[2];
	bool Cancelled[2] = {false, false}, bFreshFlock = false;
	bool bOldEndRoute = false;
	double ActivatedAt[2] = {0, 0};
	FVector FreshPosition;
	float FreshDistance = 0, FreshSpeed = 0, FreshFly = 0, FreshWalk = 0, FreshGravity = 0;
	bool bFreshOrient = false;
	ECollisionResponse FreshCollision = ECR_Block;
	FPerson& SourcePerson() { return People[Source]; }
	FPerson& TargetPerson() { return People[Target]; }
};
struct FCooldown
{
	FActiveGameplayEffectHandle Handle;
	FDelegateHandle Removed;
	float End = 0, Duration = 0;
	bool bNatural = false;
};
struct FRequest
{
	FString Token, Failure;
	int32 Case = INDEX_NONE, SourceId = INDEX_NONE, TargetId = INDEX_NONE, Authority = INDEX_NONE, Owner = INDEX_NONE;
	bool bReset = false, bIntervened = false, bInterventionCall = false, bFresh = false, bCleaning = false, bAbort = false;
	int32 Stage = 0, RoundNumber = 0, OldShots = 0, FreshShots = 0, ExpectedShots = 0;
	float Duration = 0, FirstDeadline = 0, ProjectileLife = 0, QuietHealth = 0, RestoreSourceHealth = 0, FreshHealth = 0, TargetHealth = 0;
	double Started = 0, StageAt = 0, StableAt = 0, InterventionAt = 0, FreshAt = 0;
	TArray<FPeer> Peers;
	TArray<FControl> Controls;
	TArray<FCooldown> Cooldowns;
	TArray<TWeakObjectPtr<ABwayHeroProjectileBase>> Shots;
	TWeakObjectPtr<UBwayRoundManagementComponent> Round;
	FDelegateHandle Spawned;
	FTSTicker::FDelegateHandle Ticker;
	bool bCleanupInitial = false;
	int32 RemovedCallbacks = 0;
	double CleanupAt = 0;
};
TSharedPtr<FRequest> Active;
void Fail(FRequest& R, const TCHAR* Why) { if (R.Failure.IsEmpty()) R.Failure = Why; }
void Stage(FRequest& R, int32 Value) { R.Stage = Value; R.StageAt = FPlatformTime::Seconds(); R.StableAt = 0; }
bool Live(const FPerson& P)
{
	auto* C = Pawn(P.PS.Get()); auto* A = P.Component.Get();
	return P.PS.IsValid() && P.PS->GetTeamId() == P.Team && C && !C->IsActorBeingDestroyed() && A && ASC(P.PS.Get()) == A && A->GetOwnerActor() == P.PS.Get() && A->GetAvatarActor() == C
		&& C->GetLyraAbilitySystemComponent() == A && Health(P.PS.Get()) > 0
		&& (!P.Controller.IsValid() || (C->GetController() == P.Controller.Get() && P.Controller->GetPawn() == C && P.Controller->PlayerState == P.PS.Get()));
}
bool EndpointReady(FRequest& R, FPeer& P)
{
	if (!Live(P.SourcePerson())) return false;
	auto* A = P.SourcePerson().Component.Get(); auto* Spec = UniqueSpec(A, R.Case);
	return Spec && !Spec->IsActive() && A->AbilityActorInfo.IsValid() && Spec->Ability->CanActivateAbility(Spec->Handle, A->AbilityActorInfo.Get()) && ClearTags(A);
}
bool SpawnFloor(FRequest& R, FPeer& Peer)
{
	FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient; Params.bDeferConstruction = true;
	Params.Name = FName(*FString::Printf(TEXT("BwayAsyncFloor_%s"), *R.Token));
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
	auto* Actor = Peer.World->SpawnActor<AActor>(Params); if (!Actor) return false; Peer.Floor = Actor;
	Actor->SetNetAddressable();
	auto* Box = NewObject<UBoxComponent>(Actor, TEXT("AsyncFloorCollision")); Box->SetNetAddressable(); Actor->SetRootComponent(Box);
	Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetBoxExtent(FVector(8000, 8000, 50)); Box->RegisterComponent();
	const FTransform Transform(FRotator::ZeroRotator, FVector(0, 0, 9950));
	Actor->SetActorTransform(Transform); Actor->FinishSpawning(Transform);
	return Actor->GetActorTransform().Equals(Transform, .01f);
}
void HoldController(FRequest& R, APlayerController* PC)
{
	if (!PC || R.Controls.ContainsByPredicate([PC](const auto& V) { return V.PC.Get() == PC; })) return;
	auto& C = R.Controls.AddDefaulted_GetRef(); C.PC = PC; C.Aim = PC->GetControlRotation();
	C.bAddedMoveHold = !PC->IsMoveInputIgnored(); C.bAddedLookHold = !PC->IsLookInputIgnored();
	if (C.bAddedMoveHold) PC->SetIgnoreMoveInput(true); if (C.bAddedLookHold) PC->SetIgnoreLookInput(true);
}
void Place(FRequest& R)
{
	for (auto& Peer : R.Peers) for (int32 I = 0; I < Peer.People.Num(); ++I)
	{
		auto& Person = Peer.People[I]; auto* C = Pawn(Person.PS.Get()); if (!C) continue;
		FVector Position(0, 0, 10000 + C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2);
		if (I != Peer.Source) Position += (R.Case == 2 && I == Peer.Target) ? FVector(160, 0, 0) : FVector(-2000, 2000 + 750 * I, 0);
		C->SetActorLocationAndRotation(Position, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		auto* M = C->GetCharacterMovement(); M->StopMovementImmediately(); M->PendingLaunchVelocity = FVector::ZeroVector; M->SetMovementMode(MOVE_Walking);
		if (auto* PC = Cast<APlayerController>(C->GetController())) { HoldController(R, PC); PC->SetControlRotation(FRotator::ZeroRotator); }
		C->ForceNetUpdate();
	}
}
bool AllPlaced(FRequest& R)
{
	for (auto& Peer : R.Peers) for (int32 I = 0; I < Peer.People.Num(); ++I)
	{
		if (!Live(Peer.People[I])) return false;
		auto* C = Pawn(Peer.People[I].PS.Get());
		const FVector Desired = I == Peer.Source ? FVector(0, 0, 10000 + C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
			: FVector(R.Case == 2 && I == Peer.Target ? 160 : -2000, R.Case == 2 && I == Peer.Target ? 0 : 2000 + 750 * I, 10000 + C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (FVector::Dist(C->GetActorLocation(), Desired) > 8 || C->GetVelocity().Size() > 5) return false;
	}
	return true;
}
bool ReadTuning(FRequest& R)
{
	auto* Spec = UniqueSpec(R.Peers[R.Authority].SourcePerson().Component.Get(), R.Case);
	const auto* Property = Spec ? FindFProperty<FSoftObjectProperty>(Spec->Ability->GetClass(), TEXT("KitConfig")) : nullptr;
	auto* Config = Property ? Property->GetPropertyValue_InContainer(Spec->Ability.Get()).LoadSynchronous() : nullptr;
	auto Read = [Config](const TCHAR* Name) { const auto* P = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), Name) : nullptr; return P ? P->GetPropertyValue_InContainer(Config) : -1.f; };
	R.Duration = Read(Cases[R.Case].Duration); R.FirstDeadline = R.Duration;
	if (R.Case == 2) { R.FirstDeadline = Read(TEXT("RetributionUppercutDelay")); R.Duration += .05f; }
	if (R.Case == 4) R.FirstDeadline = Read(TEXT("RadianceHealTickInterval"));
	if (R.Case == 6)
	{
		const auto* Count = Config ? FindFProperty<FIntProperty>(Config->GetClass(), TEXT("PrimaryShotCount")) : nullptr;
		R.ExpectedShots = Count ? Count->GetPropertyValue_InContainer(Config) : 0;
		R.ProjectileLife = Read(TEXT("PrimaryProjectileLifeSpan")); R.Duration *= FMath::Max(R.ExpectedShots - 1, 1);
		if (R.ExpectedShots < 2 || R.ExpectedShots > 8 || R.ProjectileLife <= 0 || R.ProjectileLife > 10) return false;
	}
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Plan token=%s case=%d trigger=%s source=%d target=%d family=%s config=%s duration=%.6f firstDeadline=%.6f shots=%d life=%.6f peers=4 guard=%d"),
		*R.Token, R.Case, R.bReset ? TEXT("reset") : TEXT("death"), R.SourceId, R.TargetId, Cases[R.Case].Family, *GetPathNameSafe(Config), R.Duration, R.FirstDeadline, R.ExpectedShots, R.ProjectileLife, GAllowActorScriptExecutionInEditor);
	return Config && R.Duration > 0 && R.Duration <= 10 && R.FirstDeadline >= .03f;
}
void Bind(FRequest& R)
{
	const TWeakPtr<FRequest> Weak = Active;
	for (int32 Index : {R.Owner, R.Authority})
	{
		auto& Peer = R.Peers[Index]; auto* A = Peer.SourcePerson().Component.Get();
		Peer.Activated = A->AbilityActivatedCallbacks.AddLambda([Weak, Index](UGameplayAbility* Ability)
		{
			auto P = Weak.Pin(); if (!P || P->bCleaning || !Ability || !Family(Ability->GetClass(), P->Case)) return;
			auto& E = P->Peers[Index]; const int32 Cycle = P->bFresh ? 1 : 0;
			const auto Handle = Ability->GetCurrentAbilitySpecHandle(); const auto Expected = Cycle ? E.NewHandle : E.OldHandle;
			if (Handle != Expected) { Fail(*P, TEXT("activation-wrong-spec")); return; }
			++E.Activations[Cycle]; E.Prediction[Cycle] = Ability->GetCurrentActivationInfo().GetActivationPredictionKey(); E.Keys[Cycle] = E.Prediction[Cycle].Current;
			E.Instance[Cycle] = Ability;
			if (Ability->GetAvatarActorFromActorInfo() != Pawn(E.SourcePerson().PS.Get()) || E.Activations[Cycle] != 1) Fail(*P, TEXT("activation-identity-or-count"));
			E.ActivatedAt[Cycle] = E.World->GetTimeSeconds();
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Activation token=%s cycle=%d peer=%d endpoint=%s pawn=%s key=%s handle=%s prediction=%d count=%d worldTime=%.6f guard=%d"),
				*P->Token, Cycle, Index, Index == P->Owner ? TEXT("owner") : TEXT("authority"), *GetPathNameSafe(Pawn(E.SourcePerson().PS.Get())), *Identity(Pawn(E.SourcePerson().PS.Get())), *Handle.ToString(), E.Keys[Cycle], E.Activations[Cycle], E.World->GetTimeSeconds(), GAllowActorScriptExecutionInEditor);
		});
		Peer.Ended = A->OnAbilityEnded.AddLambda([Weak, Index](const FAbilityEndedData& Data)
		{
			auto P = Weak.Pin(); if (!P || P->bCleaning) return; auto& E = P->Peers[Index];
			int32 Cycle = Data.AbilitySpecHandle == E.OldHandle ? 0 : (Data.AbilitySpecHandle == E.NewHandle ? 1 : INDEX_NONE); if (Cycle == INDEX_NONE) return;
			++E.Ends[Cycle]; E.Cancelled[Cycle] = Data.bWasCancelled;
			auto* Ability = Data.AbilityThatEnded.Get(); auto* A = E.SourcePerson().Component.Get();
			const bool Exact = Ability && Ability == E.Instance[Cycle].Get() && Ability->GetCurrentActivationInfo().GetActivationPredictionKey() == E.Prediction[Cycle];
			const bool Detached = A && FObjectKey(A->GetAvatarActor()) != E.SourcePerson().OldKey;
			const bool InWindow = P->bIntervened && (Index == P->Authority ? P->bInterventionCall : FPlatformTime::Seconds() - P->InterventionAt <= .75);
			// ClearAbility -> OnRemoveAbility legitimately ends with cancelled=false after avatar detachment.
			if (Cycle == 0) E.bOldEndRoute = Exact && InWindow && (Data.bWasCancelled || Detached);
			if (!Exact || E.Ends[Cycle] != 1 || (Cycle == 0 && !E.bOldEndRoute)) Fail(*P, TEXT("unexpected-ability-end-route"));
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: End token=%s cycle=%d peer=%d handle=%s count=%d cancelled=%d intervened=%d exact=%d detached=%d withinWindow=%d endRoute=%d worldTime=%.6f guard=%d"),
				*P->Token, Cycle, Index, *Data.AbilitySpecHandle.ToString(), E.Ends[Cycle], Data.bWasCancelled, P->bIntervened, Exact, Detached, InWindow, Cycle ? Exact && !Data.bWasCancelled : E.bOldEndRoute, E.World->GetTimeSeconds(), GAllowActorScriptExecutionInEditor);
		});
	}
	R.Spawned = R.Peers[R.Authority].World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([Weak](AActor* Actor)
	{
		auto P = Weak.Pin(); auto* Shot = Cast<ABwayHeroProjectileBase>(Actor); if (!P || P->bCleaning || !Shot) return;
		auto& S = P->Peers[P->Authority].SourcePerson();
		const auto* Owner = Shot->GetOwner();
		if (!Owner) { Fail(*P, TEXT("unattributed-projectile-during-isolated-fixture")); return; }
		const bool Old = FObjectKey(Owner) == S.OldKey;
		const bool Fresh = P->bFresh && !S.NewIdentity.IsEmpty() && Identity(Owner) == S.NewIdentity;
		if (!Old && Owner == Pawn(S.PS.Get()) && !Fresh)
		{
			P->Shots.Add(Shot); Fail(*P, TEXT("replacement-spawned-before-fresh-activation")); return;
		}
		if (!Old && !Fresh) return;
		P->Shots.Add(Shot); if (Fresh) ++P->FreshShots; else ++P->OldShots;
		if (Old && P->bIntervened) Fail(*P, TEXT("old-activation-spawned-after-intervention"));
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Shot token=%s cycle=%d actor=%s key=%s owner=%s afterIntervention=%d oldShots=%d freshShots=%d worldTime=%.6f"),
			*P->Token, Fresh ? 1 : 0, *Shot->GetPathName(), *Identity(Shot), *GetPathNameSafe(Shot->GetOwner()), P->bIntervened, P->OldShots, P->FreshShots, Shot->GetWorld()->GetTimeSeconds());
	}));
}
bool Setup(FRequest& R)
{
	if (!GEngine) return false;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		auto* W = Context.World(); if (!W || !W->IsGameWorld() || (W->GetNetMode() != NM_Client && W->GetNetMode() != NM_ListenServer)) continue;
		auto* GS = W->GetGameState<ABwayGameState>(); auto* Round = GS ? GS->GetRoundManagement() : nullptr;
		if (!GS || GS->PlayerArray.Num() != 4 || !Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundState() != ERoundState::RoundActive) return false;
		const int32 Index = R.Peers.AddDefaulted(); auto& Peer = R.Peers[Index]; Peer.World = W;
		for (APlayerState* State : GS->PlayerArray)
		{
			auto* PS = Cast<ABwayPlayerState>(State); auto* C = Pawn(PS); auto* A = ASC(PS);
			if (!PS || PS->IsABot() || !C || !A || A->GetAvatarActor() != C || !ClearTags(A) || Health(PS) <= 0 || !C->GetCharacterMovement()) return false;
			auto& P = Peer.People.AddDefaulted_GetRef(); P.Id = PS->GetPlayerId(); P.Team = PS->GetTeamId(); P.PS = PS; P.Component = A; P.OldPawn = C; P.OldKey = FObjectKey(C); P.Controller = C->GetController();
			P.OldIdentity = Identity(C); P.OldPath = C->GetPathName(); P.Transform = C->GetActorTransform(); P.InitialHealth = Health(PS);
			auto* M = C->GetCharacterMovement(); P.Velocity = M->Velocity; P.Pending = M->PendingLaunchVelocity; P.Mode = M->MovementMode; P.Custom = M->CustomMovementMode;
			P.Fly = M->MaxFlySpeed; P.Walk = M->MaxWalkSpeed; P.Gravity = M->GravityScale; P.bOrientToMovement = M->bOrientRotationToMovement;
			P.Collision = C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
			if (M->MovementMode != MOVE_Walking || M->Velocity.Size() > 1.f || !M->PendingLaunchVelocity.IsNearlyZero(.01f)) return false;
		}
		Peer.People.Sort([](const FPerson& A, const FPerson& B) { return A.Id < B.Id; });
		Peer.Source = Peer.People.IndexOfByPredicate([&R](const auto& P) { return P.Id == R.SourceId; }); if (Peer.Source == INDEX_NONE) return false;
		if (W->GetNetMode() == NM_ListenServer)
		{
			if (R.Authority != INDEX_NONE || !W->GetAuthGameMode()) return false; R.Authority = Index; R.Round = Round; R.RoundNumber = Round->GetCurrentRoundNumber();
			for (const auto& P : Peer.People) if (P.Id != R.SourceId && P.PS->GetTeamId() != Peer.SourcePerson().PS->GetTeamId()) { R.TargetId = P.Id; break; }
		}
		else if (Pawn(Peer.SourcePerson().PS.Get())->IsLocallyControlled()) { if (R.Owner != INDEX_NONE) return false; R.Owner = Index; }
		const auto* D = W->GetNetDriver(); if (!D) return false;
		const auto& N = D->PacketSimulationSettings;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Network token=%s peer=%d world=%s mode=%d lag=%d lagMin=%d lagMax=%d incomingMin=%d incomingMax=%d loss=%d incomingLoss=%d jitter=%d variance=%d"),
			*R.Token, Index, *W->GetPathName(), int32(W->GetNetMode()), N.PktLag, N.PktLagMin, N.PktLagMax, N.PktIncomingLagMin, N.PktIncomingLagMax, N.PktLoss, N.PktIncomingLoss, N.PktJitter, N.PktLagVariance);
		if (N.PktLag != 0 || N.PktLoss || N.PktIncomingLoss || N.PktJitter || N.PktLagVariance || N.PktLagMin || N.PktLagMax || N.PktIncomingLagMin || N.PktIncomingLagMax) return false;
	}
	if (R.Peers.Num() != 4 || R.Owner == INDEX_NONE || R.Authority == INDEX_NONE || R.TargetId == INDEX_NONE) return false;
	for (auto& Peer : R.Peers)
	{
		Peer.Target = Peer.People.IndexOfByPredicate([&R](const auto& P) { return P.Id == R.TargetId; }); if (Peer.Target == INDEX_NONE) return false;
		for (const auto& P : Peer.People)
		{
			const auto* S = R.Peers[R.Authority].People.FindByPredicate([&P](const auto& V) { return V.Id == P.Id; });
			if (!S || P.PS->GetTeamId() != S->PS->GetTeamId() || !Live(P)) return false;
		}
		if (!SpawnFloor(R, Peer)) return false;
		for (const auto& P : Peer.People) HoldController(R, Cast<APlayerController>(Pawn(P.PS.Get())->GetController()));
	}
	for (int32 I : {R.Owner, R.Authority})
	{
		auto& P = R.Peers[I]; if (!EndpointReady(R, P)) return false;
		auto* Spec = UniqueSpec(P.SourcePerson().Component.Get(), R.Case); P.OldHandle = Spec->Handle; P.AbilityClass = Spec->Ability->GetClass();
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Endpoint token=%s peer=%d endpoint=%s player=%d pawn=%s key=%s asc=%s controller=%s handle=%s class=%s round=%d"),
			*R.Token, I, I == R.Owner ? TEXT("owner") : TEXT("authority"), R.SourceId, *P.SourcePerson().OldPath, *P.SourcePerson().OldIdentity,
			*GetPathNameSafe(P.SourcePerson().Component.Get()), *GetPathNameSafe(Pawn(P.SourcePerson().PS.Get())->GetController()), *P.OldHandle.ToString(), *Spec->Ability->GetClass()->GetPathName(), R.RoundNumber);
	}
	if (R.Peers[R.Owner].AbilityClass != R.Peers[R.Authority].AbilityClass) return false;
	if (!ReadTuning(R)) return false; Bind(R); Place(R); Stage(R, 1); return true;
}
void CaptureCooldown(FRequest& R)
{
	auto* A = R.Peers[R.Authority].SourcePerson().Component.Get(); auto* Spec = UniqueSpec(A, R.Case);
	const auto* Tags = Spec ? Spec->Ability->GetCooldownTags() : nullptr;
	if (!Spec) { Fail(R, TEXT("cooldown-spec-missing")); return; }
	if (!Tags || Tags->IsEmpty()) { if (R.Case != 6) Fail(R, TEXT("authored-cooldown-tags-missing")); return; }
	const auto Handles = A->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*Tags));
	if (R.Case != 6 && Handles.Num() != 1) { Fail(R, TEXT("unique-committed-cooldown-required")); return; }
	const TWeakPtr<FRequest> Weak = Active;
	for (const auto Handle : Handles)
	{
		const auto* E = A->GetActiveGameplayEffect(Handle); if (!E || E->GetDuration() <= 0) { Fail(R, TEXT("invalid-carried-cooldown")); return; }
		auto& C = R.Cooldowns.AddDefaulted_GetRef(); C.Handle = Handle; C.End = E->GetEndTime(); C.Duration = E->GetDuration();
		if (auto* Removed = A->OnGameplayEffectRemoved_InfoDelegate(Handle)) C.Removed = Removed->AddLambda([Weak, Handle](const FGameplayEffectRemovalInfo& Info)
		{
			auto P = Weak.Pin(); if (!P || P->bCleaning) return;
			auto* C = P->Cooldowns.FindByPredicate([Handle](const auto& V) { return V.Handle == Handle; }); if (!C) return;
			const double Now = P->Peers[P->Authority].World->GetTimeSeconds(); C->bNatural = !Info.bPrematureRemoval && !Info.bPredictionRejected && Now + .01 >= C->End;
			if (!C->bNatural) Fail(*P, TEXT("carried-cooldown-removed-prematurely"));
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: CooldownExpired token=%s handle=%s natural=%d premature=%d predictionRejected=%d plannedEnd=%.6f worldTime=%.6f"),
				*P->Token, *Handle.ToString(), C->bNatural, Info.bPrematureRemoval, Info.bPredictionRejected, C->End, Now);
		});
		if (!C.Removed.IsValid()) { Fail(R, TEXT("cooldown-removal-observer-missing")); return; }
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Cooldown token=%s handle=%s duration=%.6f plannedEnd=%.6f"), *R.Token, *Handle.ToString(), C.Duration, C.End);
	}
}
bool Replacements(FRequest& R, bool bLog)
{
	bool Ready = true;
	for (int32 Index = 0; Index < R.Peers.Num(); ++Index) for (auto& P : R.Peers[Index].People)
	{
		auto* C = Pawn(P.PS.Get()); const bool Changed = R.bReset || P.Id == R.SourceId;
		if (!Live(P) || (Changed ? FObjectKey(C) == P.OldKey : FObjectKey(C) != P.OldKey)) { Ready = false; continue; }
		if (Changed && P.OldPawn.IsValid() && !P.OldPawn->IsActorBeingDestroyed()) { Ready = false; continue; }
		const FString Key = Identity(C); if (!P.NewIdentity.IsEmpty() && P.NewIdentity != Key) { Fail(R, TEXT("unexpected-third-pawn")); return false; }
		if (bLog)
		{
			P.NewIdentity = Key;
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Replacement token=%s peer=%d player=%d changed=%d oldPawn=%s oldKey=%s pawn=%s key=%s asc=%s avatar=%s health=%.6f"),
				*R.Token, Index, P.Id, Changed, *P.OldPath, *P.OldIdentity, *C->GetPathName(), *Key, *GetPathNameSafe(P.Component.Get()), *GetPathNameSafe(P.Component->GetAvatarActor()), Health(P.PS.Get()));
		}
	}
	for (int32 Index : {R.Owner, R.Authority})
	{
		auto& P = R.Peers[Index]; auto* A = P.SourcePerson().Component.Get(); auto* Spec = UniqueSpec(A, R.Case);
		if (A->FindAbilitySpecFromHandle(P.OldHandle) || !Spec || Spec->Ability->GetClass() != P.AbilityClass.Get() || Spec->Handle == P.OldHandle || Spec->IsActive() || P.Ends[0] != 1 || !P.bOldEndRoute || !ClearTags(A)) Ready = false;
		else if (bLog)
		{
			P.NewHandle = Spec->Handle;
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Grant token=%s peer=%d oldHandle=%s handle=%s class=%s sameClass=1 oldSpec=0 active=0"), *R.Token, Index, *P.OldHandle.ToString(), *P.NewHandle.ToString(), *P.AbilityClass->GetPathName());
		}
	}
	return Ready;
}
void ObserveFresh(FRequest& R)
{
	for (int32 I : {R.Owner, R.Authority})
	{
		auto& P = R.Peers[I]; auto* C = Pawn(P.SourcePerson().PS.Get()); if (!C) continue; auto* M = C->GetCharacterMovement();
		P.FreshDistance = FMath::Max(P.FreshDistance, float(FVector::Dist2D(C->GetActorLocation(), P.FreshPosition)));
		P.FreshSpeed = FMath::Max(P.FreshSpeed, float(FMath::Max(M->Velocity.Size(), M->PendingLaunchVelocity.Size())));
		P.bFreshFlock |= M->MovementMode == MOVE_Flying && C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore
			&& P.SourcePerson().Component->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity);
	}
}
bool FreshOutcome(FRequest& R)
{
	auto& Server = R.Peers[R.Authority];
	for (int32 I : {R.Owner, R.Authority})
	{
		auto& P = R.Peers[I]; auto* A = P.SourcePerson().Component.Get(); auto* Spec = A->FindAbilitySpecFromHandle(P.NewHandle); auto* C = Pawn(P.SourcePerson().PS.Get());
		if (!Spec || Spec->IsActive() || P.Activations[1] != 1 || P.Ends[1] != 1 || P.Cancelled[1] || !ClearTags(A) || !C) return false;
		auto* M = C->GetCharacterMovement();
		if (R.Case == 5 && (!P.bFreshFlock || !FMath::IsNearlyEqual(M->MaxFlySpeed, P.FreshFly) || !FMath::IsNearlyEqual(M->MaxWalkSpeed, P.FreshWalk)
			|| !FMath::IsNearlyEqual(M->GravityScale, P.FreshGravity) || M->MovementMode != MOVE_Walking || M->CustomMovementMode != 0
			|| M->bOrientRotationToMovement != P.bFreshOrient || C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) != P.FreshCollision)) return false;
		if ((R.Case == 0 || R.Case == 1 || R.Case == 3 || R.Case == 7 || R.Case == 8) && P.FreshDistance <= 100) return false;
	}
	if (R.Case == 2 && Health(Server.TargetPerson().PS.Get()) >= R.TargetHealth - .1f) return false;
	if (R.Case == 4 && Health(Server.SourcePerson().PS.Get()) <= R.FreshHealth + .1f) return false;
	if (R.Case == 6 && R.FreshShots != R.ExpectedShots) return false;
	if (Server.Prediction[1] != R.Peers[R.Owner].Prediction[1] || !Server.Prediction[1].IsValidKey()) return false;
	for (const auto& Shot : R.Shots) if (Shot.IsValid() && !Shot->IsActorBeingDestroyed()) return false;
	for (auto& Peer : R.Peers) for (const auto& P : Peer.People)
	{
		const auto* S = Server.People.FindByPredicate([&P](const auto& V) { return V.Id == P.Id; });
		if (!S || !Live(P) || !FMath::IsNearlyEqual(Health(P.PS.Get()), Health(S->PS.Get()), .1f)) return false;
	}
	return true;
}
bool Finish(FRequest& R)
{
	if (R.bCleaning)
	{
		bool Settled = R.bCleanupInitial;
		for (const auto& Shot : R.Shots) Settled &= !Shot.IsValid() || Shot->IsActorBeingDestroyed();
		for (const auto& Peer : R.Peers) Settled &= !Peer.Floor.IsValid() || Peer.Floor->IsActorBeingDestroyed();
		for (const auto& Peer : R.Peers) for (const auto& P : Peer.People)
		{
			auto* C = Pawn(P.PS.Get()); if (!C || C->IsActorBeingDestroyed()) { Settled = false; continue; }
			auto* M = C->GetCharacterMovement();
			Settled &= C->GetActorLocation().Equals(P.Transform.GetLocation(), 5.f) && M->Velocity.Equals(P.Velocity, 5.f)
				&& M->MovementMode == P.Mode && M->CustomMovementMode == P.Custom && FMath::IsNearlyEqual(M->MaxFlySpeed, P.Fly)
				&& FMath::IsNearlyEqual(M->MaxWalkSpeed, P.Walk) && FMath::IsNearlyEqual(M->GravityScale, P.Gravity)
				&& M->bOrientRotationToMovement == P.bOrientToMovement && C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == P.Collision;
			if (P.Id == R.SourceId && R.Stage >= 4) Settled &= FMath::IsNearlyEqual(Health(P.PS.Get()), R.RestoreSourceHealth, .1f) && ClearTags(P.Component.Get());
		}
		const double Age = FPlatformTime::Seconds() - R.CleanupAt;
		if (Age < .5 || (!Settled && Age < 3)) return true;
		for (int32 I = 0; I < R.Peers.Num(); ++I)
		{
			const auto& P = R.Peers[I];
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Cleanup token=%s peer=%d restored=%d players=%d floorAlive=%d"), *R.Token, I, Settled, P.People.Num(), P.Floor.IsValid() && !P.Floor->IsActorBeingDestroyed());
		}
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Complete token=%s case=%d trigger=%s pass=%d cleanup=%d delegatesRemoved=%d cooldownWrites=0 statsRestored=0 oldShots=%d freshShots=%d reason=%s guard=%d"),
			*R.Token, R.Case, R.bReset ? TEXT("reset") : TEXT("death"), R.Failure.IsEmpty() && Settled, Settled, R.RemovedCallbacks, R.OldShots, R.FreshShots, R.Failure.IsEmpty() ? TEXT("observed") : *R.Failure, GAllowActorScriptExecutionInEditor);
		Active.Reset(); return false;
	}
	R.bCleaning = true; int32 Removed = 0; bool Restored = true;
	for (auto& Peer : R.Peers)
	{
		if (Peer.Source != INDEX_NONE) if (auto* A = Peer.SourcePerson().Component.Get())
		{
			if (Peer.Activated.IsValid()) { A->AbilityActivatedCallbacks.Remove(Peer.Activated); ++Removed; }
			if (Peer.Ended.IsValid()) { A->OnAbilityEnded.Remove(Peer.Ended); ++Removed; }
			// Failure cleanup cancels only this fixture's exact current activation. Cooldowns are never removed.
			for (auto H : {Peer.OldHandle, Peer.NewHandle}) if (H.IsValid()) if (const auto* S = A->FindAbilitySpecFromHandle(H); S && S->IsActive()) A->CancelAbilityHandle(H);
		}
		for (const auto& P : Peer.People)
		{
			auto* C = Pawn(P.PS.Get()); if (!C || C->IsActorBeingDestroyed()) { Restored = false; continue; }
			auto* M = C->GetCharacterMovement(); C->SetActorTransform(P.Transform, false, nullptr, ETeleportType::TeleportPhysics);
			M->StopMovementImmediately(); M->PendingLaunchVelocity = P.Pending; M->SetMovementMode(P.Mode, P.Custom); M->Velocity = P.Velocity;
			// Only the fixture's source health deficit is restored, on a living current pawn.
			if (R.Authority != INDEX_NONE && Peer.World == R.Peers[R.Authority].World && P.Id == R.SourceId && Live(P) && R.Stage >= 4)
			{ P.Component->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), R.RestoreSourceHealth); P.PS->ForceNetUpdate(); }
			C->ForceNetUpdate();
			Restored &= C->GetActorTransform().Equals(P.Transform, .01f) && M->Velocity.Equals(P.Velocity, .01f) && M->PendingLaunchVelocity.Equals(P.Pending, .01f)
				&& M->MovementMode == P.Mode && M->CustomMovementMode == P.Custom && FMath::IsNearlyEqual(M->MaxFlySpeed, P.Fly)
				&& FMath::IsNearlyEqual(M->MaxWalkSpeed, P.Walk) && FMath::IsNearlyEqual(M->GravityScale, P.Gravity) && M->bOrientRotationToMovement == P.bOrientToMovement
				&& C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == P.Collision;
		}
	}
	if (R.Authority != INDEX_NONE)
	{
		auto& Peer = R.Peers[R.Authority]; if (Peer.World.IsValid() && R.Spawned.IsValid()) { Peer.World->RemoveOnActorSpawnedHandler(R.Spawned); ++Removed; }
		if (auto* A = Peer.SourcePerson().Component.Get()) for (auto& C : R.Cooldowns) if (C.Removed.IsValid()) if (auto* D = A->OnGameplayEffectRemoved_InfoDelegate(C.Handle)) { D->Remove(C.Removed); ++Removed; }
	}
	for (const auto& Shot : R.Shots) if (Shot.IsValid() && !Shot->IsActorBeingDestroyed()) Shot->Destroy();
	for (auto& C : R.Controls) if (C.PC.IsValid())
	{
		C.PC->SetControlRotation(C.Aim); if (C.bAddedMoveHold) C.PC->SetIgnoreMoveInput(false); if (C.bAddedLookHold) C.PC->SetIgnoreLookInput(false);
		Restored &= C.PC->GetControlRotation().Equals(C.Aim, .01f) && C.PC->IsMoveInputIgnored() == !C.bAddedMoveHold && C.PC->IsLookInputIgnored() == !C.bAddedLookHold;
	}
	for (auto& Peer : R.Peers) if (Peer.Floor.IsValid()) Restored &= Peer.Floor->Destroy();
	R.bCleanupInitial = Restored; R.RemovedCallbacks = Removed; R.CleanupAt = FPlatformTime::Seconds(); return true;
}
bool Tick(float)
{
	const auto Keep = Active; if (!Keep) return false; auto& R = *Keep; const double Now = FPlatformTime::Seconds();
	if (GAllowActorScriptExecutionInEditor) return true;
	if (R.bCleaning) return Finish(R);
	if (R.bAbort) Fail(R, TEXT("abort")); if (Now - R.Started > 90) Fail(R, TEXT("overall-timeout"));
	if (!R.Failure.IsEmpty()) return Finish(R);
	if (R.Stage == 0) { if (!Setup(R)) { Fail(R, TEXT("requires-ready-four-world-zero-lag-remote-owner")); return Finish(R); } return true; }
	if (!R.Round.IsValid() || R.Round->GetCurrentRoundNumber() != R.RoundNumber || R.Round->GetCurrentRoundState() != ERoundState::RoundActive || R.Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
	{ Fail(R, TEXT("unexpected-round-transition")); return Finish(R); }
	auto& Server = R.Peers[R.Authority]; auto& Owner = R.Peers[R.Owner];
	for (const auto& C : R.Cooldowns)
	{
		const auto* E = Server.SourcePerson().Component.IsValid() ? Server.SourcePerson().Component->GetActiveGameplayEffect(C.Handle) : nullptr;
		if (E && (!FMath::IsNearlyEqual(E->GetEndTime(), C.End, .001f) || !FMath::IsNearlyEqual(E->GetDuration(), C.Duration, .001f))) Fail(R, TEXT("carried-cooldown-original-expiry-changed"));
		if (!E && !C.bNatural) Fail(R, TEXT("carried-cooldown-disappeared-without-natural-expiry"));
	}
	if (!R.Failure.IsEmpty()) return true;
	if (R.Stage == 1 || R.Stage == 5)
	{
		if (!AllPlaced(R)) { R.StableAt = 0; if (Now - R.StageAt > 8) Fail(R, TEXT("placement-timeout")); return true; }
		if (!EndpointReady(R, Server) || !EndpointReady(R, Owner)) { if (Now - R.StageAt > 45) Fail(R, TEXT("natural-cooldown-not-ready")); return true; }
		if (!R.StableAt) R.StableAt = Now; if (Now - R.StableAt < .3) return true;
		R.bFresh = R.Stage == 5;
		if (R.bFresh)
		{
			for (const auto& C : R.Cooldowns) if (!C.bNatural) { Fail(R, TEXT("fresh-before-captured-cooldown-expiry")); return true; }
			R.FreshHealth = Health(Server.SourcePerson().PS.Get()); R.TargetHealth = Health(Server.TargetPerson().PS.Get()); R.FreshAt = Now;
			for (int32 I : {R.Owner, R.Authority})
			{
				auto& P = R.Peers[I]; auto* C = Pawn(P.SourcePerson().PS.Get()); auto* M = C->GetCharacterMovement(); P.FreshPosition = C->GetActorLocation();
				P.FreshFly = M->MaxFlySpeed; P.FreshWalk = M->MaxWalkSpeed; P.FreshGravity = M->GravityScale; P.bFreshOrient = M->bOrientRotationToMovement;
				P.FreshCollision = C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
			}
		}
		const auto Handle = R.bFresh ? Owner.NewHandle : Owner.OldHandle;
		const bool Accepted = Owner.SourcePerson().Component->TryActivateAbility(Handle);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Dispatch token=%s cycle=%d source=%d peer=%d handle=%s accepted=%d ownerNetMode=%d guard=%d"), *R.Token, R.bFresh ? 1 : 0, R.SourceId, R.Owner, *Handle.ToString(), Accepted, int32(Owner.World->GetNetMode()), GAllowActorScriptExecutionInEditor);
		if (!Accepted) { Fail(R, TEXT("owner-activation-rejected")); return true; }
		if (R.bFresh) ObserveFresh(R); Stage(R, R.bFresh ? 6 : 2); return true;
	}
	if (R.Stage == 2)
	{
		if (Server.Activations[0] == 0) { if (Now - R.StageAt > 2) Fail(R, TEXT("authority-activation-timeout")); return true; }
		auto* Spec = Server.SourcePerson().Component->FindAbilitySpecFromHandle(Server.OldHandle);
		const double Age = Server.World->GetTimeSeconds() - Server.ActivatedAt[0];
		if (!Spec || !Spec->IsActive() || Server.Activations[0] != 1 || Owner.Activations[0] != 1 || Owner.Prediction[0] != Server.Prediction[0] || !Server.Prediction[0].IsValidKey() || Age >= R.FirstDeadline)
		{ Fail(R, TEXT("missed-active-interruption-window")); return true; }
		if (R.Case == 6 && R.OldShots != 1) { Fail(R, TEXT("first-shot-required")); return true; }
		CaptureCooldown(R); if (!R.Failure.IsEmpty()) return true;
		R.bIntervened = true; R.InterventionAt = Now;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Intervention token=%s trigger=%s pawn=%s key=%s handle=%s active=1 age=%.6f deadline=%.6f guard=%d"),
			*R.Token, R.bReset ? TEXT("reset") : TEXT("death"), *Server.SourcePerson().OldPath, *Server.SourcePerson().OldIdentity, *Server.OldHandle.ToString(), Age, R.FirstDeadline, GAllowActorScriptExecutionInEditor);
		R.bInterventionCall = true;
		if (R.bReset) R.Round->ResetRoundState();
		else if (auto* H = ULyraHealthComponent::FindHealthComponent(Server.SourcePerson().OldPawn.Get()))
		{
			H->DamageSelfDestruct();
			const bool Dead = H->IsDeadOrDying() && H->GetHealth() <= 0;
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Death token=%s pawn=%s key=%s dead=%d health=%.6f"), *R.Token, *Server.SourcePerson().OldPath, *Server.SourcePerson().OldIdentity, Dead, H->GetHealth());
			if (!Dead) Fail(R, TEXT("production-death-not-observed"));
		}
		else Fail(R, TEXT("source-health-component-missing"));
		R.bInterventionCall = false;
		Stage(R, 3); return true;
	}
	if (R.Stage == 3)
	{
		if (!Replacements(R, false)) { if (Now - R.StageAt > 45) Fail(R, TEXT("production-replacement-timeout")); return true; }
		if (!Replacements(R, true)) return true;
		if (Server.Ends[0] != 1 || !Server.bOldEndRoute) { Fail(R, TEXT("old-authority-activation-not-torn-down")); return true; }
		// A deficit makes a late Radiance timer observable on the replacement; no cooldown or grant is altered.
		auto* A = Server.SourcePerson().Component.Get(); const float Maximum = A->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute());
		R.RestoreSourceHealth = Health(Server.SourcePerson().PS.Get());
		R.QuietHealth = FMath::Max(1.f, Maximum - 100.f); A->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), R.QuietHealth);
		R.TargetHealth = Health(Server.TargetPerson().PS.Get()); Place(R); Stage(R, 4); return true;
	}
	if (R.Stage == 4)
	{
		if (!Replacements(R, false)) { Fail(R, TEXT("replacement-chain-invalidated")); return true; }
		if (!FMath::IsNearlyEqual(Health(Server.SourcePerson().PS.Get()), R.QuietHealth, .01f) || !FMath::IsNearlyEqual(Health(Server.TargetPerson().PS.Get()), R.TargetHealth, .01f))
		{ Fail(R, TEXT("late-health-callback-after-intervention")); return true; }
		if (Now - R.InterventionAt < R.Duration + R.ProjectileLife + .5) return true;
		bool Quiet = AllPlaced(R);
		for (auto& P : R.Peers) Quiet &= ClearTags(P.SourcePerson().Component.Get()) && FMath::IsNearlyEqual(Health(P.SourcePerson().PS.Get()), R.QuietHealth, .1f);
		for (const auto& Shot : R.Shots) Quiet &= !Shot.IsValid() || Shot->IsActorBeingDestroyed();
		if (!Quiet) { R.StableAt = 0; if (Now - R.StageAt > 12) Fail(R, TEXT("old-callback-settlement-timeout")); return true; }
		if (!R.StableAt) R.StableAt = Now; if (Now - R.StableAt < .5) return true;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Quiet token=%s peers=4 span=%.6f oldWindow=%.6f oldShots=%d sourceHealth=%.6f targetHealth=%.6f cooldownWrites=0"), *R.Token, Now - R.StableAt, Now - R.InterventionAt, R.OldShots, R.QuietHealth, R.TargetHealth);
		Stage(R, 5); return true;
	}
	if (R.Stage == 6)
	{
		ObserveFresh(R);
		if (Now - R.FreshAt < R.Duration + R.ProjectileLife + .5) return true;
		if (!FreshOutcome(R)) { R.StableAt = 0; if (Now - R.FreshAt > R.Duration + R.ProjectileLife + 5) Fail(R, TEXT("fresh-action-outcome-or-cleanup")); return true; }
		if (!R.StableAt) R.StableAt = Now; if (Now - R.StableAt < .5) return true;
		for (int32 I : {R.Owner, R.Authority})
		{
			auto& P = R.Peers[I];
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Fresh token=%s peer=%d pawn=%s key=%s oldHandle=%s handle=%s activations=%d ends=%d cancelled=%d distance=%.6f speed=%.6f flock=%d sourceHealth=%.6f targetHealth=%.6f shots=%d expectedShots=%d"),
				*R.Token, I, *GetPathNameSafe(Pawn(P.SourcePerson().PS.Get())), *Identity(Pawn(P.SourcePerson().PS.Get())), *P.OldHandle.ToString(), *P.NewHandle.ToString(), P.Activations[1], P.Ends[1], P.Cancelled[1], P.FreshDistance, P.FreshSpeed, P.bFreshFlock,
				Health(P.SourcePerson().PS.Get()), Health(P.TargetPerson().PS.Get()), R.FreshShots, R.ExpectedShots);
		}
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Settled token=%s peers=4 span=%.6f freshSourceBefore=%.6f freshTargetBefore=%.6f"), *R.Token, Now - R.StableAt, R.FreshHealth, R.TargetHealth);
		return Finish(R);
	}
	return true;
}
FAutoConsoleCommand Command(TEXT("bway.Test.AllKitAsyncPIE"), TEXT("Disposable four-world zero-lag PIE: <fresh-token> <action 0..8> <death|reset> <remote-player-id>. Actual replacement plus fresh completed owner use; no cooldown removal."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		int32 Case = INDEX_NONE, Player = INDEX_NONE;
		bool TokenOkay = Args.Num() == 4 && !Args[0].IsEmpty() && Args[0].Len() <= 48;
		if (TokenOkay) for (TCHAR C : Args[0]) TokenOkay &= FChar::IsAlnum(C) || C == TCHAR('_') || C == TCHAR('-');
		if (Active || !TokenOkay || !LexTryParseString(Case, *Args[1]) || Case < 0 || Case >= UE_ARRAY_COUNT(Cases) || !LexTryParseString(Player, *Args[3]) || Player < 0
			|| (Args[2] != TEXT("death") && Args[2] != TEXT("reset")))
		{ UE_LOG(LogTemp, Error, TEXT("BwayAllKitAsync: Rejected reason=arguments-or-existing-request")); return; }
		Active = MakeShared<FRequest>(); Active->Token = Args[0]; Active->Case = Case; Active->SourceId = Player; Active->bReset = Args[2] == TEXT("reset");
		Active->Started = FPlatformTime::Seconds(); Active->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.f);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitAsync: Queued token=%s case=%d trigger=%s source=%d scope=same-process-PIE timeout=90"), *Active->Token, Case, *Args[2], Player);
	}));
FAutoConsoleCommand Abort(TEXT("bway.Test.AllKitAsyncPIEAbort"), TEXT("Abort only the matching request: <token>."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { if (Active && Args.Num() == 1 && Active->Token == Args[0]) Active->bAbort = true; }));
}
#endif
