// Queue one synthetic primary or Barrage activation outside the editor Python RPC guard.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayGameplayAbility_Slide.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Development/BwayCombatFixtureAbilities.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManager.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Player/LyraCheatManager.h"
#include "System/LyraGameData.h"
#include "UObject/UnrealType.h"

namespace BwayPrimaryAnimationTest
{
enum class EPhase : uint8
{
	Activate,
	MoveWindup,
	MoveRecovery,
	Death,
	DeathCleanup,
	CancelBurst,
	HealthConverge,
	HealthFirstShot,
	HealthDamage,
	HealthObserve,
	HealthCleanup,
	BarrageMoveChannel,
	BarrageSlideRelease,
	BarrageMoveRecovery,
	BarrageMoveCleanup
};

enum class EHealthMode : uint8 { None, HealDamage, DamageOnly, HealOnly };

struct FSlideEndpoint
{
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	FGameplayAbilitySpecHandle Handle;
	FDelegateHandle Activated, Ended;
	FVector PressLocation = FVector::ZeroVector;
	float CapsuleHalfHeight = 0.f, FloorAngle = 0.f;
	FRotator RotationRate = FRotator::ZeroRotator;
	bool bOrientToMovement = false;
};

struct FRequest
{
	int32 PlayerId = INDEX_NONE;
	double Deadline = 0.0;
	float DeathWorldTime = 0.0f;
	int32 AuthorityRoundNumber = INDEX_NONE;
	bool bDeathAfterActivation = false;
	bool bDeathCastAttempted = false;
	bool bDeathDispatched = false;
	float DeathShotInterval = 0.0f;
	float DeathLastInactiveWorldTime = 0.0f;
	TWeakObjectPtr<ULyraAbilitySystemComponent> DeathOwnerASC;
	TWeakObjectPtr<ULyraAbilitySystemComponent> DeathAuthorityASC;
	const TCHAR* DeathStopReason = TEXT("death-setup-failed");
	bool bMoveDuringActivation = false;
	bool bBarrage = false;
	bool bBarrageSlide = false;
	bool bSlidePrepared = false;
	bool bSlideInputHeld = false;
	bool bSlideCleanupAttempted = false;
	double SlidePressTime = 0.0;
	double SlideReleaseTime = 0.0;
	FGameplayTag SlideInputTag, SlidingTag;
	FSlideEndpoint SlideEndpoints[2]; // Owner, authority; no shared handle assumption.
	bool bCancelBurst = false;
	EHealthMode HealthMode = EHealthMode::None;
	bool bHealthPrepared = false;
	bool bHealthCastAttempted = false;
	bool bHealthCleanupCancelled = false;
	bool bHealthRestoreAttempted = false;
	bool bHealthRestored = false;
	double HealthStart = 0.0;
	double HealthCastStart = 0.0;
	float OriginalHealth = 0.0f;
	float PreparedHealth = 0.0f;
	float PreparationDamageRequest = 0.0f;
	float PreparationDamageAddend = 0.0f;
	float ActivationHealth = 0.0f;
	float FirstShotHealth = 0.0f;
	float AfterHeal = 0.0f;
	float AfterDamage = 0.0f;
	float HealthShotInterval = 0.0f;
	float HealthCooldown = 0.0f;
	int32 HealthShotCount = 0;
	const TCHAR* HealthStopReason = TEXT("observed-end");
	float CancelShotInterval = 0.0f;
	FGameplayAbilitySpecHandle CancelAuthorityHandle;
	bool bMovementStarted = false;
	float MoveStartWorldTime = 0.0f;
	float MoveCastWorldTime = 0.0f;
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
	TWeakObjectPtr<const UBwayRoundManagementComponent> HealthAuthorityRound;
	TWeakObjectPtr<const UBwayRoundManagementComponent> MoveAuthorityRound;
	TWeakObjectPtr<ABwayPlayerController> MoveAuthorityController;
	FTransform MoveOwnerOriginalTransform;
	FTransform MoveAuthorityOriginalTransform;
	FRotator MoveOwnerOriginalView = FRotator::ZeroRotator;
	FRotator MoveAuthorityOriginalView = FRotator::ZeroRotator;
	bool bMoveCastAttempted = false;
	bool bMoveSawAuthorityActive = false;
	bool bMoveCleanupApplied = false;
	bool bMoveCleanupInputsStopped = false;
	bool bMoveCleanupCancelled = false;
	bool bMoveCleanupCancellationAttempted = false;
	bool bMoveEvidenceEnded = false;
	bool bMoveTeleportSucceeded = false;
	double MoveRecoveryStart = 0.0;
	double MoveCleanupStart = 0.0;
	double MoveInputStopTime = 0.0;
	double MoveDrainQuietStart = 0.0;
	double MoveDrainMinimumSeconds = 0.25;
	double MoveLastSample = 0.0;
	float MoveShotInterval = 0.0f;
	float MoveCooldown = 0.0f;
	int32 MoveShotCount = 0;
	int32 AimInputTicks = 0;
	float AimInputTotal = 0.0f;
	TSet<FString> MoveSeenProjectiles;
	const TCHAR* MoveStopReason = TEXT("observed-recovery");
	const TCHAR* WaitReason = TEXT("client-owner-unavailable");
};

TOptional<FRequest> Pending;
FTSTicker::FDelegateHandle TickerHandle;

bool IsBarrageMovement() { return Pending.IsSet() && Pending->bBarrage && Pending->bMoveDuringActivation; }
bool IsBarrageDeath() { return Pending.IsSet() && Pending->bBarrage && Pending->bDeathAfterActivation; }
bool TickBarrageDeath();
bool TickBarrageDeathCleanup(bool bHardDeadline = false);
void RemoveBarrageSlideObservers();

bool Finish(const TCHAR* Reason)
{
	if (IsBarrageDeath() && Pending->bDeathCastAttempted && !Pending->bDeathDispatched && Pending->Phase != EPhase::DeathCleanup)
	{
		Pending->DeathStopReason = Reason;
		Pending->Phase = EPhase::DeathCleanup;
		return true;
	}
	if (IsBarrageMovement() && Pending->bMovementStarted && Pending->Phase != EPhase::BarrageMoveCleanup)
	{
		Pending->MoveStopReason = Reason;
		Pending->WaitReason = Reason;
		Pending->Phase = EPhase::BarrageMoveCleanup;
		return true;
	}
	// Any shared selector failure after preparation must still run health cleanup.
	if (Pending.IsSet() && Pending->HealthMode != EHealthMode::None && Pending->bHealthPrepared && Pending->Phase != EPhase::HealthCleanup)
	{
		Pending->HealthStopReason = Reason;
		Pending->WaitReason = Reason;
		Pending->Phase = EPhase::HealthCleanup;
		return true;
	}
	if (Pending.IsSet() && Pending->bMovementStarted)
	{
		const ABwayCharacterWithAbilities* Pawn = Pending->MovePawn.Get();
		const float Elapsed = Pawn && Pawn->GetWorld() ? Pawn->GetWorld()->GetTimeSeconds() - Pending->MoveStartWorldTime : -1.0f;
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: MoveEnd player=%d reason=%s originalPawnValid=%d elapsed=%.3f displacement=%.3f speed=%.3f inputTicks=%d inputStopped=1"),
			Pending->PlayerId, Reason, Pawn != nullptr, Elapsed,
			Pawn ? FVector::Dist(Pending->MoveStartLocation, Pawn->GetActorLocation()) : -1.0,
			Pawn ? Pawn->GetVelocity().Size2D() : -1.0, Pending->MoveInputTicks);
	}
	RemoveBarrageSlideObservers();
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
		&& ASC->AbilityActorInfo->IsLocallyControlled() && !ASC->AbilityActorInfo->IsNetAuthority()
		&& (!Pending->bSlidePrepared || ASC == Pending->SlideEndpoints[0].ASC.Get());
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
		&& (!Pending->bHealthPrepared || Round == Pending->HealthAuthorityRound.Get())
		&& Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.0f
		&& ASC && Pawn->GetLyraAbilitySystemComponent() == ASC && ASC->GetAvatarActor() == Pawn
		&& ASC->GetOwnerActor() == State && ASC->AbilityActorInfo.IsValid()
		&& ASC->AbilityActorInfo->PlayerController.Get() == Controller && ASC->AbilityActorInfo->IsNetAuthority();
}

int32 CaptureAuthorityTarget()
{
	ABwayCharacterWithAbilities* Pawn = nullptr;
	int32 Matches = 0;
	if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!IsValid(World) || !World->IsGameWorld() || World->GetNetMode() == NM_Client) continue;
		for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
		{
			ABwayCharacterWithAbilities* Candidate = *It;
			const ABwayPlayerState* State = IsValid(Candidate) ? Candidate->GetPlayerState<ABwayPlayerState>() : nullptr;
			if (IsValid(State) && State->GetPlayerId() == Pending->PlayerId && State->GetPawn() == Candidate) { Pawn = Candidate; ++Matches; }
		}
	}
	if (Matches > 1) return -1;
	ABwayPlayerState* State = Pawn ? Pawn->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (!IsLiveAuthorityTarget(Pawn, State)) return 0;
	Pending->AuthorityPawn = Pawn;
	Pending->AuthorityState = State;
	const UBwayRoundManagementComponent* Round = Pawn->GetWorld()->GetGameState<ABwayGameState>()->FindComponentByClass<UBwayRoundManagementComponent>();
	Pending->AuthorityRoundNumber = Round->GetCurrentRoundNumber();
	if (Pending->HealthMode != EHealthMode::None) Pending->HealthAuthorityRound = Round;
	if (IsBarrageMovement() || IsBarrageDeath())
	{
		Pending->MoveAuthorityRound = Round;
		Pending->MoveAuthorityController = Cast<ABwayPlayerController>(Pawn->GetController());
	}
	return 1;
}

bool TickDeath()
{
	if (IsBarrageDeath()) return TickBarrageDeath();
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

int32 CountRawlinsBullets(ABwayCharacterWithAbilities* AuthorityPawn, AActor*& FirstProjectile)
{
	FirstProjectile = nullptr;
	UClass* BulletClass = FindObject<UClass>(nullptr, TEXT("/Script/HeroRawlinsRuntime.BwayRawlinsBulletProjectile"));
	if (!BulletClass) return INDEX_NONE;
	int32 Count = 0;
	for (TActorIterator<AActor> It(AuthorityPawn->GetWorld(), BulletClass); It; ++It)
	{
		AActor* Projectile = *It;
		if (IsValid(Projectile) && Projectile->GetOwner() == AuthorityPawn && Projectile->GetInstigator() == AuthorityPawn)
		{
			FirstProjectile = Projectile;
			++Count;
		}
	}
	return Count;
}

bool FailCancelBurst(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: CancelBurstSetupFailed player=%d reason=%s guard=%d"),
		Pending->PlayerId, Reason, GAllowActorScriptExecutionInEditor);
	return Finish(Reason);
}

bool TickCancelBurst()
{
	ABwayCharacterWithAbilities* AuthorityPawn = Pending->AuthorityPawn.Get();
	ABwayPlayerState* AuthorityState = Pending->AuthorityState.Get();
	// The movement-target validator also proves the unchanged owner pawn/state/round.
	if (!IsLiveMovementTarget() || !IsLiveAuthorityTarget(AuthorityPawn, AuthorityState))
		return FailCancelBurst(TEXT("cancel-owner-or-authority-invalidated"));
	ABwayPlayerController* Controller = Pending->MoveController.Get();
	ABwayPlayerController* AuthorityController = Cast<ABwayPlayerController>(AuthorityPawn->GetController());
	if (!Controller->CheatManager || !AuthorityController->CheatManager)
		return FailCancelBurst(TEXT("cancel-cheat-manager-unavailable"));
	const FGameplayAbilitySpec* OwnerSpec = Pending->MoveState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
	const FGameplayAbilitySpec* AuthoritySpec = AuthorityState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle);
	if (!OwnerSpec || !AuthoritySpec) return FailCancelBurst(TEXT("cancel-primary-spec-lost"));
	AActor* FirstProjectile = nullptr;
	const int32 ProjectileCount = CountRawlinsBullets(AuthorityPawn, FirstProjectile);
	if (ProjectileCount == INDEX_NONE) return FailCancelBurst(TEXT("cancel-bullet-family-unavailable"));
	if (ProjectileCount > 1) return FailCancelBurst(TEXT("cancel-second-projectile-already-present"));
	if (!OwnerSpec->IsActive()) return FailCancelBurst(TEXT("cancel-owner-primary-already-inactive"));
	if (ProjectileCount == 0)
	{
		Pending->WaitReason = TEXT("cancel-authority-first-projectile-unavailable");
		return true;
	}
	if (!AuthoritySpec->IsActive()) return FailCancelBurst(TEXT("cancel-authority-primary-already-inactive"));
	const float ProjectileAge = FirstProjectile->GetGameTimeSinceCreation();
	if (!FMath::IsFinite(ProjectileAge) || ProjectileAge < 0.0f || ProjectileAge >= Pending->CancelShotInterval)
		return FailCancelBurst(TEXT("cancel-second-shot-deadline-reached"));
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: CancelBurstDispatch player=%d ownerWorld=%s ownerPawn=%s ownerController=%s authorityWorld=%s authorityPawn=%s authorityController=%s authorityActive=1 ownerActive=1 firstProjectile=%s projectileCount=%d projectileAge=%.6f shotInterval=%.6f deadlineRemaining=%.6f guard=%d"),
		Pending->PlayerId, *Controller->GetWorld()->GetPathName(), *Pending->MovePawn->GetPathName(), *Controller->GetPathName(),
		*AuthorityPawn->GetWorld()->GetPathName(), *AuthorityPawn->GetPathName(), *AuthorityController->GetPathName(),
		*FirstProjectile->GetPathName(), ProjectileCount, ProjectileAge, Pending->CancelShotInterval,
		Pending->CancelShotInterval - ProjectileAge, GAllowActorScriptExecutionInEditor);
	Controller->ServerCheat(TEXT("CancelActivatedAbilities"));
	return Finish(TEXT("cancel-burst-dispatched"));
}

bool IsLiveBarrageMoveAuthority()
{
	ABwayCharacterWithAbilities* Pawn = Pending->AuthorityPawn.Get();
	return IsLiveAuthorityTarget(Pawn, Pending->AuthorityState.Get()) && Pawn->GetController() == Pending->MoveAuthorityController.Get()
		&& Pawn->GetWorld()->GetGameState<ABwayGameState>()->FindComponentByClass<UBwayRoundManagementComponent>() == Pending->MoveAuthorityRound.Get()
		&& (!Pending->bSlidePrepared || Pawn->GetLyraAbilitySystemComponent() == Pending->SlideEndpoints[1].ASC.Get());
}

bool IsLiveBarrageDeathOwner()
{
	return IsLiveMovementTarget() && Pending->MoveState->GetLyraAbilitySystemComponent() == Pending->DeathOwnerASC.Get();
}

bool IsLiveBarrageDeathAuthority()
{
	return IsLiveBarrageMoveAuthority() && Pending->AuthorityState->GetLyraAbilitySystemComponent() == Pending->DeathAuthorityASC.Get();
}

