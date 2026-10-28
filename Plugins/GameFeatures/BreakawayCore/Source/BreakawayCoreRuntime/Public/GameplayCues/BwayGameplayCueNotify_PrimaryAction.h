#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "BwayGameplayCueNotify_PrimaryAction.generated.h"

class UAnimMontage;

/** A predicted burst cue with a replaceable, pawn-owned cosmetic primary action. */
UCLASS(Blueprintable, meta = (DisplayName = "Bway Primary Action Burst"))
class BREAKAWAYCORERUNTIME_API UBwayGameplayCueNotify_PrimaryAction : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

public:
	/** Filter a per-execution copy; never change the shared cue asset for another local player. */
	static FGameplayCueNotify_BurstEffects ResolveBurstEffects(
		const FGameplayCueNotify_BurstEffects& AuthoredEffects, bool bLocalOwner,
		bool bReduceCameraShake, bool bReduceMotionHaptics);

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

	/** One-shot, in-place montage on an evaluated slot. It must contain no gameplay notifies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primary Action")
	TObjectPtr<UAnimMontage> PrimaryActionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primary Action", meta = (ClampMin = "0.01"))
	float PrimaryActionPlayRate = 1.0f;
};
