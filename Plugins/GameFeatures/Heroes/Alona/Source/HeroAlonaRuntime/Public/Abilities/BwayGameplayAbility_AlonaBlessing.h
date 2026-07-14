#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_AlonaBlessing.generated.h"

class ABwayAlonaBlessingZone;

HEROALONARUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Alona_Blessing);

/**
 * Alona R — Blessing of the Sun. Ground heal circle in front of Alona. CD 25s.
 */
UCLASS()
class HEROALONARUNTIME_API UBwayGameplayAbility_AlonaBlessing : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_AlonaBlessing(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	FVector ResolveGroundTargetLocation() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing")
	TSubclassOf<ABwayAlonaBlessingZone> BlessingZoneClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.0"))
	float ForwardPlacementDistance = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "50.0"))
	float ZoneRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.1"))
	float ZoneDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.0"))
	float InitialHeal = 70.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.0"))
	float HealPerSecond = 35.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.0"))
	float GroundTraceUp = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.0"))
	float GroundTraceDown = 1000.f;
};
