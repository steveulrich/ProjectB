// Queue one synthetic primary or Barrage activation outside the editor Python RPC guard.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
	CancelBurst,
	HealthConverge,
	HealthFirstShot,
	HealthDamage,
	HealthObserve,
	HealthCleanup
};

enum class EHealthMode : uint8 { None, HealDamage, DamageOnly, HealOnly };

struct FRequest
{
	int32 PlayerId = INDEX_NONE;
	double Deadline = 0.0;
	float DeathWorldTime = 0.0f;
	int32 AuthorityRoundNumber = INDEX_NONE;
	bool bDeathAfterActivation = false;
	bool bMoveDuringActivation = false;
	bool bBarrage = false;
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
	float ActivationHealth = 0.0f;
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
	const TCHAR* WaitReason = TEXT("client-owner-unavailable");
};

TOptional<FRequest> Pending;
FTSTicker::FDelegateHandle TickerHandle;

bool Finish(const TCHAR* Reason)
{
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
		&& (!Pending->bHealthPrepared || Round == Pending->HealthAuthorityRound.Get())
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

bool ApplyHealthStimulus(const TCHAR* Phase, bool bHeal, float Amount)
{
	ABwayCharacterWithAbilities* Pawn = Pending->AuthorityPawn.Get();
	if (GAllowActorScriptExecutionInEditor || !IsLiveAuthorityTarget(Pawn, Pending->AuthorityState.Get())
		|| !FMath::IsFinite(Amount) || Amount <= 0.0f) return false;
	const ABwayPlayerController* Controller = Cast<ABwayPlayerController>(Pawn->GetController());
	ULyraCheatManager* Cheat = Controller ? Cast<ULyraCheatManager>(Controller->CheatManager) : nullptr;
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	const float Before = Health->GetHealth();
	if (!Cheat || (!bHeal && Amount >= Before * 0.5f)) return false;
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
		if (!FMath::IsNearlyEqual(AuthorityHealth->GetHealth(), Pending->PreparedHealth, 0.01f)
			|| AuthoritySpec->IsActive() || OwnerSpec->IsActive())
		{
			BeginHealthCleanup(TEXT("health-precondition-changed"));
			return TickHealthCleanup();
		}
		Pending->ActivationHealth = AuthorityHealth->GetHealth();
		LogHealthState(TEXT("HealthConverged"), TEXT("before-activation"));
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
		const float Amount = bHeal ? (Pending->OriginalHealth - Pending->ActivationHealth) * 0.5f : Pending->ActivationHealth * 0.025f;
		if (!ApplyHealthStimulus(bHeal ? TEXT("heal") : TEXT("damage"), bHeal, Amount)) BeginHealthCleanup(TEXT("health-stimulus-no-expected-delta"));
		else Pending->Phase = Pending->HealthMode == EHealthMode::HealDamage ? EPhase::HealthDamage : EPhase::HealthObserve;
		return true;
	}
	if (Pending->Phase == EPhase::HealthDamage)
	{
		// A later native ticker invocation separates the real heal and damage effects.
		if (!AuthoritySpec->IsActive() || !OwnerSpec->IsActive()) BeginHealthCleanup(TEXT("health-channel-ended-before-damage"));
		else if (!ApplyHealthStimulus(TEXT("damage"), false, (Pending->AfterHeal - Pending->ActivationHealth) * 0.25f))
			BeginHealthCleanup(TEXT("health-damage-no-negative-delta"));
		else if (!(Pending->AfterHeal > Pending->AfterDamage && Pending->AfterDamage > Pending->ActivationHealth))
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

bool Tick(float)
{
	if (!Pending.IsSet()) return false;
	if (FPlatformTime::Seconds() >= Pending->Deadline)
	{
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
	if (Pending->Phase == EPhase::CancelBurst) return TickCancelBurst();
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
	if (Pending->bDeathAfterActivation || Pending->bCancelBurst || (Pending->HealthMode != EHealthMode::None && !Pending->bHealthPrepared))
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
		if (Pending->HealthMode != EHealthMode::None) Pending->HealthAuthorityRound = AuthorityPawn->GetWorld()->GetGameState<ABwayGameState>()
			->FindComponentByClass<UBwayRoundManagementComponent>();
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
			if (!ApplyHealthStimulus(TEXT("prepare"), false, Pending->OriginalHealth * 0.25f))
			{
				BeginHealthCleanup(TEXT("health-preparation-no-negative-delta"));
				return TickHealthCleanup();
			}
			Pending->PreparedHealth = ULyraHealthComponent::FindHealthComponent(Pending->AuthorityPawn.Get())->GetHealth();
			Pending->Phase = EPhase::HealthConverge;
			Pending->WaitReason = TEXT("health-preparation-owner-convergence");
			return true;
		}
		if (Pawn != Pending->MovePawn.Get() || Controller != Pending->MoveController.Get() || Handle != Pending->MoveAbilityHandle
			|| AuthoritySpec->Handle != Pending->CancelAuthorityHandle
			|| !FMath::IsNearlyEqual(Health->GetHealth(), Pending->ActivationHealth, 0.01f)
			|| !FMath::IsNearlyEqual(AuthorityHealth->GetHealth(), Pending->ActivationHealth, 0.01f))
		{
			BeginHealthCleanup(TEXT("health-activation-precondition-changed"));
			return TickHealthCleanup();
		}
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

	LogNetworkSettings(World);
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

void StartRequest(const TArray<FString>& Args, bool bDeathAfterActivation, bool bMoveDuringActivation = false, bool bBarrage = false, bool bCancelBurst = false, EHealthMode HealthMode = EHealthMode::None)
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
			bCancelBurst ? TEXT("PrimaryAnimationCancelBurst") : (bBarrage ? TEXT("BarrageAnimation") : (bMoveDuringActivation ? TEXT("PrimaryAnimationMove") : (bDeathAfterActivation ? TEXT("PrimaryAnimationDeath") : TEXT("PrimaryAnimation")))));
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
	Request.Deadline = Request.HealthStart + (HealthMode == EHealthMode::None ? 5.0 : 10.0);
	Request.HealthMode = HealthMode;
	Request.bDeathAfterActivation = bDeathAfterActivation;
	Request.bMoveDuringActivation = bMoveDuringActivation;
	Request.bBarrage = bBarrage;
	Request.bCancelBurst = bCancelBurst;
	Pending = Request;
	// Movement input is consumed per character frame, so submit it each frame only for this mode.
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), (bMoveDuringActivation || bCancelBurst || HealthMode != EHealthMode::None) ? 0.0f : 0.05f);
	if (HealthMode != EHealthMode::None)
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
