#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RelicGameplayEffects.generated.h"

class UGameplayEffect;

/**
 * Data asset for Relic carrier effects
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API URelicCarrierEffectData : public UDataAsset
{
	GENERATED_BODY()
    
public:
	
	// Gameplay effect for speed modification
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> SpeedModifierEffect;
    
	// Gameplay tags to apply when carrying
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTagContainer CarrierTags;
    
	// Movement speed modifier value
	UPROPERTY(EditDefaultsOnly, Category = "Values", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MovementSpeedMultiplier = 0.8f;
};

/**
 * Data asset for Relic ability restrictions
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API URelicAbilityRestrictionData : public UDataAsset
{
	GENERATED_BODY()
    
public:
	
	// Gameplay effect for ability restrictions
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> RestrictionEffect;
    
	// Tags that block abilities while carrying
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTagContainer RestrictedAbilityTags;
};