bool TickBarrageDeathCleanup(bool bHardDeadline)
{
	Pending->Phase = EPhase::DeathCleanup;
	// Cleanup runs on the native ticker, never inside the Python console call.
	const bool CanCancel = !GAllowActorScriptExecutionInEditor;
	int32 Cancelled = 0;
	if (CanCancel && IsLiveBarrageDeathAuthority())
	{
		ULyraAbilitySystemComponent* ASC = Pending->AuthorityState->GetLyraAbilitySystemComponent();
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle);
		if (Spec && Spec->IsActive()) { ASC->CancelAbilityHandle(Spec->Handle); ++Cancelled; }
	}
	// Authority cancellation can synchronously invalidate the owner; re-resolve it.
	if (CanCancel && IsLiveBarrageDeathOwner())
	{
		ULyraAbilitySystemComponent* ASC = Pending->MoveState->GetLyraAbilitySystemComponent();
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
		if (Spec && Spec->IsActive()) { ASC->CancelAbilityHandle(Spec->Handle); ++Cancelled; }
	}
	const bool OwnerValid = IsLiveBarrageDeathOwner(), AuthorityValid = IsLiveBarrageDeathAuthority();
	const FGameplayAbilitySpec* OwnerSpec = OwnerValid ? Pending->MoveState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle) : nullptr;
	const FGameplayAbilitySpec* AuthoritySpec = AuthorityValid ? Pending->AuthorityState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle) : nullptr;
	const bool AbilityCleanupComplete = CanCancel && OwnerValid && AuthorityValid && (!OwnerSpec || !OwnerSpec->IsActive()) && (!AuthoritySpec || !AuthoritySpec->IsActive());
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageDeathCleanup player=%d reason=%s dispatched=0 ownerValid=%d authorityValid=%d ownerActive=%d authorityActive=%d cancelled=%d queueRemoved=1 abilityCleanupComplete=%d hardDeadline=%d guard=%d scope=original-fixture-spec-only"),
		Pending->PlayerId, Pending->DeathStopReason, OwnerValid, AuthorityValid, OwnerValid ? int32(OwnerSpec && OwnerSpec->IsActive()) : -1, AuthorityValid ? int32(AuthoritySpec && AuthoritySpec->IsActive()) : -1,
		Cancelled, AbilityCleanupComplete, bHardDeadline, GAllowActorScriptExecutionInEditor);
	return Finish(Pending->DeathStopReason);
}

bool PrepareBarrageDeath(ABwayCharacterWithAbilities* Pawn, ABwayPlayerController* Controller, ABwayPlayerState* State,
	const UBwayRoundManagementComponent* Round, ULyraAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle Handle, const UClass* Family)
{
	ABwayCharacterWithAbilities* AuthorityPawn = Pending->AuthorityPawn.Get();
	ULyraAbilitySystemComponent* AuthorityASC = Pending->AuthorityState->GetLyraAbilitySystemComponent();
	const FGameplayAbilitySpec* OwnerSpec = ASC->FindAbilitySpecFromHandle(Handle);
	const FGameplayAbilitySpec* AuthoritySpec = nullptr;
	int32 Matches = 0;
	for (const FGameplayAbilitySpec& Spec : AuthorityASC->GetActivatableAbilities())
		if (Family && Spec.Ability && Spec.Ability->IsA(Family)) { AuthoritySpec = &Spec; ++Matches; }
	if (!OwnerSpec || !OwnerSpec->Ability || OwnerSpec->IsActive() || Matches != 1 || AuthoritySpec->IsActive()
		|| !OwnerSpec->Ability->CheckCooldown(Handle, ASC->AbilityActorInfo.Get())
		|| !AuthoritySpec->Ability->CheckCooldown(AuthoritySpec->Handle, AuthorityASC->AbilityActorInfo.Get())) return false;
	const FSoftObjectProperty* ConfigProperty = FindFProperty<FSoftObjectProperty>(AuthoritySpec->Ability->GetClass(), TEXT("KitConfig"));
	const UObject* Config = ConfigProperty ? ConfigProperty->GetPropertyValue_InContainer(AuthoritySpec->Ability).Get() : nullptr;
	const FFloatProperty* IntervalProperty = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("BarrageShotInterval")) : nullptr;
	const FIntProperty* CountProperty = Config ? FindFProperty<FIntProperty>(Config->GetClass(), TEXT("BarrageShotCount")) : nullptr;
	const FFloatProperty* CooldownProperty = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("BarrageCooldown")) : nullptr;
	const float Interval = IntervalProperty ? IntervalProperty->GetPropertyValue_InContainer(Config) : 0.0f;
	const int32 Count = CountProperty ? CountProperty->GetPropertyValue_InContainer(Config) : 0;
	const float Cooldown = CooldownProperty ? CooldownProperty->GetPropertyValue_InContainer(Config) : 0.0f;
	AActor* Existing = nullptr;
	const int32 ExistingCount = CountRawlinsBullets(AuthorityPawn, Existing);
	if (!FMath::IsFinite(Interval) || Interval <= 0.0f || Count < 2 || !FMath::IsFinite(Cooldown) || Cooldown <= 0.0f || ExistingCount != 0) return false;
	Pending->MovePawn = Pawn;
	Pending->MoveController = Controller;
	Pending->MoveState = State;
	Pending->MoveRound = Round;
	Pending->MoveRoundNumber = Round->GetCurrentRoundNumber();
	Pending->MoveAbilityHandle = Handle;
	Pending->CancelAuthorityHandle = AuthoritySpec->Handle;
	Pending->DeathShotInterval = Interval;
	Pending->DeathLastInactiveWorldTime = AuthorityPawn->GetWorld()->GetTimeSeconds();
	Pending->DeathOwnerASC = ASC;
	Pending->DeathAuthorityASC = AuthorityASC;
	if (!IsLiveBarrageDeathOwner() || !IsLiveBarrageDeathAuthority()) return false;
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageDeathPreflight player=%d ownerWorld=%s ownerPawn=%s ownerController=%s ownerState=%s ownerRoundObject=%s ownerRound=%d ownerHandle=%s authorityWorld=%s authorityPawn=%s authorityController=%s authorityState=%s authorityRoundObject=%s authorityRound=%d authorityHandle=%s existingProjectiles=%d shotInterval=%.6f shotCount=%d cooldown=%.6f ownerCooldownReady=1 authorityCooldownReady=1 ownerASC=%s authorityASC=%s ownerAvatar=%s authorityAvatar=%s guard=%d"),
		Pending->PlayerId, *Pawn->GetWorld()->GetPathName(), *Pawn->GetPathName(), *Controller->GetPathName(), *State->GetPathName(), *Round->GetPathName(), Pending->MoveRoundNumber, *Handle.ToString(),
		*AuthorityPawn->GetWorld()->GetPathName(), *AuthorityPawn->GetPathName(), *Pending->MoveAuthorityController->GetPathName(), *Pending->AuthorityState->GetPathName(), *Pending->MoveAuthorityRound->GetPathName(), Pending->AuthorityRoundNumber,
		*Pending->CancelAuthorityHandle.ToString(), ExistingCount, Interval, Count, Cooldown, *ASC->GetPathName(), *AuthorityASC->GetPathName(), *GetPathNameSafe(ASC->GetAvatarActor()), *GetPathNameSafe(AuthorityASC->GetAvatarActor()), GAllowActorScriptExecutionInEditor);
	return true;
}

bool TickBarrageDeath()
{
	if (!IsLiveBarrageDeathOwner() || !IsLiveBarrageDeathAuthority()) return Finish(TEXT("barrage-death-original-target-invalidated"));
	ABwayCharacterWithAbilities* AuthorityPawn = Pending->AuthorityPawn.Get();
	const FGameplayAbilitySpec* OwnerSpec = Pending->MoveState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
	const FGameplayAbilitySpec* AuthoritySpec = Pending->AuthorityState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle);
	if (!OwnerSpec || !AuthoritySpec || !OwnerSpec->IsActive()) return Finish(TEXT("barrage-death-spec-lost-or-owner-inactive"));
	AActor* FirstProjectile = nullptr;
	const int32 Count = CountRawlinsBullets(AuthorityPawn, FirstProjectile);
	if (Count < 0 || Count > 1) return Finish(TEXT("barrage-death-missing-family-or-second-projectile"));
	const float Now = AuthorityPawn->GetWorld()->GetTimeSeconds();
	if (Count == 0)
	{
		// Authority spawns shot one synchronously inside activation. Once active,
		// an absent bullet is a missed setup, not permission to use a later shot.
		if (AuthoritySpec->IsActive()) return Finish(TEXT("barrage-death-first-projectile-missed"));
		Pending->DeathLastInactiveWorldTime = Now;
		Pending->WaitReason = TEXT("barrage-death-authority-first-projectile");
		return true;
	}
	const float ActivationObservationGap = Now - Pending->DeathLastInactiveWorldTime;
	const float Age = FirstProjectile->GetGameTimeSinceCreation();
	if (!AuthoritySpec->IsActive() || !FMath::IsFinite(Age) || Age < -KINDA_SMALL_NUMBER || Age >= Pending->DeathShotInterval
		|| !FMath::IsFinite(ActivationObservationGap) || ActivationObservationGap < 0.0f || ActivationObservationGap >= Pending->DeathShotInterval)
		return Finish(TEXT("barrage-death-first-shot-deadline-missed"));
	ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(AuthorityPawn);
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageDeathDispatch player=%d ownerWorld=%s ownerPawn=%s ownerController=%s authorityWorld=%s authorityPawn=%s authorityController=%s ownerHandle=%s authorityHandle=%s ownerActive=1 authorityActive=1 firstProjectile=%s projectileCount=%d projectileAge=%.6f shotInterval=%.6f deadlineRemaining=%.6f activationObservationGap=%.6f health=%.6f worldTime=%.6f guard=%d scope=production-self-destruct"),
		Pending->PlayerId, *Pending->MovePawn->GetWorld()->GetPathName(), *Pending->MovePawn->GetPathName(), *Pending->MoveController->GetPathName(),
		*AuthorityPawn->GetWorld()->GetPathName(), *AuthorityPawn->GetPathName(), *Pending->MoveAuthorityController->GetPathName(),
		*Pending->MoveAbilityHandle.ToString(), *Pending->CancelAuthorityHandle.ToString(), *FirstProjectile->GetPathName(), Count, Age, Pending->DeathShotInterval, Pending->DeathShotInterval - Age, ActivationObservationGap,
		Health->GetHealth(), AuthorityPawn->GetWorld()->GetTimeSeconds(), GAllowActorScriptExecutionInEditor);
	Pending->bDeathDispatched = true;
	Health->DamageSelfDestruct();
	// Death can detach/destroy the pawn and remove its granted spec synchronously.
	AuthorityPawn = Pending->AuthorityPawn.Get();
	Health = AuthorityPawn ? ULyraHealthComponent::FindHealthComponent(AuthorityPawn) : nullptr;
	ABwayPlayerState* AuthorityState = Pending->AuthorityState.Get();
	ULyraAbilitySystemComponent* AuthorityASC = Pending->DeathAuthorityASC.Get();
	const bool AuthorityASCMatches = AuthorityASC && AuthorityState && AuthorityState->GetLyraAbilitySystemComponent() == AuthorityASC;
	AuthoritySpec = AuthorityASCMatches ? AuthorityASC->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle) : nullptr;
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageDeathReadback player=%d authorityPawn=%s pawnValid=%d health=%.6f dead=%d specFound=%d active=%d authorityASC=%s originalASCValid=%d originalASCMatches=%d avatar=%s movementStopped=%d guard=%d scope=post-self-destruct-observation"),
		Pending->PlayerId, *GetPathNameSafe(AuthorityPawn), AuthorityPawn != nullptr, Health ? Health->GetHealth() : -1.0f, Health ? int32(Health->IsDeadOrDying()) : -1,
		AuthoritySpec != nullptr, AuthorityASCMatches ? int32(AuthoritySpec && AuthoritySpec->IsActive()) : -1, *GetPathNameSafe(AuthorityASC), AuthorityASC != nullptr, AuthorityASCMatches, *GetPathNameSafe(AuthorityASC ? AuthorityASC->GetAvatarActor() : nullptr),
		AuthorityASC ? AuthorityASC->GetTagCount(TAG_Gameplay_MovementStopped) : -1, GAllowActorScriptExecutionInEditor);
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageDeathCleanup player=%d reason=death-dispatched dispatched=1 cancelled=0 queueRemoved=1 abilityCleanupRequested=0 guard=%d scope=native-queue-only"), Pending->PlayerId, GAllowActorScriptExecutionInEditor);
	return Finish(TEXT("death-dispatched"));
}

ABwayCharacterWithAbilities* SlidePawn(int32 Index) { return Index == 0 ? Pending->MovePawn.Get() : Pending->AuthorityPawn.Get(); }

bool IsLiveBarrageSlideEndpoint(int32 Index)
{
	return (Index == 0 ? IsLiveMovementTarget() : IsLiveBarrageMoveAuthority())
		&& SlidePawn(Index)->GetLyraAbilitySystemComponent() == Pending->SlideEndpoints[Index].ASC.Get();
}

bool HasBarrageSlideSupport(const UBwayCharacterMovementComponent* Move)
{
	return Move && !Move->IsFalling() && !Move->IsSlideJumping()
		&& (Move->MovementMode == MOVE_Walking || Move->IsSliding())
		&& Move->CurrentFloor.HitResult.IsValidBlockingHit()
		&& FMath::IsFinite(Move->CurrentFloor.GetDistanceToFloor())
		&& Move->CurrentFloor.GetDistanceToFloor() >= -0.5f
		&& Move->CurrentFloor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST + 0.01f
		&& Move->CurrentFloor.HitResult.ImpactNormal.Z >= 0.99f;
}

bool HasBarrageWalkingSupport(const UBwayCharacterMovementComponent* Move)
{
	return Move && Move->MovementMode == MOVE_Walking && !Move->IsSlideJumping()
		&& Move->CurrentFloor.HitResult.IsValidBlockingHit() && Move->CurrentFloor.IsWalkableFloor()
		&& Move->IsWalkable(Move->CurrentFloor.HitResult)
		&& FMath::IsFinite(Move->CurrentFloor.GetDistanceToFloor())
		&& Move->CurrentFloor.GetDistanceToFloor() >= -0.5f
		&& Move->CurrentFloor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST + 0.01f;
}

void LogBarrageSlideState(const TCHAR* Phase)
{
	if (!Pending->bBarrageSlide || !Pending->bSlidePrepared) return;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const bool Valid = IsLiveBarrageSlideEndpoint(Index);
		ABwayCharacterWithAbilities* Pawn = Valid ? SlidePawn(Index) : nullptr;
		const auto* Move = Pawn ? Pawn->GetBwayCharacterMovement() : nullptr;
		const auto* ASC = Valid ? Pending->SlideEndpoints[Index].ASC.Get() : nullptr;
		const auto* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Pending->SlideEndpoints[Index].Handle) : nullptr;
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageSlideState player=%d phase=%s endpoint=%s valid=%d pawn=%s asc=%s slideHandle=%s mode=%d custom=%d sliding=%d slideActive=%d inputPressed=%d held=%d slidingTag=%d movementStopped=%d speed=%.6f displacementSincePress=%.6f minSlide=%.6f maxSpeed=%.6f minAnalog=%.6f crouched=%d capsuleHalfHeight=%.6f floorAngle=%.6f floorDistance=%.6f floorNormalZ=%.6f supported=%d inputTicks=%d aimTicks=%d guard=%d floorBlocking=%d floorPenetrating=%d slideJump=%d maxFloorDistance=%.6f floorWalkable=%d walkableSupported=%d orient=%d rotationPitch=%.6f rotationYaw=%.6f rotationRoll=%.6f supportScope=%s"),
			Pending->PlayerId, Phase, Index == 0 ? TEXT("owner") : TEXT("authority"), Valid, *GetPathNameSafe(Pawn), *GetPathNameSafe(ASC), *Pending->SlideEndpoints[Index].Handle.ToString(),
			Move ? int32(Move->MovementMode) : -1, Move ? int32(Move->CustomMovementMode) : -1, Move && Move->IsSliding(), Spec ? int32(Spec->IsActive()) : -1, Spec ? int32(Spec->InputPressed) : -1,
			Pending->bSlideInputHeld, ASC ? ASC->GetTagCount(Pending->SlidingTag) : -1, ASC ? ASC->GetTagCount(TAG_Gameplay_MovementStopped) : -1,
			Pawn ? Pawn->GetVelocity().Size() : -1., Pawn ? FVector::Dist2D(Pawn->GetActorLocation(), Pending->SlideEndpoints[Index].PressLocation) : -1.,
			Move ? Move->MinSlideSpeed : -1.f, Move ? Move->GetMaxSpeed() : -1.f, Move ? Move->GetMinAnalogSpeed() : -1.f, Pawn ? int32(Pawn->bIsCrouched) : -1,
			Pawn ? Pawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : -1.f, Move ? Move->GetWalkableFloorAngle() : -1.f,
			Move ? Move->CurrentFloor.GetDistanceToFloor() : -1.f, Move ? Move->CurrentFloor.HitResult.ImpactNormal.Z : -1.f,
			HasBarrageSlideSupport(Move), Pending->MoveInputTicks, Pending->AimInputTicks, GAllowActorScriptExecutionInEditor,
			Move && Move->CurrentFloor.HitResult.bBlockingHit, Move && Move->CurrentFloor.HitResult.bStartPenetrating, Move && Move->IsSlideJumping(), UCharacterMovementComponent::MAX_FLOOR_DIST,
			Move && Move->CurrentFloor.IsWalkableFloor(), HasBarrageWalkingSupport(Move), Move ? int32(Move->bOrientRotationToMovement) : -1,
			Move ? Move->RotationRate.Pitch : 0., Move ? Move->RotationRate.Yaw : 0., Move ? Move->RotationRate.Roll : 0.,
			Pending->Phase == EPhase::BarrageMoveCleanup ? TEXT("post-evidence") : (Pending->Phase == EPhase::Activate || Pending->Phase == EPhase::BarrageMoveChannel || Pending->bSlideInputHeld || (Move && Move->IsSliding()) ? TEXT("flat-slide") : TEXT("walking-recovery")));
	}
}

