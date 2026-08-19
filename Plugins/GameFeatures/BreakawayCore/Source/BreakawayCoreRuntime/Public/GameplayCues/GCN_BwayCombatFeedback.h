// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "NiagaraSystem.h"
#include "GCN_BwayCombatFeedback.generated.h"

/**
 * Base GameplayCue Notify for Breakaway combat feedback.
 * Provides reusable infrastructure for hit confirmation, damage numbers,
 * slide effects, and other "game feel" polish.
 *
 * Subclass this to create specific feedback cues:
 * - GCN_BW_HitConfirmation: Flash + sound on dealing damage
 * - GCN_BW_DamageNumber: Floating damage text
 * - GCN_BW_SlideTrail: Niagara trail during slide
 * - GCN_BW_RelicPickup: Pickup confirmation VFX + SFX
 * - GCN_BW_GoalScored: Celebration VFX + screen effects
 *
 * Usage:
 * Tag these with GameplayCue.Breakaway.Combat.* tags and trigger via:
 *   ASC->ExecuteGameplayCue(Tag, EffectContext);
 * or via GameplayEffect GameplayCue containers.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UGCN_BwayCombatFeedback : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGCN_BwayCombatFeedback();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** Sound to play on hit confirmation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback|Audio")
	TObjectPtr<USoundBase> FeedbackSound;

	/** Camera shake class to apply to the instigator */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback|Camera")
	TSubclassOf<UCameraShakeBase> FeedbackCameraShake;

	/** Camera shake scale multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback|Camera")
	float CameraShakeScale = 1.0f;

	/** Optional gamepad force feedback. Honors accessibility reduced-motion when config is present. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback|Haptics")
	TObjectPtr<UForceFeedbackEffect> FeedbackForceFeedback;

	/** Niagara system to spawn at the effect location */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback|VFX")
	TObjectPtr<UNiagaraSystem> FeedbackVFX;
};
