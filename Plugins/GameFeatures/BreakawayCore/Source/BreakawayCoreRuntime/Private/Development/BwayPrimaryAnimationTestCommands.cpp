// Queue one synthetic primary activation outside the editor Python RPC guard.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Development/BwayCombatFixtureAbilities.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/UnrealType.h"

namespace BwayPrimaryAnimationTest
{
enum class EPhase : uint8
{
	Activate,
	MoveWindup,
	MoveRecovery,
	Death
};

struct FRequest
{
	int32 PlayerId = INDEX_NONE;
	double Deadline = 0.0;
	float DeathWorldTime = 0.0f;
	int32 AuthorityRoundNumber = INDEX_NONE;
	bool bDeathAfterActivation = false;
	bool bMoveDuringActivation = false;
	bool bMovementStarted = false;
	float MoveStartWorldTime = 0.0f;
	float MoveEndWorldTime = 0.0f;
	int32 MoveRoundNumber = INDEX_NONE;
	int32 MoveInputTicks = 0;
	FVector MoveStartLocation = FVector::ZeroVector;
	FVector MoveDirection = FVector::ZeroVector;
	FGameplayAbilitySpecHandle MoveAbilityHandle;
	TWeakObjectPtr<ABwayCharacterWithAbilities> MovePawn;
	TWeakObjectPtr<ABwayPlayerController> MoveController;
	TWeakObjectPtr<ABwayPlayerState> MoveState;
	TWeakObjectPtr<const UBwayRoundManagementComponent> MoveRound;
	EPhase Phase = EPhase::Activate;
	TWeakObjectPtr<ABwayCharacterWithAbilities> AuthorityPawn;
	TWeakObjectPtr<ABwayPlayerState> AuthorityState;
	const TCHAR* WaitReason = TEXT("client-owner-unavailable");
};

TOptional<FRequest> Pending;
FTSTicker::FDelegateHandle TickerHandle;

bool Finish(const TCHAR* Reason)
{
	if (Pending.IsSet() && Pending->bMovementStarted)
	{
		const ABwayCharacterWithAbilities* Pawn = Pending->MovePawn.Get();
		const float Elapsed = Pawn && Pawn->GetWorld() ? Pawn->GetWorld()->GetTimeSeconds() - Pending->MoveStartWorldTime : -1.0f;
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: MoveEnd player=%d reason=%s originalPawnValid=%d elapsed=%.3f displacement=%.3f speed=%.3f inputTicks=%d inputStopped=1"),
			Pending->PlayerId, Reason, Pawn != nullptr, Elapsed,
			Pawn ? FVector::Dist(Pending->MoveStartLocation, Pawn->GetActorLocation()) : -1.0,
			Pawn ? Pawn->GetVelocity().Size2D() : -1.0, Pending->MoveInputTicks);
	}
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Complete player=%d reason=%s cleanup=1"),
		Pending.IsSet() ? Pending->PlayerId : INDEX_NONE, Reason);
	Pending.Reset();
	TickerHandle.Reset();
	return false;
}

bool IsLiveMovementTarget()
{
	ABwayCharacterWithAbilities* Pawn = Pending->MovePawn.Get();
	ABwayPlayerController* Controller = Pending->MoveController.Get();
	ABwayPlayerState* State = Pending->MoveState.Get();
	const UBwayRoundManagementComponent* Round = Pending->MoveRound.Get();
	if (!Pawn || !Controller || !State || !Round || !Controller->IsLocalController()
		|| Controller->GetPawn() != Pawn || Controller->GetPlayerState<ABwayPlayerState>() != State
		|| Pawn->GetController() != Controller || Pawn->GetPlayerState() != State || State->GetPawn() != Pawn
		|| State->GetPlayerId() != Pending->PlayerId) return false;
	UWorld* World = Pawn->GetWorld();
	const ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	const ULyraAbilitySystemComponent* ASC = State->GetLyraAbilitySystemComponent();
	return World && World->IsGameWorld() && World->GetNetMode() == NM_Client && Controller->GetWorld() == World
		&& GameState && GameState->FindComponentByClass<UBwayRoundManagementComponent>() == Round
		&& Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && Round->GetCurrentRoundNumber() == Pending->MoveRoundNumber
		&& Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.0f
		&& ASC && Pawn->GetLyraAbilitySystemComponent() == ASC && ASC->GetAvatarActor() == Pawn
		&& ASC->GetOwnerActor() == State && ASC->AbilityActorInfo.IsValid()
		&& ASC->AbilityActorInfo->PlayerController.Get() == Controller
		&& ASC->AbilityActorInfo->IsLocallyControlled() && !ASC->AbilityActorInfo->IsNetAuthority();
}