void RemoveBarrageSlideObservers()
{
	if (!Pending.IsSet() || !Pending->bBarrageSlide) return;
	int32 Removed = 0;
	for (FSlideEndpoint& Endpoint : Pending->SlideEndpoints)
	{
		if (ULyraAbilitySystemComponent* ASC = Endpoint.ASC.Get())
		{
			if (Endpoint.Activated.IsValid()) { ASC->AbilityActivatedCallbacks.Remove(Endpoint.Activated); ++Removed; }
			if (Endpoint.Ended.IsValid()) { ASC->AbilityEndedCallbacks.Remove(Endpoint.Ended); ++Removed; }
		}
		Endpoint.Activated.Reset(); Endpoint.Ended.Reset();
	}
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageSlideObserversRemoved player=%d removed=%d held=%d guard=%d"), Pending->PlayerId, Removed, Pending->bSlideInputHeld, GAllowActorScriptExecutionInEditor);
}

bool PrepareBarrageSlide()
{
	Pending->SlideInputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Slide"));
	Pending->SlidingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sliding"));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		ABwayCharacterWithAbilities* Pawn = SlidePawn(Index);
		auto* Move = Pawn->GetBwayCharacterMovement();
		auto* ASC = Pawn->GetLyraAbilitySystemComponent();
		FSlideEndpoint& Endpoint = Pending->SlideEndpoints[Index];
		Endpoint.ASC = ASC;
		int32 Bindings = 0;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.GetDynamicSpecSourceTags().HasTagExact(Pending->SlideInputTag)) continue;
			++Bindings;
			if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_Slide>() && !Spec.IsActive()
				&& (Index != 0 || !Spec.InputPressed) && Spec.Ability->CheckCooldown(Spec.Handle, ASC->AbilityActorInfo.Get())) Endpoint.Handle = Spec.Handle;
		}
		if (!Move || Bindings != 1 || !Endpoint.Handle.IsValid() || !HasBarrageSlideSupport(Move)
			|| Pawn->bIsCrouched || Pawn->bPressedJump || Move->MinSlideSpeed != 0.f || ASC->HasMatchingGameplayTag(Pending->SlidingTag)) return false;
		for (float Value : { Move->BaseSlideSpeed, Move->SlideMaxSpeedFactor, Move->SlideSlopeAccelerationFactor, Move->SlideSteerSpeed,
			Move->SlideFrictionPower, Move->SlideBaseFrictionFactor, Move->SlideWalkableFloorAngle })
			if (!FMath::IsFinite(Value) || Value < 0.f) return false;
		if (Move->BaseSlideSpeed <= 0.f || Move->SlideMaxSpeedFactor <= 0.f) return false;
		Endpoint.CapsuleHalfHeight = Pawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		Endpoint.FloorAngle = Move->GetWalkableFloorAngle(); Endpoint.RotationRate = Move->RotationRate;
		Endpoint.bOrientToMovement = Move->bOrientRotationToMovement;
		Endpoint.PressLocation = Pawn->GetActorLocation();
		const auto* SlideSpec = ASC->FindAbilitySpecFromHandle(Endpoint.Handle);
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageSlidePreflight player=%d endpoint=%s pawn=%s asc=%s slideHandle=%s minSlide=%.6f baseSlideSpeed=%.6f maxFactor=%.6f slopeFactor=%.6f steerSpeed=%.6f frictionPower=%.6f baseFriction=%.6f slideFloorAngle=%.6f capsuleHalfHeight=%.6f originalFloorAngle=%.6f orient=%d rotationPitch=%.6f rotationYaw=%.6f rotationRoll=%.6f supported=1 slideCooldownReady=1 guard=%d slideClass=%s slideInput=%s bindings=%d scope=grounded-held-zero-threshold"),
			Pending->PlayerId, Index == 0 ? TEXT("owner") : TEXT("authority"), *Pawn->GetPathName(), *ASC->GetPathName(), *Endpoint.Handle.ToString(), Move->MinSlideSpeed,
			Move->BaseSlideSpeed, Move->SlideMaxSpeedFactor, Move->SlideSlopeAccelerationFactor, Move->SlideSteerSpeed, Move->SlideFrictionPower, Move->SlideBaseFrictionFactor,
			Move->SlideWalkableFloorAngle, Endpoint.CapsuleHalfHeight, Endpoint.FloorAngle, Endpoint.bOrientToMovement, Endpoint.RotationRate.Pitch, Endpoint.RotationRate.Yaw, Endpoint.RotationRate.Roll, GAllowActorScriptExecutionInEditor,
			*GetPathNameSafe(SlideSpec->Ability->GetClass()), *Pending->SlideInputTag.ToString(), Bindings);
	}
	Pending->bSlidePrepared = true;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const TWeakObjectPtr<ULyraAbilitySystemComponent> WeakASC = Pending->SlideEndpoints[Index].ASC;
		const double RequestStart = Pending->HealthStart;
		auto Observe = [WeakASC, RequestStart, Index](UGameplayAbility* Ability, const TCHAR* Event)
		{
			if (!Pending.IsSet() || Pending->HealthStart != RequestStart || !Pending->bSlidePrepared || !Ability || !IsLiveBarrageSlideEndpoint(Index) || Pending->SlideEndpoints[Index].ASC != WeakASC) return;
			const auto Handle = Ability->GetCurrentAbilitySpecHandle();
			const bool Slide = Handle == Pending->SlideEndpoints[Index].Handle;
			if (!Slide && Handle != (Index == 0 ? Pending->MoveAbilityHandle : Pending->CancelAuthorityHandle)) return;
			UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageSlideAbilityEvent player=%d endpoint=%s event=%s family=%s handle=%s guard=%d"), Pending->PlayerId, Index == 0 ? TEXT("owner") : TEXT("authority"), Event, Slide ? TEXT("slide") : TEXT("barrage"), *Handle.ToString(), GAllowActorScriptExecutionInEditor);
			LogBarrageSlideState(Event);
		};
		Pending->SlideEndpoints[Index].Activated = WeakASC->AbilityActivatedCallbacks.AddLambda([Observe](UGameplayAbility* Ability) { Observe(Ability, TEXT("activated")); });
		Pending->SlideEndpoints[Index].Ended = WeakASC->AbilityEndedCallbacks.AddLambda([Observe](UGameplayAbility* Ability) { Observe(Ability, TEXT("ended")); });
	}
	return true;
}

bool BarrageSlideInput(bool Pressed, const TCHAR* Reason)
{
	if (GAllowActorScriptExecutionInEditor || !IsLiveBarrageSlideEndpoint(0)) return false;
	ULyraAbilitySystemComponent* ASC = Pending->SlideEndpoints[0].ASC.Get();
	const auto* CapturedSpec = ASC->FindAbilitySpecFromHandle(Pending->SlideEndpoints[0].Handle);
	int32 Bindings = 0;
	for (const auto& Spec : ASC->GetActivatableAbilities())
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(Pending->SlideInputTag)) ++Bindings;
	if (Bindings != 1 || !CapturedSpec || !CapturedSpec->Ability || !CapturedSpec->Ability->IsA<UBwayGameplayAbility_Slide>()
		|| !CapturedSpec->GetDynamicSpecSourceTags().HasTagExact(Pending->SlideInputTag)) return false;
	Pending->bSlideInputHeld = Pressed;
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageSlideInput player=%d pressed=%d reason=%s pawn=%s asc=%s handle=%s guard=%d"), Pending->PlayerId, Pressed, Reason, *Pending->MovePawn->GetPathName(), *ASC->GetPathName(), *Pending->SlideEndpoints[0].Handle.ToString(), GAllowActorScriptExecutionInEditor);
	if (Pressed) ASC->AbilityInputTagPressed(Pending->SlideInputTag); else ASC->AbilityInputTagReleased(Pending->SlideInputTag);
	ASC->ProcessAbilityInput(0.f, false);
	return true;
}

bool BarrageSlideIsWalkingRestored()
{
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!IsLiveBarrageSlideEndpoint(Index)) return false;
		const auto* Pawn = SlidePawn(Index); const auto* Move = Pawn->GetBwayCharacterMovement();
		const FSlideEndpoint& Endpoint = Pending->SlideEndpoints[Index];
		const auto* Spec = Endpoint.ASC->FindAbilitySpecFromHandle(Endpoint.Handle);
		if (!Spec || Spec->IsActive() || (Index == 0 && Spec->InputPressed) || Endpoint.ASC->HasMatchingGameplayTag(Pending->SlidingTag) || Pawn->bIsCrouched
			|| Move->MovementMode != MOVE_Walking || Move->IsSlideJumping() || Move->bOrientRotationToMovement != Endpoint.bOrientToMovement
			|| !FMath::IsNearlyEqual(Pawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), Endpoint.CapsuleHalfHeight, 0.01f)
			|| !FMath::IsNearlyEqual(Move->GetWalkableFloorAngle(), Endpoint.FloorAngle, 0.01f) || !Move->RotationRate.Equals(Endpoint.RotationRate, 0.01f)) return false;
	}
	return !Pending->bSlideInputHeld;
}

bool BarrageSlideIsReleasedForCleanup()
{
	if (Pending->bSlideInputHeld) return false;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!IsLiveBarrageSlideEndpoint(Index)) return false;
		const FSlideEndpoint& Endpoint = Pending->SlideEndpoints[Index];
		const auto* Spec = Endpoint.ASC->FindAbilitySpecFromHandle(Endpoint.Handle);
		if (!Spec || Spec->IsActive() || (Index == 0 && Spec->InputPressed)
			|| Endpoint.ASC->HasMatchingGameplayTag(Pending->SlidingTag)) return false;
	}
	return true;
}

int32 TickBarrageSlideReadiness()
{
	if (!IsLiveBarrageSlideEndpoint(0) || !IsLiveBarrageSlideEndpoint(1)) return -1;
	const double Now = FPlatformTime::Seconds();
	if (Now - Pending->HealthStart > 4.0) return -1;
	for (int32 Index = 0; Index < 2; ++Index)
		if (!HasBarrageSlideSupport(SlidePawn(Index)->GetBwayCharacterMovement())) return -1;
	if (!Pending->bSlideInputHeld)
	{
		for (int32 Index = 0; Index < 2; ++Index)
			if (SlidePawn(Index)->GetVelocity().Size2D() <= 100.f || FVector::Dist2D(SlidePawn(Index)->GetActorLocation(), Pending->SlideEndpoints[Index].PressLocation) <= 2.f) return 0;
		for (int32 Index = 0; Index < 2; ++Index) Pending->SlideEndpoints[Index].PressLocation = SlidePawn(Index)->GetActorLocation();
		Pending->SlidePressTime = Now;
		LogBarrageSlideState(TEXT("before-press"));
		if (!BarrageSlideInput(true, TEXT("moving-warmup"))) return -1;
		return 0;
	}
	if (Now - Pending->SlidePressTime > 2.0) return -1;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const auto* Pawn = SlidePawn(Index); const auto* Move = Pawn->GetBwayCharacterMovement();
		const auto* Spec = Pending->SlideEndpoints[Index].ASC->FindAbilitySpecFromHandle(Pending->SlideEndpoints[Index].Handle);
		if (!Spec || !Spec->IsActive() || !Move->IsSliding() || Pawn->GetVelocity().Size2D() <= 20.f
			|| FVector::Dist2D(Pawn->GetActorLocation(), Pending->SlideEndpoints[Index].PressLocation) <= 2.f) return 0;
	}
	LogBarrageSlideState(TEXT("moving-slide-ready"));
	return 1;
}

void LogBarrageMoveState(const TCHAR* Phase)
{
	if (!IsLiveMovementTarget() || !IsLiveBarrageMoveAuthority()) return;
	ABwayCharacterWithAbilities* Owner = Pending->MovePawn.Get();
	ABwayCharacterWithAbilities* Authority = Pending->AuthorityPawn.Get();
	const UCharacterMovementComponent* OwnerMove = Owner->GetCharacterMovement();
	const UCharacterMovementComponent* AuthorityMove = Authority->GetCharacterMovement();
	if (!OwnerMove || !AuthorityMove) return;
	const ULyraAbilitySystemComponent* OwnerASC = Pending->MoveState->GetLyraAbilitySystemComponent();
	const ULyraAbilitySystemComponent* AuthorityASC = Pending->AuthorityState->GetLyraAbilitySystemComponent();
	const FGameplayAbilitySpec* OwnerSpec = OwnerASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
	const FGameplayAbilitySpec* AuthoritySpec = AuthorityASC->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle);
	FVector ViewLocation;
	FRotator ViewRotation;
	Pending->MoveAuthorityController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector O = Owner->GetActorLocation(), A = Authority->GetActorLocation();
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveState player=%d phase=%s elapsed=%.6f ownerActive=%d authorityActive=%d ownerTag=%d authorityTag=%d ownerX=%.6f ownerY=%.6f ownerZ=%.6f authorityX=%.6f authorityY=%.6f authorityZ=%.6f ownerSpeed=%.6f authoritySpeed=%.6f ownerControlYaw=%.6f authorityControlYaw=%.6f authorityViewYaw=%.6f authorityViewPitch=%.6f ownerMaxSpeed=%.6f authorityMaxSpeed=%.6f ownerMode=%d authorityMode=%d ownerBraking=%.6f authorityBraking=%.6f ownerFriction=%.6f authorityFriction=%.6f inputTicks=%d aimTicks=%d requestedYaw=%.6f ownerCooldownReady=%d authorityCooldownReady=%d guard=%d castElapsed=%.6f ownerBrakingFactor=%.6f authorityBrakingFactor=%.6f"),
		Pending->PlayerId, Phase, Owner->GetWorld()->GetTimeSeconds() - Pending->MoveStartWorldTime,
		OwnerSpec ? int32(OwnerSpec->IsActive()) : -1, AuthoritySpec ? int32(AuthoritySpec->IsActive()) : -1,
		OwnerASC->GetTagCount(TAG_Gameplay_MovementStopped), AuthorityASC->GetTagCount(TAG_Gameplay_MovementStopped), O.X, O.Y, O.Z, A.X, A.Y, A.Z,
		Owner->GetVelocity().Size2D(), Authority->GetVelocity().Size2D(), Pending->MoveController->GetControlRotation().Yaw,
		Pending->MoveAuthorityController->GetControlRotation().Yaw, ViewRotation.Yaw, ViewRotation.Pitch,
		OwnerMove->GetMaxSpeed(), AuthorityMove->GetMaxSpeed(), int32(OwnerMove->MovementMode), int32(AuthorityMove->MovementMode),
		OwnerMove->GetMaxBrakingDeceleration(), AuthorityMove->GetMaxBrakingDeceleration(),
		OwnerMove->bUseSeparateBrakingFriction ? OwnerMove->BrakingFriction : OwnerMove->GroundFriction,
		AuthorityMove->bUseSeparateBrakingFriction ? AuthorityMove->BrakingFriction : AuthorityMove->GroundFriction,
		Pending->MoveInputTicks, Pending->AimInputTicks, Pending->AimInputTotal,
		OwnerSpec && OwnerSpec->Ability ? int32(OwnerSpec->Ability->CheckCooldown(OwnerSpec->Handle, OwnerASC->AbilityActorInfo.Get())) : -1,
		AuthoritySpec && AuthoritySpec->Ability ? int32(AuthoritySpec->Ability->CheckCooldown(AuthoritySpec->Handle, AuthorityASC->AbilityActorInfo.Get())) : -1,
		GAllowActorScriptExecutionInEditor, Pending->bMoveCastAttempted ? Owner->GetWorld()->GetTimeSeconds() - Pending->MoveCastWorldTime : -1.0f,
		OwnerMove->BrakingFrictionFactor, AuthorityMove->BrakingFrictionFactor);
	LogBarrageSlideState(Phase);
}

