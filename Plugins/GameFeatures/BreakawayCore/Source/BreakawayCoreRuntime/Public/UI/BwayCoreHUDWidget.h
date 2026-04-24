// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BwayCoreHUDWidget.generated.h"

class ULyraHealthComponent;
class ULyraAbilitySystemComponent;
class ABwayGameState;
class ABwayPlayerState;

/**
 * UBwayCoreHUDWidget
 * 
 * The main gameplay HUD widget for Breakaway.
 * Provides health, ability, score, and timer display integration.
 * Designed to be extended in Blueprint for visual implementation.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Core HUD"))
class BREAKAWAYCORERUNTIME_API UBwayCoreHUDWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayCoreHUDWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget interface

	// ========== DATA ACCESS ==========

	/** Get the current health percentage (0.0-1.0) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Health")
	float GetHealthPercent() const;

	/** Get the current health value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Health")
	float GetCurrentHealth() const;

	/** Get the max health value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Health")
	float GetMaxHealth() const;

	/** Get the team 1 score */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Score")
	int32 GetTeam1Score() const;

	/** Get the team 2 score */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Score")
	int32 GetTeam2Score() const;

	/** Get the local player's team index (0 or 1, -1 if unknown) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Score")
	int32 GetLocalPlayerTeam() const;

	/** Get remaining round time in seconds */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Timer")
	int32 GetRoundTimeRemaining() const;

	/** Get formatted round time string (MM:SS) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Timer")
	FText GetRoundTimeFormatted() const;

	/** Get current round number */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Timer")
	int32 GetCurrentRoundNumber() const;

	/** Get which team currently possesses the relic (-1 = neutral) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Relic")
	int32 GetRelicPossessingTeam() const;

	/** Check if relic is currently carried */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Relic")
	bool IsRelicCarried() const;

	// ========== BLUEPRINT EVENTS ==========

	/** Called when health changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnHealthChanged(float NewHealth, float MaxHealth, float HealthPercent);

	/** Called when team scores change */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnScoreChanged(int32 Team1Score, int32 Team2Score);

	/** Called every second with updated round time */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRoundTimeUpdated(int32 SecondsRemaining, const FText& FormattedTime);

	/** Called when round state changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRoundStateChanged(FName NewState);

	/** Called when relic possession changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRelicPossessionChanged(int32 TeamIndex, bool bIsCarried);

	/** Called when the local player dies */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnLocalPlayerDied();

	/** Called when the local player respawns */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnLocalPlayerRespawned();

private:
	/** Bind to game state delegates */
	void BindToGameState();
	
	/** Unbind from game state delegates */
	void UnbindFromGameState();

	/** Bind to local player's health component */
	void BindToHealthComponent();

	/** Handle health changed callback */
	UFUNCTION()
	void HandleHealthChanged(ULyraHealthComponent* HealthComp, float OldValue, float NewValue, AActor* Instigator);

	/** Handle score changed callback (from ScoringComponent) */
	UFUNCTION()
	void HandleScoreChanged(int32 TeamIndex, int32 NewScore);

	/** Handle round time changed callback */
	UFUNCTION()
	void HandleRoundTimeChanged(int32 SecondsRemaining);

	/** Handle round state changed callback */
	UFUNCTION()
	void HandleRoundStateChanged(FName NewState);

	/** Get the game state */
	ABwayGameState* GetBwayGameState() const;

	/** Get the local player's pawn health component */
	ULyraHealthComponent* GetLocalPlayerHealthComponent() const;

	/** Cached references */
	UPROPERTY()
	TWeakObjectPtr<ABwayGameState> CachedGameState;

	UPROPERTY()
	TWeakObjectPtr<ULyraHealthComponent> CachedHealthComponent;

	/** Track if we're bound to the health component */
	bool bBoundToHealth = false;

	/** Last known health for change detection */
	float LastKnownHealth = -1.0f;
};

