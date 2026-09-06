#pragma once

#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "BwayUpgradeComponent.generated.h"

class UBwayUpgradeCatalog;

USTRUCT(BlueprintType)
struct FBwayOwnedUpgrade
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	FName Id;
	UPROPERTY(BlueprintReadOnly)
	int32 Rank = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBwayUpgradesChanged);

/** Match ownership lives beside the persistent PlayerState ASC, independently of the pawn. */
UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayUpgradeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBwayUpgradeComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure)
	UBwayUpgradeCatalog* GetCatalog() const { return Catalog; }
	/** Authority setup only; a catalog cannot change while ranks are owned. */
	bool ConfigureCatalog(UBwayUpgradeCatalog* InCatalog);
	UFUNCTION(BlueprintPure)
	int32 GetOwnedRank(FName Id) const;
	UFUNCTION(BlueprintPure)
	TArray<FBwayOwnedUpgrade> GetOwnedUpgrades() const { return OwnedUpgrades; }
	UFUNCTION(BlueprintPure)
	bool IsPurchaseWindowOpen() const;
	/** Clients supply identity and their last observed rank, never price or effect parameters. */
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerPurchaseUpgrade(FName Id, int32 ExpectedCurrentRank);
	bool TryPurchaseUpgrade(FName Id, int32 ExpectedCurrentRank);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void ResetUpgrades();
	UPROPERTY(BlueprintAssignable)
	FBwayUpgradesChanged OnUpgradesChanged;

private:
	UPROPERTY(EditDefaultsOnly, Replicated)
	TObjectPtr<UBwayUpgradeCatalog> Catalog;
	UPROPERTY(ReplicatedUsing=OnRep_OwnedUpgrades)
	TArray<FBwayOwnedUpgrade> OwnedUpgrades;
	UFUNCTION()
	void OnRep_OwnedUpgrades();
	TMap<FName, FActiveGameplayEffectHandle> EffectHandles;
	bool bTransactionInProgress = false;
};
