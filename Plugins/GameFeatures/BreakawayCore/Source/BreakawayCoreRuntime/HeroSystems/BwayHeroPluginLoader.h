// BwayHeroPluginLoader.h
#pragma once

#include "GameFeaturePluginOperationResult.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "BwayHeroPluginLoader.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHeroPluginsReady);

/**
 * Async BP node: activates every hero Game-Feature plugin found under
 * "/GameFeatures/Heroes/*" and fires when all are in the **Active** state.
 */
UCLASS()
class UBwayHeroPluginLoader : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Blueprint entry point */
	UFUNCTION(BlueprintCallable,
			  meta=(WorldContext="WorldContextObject",
					BlueprintInternalUseOnly="true"))
	static UBwayHeroPluginLoader* LoadAllHeroPlugins(UObject* WorldContextObject);

	/** Blueprint-assignable completion signal */
	UPROPERTY(BlueprintAssignable)
	FHeroPluginsReady OnCompleted;

protected:
	virtual void Activate() override;

private:
	/* internal */
	void StartNext();
	void HandleLoadComplete(const UE::GameFeatures::FResult& Result);

	TArray<FString> PendingURLs;
};
