#pragma once

#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "BwayUpgradeComponent.generated.h"

class APawn;
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBwayPurchaseResult, FName, Id, bool, bSuccess);

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
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	bool ConfigureCatalog(UBwayUpgradeCatalog* InCatalog);
	UFUNCTION(BlueprintPure)
	int32 GetOwnedRank(FName Id) const;
	UFUNCTION(BlueprintPure)
	TArray<FBwayOwnedUpgrade> GetOwnedUpgrades() const { return OwnedUpgrades; }
	UFUNCTION(BlueprintPure)
	bool IsPurchaseWindowOpen() const;
	/** Canonical upgrade shop eligibility check (Prematch, Warmup, PostRound non-match-end, Dead, or alive in friendly Base Healing Zone). */
	UFUNCTION(BlueprintPure, Category="Economy|Base")
	bool CanPurchaseUpgrades() const { return IsPurchaseWindowOpen(); }
	/** Explicitly set or clear base healing zone presence for this player. */
	UFUNCTION(BlueprintCallable, Category="Economy|Base")
	void SetInHealingZone(bool bInZone, int32 ZoneTeamIndex = -1);
	/** True when the owning player is alive inside their own team's base healing zone. */
	UFUNCTION(BlueprintPure, Category="Economy|Base")
	bool IsPlayerInHealingZone() const;
	/** Clients supply identity and their last observed rank, never price or effect parameters. */
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerPurchaseUpgrade(FName Id, int32 ExpectedCurrentRank);
	/** Server RPC alias supporting live base healing zone and standard purchase windows. */
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerTryPurchaseUpgrade(FName Id, int32 ExpectedCurrentRank = -1);
	bool TryPurchaseUpgrade(FName Id, int32 ExpectedCurrentRank = -1);

	/** Sell the highest owned rank of Id, refunding its catalog cost and restoring the prior rank. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Economy")
	void ServerSellUpgrade(FName Id);
	bool TrySellUpgrade(FName Id);
	UFUNCTION(BlueprintPure, Category="Economy")
	bool CanSellUpgrade(FName Id) const;

	/** Undo the most recent buy or sell transaction in the current shop session. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Economy")
	void ServerUndoLastTransaction();
	bool TryUndoLastTransaction();
	UFUNCTION(BlueprintPure, Category="Economy")
	bool CanUndoLastTransaction() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void ResetUpgrades();
	UPROPERTY(BlueprintAssignable)
	FBwayUpgradesChanged OnUpgradesChanged;
	UPROPERTY(BlueprintAssignable)
	FBwayPurchaseResult OnPurchaseResult;

private:
	struct FBwayUpgradeTransactionRecord
	{
		FName Id;
		int32 PreviousRank = 0;
		int32 NewRank = 0;
		int32 GoldCost = 0;
		bool bWasPurchase = true;
	};
	bool ApplyOwnedRankInternal(FName Id, int32 TargetRank, class UAbilitySystemComponent* ASC, const struct FBwayUpgradeDefinition* Definition);
	virtual void BeginPlay() override;
	UFUNCTION(Client, Reliable)
	void ClientPurchaseResult(FName Id, bool bSuccess);
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_OwnedUpgrades)
	TObjectPtr<UBwayUpgradeCatalog> Catalog;
	UPROPERTY(ReplicatedUsing=OnRep_OwnedUpgrades)
	TArray<FBwayOwnedUpgrade> OwnedUpgrades;
	UPROPERTY(ReplicatedUsing=OnRep_OwnedUpgrades)
	int32 ReplicatedUndoCount = 0;
	UFUNCTION()
	void OnRep_OwnedUpgrades();
	TMap<FName, FActiveGameplayEffectHandle> EffectHandles;
	TArray<FBwayUpgradeTransactionRecord> TransactionHistory;
	bool bTransactionInProgress = false;
	bool bExplicitInHealingZone = false;
	int32 ExplicitHealingZoneTeamIndex = -1;
	int32 ExplicitHealingZoneDepth = 0;
	TWeakObjectPtr<const APawn> ExplicitHealingZonePawn;
	FVector ExplicitHealingZonePawnLocation = FVector(FLT_MAX);
};