void LogBarrageMoveOrigin(const TCHAR* Endpoint, const ABwayCharacterWithAbilities* Pawn, const ABwayPlayerController* Controller)
{
	const FVector P = Pawn->GetActorLocation(), S = Pawn->GetActorScale3D();
	const FRotator R = Pawn->GetActorRotation(), V = Controller->GetControlRotation();
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveOrigin player=%d endpoint=%s pawn=%s controller=%s state=%s net=%d role=%d x=%.6f y=%.6f z=%.6f pitch=%.6f yaw=%.6f roll=%.6f viewPitch=%.6f viewYaw=%.6f viewRoll=%.6f scaleX=%.6f scaleY=%.6f scaleZ=%.6f guard=%d"),
		Pending->PlayerId, Endpoint, *Pawn->GetPathName(), *Controller->GetPathName(), *GetPathNameSafe(Pawn->GetPlayerState()),
		int32(Pawn->GetNetMode()), int32(Pawn->GetLocalRole()), P.X, P.Y, P.Z, R.Pitch, R.Yaw, R.Roll, V.Pitch, V.Yaw, V.Roll, S.X, S.Y, S.Z, GAllowActorScriptExecutionInEditor);
}

int32 PrepareBarrageMovement()
{
	const int32 AuthorityMatches = CaptureAuthorityTarget();
	if (AuthorityMatches != 1) return AuthorityMatches;
	ABwayCharacterWithAbilities* Owner = Pending->MovePawn.Get();
	ABwayCharacterWithAbilities* Authority = Pending->AuthorityPawn.Get();
	const ULyraAbilitySystemComponent* OwnerASC = Pending->MoveState->GetLyraAbilitySystemComponent();
	const ULyraAbilitySystemComponent* AuthorityASC = Pending->AuthorityState->GetLyraAbilitySystemComponent();
	const FGameplayAbilitySpec* OwnerSpec = OwnerASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
	const UClass* Family = FindObject<UClass>(nullptr, TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage"));
	const FGameplayAbilitySpec* AuthoritySpec = nullptr;
	int32 SpecCount = 0;
	for (const FGameplayAbilitySpec& Spec : AuthorityASC->GetActivatableAbilities())
		if (Family && Spec.Ability && Spec.Ability->IsA(Family)) { AuthoritySpec = &Spec; ++SpecCount; }
	if (!OwnerSpec || !OwnerSpec->Ability || OwnerSpec->IsActive() || SpecCount != 1 || AuthoritySpec->IsActive()
		|| !OwnerSpec->Ability->CheckCooldown(OwnerSpec->Handle, OwnerASC->AbilityActorInfo.Get())
		|| !AuthoritySpec->Ability->CheckCooldown(AuthoritySpec->Handle, AuthorityASC->AbilityActorInfo.Get())) return -1;
	const FSoftObjectProperty* ConfigProperty = FindFProperty<FSoftObjectProperty>(AuthoritySpec->Ability->GetClass(), TEXT("KitConfig"));
	const UObject* Config = ConfigProperty ? ConfigProperty->GetPropertyValue_InContainer(AuthoritySpec->Ability).Get() : nullptr;
	const FFloatProperty* Interval = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("BarrageShotInterval")) : nullptr;
	const FIntProperty* Count = Config ? FindFProperty<FIntProperty>(Config->GetClass(), TEXT("BarrageShotCount")) : nullptr;
	const FFloatProperty* Cooldown = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("BarrageCooldown")) : nullptr;
	Pending->MoveShotInterval = Interval ? Interval->GetPropertyValue_InContainer(Config) : 0.0f;
	Pending->MoveShotCount = Count ? Count->GetPropertyValue_InContainer(Config) : 0;
	Pending->MoveCooldown = Cooldown ? Cooldown->GetPropertyValue_InContainer(Config) : 0.0f;
	AActor* ExistingProjectile = nullptr;
	if (!IsLiveMovementTarget() || !IsLiveBarrageMoveAuthority() || !Owner->GetCharacterMovement() || !Authority->GetCharacterMovement()
		|| Owner->GetCharacterMovement()->MovementMode != MOVE_Walking || Authority->GetCharacterMovement()->MovementMode != MOVE_Walking
		|| Owner->GetVelocity().Size() > 1.0 || Authority->GetVelocity().Size() > 1.0
		|| FVector::Dist(Owner->GetActorLocation(), Authority->GetActorLocation()) > 2.0
		|| Pending->MoveController->IsMoveInputIgnored() || Pending->MoveController->IsLookInputIgnored()
		|| !Pending->MoveController->RotationInput.IsNearlyZero(0.01f)
		|| OwnerASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped) || AuthorityASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)
		|| CountRawlinsBullets(Authority, ExistingProjectile) != 0 || Pending->MoveShotCount < 2
		|| !FMath::IsFinite(Pending->MoveShotInterval) || Pending->MoveShotInterval <= 0.0f
		|| !FMath::IsFinite(Pending->MoveCooldown) || Pending->MoveCooldown <= 0.0f
		|| (Pending->MoveShotCount - 1) * Pending->MoveShotInterval > 5.0f) return -1;
	Pending->CancelAuthorityHandle = AuthoritySpec->Handle;
	Pending->MoveOwnerOriginalTransform = Owner->GetActorTransform();
	Pending->MoveAuthorityOriginalTransform = Authority->GetActorTransform();
	Pending->MoveOwnerOriginalView = Pending->MoveController->GetControlRotation();
	Pending->MoveAuthorityOriginalView = Pending->MoveAuthorityController->GetControlRotation();
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMovePreflight player=%d ownerPawn=%s ownerController=%s authorityPawn=%s authorityController=%s ownerHandle=%s authorityHandle=%s ownerRound=%d authorityRound=%d shotCount=%d shotInterval=%.6f duration=%.6f cooldown=%.6f warmup=0.3 recovery=1.0 yawInputRate=15 yawInputLimit=20 existingProjectiles=0 guard=%d scope=%s"),
		Pending->PlayerId, *Owner->GetPathName(), *Pending->MoveController->GetPathName(), *Authority->GetPathName(), *Pending->MoveAuthorityController->GetPathName(),
		*Pending->MoveAbilityHandle.ToString(), *Pending->CancelAuthorityHandle.ToString(), Pending->MoveRoundNumber, Pending->AuthorityRoundNumber,
		Pending->MoveShotCount, Pending->MoveShotInterval, (Pending->MoveShotCount - 1) * Pending->MoveShotInterval, Pending->MoveCooldown, GAllowActorScriptExecutionInEditor,
		Pending->bBarrageSlide ? TEXT("grounded-held-slide-and-aim") : TEXT("walking-input-and-aim"));
	LogBarrageMoveOrigin(TEXT("owner"), Owner, Pending->MoveController.Get());
	LogBarrageMoveOrigin(TEXT("authority"), Authority, Pending->MoveAuthorityController.Get());
	if (Pending->bBarrageSlide && !PrepareBarrageSlide()) return -1;
	return 1;
}

void ObserveBarrageMoveShots()
{
	UClass* Family = FindObject<UClass>(nullptr, TEXT("/Script/HeroRawlinsRuntime.BwayRawlinsBulletProjectile"));
	if (!Family) return;
	for (TActorIterator<AActor> It(Pending->AuthorityPawn->GetWorld(), Family); It; ++It)
	{
		AActor* Shot = *It;
		if (!IsValid(Shot) || Shot->GetOwner() != Pending->AuthorityPawn.Get() || Shot->GetInstigator() != Pending->AuthorityPawn.Get()
			|| Shot->GetVelocity().IsNearlyZero() || Pending->MoveSeenProjectiles.Contains(Shot->GetPathName())) continue;
		Pending->MoveSeenProjectiles.Add(Shot->GetPathName());
		FVector ViewLocation;
		FRotator ViewRotation;
		Pending->MoveAuthorityController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector Velocity = Shot->GetVelocity();
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveShot player=%d projectile=%s authorityPawn=%s age=%.6f worldTime=%.6f vx=%.6f vy=%.6f vz=%.6f headingYaw=%.6f headingPitch=%.6f authorityViewYaw=%.6f authorityViewPitch=%.6f ownerControlYaw=%.6f inputTicks=%d aimTicks=%d guard=%d scope=first-observed-projectile"),
			Pending->PlayerId, *Shot->GetPathName(), *Pending->AuthorityPawn->GetPathName(), Shot->GetGameTimeSinceCreation(), Shot->GetWorld()->GetTimeSeconds(),
			Velocity.X, Velocity.Y, Velocity.Z, Velocity.Rotation().Yaw, Velocity.Rotation().Pitch, ViewRotation.Yaw, ViewRotation.Pitch,
			Pending->MoveController->GetControlRotation().Yaw, Pending->MoveInputTicks, Pending->AimInputTicks, GAllowActorScriptExecutionInEditor);
	}
}

void BeginBarrageMoveCleanup(const TCHAR* Reason)
{
	Pending->MoveStopReason = Reason;
	Pending->WaitReason = Reason;
	Pending->Phase = EPhase::BarrageMoveCleanup;
}

double GetBarrageMoveDrainMinimumSeconds()
{
	// Allow an owner/server round trip for already queued movement, then scheduling
	// slack. This is a cleanup wait only; never change packet simulation or saved moves.
	double DelayMilliseconds = 0.0;
	for (const ABwayPlayerController* Controller : { Pending->MoveController.Get(), Pending->MoveAuthorityController.Get() })
	{
		const UNetDriver* Driver = Controller && Controller->GetWorld() ? Controller->GetWorld()->GetNetDriver() : nullptr;
		if (!Driver) continue;
		const FPacketSimulationSettings& Settings = Driver->PacketSimulationSettings;
		const double Outgoing = FMath::Max3(0.0, double(Settings.PktLag) + FMath::Abs(double(Settings.PktLagVariance)),
			FMath::Max(double(Settings.PktLagMin), double(Settings.PktLagMax)) + FMath::Max(0.0, double(Settings.PktJitter)));
		const double Incoming = FMath::Max3(0.0, double(Settings.PktIncomingLagMin), double(Settings.PktIncomingLagMax));
		DelayMilliseconds += Outgoing + Incoming;
	}
	return 0.25 + DelayMilliseconds / 1000.0;
}

