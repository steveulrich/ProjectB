// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayCues/GCN_BwayCombatFeedback.h"

#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GCN_BwayCombatFeedback)

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarBwayDebugCueFeedback(
	TEXT("bway.Combat.DebugCueFeedback"), 0,
	TEXT("Log actual combat cue execution and local camera/haptic ownership."));
#endif

UGCN_BwayCombatFeedback::UGCN_BwayCombatFeedback()
{
}

bool UGCN_BwayCombatFeedback::ShouldPlayFeedbackSound(bool bLocalFeedback) const
{
	return bLocalFeedback || (GameplayCueTag != BwayGameplayCueTags::CombatHitConfirm
		&& GameplayCueTag != BwayGameplayCueTags::CombatHit);
}

bool UGCN_BwayCombatFeedback::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return false;
	}

	const UBwayCombatReadabilityConfig* FeedbackConfig =
		UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(MyTarget);

	FVector EffectLocation = MyTarget->GetActorLocation();
	if (!Parameters.Location.IsNearlyZero())
	{
		EffectLocation = FVector(Parameters.Location);
	}

	const bool bSkipShake = FeedbackConfig && FeedbackConfig->bReduceCameraShake;
	const bool bSkipHaptics = FeedbackConfig && FeedbackConfig->bReduceMotionHaptics;
	const APawn* TargetPawn = Cast<APawn>(MyTarget);
	APlayerController* FeedbackPC = TargetPawn ? Cast<APlayerController>(TargetPawn->GetController()) : nullptr;
	const bool bLocalFeedback = FeedbackPC && FeedbackPC->IsLocalController();
	if (FeedbackSound && ShouldPlayFeedbackSound(bLocalFeedback))
	{
		if (GameplayCueTag == BwayGameplayCueTags::CombatHitConfirm || GameplayCueTag == BwayGameplayCueTags::CombatHit)
		{
			UGameplayStatics::PlaySound2D(MyTarget, FeedbackSound);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(MyTarget, FeedbackSound, EffectLocation);
		}
	}
	bool bCameraStarted = false;
	bool bHapticsDispatched = false;

	// GAS invokes the cue on its ASC's avatar. The hit router targets the attacker,
	// and its explicit cue parameters need not contain an EffectContext. Each peer
	// already receives the cue, so only its local target may drive camera/haptics.
	if (bLocalFeedback && FeedbackCameraShake && !bSkipShake && FeedbackPC->PlayerCameraManager)
	{
		bCameraStarted = FeedbackPC->PlayerCameraManager->StartCameraShake(FeedbackCameraShake, CameraShakeScale) != nullptr;
	}

	if (bLocalFeedback && FeedbackForceFeedback && !bSkipHaptics)
	{
		FeedbackPC->ClientPlayForceFeedback(FeedbackForceFeedback);
		bHapticsDispatched = true;
	}

	if (FeedbackVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			MyTarget,
			FeedbackVFX,
			EffectLocation,
			Parameters.Normal.Rotation(),
			FVector::OneVector,
			true,
			true,
			ENCPoolMethod::AutoRelease);
	}

#if !UE_BUILD_SHIPPING
	if (CVarBwayDebugCueFeedback.GetValueOnGameThread() != 0)
	{
		const APlayerState* PS = TargetPawn ? TargetPawn->GetPlayerState() : nullptr;
		UE_LOG(LogTemp, Display, TEXT("BwayCueFeedback: Executed tag=%s player=%d net=%d local=%d context=%d shakeConfigured=%d shakeSkipped=%d cameraStarted=%d hapticsConfigured=%d hapticsSkipped=%d hapticsDispatched=%d soundConfigured=%d vfxConfigured=%d"),
			*GameplayCueTag.ToString(), PS ? PS->GetPlayerId() : -1, int32(MyTarget->GetNetMode()), bLocalFeedback,
			Parameters.EffectContext.IsValid(), FeedbackCameraShake != nullptr, bSkipShake, bCameraStarted,
			FeedbackForceFeedback != nullptr, bSkipHaptics, bHapticsDispatched, FeedbackSound != nullptr, FeedbackVFX != nullptr);
	}
#endif
	return true;
}
