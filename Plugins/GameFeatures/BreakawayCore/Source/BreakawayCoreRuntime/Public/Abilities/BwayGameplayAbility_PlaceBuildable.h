#pragma once

#include "CoreMinimal.h"
#include "Abilities/BwayGameplayAbility.h"
#include "BwayGameplayAbility_PlaceBuildable.generated.h"

class ABwayTargetActor_ActorPlacementFace;
class ABwayWorldReticle_ActorVisualization;
class UAbilityTask_WaitTargetData;
class UMaterialInterface;
class UBwayBuildableDataAsset;

/**
 * Buildable placement base (InputTag.Ability.Buildable / key 1).
 * Create one Blueprint child per hero in that hero's ability set so display data can be authored per buildable.
 * Resolves the locked hero's BuildableDataAsset, shows a placement preview ghost via WaitTargetData,
 * and spawns via UBwayBuildablePlacementLibrary on confirm.
 *
 * Confirm/Cancel use Lyra IMC + InputConfig + WaitTargetData UserConfirmed
 * (native InputTag.ConfirmTargeting / CancelTargeting → LocalInputConfirm/Cancel,
 * and/or humanoid Confirm/Cancel GAs that forward to the same ASC calls).
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_PlaceBuildable : public UBwayGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_PlaceBuildable(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Maximum ground surface angle (degrees from vertical) allowed for placement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxPlacementAngleDegrees = 30.0f;

	/** Maximum distance from the owner the placement trace can reach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0"))
	float MaxPlacementDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0"))
	float MaxFootprintZDifference = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0"))
	float FootprintTraceDownLength = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0"))
	float FootprintTraceUpOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Visuals")
	TObjectPtr<UMaterialInterface> ValidPlacementMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Visuals")
	TObjectPtr<UMaterialInterface> InvalidPlacementMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Visuals")
	TSubclassOf<ABwayWorldReticle_ActorVisualization> ReticleClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Targeting")
	TSubclassOf<ABwayTargetActor_ActorPlacementFace> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Targeting")
	bool bDebugTargeting = false;

	/** Apply ExposeOnSpawn parameters to the deferred target actor before FinishSpawning. */
	void ConfigurePlacementTargetActor(ABwayTargetActor_ActorPlacementFace* TargetActor, UBwayBuildableDataAsset* BuildableData);

	UFUNCTION()
	void OnTargetDataValid(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

	/** Diagnostic: prove ASC LocalInputConfirm/Cancel reach WaitTargetData subscribers. */
	UFUNCTION()
	void OnDebugLocalConfirm();

	UFUNCTION()
	void OnDebugLocalCancel();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> ActiveWaitTargetDataTask;

	bool bPlacementInputDebugCallbacksBound = false;

	void BindPlacementInputDebugCallbacks(UAbilitySystemComponent* ASC);
	void UnbindPlacementInputDebugCallbacks(UAbilitySystemComponent* ASC);
};