bool TickBarrageMoveCleanup(bool bHardDeadline = false)
{
	bool OwnerValid = IsLiveMovementTarget(), AuthorityValid = IsLiveBarrageMoveAuthority();
	if (!Pending->bMoveEvidenceEnded)
	{
		LogBarrageMoveState(TEXT("evidence-end"));
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveEvidenceEnd player=%d reason=%s ownerValid=%d authorityValid=%d inputTicks=%d aimTicks=%d observedShots=%d guard=%d"),
			Pending->PlayerId, Pending->MoveStopReason, OwnerValid, AuthorityValid, Pending->MoveInputTicks, Pending->AimInputTicks, Pending->MoveSeenProjectiles.Num(), GAllowActorScriptExecutionInEditor);
		Pending->bMoveEvidenceEnded = true;
	}
	if (!GAllowActorScriptExecutionInEditor && !Pending->bMoveCleanupApplied)
	{
		if (Pending->bBarrageSlide && !Pending->bSlideCleanupAttempted)
		{
			Pending->bSlideCleanupAttempted = true;
			if (Pending->bSlideInputHeld) BarrageSlideInput(false, TEXT("fixture-cleanup"));
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (!IsLiveBarrageSlideEndpoint(Index)) continue;
				ULyraAbilitySystemComponent* ASC = Pending->SlideEndpoints[Index].ASC.Get();
				const auto* Spec = ASC->FindAbilitySpecFromHandle(Pending->SlideEndpoints[Index].Handle);
				if (Spec && Spec->IsActive()) { ASC->CancelAbilityHandle(Spec->Handle); Pending->bMoveCleanupCancelled = true; }
			}
			OwnerValid = IsLiveMovementTarget(); AuthorityValid = IsLiveBarrageMoveAuthority();
		}
		// Stop input once, then let queued client moves settle before restoring the
		// authority transform. Repeated stops would hide movement still in flight.
		if (!Pending->bMoveCleanupInputsStopped)
		{
			if (OwnerValid)
			{
				Pending->MovePawn->ConsumeMovementInputVector();
				Pending->MovePawn->GetCharacterMovement()->StopMovementImmediately();
				Pending->MoveController->RotationInput = FRotator::ZeroRotator;
			}
			if (AuthorityValid)
			{
				Pending->AuthorityPawn->ConsumeMovementInputVector();
				Pending->AuthorityPawn->GetCharacterMovement()->StopMovementImmediately();
			}
			Pending->MoveInputStopTime = FPlatformTime::Seconds();
			Pending->MoveDrainMinimumSeconds = GetBarrageMoveDrainMinimumSeconds();
			Pending->bMoveCleanupInputsStopped = true;
			UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveDrainStart player=%d ownerValid=%d authorityValid=%d minimumSeconds=%.6f quietSeconds=0.1 guard=%d scope=post-evidence-input-drain"),
				Pending->PlayerId, OwnerValid, AuthorityValid, Pending->MoveDrainMinimumSeconds, GAllowActorScriptExecutionInEditor);
		}
		if (Pending->bMoveCastAttempted && !Pending->bMoveCleanupCancellationAttempted)
		{
			Pending->bMoveCleanupCancellationAttempted = true;
			if (AuthorityValid)
			{
				ULyraAbilitySystemComponent* ASC = Pending->AuthorityState->GetLyraAbilitySystemComponent();
				const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle);
				if (Spec && Spec->IsActive()) { ASC->CancelAbilityHandle(Spec->Handle); Pending->bMoveCleanupCancelled = true; }
			}
			OwnerValid = IsLiveMovementTarget();
			if (OwnerValid)
			{
				ULyraAbilitySystemComponent* ASC = Pending->MoveState->GetLyraAbilitySystemComponent();
				const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
				if (Spec && Spec->IsActive()) { ASC->CancelAbilityHandle(Spec->Handle); Pending->bMoveCleanupCancelled = true; }
			}
		}
		OwnerValid = IsLiveMovementTarget();
		AuthorityValid = IsLiveBarrageMoveAuthority();
		const FGameplayAbilitySpec* OwnerSpec = OwnerValid ? Pending->MoveState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle) : nullptr;
		const FGameplayAbilitySpec* AuthoritySpec = AuthorityValid ? Pending->AuthorityState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle) : nullptr;
		if ((OwnerSpec && OwnerSpec->IsActive()) || (AuthoritySpec && AuthoritySpec->IsActive()))
		{
			if (!bHardDeadline) return true;
			UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveCleanupBlocked player=%d reason=channel-still-active guard=%d"), Pending->PlayerId, GAllowActorScriptExecutionInEditor);
		}
		else
		{
			const double Now = FPlatformTime::Seconds();
			// A failed gameplay-setting assertion must not prevent safe fixture
			// restoration. Keep exact input/ability release and transport quiet here;
			// the final verdict still requires every original setting below.
			const bool Quiet = OwnerValid && AuthorityValid
				&& (!Pending->bBarrageSlide || BarrageSlideIsReleasedForCleanup())
				&& Pending->MovePawn->GetPendingMovementInputVector().IsNearlyZero(0.01)
				&& Pending->AuthorityPawn->GetPendingMovementInputVector().IsNearlyZero(0.01)
				&& Pending->MoveController->RotationInput.IsNearlyZero(0.01)
				&& Pending->MoveAuthorityController->RotationInput.IsNearlyZero(0.01)
				&& Pending->MovePawn->GetVelocity().Size() <= 1.0 && Pending->AuthorityPawn->GetVelocity().Size() <= 1.0
				&& FVector::Dist(Pending->MovePawn->GetActorLocation(), Pending->AuthorityPawn->GetActorLocation()) <= 2.0;
			if (!Quiet) Pending->MoveDrainQuietStart = 0.0;
			else if (Pending->MoveDrainQuietStart == 0.0) Pending->MoveDrainQuietStart = Now;
			const bool Drained = Quiet && Now - Pending->MoveInputStopTime >= Pending->MoveDrainMinimumSeconds
				&& Now - Pending->MoveDrainQuietStart >= 0.1;
			if (!Drained)
			{
				if (!bHardDeadline && OwnerValid && AuthorityValid) return true;
				UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveCleanupBlocked player=%d reason=movement-drain-not-settled ownerValid=%d authorityValid=%d elapsed=%.6f minimumSeconds=%.6f quiet=%d guard=%d"),
					Pending->PlayerId, OwnerValid, AuthorityValid, Now - Pending->MoveInputStopTime, Pending->MoveDrainMinimumSeconds, Quiet, GAllowActorScriptExecutionInEditor);
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveDrainReady player=%d elapsed=%.6f minimumSeconds=%.6f quietSeconds=%.6f ownerSpeed=%.6f authoritySpeed=%.6f ownerPendingInput=%.6f authorityPendingInput=%.6f ownerLookInput=%.6f authorityLookInput=%.6f endpointDistance=%.6f guard=%d scope=post-evidence-input-drain"),
					Pending->PlayerId, Now - Pending->MoveInputStopTime, Pending->MoveDrainMinimumSeconds, Now - Pending->MoveDrainQuietStart,
					Pending->MovePawn->GetVelocity().Size(), Pending->AuthorityPawn->GetVelocity().Size(),
					Pending->MovePawn->GetPendingMovementInputVector().Size(), Pending->AuthorityPawn->GetPendingMovementInputVector().Size(),
					Pending->MoveController->RotationInput.GetManhattanDistance(FRotator::ZeroRotator), Pending->MoveAuthorityController->RotationInput.GetManhattanDistance(FRotator::ZeroRotator),
					FVector::Dist(Pending->MovePawn->GetActorLocation(), Pending->AuthorityPawn->GetActorLocation()), GAllowActorScriptExecutionInEditor);
				Pending->MoveController->SetControlRotation(Pending->MoveOwnerOriginalView);
				ABwayCharacterWithAbilities* Authority = Pending->AuthorityPawn.Get();
				Pending->MoveAuthorityController->SetControlRotation(Pending->MoveAuthorityOriginalView);
				Pending->bMoveTeleportSucceeded = Authority->TeleportTo(Pending->MoveAuthorityOriginalTransform.GetLocation(), Pending->MoveAuthorityOriginalTransform.Rotator(), false, false);
				// Authority correction, with no client transform write or correction suppression.
				if (IsLiveBarrageMoveAuthority())
				{
					Pending->AuthorityPawn->GetCharacterMovement()->ForceClientAdjustment();
					Pending->AuthorityPawn->ForceNetUpdate();
				}
				Pending->MoveCleanupStart = FPlatformTime::Seconds();
				Pending->bMoveCleanupApplied = true;
			}
		}
	}
	OwnerValid = IsLiveMovementTarget();
	AuthorityValid = IsLiveBarrageMoveAuthority();
	const double OwnerError = OwnerValid ? FVector::Dist(Pending->MovePawn->GetActorLocation(), Pending->MoveOwnerOriginalTransform.GetLocation()) : -1.0;
	const double AuthorityError = AuthorityValid ? FVector::Dist(Pending->AuthorityPawn->GetActorLocation(), Pending->MoveAuthorityOriginalTransform.GetLocation()) : -1.0;
	const double OwnerViewError = OwnerValid ? (Pending->MoveController->GetControlRotation() - Pending->MoveOwnerOriginalView).GetNormalized().GetManhattanDistance(FRotator::ZeroRotator) : -1.0;
	const double AuthorityViewError = AuthorityValid ? (Pending->MoveAuthorityController->GetControlRotation() - Pending->MoveAuthorityOriginalView).GetNormalized().GetManhattanDistance(FRotator::ZeroRotator) : -1.0;
	const double OwnerRotationError = OwnerValid ? FMath::RadiansToDegrees(Pending->MovePawn->GetActorQuat().AngularDistance(Pending->MoveOwnerOriginalTransform.GetRotation())) : -1.0;
	const double AuthorityRotationError = AuthorityValid ? FMath::RadiansToDegrees(Pending->AuthorityPawn->GetActorQuat().AngularDistance(Pending->MoveAuthorityOriginalTransform.GetRotation())) : -1.0;
	const FGameplayAbilitySpec* FinalOwnerSpec = OwnerValid ? Pending->MoveState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle) : nullptr;
	const FGameplayAbilitySpec* FinalAuthoritySpec = AuthorityValid ? Pending->AuthorityState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle) : nullptr;
	const bool TransformRestored = Pending->bMoveCleanupApplied && Pending->bMoveTeleportSucceeded && OwnerValid && AuthorityValid
		&& OwnerError <= 3.0 && AuthorityError <= 1.0 && OwnerViewError <= 1.0 && AuthorityViewError <= 1.0
		&& OwnerRotationError <= 1.0 && AuthorityRotationError <= 1.0;
	const bool Restored = TransformRestored && (!Pending->bBarrageSlide || BarrageSlideIsWalkingRestored())
		&& FinalOwnerSpec && !FinalOwnerSpec->IsActive() && FinalAuthoritySpec && !FinalAuthoritySpec->IsActive()
		&& !Pending->MoveState->GetLyraAbilitySystemComponent()->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)
		&& !Pending->AuthorityState->GetLyraAbilitySystemComponent()->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)
		&& Pending->MovePawn->GetVelocity().Size() <= 1.0 && Pending->AuthorityPawn->GetVelocity().Size() <= 1.0;
	if (!bHardDeadline && OwnerValid && AuthorityValid && (!Restored || FPlatformTime::Seconds() - Pending->MoveCleanupStart < 0.5)) return true;
	LogBarrageMoveState(TEXT("cleanup"));
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageMoveCleanup player=%d reason=%s restored=%d ownerValid=%d authorityValid=%d authorityTeleport=%d clientTransformWrites=0 ownerPositionError=%.6f authorityPositionError=%.6f ownerViewError=%.6f authorityViewError=%.6f inputStopped=1 cleanupCancelled=%d hardDeadline=%d guard=%d ownerRotationError=%.6f authorityRotationError=%.6f scope=fixture-restoration-only"),
		Pending->PlayerId, Pending->MoveStopReason, Restored, OwnerValid, AuthorityValid, Pending->bMoveTeleportSucceeded,
		OwnerError, AuthorityError, OwnerViewError, AuthorityViewError, Pending->bMoveCleanupCancelled, bHardDeadline, GAllowActorScriptExecutionInEditor, OwnerRotationError, AuthorityRotationError);
	if (Pending->bBarrageSlide)
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: BarrageSlideCleanup player=%d restored=%d walkingRestored=%d held=%d cleanupCancelled=%d guard=%d transformRestored=%d inputReleased=%d"), Pending->PlayerId, Restored, BarrageSlideIsWalkingRestored(), Pending->bSlideInputHeld, Pending->bMoveCleanupCancelled, GAllowActorScriptExecutionInEditor, TransformRestored, BarrageSlideIsReleasedForCleanup());
	return Finish(Pending->MoveStopReason);
}

bool TickBarrageMovement(float DeltaSeconds)
{
	if (Pending->Phase == EPhase::BarrageMoveCleanup) return TickBarrageMoveCleanup();
	if (!IsLiveMovementTarget() || !IsLiveBarrageMoveAuthority())
	{
		BeginBarrageMoveCleanup(TEXT("barrage-movement-original-target-invalidated"));
		return TickBarrageMoveCleanup();
	}
	ABwayCharacterWithAbilities* Owner = Pending->MovePawn.Get();
	ABwayCharacterWithAbilities* Authority = Pending->AuthorityPawn.Get();
	const FGameplayAbilitySpec* OwnerSpec = Pending->MoveState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
	const FGameplayAbilitySpec* AuthoritySpec = Pending->AuthorityState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle);
	bool MovementScope = Owner->GetCharacterMovement()->MovementMode == MOVE_Walking && Authority->GetCharacterMovement()->MovementMode == MOVE_Walking;
	if (Pending->bBarrageSlide)
	{
		MovementScope = IsLiveBarrageSlideEndpoint(0) && IsLiveBarrageSlideEndpoint(1);
		for (int32 Index = 0; Index < 2 && MovementScope; ++Index)
		{
			const auto* Move = SlidePawn(Index)->GetBwayCharacterMovement();
			// Keep flat support while channeling or while the replicated release
			// still leaves this endpoint sliding. Normal walking can follow a ramp.
			MovementScope = Move && (Pending->Phase == EPhase::BarrageMoveChannel || Move->IsSliding()
				? HasBarrageSlideSupport(Move) : HasBarrageWalkingSupport(Move));
		}
	}
	if (!OwnerSpec || !AuthoritySpec || !MovementScope)
	{
		BeginBarrageMoveCleanup(TEXT("barrage-movement-spec-or-walking-scope-changed"));
		return TickBarrageMoveCleanup();
	}
	ObserveBarrageMoveShots();
	Owner->AddMovementInput(Pending->MoveDirection);
	++Pending->MoveInputTicks;
	if (Pending->Phase == EPhase::BarrageMoveChannel)
	{
		if (AuthoritySpec->IsActive()) Pending->bMoveSawAuthorityActive = true;
		if (OwnerSpec->IsActive() && AuthoritySpec->IsActive() && Pending->AimInputTotal < 20.0f)
		{
			const float YawInput = FMath::Min(15.0f * FMath::Clamp(DeltaSeconds, 0.0f, 0.05f), 20.0f - Pending->AimInputTotal);
			Owner->AddControllerYawInput(YawInput);
			Pending->AimInputTotal += YawInput;
			++Pending->AimInputTicks;
		}
		if (Pending->bMoveSawAuthorityActive && !OwnerSpec->IsActive() && !AuthoritySpec->IsActive())
		{
			LogBarrageMoveState(TEXT("channel-end"));
			if (Pending->bBarrageSlide)
			{
				Pending->Phase = EPhase::BarrageSlideRelease;
				Pending->SlideReleaseTime = FPlatformTime::Seconds();
				if (!BarrageSlideInput(false, TEXT("natural-channel-end"))) return Finish(TEXT("barrage-slide-release-target-invalid"));
				LogBarrageMoveState(TEXT("slide-release"));
				return true;
			}
			Pending->MoveRecoveryStart = FPlatformTime::Seconds();
			Pending->Phase = EPhase::BarrageMoveRecovery;
			LogBarrageMoveState(TEXT("recovery-start"));
		}
	}
	else if (Pending->Phase == EPhase::BarrageSlideRelease)
	{
		if (FPlatformTime::Seconds() - Pending->SlideReleaseTime > 1.0) return Finish(TEXT("barrage-slide-release-timeout"));
		if (BarrageSlideIsWalkingRestored())
		{
			Pending->MoveRecoveryStart = FPlatformTime::Seconds();
			Pending->Phase = EPhase::BarrageMoveRecovery;
			LogBarrageMoveState(TEXT("recovery-start"));
		}
	}
	else if (FPlatformTime::Seconds() - Pending->MoveRecoveryStart >= 1.0)
	{
		BeginBarrageMoveCleanup(TEXT("observed-recovery"));
		return TickBarrageMoveCleanup();
	}
	if (FPlatformTime::Seconds() - Pending->MoveLastSample >= 0.1)
	{
		LogBarrageMoveState(Pending->Phase == EPhase::BarrageMoveChannel ? TEXT("channel") : (Pending->Phase == EPhase::BarrageSlideRelease ? TEXT("slide-release-wait") : TEXT("recovery")));
		Pending->MoveLastSample = FPlatformTime::Seconds();
	}
	return true;
}

const TCHAR* HealthModeName()
{
	switch (Pending->HealthMode)
	{
	case EHealthMode::HealDamage: return TEXT("heal-damage");
	case EHealthMode::DamageOnly: return TEXT("damage-only");
	case EHealthMode::HealOnly: return TEXT("heal-only");
	default: return TEXT("none");
	}
}

void LogHealthState(const TCHAR* Marker, const TCHAR* Phase, float Requested = 0.0f, float Before = -1.0f, float After = -1.0f)
{
	const ABwayCharacterWithAbilities* Owner = Pending->MovePawn.Get();
	const ABwayCharacterWithAbilities* Authority = Pending->AuthorityPawn.Get();
	const ULyraHealthComponent* OwnerHealth = ULyraHealthComponent::FindHealthComponent(Owner);
	const ULyraHealthComponent* AuthorityHealth = ULyraHealthComponent::FindHealthComponent(Authority);
	const ULyraAbilitySystemComponent* OwnerASC = Pending->MoveState.IsValid() ? Pending->MoveState->GetLyraAbilitySystemComponent() : nullptr;
	const ULyraAbilitySystemComponent* AuthorityASC = Pending->AuthorityState.IsValid() ? Pending->AuthorityState->GetLyraAbilitySystemComponent() : nullptr;
	const FGameplayAbilitySpec* OwnerSpec = OwnerASC ? OwnerASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle) : nullptr;
	const FGameplayAbilitySpec* AuthoritySpec = AuthorityASC ? AuthorityASC->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle) : nullptr;
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: %s player=%d mode=%s phase=%s guard=%d ownerWorld=%s ownerPawn=%s ownerController=%s authorityWorld=%s authorityPawn=%s authorityController=%s ownerHandle=%s authorityHandle=%s ownerActive=%d authorityActive=%d ownerMovementStopped=%d authorityMovementStopped=%d ownerHealth=%.6f authorityHealth=%.6f originalHealth=%.6f activationHealth=%.6f afterHeal=%.6f afterDamage=%.6f requested=%.6f before=%.6f after=%.6f delta=%.6f elapsed=%.6f castElapsed=%.6f ownerCooldownReady=%d authorityCooldownReady=%d"),
		Marker, Pending->PlayerId, HealthModeName(), Phase, GAllowActorScriptExecutionInEditor,
		*GetPathNameSafe(Owner ? Owner->GetWorld() : nullptr), *GetPathNameSafe(Owner), *GetPathNameSafe(Pending->MoveController.Get()),
		*GetPathNameSafe(Authority ? Authority->GetWorld() : nullptr), *GetPathNameSafe(Authority), *GetPathNameSafe(Authority ? Authority->GetController() : nullptr),
		*Pending->MoveAbilityHandle.ToString(), *Pending->CancelAuthorityHandle.ToString(),
		OwnerSpec ? int32(OwnerSpec->IsActive()) : -1, AuthoritySpec ? int32(AuthoritySpec->IsActive()) : -1,
		OwnerASC ? int32(OwnerASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)) : -1,
		AuthorityASC ? int32(AuthorityASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)) : -1,
		OwnerHealth ? OwnerHealth->GetHealth() : -1.0f, AuthorityHealth ? AuthorityHealth->GetHealth() : -1.0f,
		Pending->OriginalHealth, Pending->ActivationHealth, Pending->AfterHeal, Pending->AfterDamage, Requested, Before, After, After - Before,
		FPlatformTime::Seconds() - Pending->HealthStart, Pending->bHealthCastAttempted ? FPlatformTime::Seconds() - Pending->HealthCastStart : -1.0,
		OwnerSpec && OwnerSpec->Ability && OwnerASC->AbilityActorInfo.IsValid() ? int32(OwnerSpec->Ability->CheckCooldown(OwnerSpec->Handle, OwnerASC->AbilityActorInfo.Get())) : -1,
		AuthoritySpec && AuthoritySpec->Ability && AuthorityASC->AbilityActorInfo.IsValid() ? int32(AuthoritySpec->Ability->CheckCooldown(AuthoritySpec->Handle, AuthorityASC->AbilityActorInfo.Get())) : -1);
}