bool IsLiveAuthorityTarget(ABwayCharacterWithAbilities* Pawn, ABwayPlayerState* State)
{
	if (!IsValid(Pawn) || !IsValid(State) || !Pawn->HasAuthority() || Pawn->GetPlayerState() != State
		|| State->GetPlayerId() != Pending->PlayerId) return false;
	UWorld* World = Pawn->GetWorld();
	if (!IsValid(World) || !World->IsGameWorld()) return false;
	const ABwayPlayerController* Controller = Cast<ABwayPlayerController>(Pawn->GetController());
	const ABwayGameState* GameState = Pawn->GetWorld()->GetGameState<ABwayGameState>();
	const UBwayRoundManagementComponent* Round = GameState ? GameState->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	const ULyraAbilitySystemComponent* ASC = State->GetLyraAbilitySystemComponent();
	return Controller && Controller->GetPlayerState<ABwayPlayerState>() == State && Controller->GetPawn() == Pawn
		&& Round && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
		&& (Pending->AuthorityRoundNumber == INDEX_NONE || Round->GetCurrentRoundNumber() == Pending->AuthorityRoundNumber)
		&& Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.0f
		&& ASC && Pawn->GetLyraAbilitySystemComponent() == ASC && ASC->GetAvatarActor() == Pawn
		&& ASC->GetOwnerActor() == State && ASC->AbilityActorInfo.IsValid()
		&& ASC->AbilityActorInfo->PlayerController.Get() == Controller && ASC->AbilityActorInfo->IsNetAuthority();
}

bool TickDeath()
{
	ABwayCharacterWithAbilities* Pawn = Pending->AuthorityPawn.Get();
	ABwayPlayerState* State = Pending->AuthorityState.Get();
	// Never redirect a delayed death to a replacement pawn or a new round.
	if (!IsLiveAuthorityTarget(Pawn, State)) return Finish(TEXT("death-target-invalidated"));
	if (Pawn->GetWorld()->GetTimeSeconds() < Pending->DeathWorldTime) return true;
	ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: DeathDispatch player=%d world=%s avatar=%s health=%.3f delay=0.2 guard=%d scope=production-self-destruct"),
		Pending->PlayerId, *Pawn->GetWorld()->GetPathName(), *Pawn->GetPathName(), Health->GetHealth(), GAllowActorScriptExecutionInEditor);
	Health->DamageSelfDestruct();
	return Finish(TEXT("death-dispatched"));
}

void LogNetworkSettings(const UWorld* OwnerWorld)
{
	if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		const UWorld* World = Context.World();
		if (!IsValid(World) || !World->IsGameWorld()) continue;
		const UNetDriver* Driver = World->GetNetDriver();
		if (!IsValid(Driver)) continue;
		// Public development-only state in this engine; read without changing emulation.
		const FPacketSimulationSettings& Settings = Driver->PacketSimulationSettings;
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: NetworkSettings player=%d world=%s net=%d ownerWorld=%d driver=%s PktLag=%d PktLagMin=%d PktLagMax=%d PktLoss=%d PktIncomingLagMin=%d PktIncomingLagMax=%d PktIncomingLoss=%d"),
			Pending->PlayerId, *World->GetPathName(), int32(World->GetNetMode()), World == OwnerWorld, *Driver->GetName(),
			Settings.PktLag, Settings.PktLagMin, Settings.PktLagMax, Settings.PktLoss,
			Settings.PktIncomingLagMin, Settings.PktIncomingLagMax, Settings.PktIncomingLoss);
	}
}

