#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayBuildablePlacementLibrary.generated.h"

class ABuildableActor;
class APlayerController;
class UBwayBuildableDataAsset;
class USkeletalMesh;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayBuildablePlacementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Buildables", meta = (WorldContext = "WorldContextObject"))
	static ABuildableActor* SpawnBuildableForPlayer(const UObject* WorldContextObject, APlayerController* PlayerController, UBwayBuildableDataAsset* BuildableData, const FTransform& SpawnTransform);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables", meta = (WorldContext = "WorldContextObject"))
	static bool CanPlayerPlaceBuildable(const UObject* WorldContextObject, const APlayerController* PlayerController, const UBwayBuildableDataAsset* BuildableData, FText& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables", meta = (WorldContext = "WorldContextObject"))
	static UBwayBuildableDataAsset* ResolveBuildableDataForPlayer(const UObject* WorldContextObject, const APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables", meta = (WorldContext = "WorldContextObject"))
	static int32 CountActiveBuildablesForPlayer(const UObject* WorldContextObject, const APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables")
	static bool MakePlacementTransformFromHit(const FHitResult& HitResult, const APlayerController* PlayerController, FTransform& OutTransform);

	/** Player cosmetic index into BuildableData->CosmeticBuildableMeshes. Returns 0 until loadout selection exists. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables")
	static int32 GetBuildableCosmeticIndexForPlayer(const APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables", meta = (WorldContext = "WorldContextObject"))
	static USkeletalMesh* ResolvePreviewMeshForPlayer(const UObject* WorldContextObject, const APlayerController* PlayerController, const UBwayBuildableDataAsset* BuildableData);

	/** Applies data-asset mesh to the buildable root skeletal mesh when the BP left it unset. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Buildables")
	static void ApplyResolvedBuildableMesh(ABuildableActor* Buildable, const UBwayBuildableDataAsset* BuildableData, int32 CosmeticIndex);
};