void BeginHealthCleanup(const TCHAR* Reason)
{
	Pending->HealthStopReason = Reason;
	Pending->Phase = EPhase::HealthCleanup;
	Pending->WaitReason = Reason;
}

float HealthDamageBudget(float Requested)
{
	const ULyraAbilitySystemComponent* ASC = Pending->AuthorityState.IsValid() ? Pending->AuthorityState->GetLyraAbilitySystemComponent() : nullptr;
	const ULyraCombatSet* Combat = ASC ? ASC->GetSet<ULyraCombatSet>() : nullptr;
	if (!Combat || !FMath::IsFinite(Combat->GetBaseDamage())) return -1.0f;
	// GameData's DamageSelf GE adds the source BaseDamage to its set-by-caller
	// modifier. Keep the larger observed preparation addend as a conservative guard.
	return Requested + FMath::Max3(0.0f, Combat->GetBaseDamage(), Pending->PreparationDamageAddend);
}

bool HasHealthPreparationHeadroom(float Current)
{
	const float Loss = Pending->OriginalHealth - Pending->PreparedHealth;
	const float DamageBudget = HealthDamageBudget(Pending->OriginalHealth * 0.01f);
	return FMath::IsFinite(Current) && Loss > 0.0f && DamageBudget > 0.0f
		&& Current >= Pending->PreparedHealth - 0.01f && Current <= Pending->PreparedHealth + Loss * 0.25f + 0.01f
		&& Pending->OriginalHealth - Current > DamageBudget + Pending->OriginalHealth * 0.05f;
}

bool ApplyHealthStimulus(const TCHAR* Phase, bool bHeal, float Amount)
{
	ABwayCharacterWithAbilities* Pawn = Pending->AuthorityPawn.Get();
	if (GAllowActorScriptExecutionInEditor || !IsLiveAuthorityTarget(Pawn, Pending->AuthorityState.Get())
		|| !FMath::IsFinite(Amount) || Amount <= 0.0f) return false;
	const ABwayPlayerController* Controller = Cast<ABwayPlayerController>(Pawn->GetController());
	ULyraCheatManager* Cheat = Controller ? Cast<ULyraCheatManager>(Controller->CheatManager) : nullptr;
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	const float Before = Health->GetHealth();
	const float DamageBudget = bHeal ? 0.0f : HealthDamageBudget(Amount);
	if (!Cheat || (!bHeal && (DamageBudget <= 0.0f || DamageBudget >= Before * 0.5f))) return false;
	const FString Effect = bHeal ? ULyraGameData::Get().HealGameplayEffect_SetByCaller.ToString()
		: ULyraGameData::Get().DamageGameplayEffect_SetByCaller.ToString();
	// These native production methods apply the configured set-by-caller GameplayEffect.
	// No interpreter RPC, numeric attribute override, or cooldown adjustment is involved.
	if (bHeal) Cheat->HealSelf(Amount); else Cheat->DamageSelf(Amount);
	Health = ULyraHealthComponent::FindHealthComponent(Pending->AuthorityPawn.Get());
	const float After = Health ? Health->GetHealth() : -1.0f;
	if (FCString::Strcmp(Phase, TEXT("heal")) == 0) Pending->AfterHeal = After;
	if (FCString::Strcmp(Phase, TEXT("damage")) == 0) Pending->AfterDamage = After;
	LogHealthState(TEXT("HealthEffect"), Phase, Amount, Before, After);
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: HealthEffectRoute player=%d mode=%s phase=%s method=%s effect=%s guard=%d"),
		Pending->PlayerId, HealthModeName(), Phase, bHeal ? TEXT("HealSelf") : TEXT("DamageSelf"), *Effect, GAllowActorScriptExecutionInEditor);
	return FMath::IsFinite(After) && After > 0.0f && (bHeal ? After > Before : After < Before);
}

bool TickHealthCleanup(bool bHardDeadline = false)
{
	ABwayCharacterWithAbilities* Authority = Pending->AuthorityPawn.Get();
	bool AuthorityValid = IsLiveAuthorityTarget(Authority, Pending->AuthorityState.Get());
	bool OwnerValid = IsLiveMovementTarget();
	if (!GAllowActorScriptExecutionInEditor && AuthorityValid && Pending->bHealthPrepared)
	{
		ULyraAbilitySystemComponent* AuthorityASC = Pending->AuthorityState->GetLyraAbilitySystemComponent();
		ULyraAbilitySystemComponent* OwnerASC = OwnerValid ? Pending->MoveState->GetLyraAbilitySystemComponent() : nullptr;
		const FGameplayAbilitySpec* AuthoritySpec = AuthorityASC->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle);
		const FGameplayAbilitySpec* OwnerSpec = OwnerASC ? OwnerASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle) : nullptr;
		if (Pending->bHealthCastAttempted && !Pending->bHealthCleanupCancelled
			&& ((AuthoritySpec && AuthoritySpec->IsActive()) || (OwnerSpec && OwnerSpec->IsActive())))
		{
			LogHealthState(TEXT("HealthCleanupCancel"), Pending->HealthStopReason);
			// Cancel only this fixture's captured Barrage handles, never other abilities.
			if (AuthoritySpec && AuthoritySpec->IsActive()) AuthorityASC->CancelAbilityHandle(Pending->CancelAuthorityHandle);
			OwnerValid = IsLiveMovementTarget();
			OwnerASC = OwnerValid ? Pending->MoveState->GetLyraAbilitySystemComponent() : nullptr;
			OwnerSpec = OwnerASC ? OwnerASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle) : nullptr;
			if (OwnerSpec && OwnerSpec->IsActive()) OwnerASC->CancelAbilityHandle(Pending->MoveAbilityHandle);
			Pending->bHealthCleanupCancelled = true;
		}
		// Cancellation can synchronously end abilities and replace actor-info bindings.
		Authority = Pending->AuthorityPawn.Get();
		AuthorityValid = IsLiveAuthorityTarget(Authority, Pending->AuthorityState.Get());
		OwnerValid = IsLiveMovementTarget();
		AuthorityASC = AuthorityValid ? Pending->AuthorityState->GetLyraAbilitySystemComponent() : nullptr;
		OwnerASC = OwnerValid ? Pending->MoveState->GetLyraAbilitySystemComponent() : nullptr;
		AuthoritySpec = AuthorityASC ? AuthorityASC->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle) : nullptr;
		OwnerSpec = OwnerASC ? OwnerASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle) : nullptr;
		// Restore only the unchanged original authority pawn after known endpoints are inactive.
		if ((!AuthoritySpec || !AuthoritySpec->IsActive()) && (!OwnerSpec || !OwnerSpec->IsActive())
			&& IsLiveAuthorityTarget(Authority, Pending->AuthorityState.Get()) && !Pending->bHealthRestoreAttempted)
		{
			Pending->bHealthRestoreAttempted = true;
			const float Current = ULyraHealthComponent::FindHealthComponent(Authority)->GetHealth();
			const float Missing = Pending->OriginalHealth - Current;
			if (Missing > KINDA_SMALL_NUMBER) ApplyHealthStimulus(TEXT("restore"), true, Missing);
			const ULyraHealthComponent* RestoredHealth = ULyraHealthComponent::FindHealthComponent(Pending->AuthorityPawn.Get());
			Pending->bHealthRestored = RestoredHealth && FMath::IsNearlyEqual(RestoredHealth->GetHealth(), Pending->OriginalHealth, 0.01f);
		}
	}
	AuthorityValid = IsLiveAuthorityTarget(Pending->AuthorityPawn.Get(), Pending->AuthorityState.Get());
	OwnerValid = IsLiveMovementTarget();
	const ULyraHealthComponent* OwnerHealth = OwnerValid ? ULyraHealthComponent::FindHealthComponent(Pending->MovePawn.Get()) : nullptr;
	const bool OwnerConverged = OwnerHealth && FMath::IsNearlyEqual(OwnerHealth->GetHealth(), Pending->OriginalHealth, 0.01f);
	const bool Restored = !Pending->bHealthPrepared || Pending->bHealthRestored;
	if (!bHardDeadline && AuthorityValid && ((!Restored && !Pending->bHealthRestoreAttempted) || (Restored && OwnerValid && !OwnerConverged))) return true;
	LogHealthState(TEXT("HealthCleanupState"), Pending->HealthStopReason);
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: HealthCleanup player=%d mode=%s reason=%s mutated=%d restoreAttempted=%d restored=%d ownerConverged=%d authorityValid=%d ownerValid=%d cleanupCancelled=%d hardDeadline=%d guard=%d scope=fixture-restoration-only"),
		Pending->PlayerId, HealthModeName(), Pending->HealthStopReason, Pending->bHealthPrepared, Pending->bHealthRestoreAttempted, Restored, OwnerConverged,
		AuthorityValid, OwnerValid, Pending->bHealthCleanupCancelled, bHardDeadline, GAllowActorScriptExecutionInEditor);
	return Finish(Pending->HealthStopReason);
}

bool TickHealth()
{
	if (Pending->Phase == EPhase::HealthCleanup) return TickHealthCleanup();
	ABwayCharacterWithAbilities* Authority = Pending->AuthorityPawn.Get();
	if (!IsLiveMovementTarget() || !IsLiveAuthorityTarget(Authority, Pending->AuthorityState.Get()))
	{
		BeginHealthCleanup(TEXT("health-original-target-invalidated"));
		return TickHealthCleanup();
	}
	const ULyraHealthComponent* AuthorityHealth = ULyraHealthComponent::FindHealthComponent(Authority);
	const ULyraHealthComponent* OwnerHealth = ULyraHealthComponent::FindHealthComponent(Pending->MovePawn.Get());
	const FGameplayAbilitySpec* AuthoritySpec = Pending->AuthorityState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle);
	const FGameplayAbilitySpec* OwnerSpec = Pending->MoveState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
	if (!AuthoritySpec || !OwnerSpec)
	{
		BeginHealthCleanup(TEXT("health-captured-spec-lost"));
		return TickHealthCleanup();
	}
	if (Pending->Phase == EPhase::HealthConverge)
	{
		if (!FMath::IsNearlyEqual(OwnerHealth->GetHealth(), AuthorityHealth->GetHealth(), 0.01f)) return true;
		if (!HasHealthPreparationHeadroom(AuthorityHealth->GetHealth())
			|| AuthoritySpec->IsActive() || OwnerSpec->IsActive())
		{
			BeginHealthCleanup(TEXT("health-precondition-changed"));
			return TickHealthCleanup();
		}
		// Recheck convergence and capture the final baseline in the actual activation tick.
		Pending->Phase = EPhase::Activate;
		return true;
	}
	if (Pending->Phase == EPhase::HealthFirstShot)
	{
		AActor* FirstProjectile = nullptr;
		const int32 Count = CountRawlinsBullets(Authority, FirstProjectile);
		if (Count == 0 && OwnerSpec->IsActive()) return true;
		if (Count != 1 || !AuthoritySpec->IsActive() || !OwnerSpec->IsActive()
			|| FirstProjectile->GetGameTimeSinceCreation() >= Pending->HealthShotInterval)
		{
			BeginHealthCleanup(TEXT("health-first-shot-window-missed"));
			return TickHealthCleanup();
		}
		LogHealthState(TEXT("HealthFirstShotState"), TEXT("before-stimulus"));
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: HealthFirstShot player=%d mode=%s firstProjectile=%s projectileCount=%d projectileAge=%.6f shotInterval=%.6f shotCount=%d cooldown=%.6f guard=%d"),
			Pending->PlayerId, HealthModeName(), *FirstProjectile->GetPathName(), Count, FirstProjectile->GetGameTimeSinceCreation(),
			Pending->HealthShotInterval, Pending->HealthShotCount, Pending->HealthCooldown, GAllowActorScriptExecutionInEditor);
		const bool bHeal = Pending->HealthMode != EHealthMode::DamageOnly;
		Pending->FirstShotHealth = AuthorityHealth->GetHealth();
		const float PlannedDamageRequest = Pending->OriginalHealth * 0.01f;
		const float PlannedDamage = HealthDamageBudget(PlannedDamageRequest);
		if (PlannedDamage <= 0.0f || (bHeal && Pending->OriginalHealth - Pending->FirstShotHealth <= PlannedDamage + Pending->OriginalHealth * 0.05f))
		{
			BeginHealthCleanup(TEXT("health-first-shot-heal-headroom-insufficient"));
			return TickHealthCleanup();
		}
		const float Amount = bHeal ? Pending->OriginalHealth - Pending->FirstShotHealth : PlannedDamageRequest;
		if (!ApplyHealthStimulus(bHeal ? TEXT("heal") : TEXT("damage"), bHeal, Amount)) BeginHealthCleanup(TEXT("health-stimulus-no-expected-delta"));
		else if (!bHeal && Pending->AfterDamage >= Pending->ActivationHealth) BeginHealthCleanup(TEXT("health-damage-control-not-below-baseline"));
		else Pending->Phase = Pending->HealthMode == EHealthMode::HealDamage ? EPhase::HealthDamage : EPhase::HealthObserve;
		return true;
	}
	if (Pending->Phase == EPhase::HealthDamage)
	{
		// A later native ticker invocation separates the real heal and damage effects.
		const float DamageBudget = HealthDamageBudget(Pending->OriginalHealth * 0.01f);
		const float BaselineCeiling = FMath::Max(Pending->ActivationHealth, Pending->FirstShotHealth);
		if (!AuthoritySpec->IsActive() || !OwnerSpec->IsActive()) BeginHealthCleanup(TEXT("health-channel-ended-before-damage"));
		else if (DamageBudget <= 0.0f || Pending->AfterHeal - BaselineCeiling <= DamageBudget + Pending->OriginalHealth * 0.05f)
			BeginHealthCleanup(TEXT("health-observed-heal-headroom-insufficient"));
		else if (!ApplyHealthStimulus(TEXT("damage"), false, Pending->OriginalHealth * 0.01f))
			BeginHealthCleanup(TEXT("health-damage-no-negative-delta"));
		else if (!(Pending->AfterHeal > Pending->AfterDamage && Pending->AfterDamage > Pending->ActivationHealth && Pending->AfterDamage > Pending->FirstShotHealth))
			BeginHealthCleanup(TEXT("health-above-baseline-precondition-failed"));
		else Pending->Phase = EPhase::HealthObserve;
		return true;
	}
	if (Pending->Phase == EPhase::HealthObserve && !AuthoritySpec->IsActive() && !OwnerSpec->IsActive())
	{
		// Shot logs and the independent observer decide interrupted versus completed.
		LogHealthState(TEXT("HealthObservedEnd"), TEXT("both-inactive"));
		BeginHealthCleanup(TEXT("observed-end"));
		return TickHealthCleanup();
	}
	return true;
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