bool Tick(float)
{
	if (!Pending.IsSet()) return false;
	if (FPlatformTime::Seconds() >= Pending->Deadline)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: TIMEOUT player=%d reason=%s"), Pending->PlayerId, Pending->WaitReason);
		return Finish(TEXT("timeout"));
	}
	if (GAllowActorScriptExecutionInEditor)
	{
		Pending->WaitReason = TEXT("editor-script-guard");
		return true;
	}
	if (Pending->Phase == EPhase::Death) return TickDeath();
	if (Pending->bMovementStarted)
	{
		if (!IsLiveMovementTarget()) return Finish(TEXT("movement-target-invalidated"));
		ABwayCharacterWithAbilities* MovePawn = Pending->MovePawn.Get();
		const float Now = MovePawn->GetWorld()->GetTimeSeconds();
		if (Pending->Phase == EPhase::MoveRecovery && Now >= Pending->MoveEndWorldTime)
		{
			return Finish(TEXT("movement-complete"));
		}
		MovePawn->AddMovementInput(Pending->MoveDirection);
		++Pending->MoveInputTicks;
		if (Pending->Phase == EPhase::MoveRecovery || Now < Pending->MoveStartWorldTime + 0.3f) return true;
		Pending->Phase = EPhase::Activate;
	}

	ABwayPlayerController* Controller = nullptr;
	int32 Matches = 0;
	if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!IsValid(World) || !World->IsGameWorld() || World->GetNetMode() != NM_Client) continue;
		for (TActorIterator<ABwayPlayerController> It(World); It; ++It)
		{
			ABwayPlayerController* Candidate = *It;
			if (!IsValid(Candidate)) continue;
			const ABwayPlayerState* State = Candidate->GetPlayerState<ABwayPlayerState>();
			if (Candidate->IsLocalController() && IsValid(State) && State->GetPlayerId() == Pending->PlayerId)
			{
				Controller = Candidate;
				++Matches;
			}
		}
	}
	if (Matches > 1)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Ambiguous owner player=%d matches=%d"), Pending->PlayerId, Matches);
		return Finish(TEXT("ambiguous-client-owner"));
	}
	if (!Controller)
	{
		Pending->WaitReason = TEXT("client-owner-unavailable");
		return true;
	}

	UWorld* World = Controller->GetWorld();
	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	const UBwayRoundManagementComponent* Round = GameState ? GameState->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
	ABwayCharacterWithAbilities* Pawn = Cast<ABwayCharacterWithAbilities>(Controller->GetPawn());
	ABwayPlayerState* State = Controller->GetPlayerState<ABwayPlayerState>();
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || !IsValid(Pawn)
		|| Pawn->GetController() != Controller || Pawn->GetPlayerState() != State
		|| !Health || Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)
	{
		Pending->WaitReason = TEXT("playing-live-owned-pawn-unavailable");
		return true;
	}
	ULyraAbilitySystemComponent* ASC = State->GetLyraAbilitySystemComponent();
	if (!ASC || Pawn->GetLyraAbilitySystemComponent() != ASC || ASC->GetAvatarActor() != Pawn
		|| ASC->GetOwnerActor() != State || !ASC->AbilityActorInfo.IsValid()
		|| ASC->AbilityActorInfo->PlayerController.Get() != Controller
		|| !ASC->AbilityActorInfo->IsLocallyControlled() || ASC->AbilityActorInfo->IsNetAuthority())
	{
		Pending->WaitReason = TEXT("asc-owner-avatar-disagreement");
		return true;
	}

	FGameplayAbilitySpecHandle Handle;
	FString AbilityName;
	int32 PrimaryCount = 0;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!BwayCombatFixture::IsPrimaryAbility(Spec.Ability)) continue;
		Handle = Spec.Handle;
		AbilityName = GetNameSafe(Spec.Ability);
		++PrimaryCount;
	}
	if (PrimaryCount > 1)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Ambiguous primary player=%d count=%d"), Pending->PlayerId, PrimaryCount);
		return Finish(TEXT("ambiguous-primary"));
	}
	if (!Handle.IsValid())
	{
		if (Pending->bMovementStarted) return Finish(TEXT("movement-primary-unavailable"));
		Pending->WaitReason = TEXT("primary-spec-unavailable");
		return true;
	}
	if (Pending->bMoveDuringActivation)
	{
		if (!Pending->bMovementStarted)
		{
			Pending->MovePawn = Pawn;
			Pending->MoveController = Controller;
			Pending->MoveState = State;
			Pending->MoveRound = Round;
			Pending->MoveRoundNumber = Round->GetCurrentRoundNumber();
			Pending->MoveAbilityHandle = Handle;
			Pending->MoveStartLocation = Pawn->GetActorLocation();
			Pending->MoveDirection = Pawn->GetActorForwardVector().GetSafeNormal2D();
			if (Pending->MoveDirection.IsNearlyZero() || !IsLiveMovementTarget()) return Finish(TEXT("movement-setup-invalid"));
			Pending->bMovementStarted = true;
			Pending->MoveStartWorldTime = World->GetTimeSeconds();
			Pending->Phase = EPhase::MoveWindup;
			Pending->WaitReason = TEXT("movement-windup");
			UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: MoveStart player=%d world=%s avatar=%s round=%d direction=%s speed=%.3f warmup=0.3 recovery=1.0 guard=%d"),
				Pending->PlayerId, *World->GetPathName(), *Pawn->GetPathName(), Pending->MoveRoundNumber,
				*Pending->MoveDirection.ToCompactString(), Pawn->GetVelocity().Size2D(), GAllowActorScriptExecutionInEditor);
			Pawn->AddMovementInput(Pending->MoveDirection);
			++Pending->MoveInputTicks;
			return true;
		}
		if (!IsLiveMovementTarget() || Pawn != Pending->MovePawn.Get() || Controller != Pending->MoveController.Get()
			|| Handle != Pending->MoveAbilityHandle) return Finish(TEXT("movement-target-or-primary-changed"));
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: MovingFire player=%d elapsed=%.3f displacement=%.3f speed=%.3f inputTicks=%d guard=%d"),
			Pending->PlayerId, World->GetTimeSeconds() - Pending->MoveStartWorldTime,
			FVector::Dist(Pending->MoveStartLocation, Pawn->GetActorLocation()), Pawn->GetVelocity().Size2D(),
			Pending->MoveInputTicks, GAllowActorScriptExecutionInEditor);
	}
	if (Pending->bDeathAfterActivation)
	{
		ABwayCharacterWithAbilities* AuthorityPawn = nullptr;
		int32 AuthorityMatches = 0;
		if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* AuthorityWorld = Context.World();
			if (!IsValid(AuthorityWorld) || !AuthorityWorld->IsGameWorld() || AuthorityWorld->GetNetMode() == NM_Client) continue;
			for (TActorIterator<ABwayCharacterWithAbilities> It(AuthorityWorld); It; ++It)
			{
				ABwayCharacterWithAbilities* Candidate = *It;
				const ABwayPlayerState* CandidateState = IsValid(Candidate) ? Candidate->GetPlayerState<ABwayPlayerState>() : nullptr;
				if (IsValid(CandidateState) && CandidateState->GetPlayerId() == Pending->PlayerId && CandidateState->GetPawn() == Candidate)
				{
					AuthorityPawn = Candidate;
					++AuthorityMatches;
				}
			}
		}
		if (AuthorityMatches > 1) return Finish(TEXT("ambiguous-authority-death-target"));
		ABwayPlayerState* AuthorityState = AuthorityPawn ? AuthorityPawn->GetPlayerState<ABwayPlayerState>() : nullptr;
		if (!IsLiveAuthorityTarget(AuthorityPawn, AuthorityState))
		{
			Pending->WaitReason = TEXT("playing-live-authority-death-target-unavailable");
			return true;
		}
		Pending->AuthorityPawn = AuthorityPawn;
		Pending->AuthorityState = AuthorityState;
		Pending->AuthorityRoundNumber = AuthorityPawn->GetWorld()->GetGameState<ABwayGameState>()
			->FindComponentByClass<UBwayRoundManagementComponent>()->GetCurrentRoundNumber();
	}

	LogNetworkSettings(World);
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Dispatch player=%d world=%s net=%d local=%d role=%d remoteRole=%d guard=%d avatar=%s ability=%s handle=%s"),
		Pending->PlayerId, *World->GetPathName(), int32(World->GetNetMode()), Controller->IsLocalController(),
		int32(Pawn->GetLocalRole()), int32(Pawn->GetRemoteRole()), GAllowActorScriptExecutionInEditor,
		*Pawn->GetPathName(), *AbilityName, *Handle.ToString());
	const TWeakObjectPtr<ABwayCharacterWithAbilities> WeakPawn(Pawn);
	const TWeakObjectPtr<ULyraAbilitySystemComponent> WeakASC(ASC);
	const bool Activated = ASC->TryActivateAbility(Handle, false);

	// Re-resolve after activation; synchronous ability callbacks may end or replace the avatar.
	Pawn = WeakPawn.Get();
	ASC = WeakASC.Get();
	const FGameplayAbilitySpec* AfterSpec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	UAnimInstance* Anim = Pawn && Pawn->GetMesh() ? Pawn->GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
	const FBoolProperty* Firing = Anim ? FindFProperty<FBoolProperty>(Anim->GetClass(), TEXT("GameplayTag_IsFiring")) : nullptr;
	const FDoubleProperty* SinceFire = Anim ? FindFProperty<FDoubleProperty>(Anim->GetClass(), TEXT("TimeSinceFiredWeapon")) : nullptr;
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Readback player=%d activated=%d active=%d avatarMatches=%d anim=%s montage=%s montagePosition=%.3f firing=%d sinceFire=%.3f scope=immediate-local-readback"),
		Pending->PlayerId, Activated, AfterSpec && AfterSpec->IsActive(), Pawn && ASC && ASC->GetAvatarActor() == Pawn,
		*GetNameSafe(Anim), *GetNameSafe(Montage), Montage ? Anim->Montage_GetPosition(Montage) : -1.0f,
		Firing ? int32(Firing->GetPropertyValue_InContainer(Anim)) : -1,
		SinceFire ? SinceFire->GetPropertyValue_InContainer(Anim) : -1.0f);
	if (Activated && Pending->bMoveDuringActivation)
	{
		if (!IsLiveMovementTarget()) return Finish(TEXT("movement-target-invalidated-after-activation"));
		Pending->Phase = EPhase::MoveRecovery;
		Pending->MoveEndWorldTime = Pending->MovePawn->GetWorld()->GetTimeSeconds() + 1.0f;
		Pending->WaitReason = TEXT("movement-recovery");
		return true;
	}
	if (Activated && Pending->bDeathAfterActivation)
	{
		ABwayCharacterWithAbilities* AuthorityPawn = Pending->AuthorityPawn.Get();
		if (!IsLiveAuthorityTarget(AuthorityPawn, Pending->AuthorityState.Get())) return Finish(TEXT("death-target-invalidated"));
		Pending->Phase = EPhase::Death;
		Pending->DeathWorldTime = AuthorityPawn->GetWorld()->GetTimeSeconds() + 0.2f;
		Pending->WaitReason = TEXT("queued-production-death");
		return true;
	}
	return Finish(Activated ? TEXT("activation-accepted") : TEXT("activation-rejected"));
}

