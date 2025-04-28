#pragma once

#include "CoreMinimal.h"
#include "Player/LyraPlayerState.h" // Or your base Lyra PlayerState
#include "BwayPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABwayPlayerState : public ALyraPlayerState
{
	GENERATED_BODY()

	// In BwayPlayerState.h, add these declarations:
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	ABwayPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Relic request state
	UPROPERTY(ReplicatedUsing = OnRep_IsRequestingRelic, BlueprintReadOnly, Category = "Relic")
	bool bIsRequestingRelic = false;

	UFUNCTION(BlueprintPure) FORCEINLINE bool IsRequestingRelic() const { return bIsRequestingRelic; }

	UFUNCTION()
	void OnRep_IsRequestingRelic();

	// Function called by GA to set the state on the server
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Relic")
	void ServerSetIsRequestingRelic(bool bNewRequestingState);

	// Delegate broadcast when the request state changes (useful for UI)
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRequestingRelicChanged, bool, bIsRequesting);
	UPROPERTY(BlueprintAssignable, Category = "Relic|UI")
	FOnRequestingRelicChanged OnRequestingRelicChanged;
};