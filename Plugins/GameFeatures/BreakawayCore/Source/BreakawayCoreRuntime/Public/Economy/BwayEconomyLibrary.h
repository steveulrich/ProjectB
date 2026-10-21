#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayEffect.h"
#include "BwayEconomyLibrary.generated.h"

class APlayerState;

/** Fixed native debit definition; instant effects report execution without an active handle. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayEffect_SpendGold : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UBwayGameplayEffect_SpendGold();
	static const FName DebitMagnitudeName;
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayEconomyLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Debit a server-resolved price, without partial payment. This is not a purchase RPC:
	 * callers must validate the catalog item, rank, and purchase window separately.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Economy")
	static bool TrySpendGold(APlayerState* PlayerState, int32 Cost);

	/** Refunds gold from a shop sell or undo transaction without inflating MatchGoldEarned. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Economy")
	static bool TryRefundGold(APlayerState* PlayerState, int32 RefundAmount);

	/** Seeds initial pre-round match gold (default 1000) without inflating MatchGoldEarned. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Economy")
	static bool GrantStartingMatchGold(APlayerState* PlayerState, int32 StartingGold = 1000);
};