void StartRequest(const TArray<FString>& Args, bool bDeathAfterActivation, bool bMoveDuringActivation = false)
{
	int32 PlayerId = 0;
	bool ValidId = Args.Num() == 1 && !Args[0].IsEmpty();
	if (ValidId) for (TCHAR Character : Args[0])
	{
		const int32 Digit = int32(Character) - int32(TEXT('0'));
		if (Digit < 0 || Digit > 9 || PlayerId > (MAX_int32 - Digit) / 10)
		{
			ValidId = false;
			break;
		}
		PlayerId = PlayerId * 10 + Digit;
	}
	if (!ValidId)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Usage bway.Test.%s <nonnegative-int32-player-id>"),
			bMoveDuringActivation ? TEXT("PrimaryAnimationMove") : (bDeathAfterActivation ? TEXT("PrimaryAnimationDeath") : TEXT("PrimaryAnimation")));
		return;
	}
	if (Pending.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPrimaryAnimation: Request rejected; player=%d already pending"), Pending->PlayerId);
		return;
	}
	FRequest Request;
	Request.PlayerId = PlayerId;
	Request.Deadline = FPlatformTime::Seconds() + 5.0;
	Request.bDeathAfterActivation = bDeathAfterActivation;
	Request.bMoveDuringActivation = bMoveDuringActivation;
	Pending = Request;
	// Movement input is consumed per character frame, so submit it each frame only for this mode.
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), bMoveDuringActivation ? 0.0f : 0.05f);
	if (bMoveDuringActivation)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=5.0 move=1"), PlayerId);
	}
	else if (bDeathAfterActivation)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=5.0 death=1"), PlayerId);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=5.0"), PlayerId);
	}
}