bool Tick(float DeltaSeconds)
{
	if (!Pending.IsSet()) return false;
	if (FPlatformTime::Seconds() >= Pending->Deadline)
	{
		if (IsBarrageDeath() && Pending->bDeathCastAttempted)
		{
			Pending->DeathStopReason = TEXT("barrage-death-hard-timeout");
			return TickBarrageDeathCleanup(true);
		}
		if (IsBarrageMovement() && Pending->bMovementStarted)
		{
			// Keep the slide gate's causal failure when safe cleanup reaches its
			// deadline. The cleanup marker separately records hardDeadline=1.
			if (!Pending->bBarrageSlide || Pending->Phase != EPhase::BarrageMoveCleanup)
				BeginBarrageMoveCleanup(TEXT("barrage-movement-hard-timeout"));
			return TickBarrageMoveCleanup(true);
		}
		if (Pending->HealthMode != EHealthMode::None)
		{
			BeginHealthCleanup(TEXT("health-hard-timeout"));
			return TickHealthCleanup(true);
		}
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: TIMEOUT player=%d reason=%s"), Pending->PlayerId, Pending->WaitReason);
		return Finish(TEXT("timeout"));
	}
	if (GAllowActorScriptExecutionInEditor)
	{
		Pending->WaitReason = TEXT("editor-script-guard");
		return true;
	}
	if (Pending->HealthMode != EHealthMode::None)
	{
		// Reserve the last two seconds for exact-spec cancellation and replicated restoration.
		if (FPlatformTime::Seconds() >= Pending->Deadline - 2.0 && Pending->Phase != EPhase::HealthCleanup)
			BeginHealthCleanup(TEXT("health-observation-timeout"));
		if (Pending->Phase != EPhase::Activate) return TickHealth();
		if (Pending->bHealthPrepared && (!IsLiveMovementTarget()
			|| !IsLiveAuthorityTarget(Pending->AuthorityPawn.Get(), Pending->AuthorityState.Get())))
		{
			BeginHealthCleanup(TEXT("health-original-target-invalidated"));
			return TickHealthCleanup();
		}
	}
	if (Pending->Phase == EPhase::Death) return TickDeath();
	if (Pending->Phase == EPhase::DeathCleanup) return TickBarrageDeathCleanup();
	if (Pending->Phase == EPhase::CancelBurst) return TickCancelBurst();
	if (IsBarrageMovement() && Pending->bMovementStarted)
	{
		if (FPlatformTime::Seconds() >= Pending->Deadline - 2.0 && Pending->Phase != EPhase::BarrageMoveCleanup)
			BeginBarrageMoveCleanup(TEXT("barrage-movement-observation-timeout"));
		if (Pending->Phase == EPhase::BarrageMoveChannel || Pending->Phase == EPhase::BarrageSlideRelease || Pending->Phase == EPhase::BarrageMoveRecovery || Pending->Phase == EPhase::BarrageMoveCleanup)
			return TickBarrageMovement(DeltaSeconds);
		if (!IsLiveBarrageMoveAuthority()) return Finish(TEXT("barrage-movement-authority-invalidated"));
	}
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
		if (IsBarrageMovement() && FPlatformTime::Seconds() - Pending->MoveLastSample >= 0.1)
		{
			LogBarrageMoveState(TEXT("warmup"));
			Pending->MoveLastSample = FPlatformTime::Seconds();
		}
		if (Pending->Phase == EPhase::MoveRecovery || Now < Pending->MoveStartWorldTime + 0.3f) return true;
		if (Pending->bBarrageSlide)
		{
			const int32 Ready = TickBarrageSlideReadiness();
			if (Ready < 0) return Finish(TEXT("barrage-slide-moving-readiness-failed"));
			if (Ready == 0) { Pending->WaitReason = TEXT("barrage-slide-moving-readiness"); return true; }
		}
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
	const TCHAR* FamilyName = Pending->bBarrage
		? TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage") : TEXT("Primary");
	const UClass* BarrageFamily = Pending->bBarrage ? FindObject<UClass>(nullptr, FamilyName) : nullptr;
	int32 PrimaryCount = 0;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const bool bMatches = Pending->bBarrage
			? (BarrageFamily && Spec.Ability && Spec.Ability->IsA(BarrageFamily))
			: BwayCombatFixture::IsPrimaryAbility(Spec.Ability);
		if (!bMatches) continue;
		Handle = Spec.Handle;
		AbilityName = GetNameSafe(Spec.Ability);
		++PrimaryCount;
	}
	if (PrimaryCount > 1)
	{
		if (Pending->bBarrage)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Ambiguous barrage player=%d count=%d family=%s"), Pending->PlayerId, PrimaryCount, FamilyName);
			return Finish(TEXT("ambiguous-barrage"));
		}
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Ambiguous primary player=%d count=%d"), Pending->PlayerId, PrimaryCount);
		return Finish(TEXT("ambiguous-primary"));
	}
	if (!Handle.IsValid())
	{
		if (Pending->bMovementStarted) return Finish(TEXT("movement-primary-unavailable"));
		Pending->WaitReason = Pending->bBarrage ? TEXT("barrage-spec-unavailable") : TEXT("primary-spec-unavailable");
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
			if (IsBarrageMovement())
			{
				const int32 Ready = PrepareBarrageMovement();
				if (Ready < 0) return Finish(TEXT("barrage-movement-walking-idle-ready-precondition-failed"));
				if (Ready == 0) { Pending->WaitReason = TEXT("barrage-movement-authority-unavailable"); return true; }
			}
			Pending->bMovementStarted = true;
			Pending->MoveStartWorldTime = World->GetTimeSeconds();
			Pending->Phase = EPhase::MoveWindup;
			Pending->WaitReason = TEXT("movement-windup");
			UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: MoveStart player=%d world=%s avatar=%s round=%d direction=%s speed=%.3f warmup=0.3 recovery=1.0 guard=%d"),
				Pending->PlayerId, *World->GetPathName(), *Pawn->GetPathName(), Pending->MoveRoundNumber,
				*Pending->MoveDirection.ToCompactString(), Pawn->GetVelocity().Size2D(), GAllowActorScriptExecutionInEditor);
			Pawn->AddMovementInput(Pending->MoveDirection);
			++Pending->MoveInputTicks;
			if (IsBarrageMovement()) LogBarrageMoveState(TEXT("warmup-start"));
			return true;
		}
		if (!IsLiveMovementTarget() || Pawn != Pending->MovePawn.Get() || Controller != Pending->MoveController.Get()
			|| Handle != Pending->MoveAbilityHandle) return Finish(TEXT("movement-target-or-primary-changed"));
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: MovingFire player=%d elapsed=%.3f displacement=%.3f speed=%.3f inputTicks=%d guard=%d"),
			Pending->PlayerId, World->GetTimeSeconds() - Pending->MoveStartWorldTime,
			FVector::Dist(Pending->MoveStartLocation, Pawn->GetActorLocation()), Pawn->GetVelocity().Size2D(),
			Pending->MoveInputTicks, GAllowActorScriptExecutionInEditor);
	}
	if (Pending->bDeathAfterActivation || Pending->bCancelBurst || (Pending->HealthMode != EHealthMode::None && !Pending->bHealthPrepared))
	{
		const int32 AuthorityMatches = CaptureAuthorityTarget();
		if (AuthorityMatches < 0) return Finish(TEXT("ambiguous-authority-death-target"));
		if (AuthorityMatches == 0)
		{
			Pending->WaitReason = TEXT("playing-live-authority-death-target-unavailable");
			return true;
		}
	}
	if (Pending->HealthMode != EHealthMode::None)
	{
		ABwayCharacterWithAbilities* AuthorityPawn = Pending->AuthorityPawn.Get();
		ULyraAbilitySystemComponent* AuthorityASC = Pending->AuthorityState->GetLyraAbilitySystemComponent();
		const FGameplayAbilitySpec* OwnerSpec = ASC->FindAbilitySpecFromHandle(Handle);
		const FGameplayAbilitySpec* AuthoritySpec = nullptr;
		int32 AuthoritySpecCount = 0;
		for (const FGameplayAbilitySpec& Spec : AuthorityASC->GetActivatableAbilities())
		{
			if (Spec.Ability && BarrageFamily && Spec.Ability->IsA(BarrageFamily)) { AuthoritySpec = &Spec; ++AuthoritySpecCount; }
		}
		if (!OwnerSpec || !OwnerSpec->Ability || OwnerSpec->IsActive() || AuthoritySpecCount != 1 || AuthoritySpec->IsActive()
			|| !OwnerSpec->Ability->CheckCooldown(Handle, ASC->AbilityActorInfo.Get())
			|| !AuthoritySpec->Ability->CheckCooldown(AuthoritySpec->Handle, AuthorityASC->AbilityActorInfo.Get()))
		{
			BeginHealthCleanup(TEXT("health-barrage-not-unique-idle-and-ready"));
			return TickHealthCleanup();
		}
		const ULyraHealthComponent* AuthorityHealth = ULyraHealthComponent::FindHealthComponent(AuthorityPawn);
		if (!Pending->bHealthPrepared)
		{
			const ABwayPlayerController* AuthorityController = Cast<ABwayPlayerController>(AuthorityPawn->GetController());
			Pending->MovePawn = Pawn;
			Pending->MoveController = Controller;
			Pending->MoveState = State;
			Pending->MoveRound = Round;
			Pending->MoveRoundNumber = Round->GetCurrentRoundNumber();
			Pending->MoveAbilityHandle = Handle;
			Pending->CancelAuthorityHandle = AuthoritySpec->Handle;
			Pending->OriginalHealth = AuthorityHealth->GetHealth();
			const FSoftObjectProperty* ConfigProperty = FindFProperty<FSoftObjectProperty>(AuthoritySpec->Ability->GetClass(), TEXT("KitConfig"));
			const UObject* Config = ConfigProperty ? ConfigProperty->GetPropertyValue_InContainer(AuthoritySpec->Ability).Get() : nullptr;
			const FFloatProperty* IntervalProperty = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("BarrageShotInterval")) : nullptr;
			const FIntProperty* CountProperty = Config ? FindFProperty<FIntProperty>(Config->GetClass(), TEXT("BarrageShotCount")) : nullptr;
			const FFloatProperty* CooldownProperty = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("BarrageCooldown")) : nullptr;
			Pending->HealthShotInterval = IntervalProperty ? IntervalProperty->GetPropertyValue_InContainer(Config) : 0.0f;
			Pending->HealthShotCount = CountProperty ? CountProperty->GetPropertyValue_InContainer(Config) : 0;
			Pending->HealthCooldown = CooldownProperty ? CooldownProperty->GetPropertyValue_InContainer(Config) : 0.0f;
			AActor* ExistingProjectile = nullptr;
			const int32 ExistingProjectiles = CountRawlinsBullets(AuthorityPawn, ExistingProjectile);
			if (!IsLiveMovementTarget() || !AuthorityController || !Cast<ULyraCheatManager>(AuthorityController->CheatManager)
				|| !FMath::IsFinite(Pending->OriginalHealth) || Pending->OriginalHealth <= 1.0f
				|| !FMath::IsNearlyEqual(Pending->OriginalHealth, AuthorityHealth->GetMaxHealth(), 0.01f)
				|| !FMath::IsNearlyEqual(Health->GetHealth(), Pending->OriginalHealth, 0.01f)
				|| !FMath::IsNearlyEqual(Health->GetMaxHealth(), Pending->OriginalHealth, 0.01f)
				|| ExistingProjectiles != 0 || Pending->HealthShotCount < 2
				|| !FMath::IsFinite(Pending->HealthShotInterval) || Pending->HealthShotInterval <= 0.0f
				|| !FMath::IsFinite(Pending->HealthCooldown) || Pending->HealthCooldown <= 0.0f)
			{
				BeginHealthCleanup(TEXT("health-full-health-loaded-kit-precondition-failed"));
				return TickHealthCleanup();
			}
			LogHealthState(TEXT("HealthPreflight"), TEXT("full-health"));
			UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: HealthConfig player=%d mode=%s config=%s ownerMaxHealth=%.6f authorityMaxHealth=%.6f existingProjectiles=%d shotCount=%d shotInterval=%.6f cooldown=%.6f ownerRound=%d authorityRound=%d guard=%d"),
				Pending->PlayerId, HealthModeName(), *Config->GetPathName(), Health->GetMaxHealth(), AuthorityHealth->GetMaxHealth(), ExistingProjectiles,
				Pending->HealthShotCount, Pending->HealthShotInterval, Pending->HealthCooldown, Pending->MoveRoundNumber, Pending->AuthorityRoundNumber, GAllowActorScriptExecutionInEditor);
			// Mark possible mutation before calling production damage so every failure owns restoration.
			Pending->bHealthPrepared = true;
			Pending->PreparationDamageRequest = Pending->OriginalHealth * 0.25f;
			if (!ApplyHealthStimulus(TEXT("prepare"), false, Pending->PreparationDamageRequest))
			{
				BeginHealthCleanup(TEXT("health-preparation-no-negative-delta"));
				return TickHealthCleanup();
			}
			Pending->PreparedHealth = ULyraHealthComponent::FindHealthComponent(Pending->AuthorityPawn.Get())->GetHealth();
			Pending->PreparationDamageAddend = FMath::Max(0.0f, Pending->OriginalHealth - Pending->PreparedHealth - Pending->PreparationDamageRequest);
			Pending->Phase = EPhase::HealthConverge;
			Pending->WaitReason = TEXT("health-preparation-owner-convergence");
			return true;
		}
		if (Pawn != Pending->MovePawn.Get() || Controller != Pending->MoveController.Get() || Handle != Pending->MoveAbilityHandle
			|| AuthoritySpec->Handle != Pending->CancelAuthorityHandle)
		{
			BeginHealthCleanup(TEXT("health-activation-precondition-changed"));
			return TickHealthCleanup();
		}
		if (!FMath::IsNearlyEqual(Health->GetHealth(), AuthorityHealth->GetHealth(), 0.01f))
		{
			Pending->Phase = EPhase::HealthConverge;
			Pending->WaitReason = TEXT("health-preparation-owner-convergence");
			return true;
		}
		if (!HasHealthPreparationHeadroom(AuthorityHealth->GetHealth()))
		{
			BeginHealthCleanup(TEXT("health-activation-headroom-or-regen-bound-failed"));
			return TickHealthCleanup();
		}
		Pending->ActivationHealth = AuthorityHealth->GetHealth();
		LogHealthState(TEXT("HealthConverged"), TEXT("before-activation"));
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: HealthHeadroom player=%d mode=%s preparedHealth=%.6f preparationLoss=%.6f preparationGain=%.6f maxPreparationGain=%.6f activationHealth=%.6f sourceBaseDamage=%.6f observedDamageAddend=%.6f plannedDamageRequest=%.6f damageBudget=%.6f minimumHeadroomMargin=%.6f guard=%d scope=fixture-sizing-not-gameplay-verdict"),
			Pending->PlayerId, HealthModeName(), Pending->PreparedHealth, Pending->OriginalHealth - Pending->PreparedHealth,
			Pending->ActivationHealth - Pending->PreparedHealth, (Pending->OriginalHealth - Pending->PreparedHealth) * 0.25f,
			Pending->ActivationHealth, AuthorityASC->GetNumericAttribute(ULyraCombatSet::GetBaseDamageAttribute()), Pending->PreparationDamageAddend,
			Pending->OriginalHealth * 0.01f, HealthDamageBudget(Pending->OriginalHealth * 0.01f), Pending->OriginalHealth * 0.05f, GAllowActorScriptExecutionInEditor);
		Pending->bHealthCastAttempted = true;
		Pending->HealthCastStart = FPlatformTime::Seconds();
		LogHealthState(TEXT("HealthActivation"), TEXT("before-activation"));
	}
	if (Pending->bCancelBurst)
	{
		UClass* PrimaryFamily = FindObject<UClass>(nullptr, TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsPrimary"));
		const FGameplayAbilitySpec* OwnerSpec = ASC->FindAbilitySpecFromHandle(Handle);
		ABwayCharacterWithAbilities* AuthorityPawn = Pending->AuthorityPawn.Get();
		ABwayPlayerController* AuthorityController = Cast<ABwayPlayerController>(AuthorityPawn->GetController());
		if (!PrimaryFamily || !OwnerSpec || !OwnerSpec->Ability || !OwnerSpec->Ability->IsA(PrimaryFamily) || OwnerSpec->IsActive())
			return FailCancelBurst(TEXT("cancel-requires-inactive-rawlins-primary"));
		const FGameplayAbilitySpec* AuthoritySpec = nullptr;
		int32 AuthoritySpecCount = 0;
		for (const FGameplayAbilitySpec& Spec : Pending->AuthorityState->GetLyraAbilitySystemComponent()->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA(PrimaryFamily)) { AuthoritySpec = &Spec; ++AuthoritySpecCount; }
		}
		if (AuthoritySpecCount != 1 || AuthoritySpec->IsActive()) return FailCancelBurst(TEXT("cancel-authority-primary-not-unique-or-idle"));
		const FSoftObjectProperty* ConfigProperty = FindFProperty<FSoftObjectProperty>(AuthoritySpec->Ability->GetClass(), TEXT("KitConfig"));
		const UObject* Config = ConfigProperty ? ConfigProperty->GetPropertyValue_InContainer(AuthoritySpec->Ability).Get() : nullptr;
		const FFloatProperty* IntervalProperty = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("PrimaryShotInterval")) : nullptr;
		const FIntProperty* CountProperty = Config ? FindFProperty<FIntProperty>(Config->GetClass(), TEXT("PrimaryShotCount")) : nullptr;
		const float ShotInterval = IntervalProperty ? IntervalProperty->GetPropertyValue_InContainer(Config) : 0.0f;
		if (!CountProperty || CountProperty->GetPropertyValue_InContainer(Config) != 2 || !FMath::IsFinite(ShotInterval) || ShotInterval <= 0.0f)
			return FailCancelBurst(TEXT("cancel-loaded-two-shot-config-unavailable"));
		AActor* ExistingProjectile = nullptr;
		const int32 ExistingProjectiles = CountRawlinsBullets(AuthorityPawn, ExistingProjectile);
		if (ExistingProjectiles != 0) return FailCancelBurst(TEXT("cancel-existing-projectile-or-missing-family"));
		if (!Controller->CheatManager || !AuthorityController->CheatManager) return FailCancelBurst(TEXT("cancel-cheat-manager-unavailable"));
		Pending->MovePawn = Pawn;
		Pending->MoveController = Controller;
		Pending->MoveState = State;
		Pending->MoveRound = Round;
		Pending->MoveRoundNumber = Round->GetCurrentRoundNumber();
		Pending->MoveAbilityHandle = Handle;
		Pending->CancelAuthorityHandle = AuthoritySpec->Handle;
		Pending->CancelShotInterval = ShotInterval;
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: CancelBurstPreflight player=%d ownerWorld=%s ownerPawn=%s ownerController=%s authorityWorld=%s authorityPawn=%s authorityController=%s ownerCheat=%s authorityCheat=%s existingProjectiles=%d shotInterval=%.6f guard=%d"),
			Pending->PlayerId, *World->GetPathName(), *Pawn->GetPathName(), *Controller->GetPathName(),
			*AuthorityPawn->GetWorld()->GetPathName(), *AuthorityPawn->GetPathName(), *AuthorityController->GetPathName(),
			*GetNameSafe(Controller->CheatManager), *GetNameSafe(AuthorityController->CheatManager), ExistingProjectiles, ShotInterval, GAllowActorScriptExecutionInEditor);
	}

	if (IsBarrageDeath())
	{
		if (!PrepareBarrageDeath(Pawn, Controller, State, Round, ASC, Handle, BarrageFamily))
			return Finish(TEXT("barrage-death-idle-ready-first-shot-precondition-failed"));
		Pending->bDeathCastAttempted = true;
	}
	LogNetworkSettings(World);
	if (IsBarrageMovement())
	{
		const FGameplayAbilitySpec* OwnerSpec = ASC->FindAbilitySpecFromHandle(Pending->MoveAbilityHandle);
		const FGameplayAbilitySpec* AuthoritySpec = IsLiveBarrageMoveAuthority()
			? Pending->AuthorityState->GetLyraAbilitySystemComponent()->FindAbilitySpecFromHandle(Pending->CancelAuthorityHandle) : nullptr;
		const bool CastMovementReady = Pending->bBarrageSlide
			? IsLiveBarrageSlideEndpoint(0) && IsLiveBarrageSlideEndpoint(1) && Pending->bSlideInputHeld
				&& Pawn->GetBwayCharacterMovement()->IsSliding() && Pending->AuthorityPawn->GetBwayCharacterMovement()->IsSliding()
				&& HasBarrageSlideSupport(Pawn->GetBwayCharacterMovement()) && HasBarrageSlideSupport(Pending->AuthorityPawn->GetBwayCharacterMovement())
			: Pawn->GetCharacterMovement()->MovementMode == MOVE_Walking && Pending->AuthorityPawn->GetCharacterMovement()->MovementMode == MOVE_Walking;
		if (!OwnerSpec || !AuthoritySpec || OwnerSpec->IsActive() || AuthoritySpec->IsActive() || !CastMovementReady
			|| !OwnerSpec->Ability->CheckCooldown(OwnerSpec->Handle, ASC->AbilityActorInfo.Get())
			|| !AuthoritySpec->Ability->CheckCooldown(AuthoritySpec->Handle, Pending->AuthorityState->GetLyraAbilitySystemComponent()->AbilityActorInfo.Get()))
			return Finish(TEXT("barrage-movement-cast-precondition-changed"));
		Pending->bMoveCastAttempted = true;
		Pending->MoveCastWorldTime = World->GetTimeSeconds();
		LogBarrageMoveState(TEXT("cast"));
	}
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Dispatch player=%d world=%s net=%d local=%d role=%d remoteRole=%d guard=%d avatar=%s ability=%s handle=%s family=%s"),
		Pending->PlayerId, *World->GetPathName(), int32(World->GetNetMode()), Controller->IsLocalController(),
		int32(Pawn->GetLocalRole()), int32(Pawn->GetRemoteRole()), GAllowActorScriptExecutionInEditor,
		*Pawn->GetPathName(), *AbilityName, *Handle.ToString(), FamilyName);
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
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Readback player=%d activated=%d active=%d avatarMatches=%d anim=%s montage=%s montagePosition=%.3f firing=%d sinceFire=%.3f scope=immediate-local-readback family=%s"),
		Pending->PlayerId, Activated, AfterSpec && AfterSpec->IsActive(), Pawn && ASC && ASC->GetAvatarActor() == Pawn,
		*GetNameSafe(Anim), *GetNameSafe(Montage), Montage ? Anim->Montage_GetPosition(Montage) : -1.0f,
		Firing ? int32(Firing->GetPropertyValue_InContainer(Anim)) : -1,
		SinceFire ? SinceFire->GetPropertyValue_InContainer(Anim) : -1.0f, FamilyName);
	if (Pending->HealthMode != EHealthMode::None)
	{
		if (!Activated)
		{
			BeginHealthCleanup(TEXT("health-activation-rejected"));
			return TickHealthCleanup();
		}
		Pending->Phase = EPhase::HealthFirstShot;
		Pending->WaitReason = TEXT("health-authority-first-projectile");
		return true;
	}
	if (Activated && Pending->bCancelBurst)
	{
		Pending->Phase = EPhase::CancelBurst;
		Pending->WaitReason = TEXT("cancel-authority-first-projectile-unavailable");
		return true;
	}
	if (Activated && Pending->bMoveDuringActivation)
	{
		if (!IsLiveMovementTarget()) return Finish(TEXT("movement-target-invalidated-after-activation"));
		if (IsBarrageMovement())
		{
			Pending->Phase = EPhase::BarrageMoveChannel;
			Pending->WaitReason = TEXT("barrage-movement-channel");
			LogBarrageMoveState(TEXT("channel-start"));
			return true;
		}
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
		if (!IsBarrageDeath()) Pending->DeathWorldTime = AuthorityPawn->GetWorld()->GetTimeSeconds() + 0.2f;
		Pending->WaitReason = IsBarrageDeath() ? TEXT("barrage-death-authority-first-projectile") : TEXT("queued-production-death");
		return true;
	}
	return Finish(Activated ? TEXT("activation-accepted") : TEXT("activation-rejected"));
}

