// RelicDataAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RelicDataAsset.generated.h"

class URelicSettings;

/**
 * Data asset that wraps RelicSettings for spawning different relic types
 * Similar to BwayHeroDataAsset pattern
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API URelicDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
    
public:
	URelicDataAsset();

	// Display name for this relic type
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic")
	FText RelicDisplayName;

	// The settings that define this relic's behavior and appearance
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic")
	TObjectPtr<URelicSettings> RelicSettings;

	// Optional: Actor class override if you need relic-specific logic
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic")
	TSubclassOf<class ARelicActor> RelicActorClass;

	//~ UPrimaryDataAsset Interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset Interface
};