void Start(const TArray<FString>& Args)
{
	StartRequest(Args, false);
}

void StartDeath(const TArray<FString>& Args)
{
	StartRequest(Args, true);
}

void StartMove(const TArray<FString>& Args)
{
	StartRequest(Args, false, true);
}

void Cancel()
{
	if (!Pending.IsSet()) return;
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Finish(TEXT("cancelled"));
}

FAutoConsoleCommand Command(TEXT("bway.Test.PrimaryAnimation"),
	TEXT("Development only: queue one primary activation for an exact local NM_Client player ID; no aim or fixture changes."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&Start));
FAutoConsoleCommand DeathCommand(TEXT("bway.Test.PrimaryAnimationDeath"),
	TEXT("Development only: activate one remote-owner primary, then self-destruct its original authority pawn after 0.2 seconds while Playing."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartDeath));
FAutoConsoleCommand MoveCommand(TEXT("bway.Test.PrimaryAnimationMove"),
	TEXT("Development only: move an original remote-owner pawn forward for 0.3 seconds, activate its primary once, then submit movement input for one more second."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartMove));
FAutoConsoleCommand CancelCommand(TEXT("bway.Test.PrimaryAnimationCancel"),
	TEXT("Cancel the pending development primary-animation activation."), FConsoleCommandDelegate::CreateStatic(&Cancel));
}
#endif
