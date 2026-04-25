#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayBuildablePlacementLibrary.generated.h"

class ABuildableActor;
class APlayerController;
class UBwayBuildableDataAsset;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayBuildablePlacementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Buildables", meta = (WorldContext = "WorldContextObject"))
	static ABuildableActor* SpawnBuildableForPlayer(const UObject* WorldContextObject, APlayerController* PlayerController, UBwayBuildableDataAsset* BuildableData, const FTransform& SpawnTransform);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables", meta = (WorldContext = "WorldContextObject"))
	static bool CanPlayerPlaceBuildable(const UObject* WorldContextObject, APlayerController* PlayerController, UBwayBuildableDataAsset* BuildableData, FText& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables", meta = (WorldContext = "WorldContextObject"))
	static int32 CountActiveBuildablesForPlayer(const UObject* WorldContextObject, APlayerController* PlayerController);
};