void StartRequest(const TArray<FString>& Args, bool bDeathAfterActivation, bool bMoveDuringActivation = false, bool bBarrage = false, bool bCancelBurst = false, EHealthMode HealthMode = EHealthMode::None, bool bBarrageSlide = false)
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
			bBarrageSlide ? TEXT("BarrageAnimationSlide") : (bCancelBurst ? TEXT("PrimaryAnimationCancelBurst") : (bBarrage ? (bDeathAfterActivation ? TEXT("BarrageAnimationDeath") : TEXT("BarrageAnimation")) : (bMoveDuringActivation ? TEXT("PrimaryAnimationMove") : (bDeathAfterActivation ? TEXT("PrimaryAnimationDeath") : TEXT("PrimaryAnimation"))))));
		return;
	}
	if (Pending.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPrimaryAnimation: Request rejected; player=%d already pending"), Pending->PlayerId);
		return;
	}
	FRequest Request;
	Request.PlayerId = PlayerId;
	Request.HealthStart = FPlatformTime::Seconds();
	Request.Deadline = Request.HealthStart + (HealthMode != EHealthMode::None || (bBarrage && bMoveDuringActivation) ? 10.0 : 5.0);
	Request.HealthMode = HealthMode;
	Request.bDeathAfterActivation = bDeathAfterActivation;
	Request.bMoveDuringActivation = bMoveDuringActivation;
	Request.bBarrage = bBarrage;
	Request.bBarrageSlide = bBarrageSlide;
	Request.bCancelBurst = bCancelBurst;
	Pending = Request;
	// Movement input is consumed per character frame, so submit it each frame only for this mode.
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), (bMoveDuringActivation || bCancelBurst || (bBarrage && bDeathAfterActivation) || HealthMode != EHealthMode::None) ? 0.0f : 0.05f);
	if (bBarrageSlide)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=10.0 barrageSlide=1 family=/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage"), PlayerId);
	}
	else if (bBarrage && bDeathAfterActivation)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=5.0 barrageDeath=1 family=/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage"), PlayerId);
	}
	else if (bBarrage && bMoveDuringActivation)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=10.0 barrageMove=1 family=/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage"), PlayerId);
	}
	else if (HealthMode != EHealthMode::None)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=10.0 healthMode=%s family=/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage"), PlayerId, HealthModeName());
	}
	else if (bCancelBurst)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=5.0 cancelBurst=1"), PlayerId);
	}
	else if (bBarrage)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=5.0 family=/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage"), PlayerId);
	}
	else if (bMoveDuringActivation)
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

void StartBarrage(const TArray<FString>& Args)
{
	StartRequest(Args, false, false, true);
}

void StartBarrageDeath(const TArray<FString>& Args)
{
	StartRequest(Args, true, false, true);
}

void StartBarrageMove(const TArray<FString>& Args)
{
	StartRequest(Args, false, true, true);
}

void StartBarrageSlide(const TArray<FString>& Args)
{
	StartRequest(Args, false, true, true, false, EHealthMode::None, true);
}

void StartCancelBurst(const TArray<FString>& Args)
{
	StartRequest(Args, false, false, false, true);
}

void StartBarrageHealth(const TArray<FString>& Args)
{
	EHealthMode Mode = EHealthMode::None;
	if (Args.Num() == 2)
	{
		if (Args[1] == TEXT("heal-damage")) Mode = EHealthMode::HealDamage;
		else if (Args[1] == TEXT("damage-only")) Mode = EHealthMode::DamageOnly;
		else if (Args[1] == TEXT("heal-only")) Mode = EHealthMode::HealOnly;
	}
	if (Mode == EHealthMode::None)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Usage bway.Test.BarrageAnimationHealth <nonnegative-int32-player-id> <heal-damage|damage-only|heal-only>"));
		return;
	}
	StartRequest({ Args[0] }, false, false, true, false, Mode);
}

void Cancel()
{
	if (!Pending.IsSet()) return;
	if (IsBarrageDeath() && Pending->bDeathCastAttempted && !Pending->bDeathDispatched)
	{
		Pending->DeathStopReason = TEXT("barrage-death-fixture-cancelled");
		Pending->Phase = EPhase::DeathCleanup;
		return;
	}
	if (IsBarrageMovement() && Pending->bMovementStarted)
	{
		BeginBarrageMoveCleanup(TEXT("barrage-movement-fixture-cancelled"));
		return;
	}
	if (Pending->HealthMode != EHealthMode::None)
	{
		// Keep the native ticker alive to restore after exact-spec cancellation outside Python.
		BeginHealthCleanup(TEXT("health-fixture-cancelled"));
		return;
	}
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Finish(TEXT("cancelled"));
}

FAutoConsoleCommand Command(TEXT("bway.Test.PrimaryAnimation"),
	TEXT("Development only: queue one primary activation for an exact local NM_Client player ID; no aim or fixture changes."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&Start));
FAutoConsoleCommand BarrageCommand(TEXT("bway.Test.BarrageAnimation"),
	TEXT("Development only: queue one already granted Rawlins Barrage activation for an exact local NM_Client player ID; no aim or fixture changes."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartBarrage));
FAutoConsoleCommand BarrageMoveCommand(TEXT("bway.Test.BarrageAnimationMove"),
	TEXT("Development only: exercise one exact remote-owner walking Barrage with native movement and yaw input, observe recovery, then restore original authority transform and views."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartBarrageMove));
FAutoConsoleCommand BarrageDeathCommand(TEXT("bway.Test.BarrageAnimationDeath"),
	TEXT("Development only: activate one exact remote-owner Barrage, then self-destruct its captured authority pawn after the first owned bullet and before shot two; never reset cooldowns or force respawn."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartBarrageDeath));
FAutoConsoleCommand BarrageSlideCommand(TEXT("bway.Test.BarrageAnimationSlide"),
	TEXT("Development only: move, press the granted common slide, activate Barrage during measured slide, hold through the channel, release and recover walking, then restore the fixture; no tuning or cooldown changes."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartBarrageSlide));
FAutoConsoleCommand BarrageHealthCommand(TEXT("bway.Test.BarrageAnimationHealth"),
	TEXT("Development only: prepare a healable deficit, activate one exact remote-owner Barrage, apply heal-damage, damage-only, or heal-only via production effects, observe and restore; never reset cooldowns."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartBarrageHealth));
FAutoConsoleCommand CancelBurstCommand(TEXT("bway.Test.PrimaryAnimationCancelBurst"),
	TEXT("Development only: activate an exact remote-owner Rawlins primary, then queue its native server cancellation after the first authority bullet and before shot two."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartCancelBurst));
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
