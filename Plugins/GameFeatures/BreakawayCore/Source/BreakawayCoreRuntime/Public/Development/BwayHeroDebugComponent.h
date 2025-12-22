// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BwayHeroDebugComponent.generated.h"

class ULyraAbilitySystemComponent;
class ABwayPlayerState;

/**
 * UBwayHeroDebugComponent
 * 
 * Debug component that displays hero and ability information on-screen.
 * Attach to characters to visualize:
 * - Current hero selection
 * - Granted abilities and their input tags
 * - Ability activation attempts
 * 
 * Enable with console command: ToggleHeroDebug
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayHeroDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBwayHeroDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Toggle debug display on/off */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void ToggleDebugDisplay();

	/** Set debug display state directly */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void SetDebugDisplayEnabled(bool bEnabled);

	/** Check if debug display is currently enabled */
	UFUNCTION(BlueprintPure, Category = "Debug")
	bool IsDebugDisplayEnabled() const { return bShowDebugInfo; }

	/** Log an ability activation attempt (call this from abilities) */
	static void LogAbilityActivation(const FString& AbilityName, bool bSuccess, const FString& FailReason = TEXT(""));

	/** Get the singleton debug component for the local player (if any) */
	static UBwayHeroDebugComponent* GetLocalPlayerDebugComponent();

protected:
	/** Draw debug info to screen */
	void DrawDebugInfo();

	/** Get hero name from player state */
	FString GetCurrentHeroName() const;

	/** Get granted ability count and list */
	void GetGrantedAbilitiesInfo(int32& OutCount, TArray<FString>& OutAbilityNames) const;

	/** Get the ability system component */
	ULyraAbilitySystemComponent* GetAbilitySystemComponent() const;

	/** Get the player state */
	ABwayPlayerState* GetBwayPlayerState() const;

private:
	/** Whether to show debug info on screen */
	UPROPERTY()
	bool bShowDebugInfo;

	/** Cached ability activation log (last N activations) */
	static TArray<FString> AbilityActivationLog;
	static constexpr int32 MaxLogEntries = 10;

	/** Static instance for local player */
	static TWeakObjectPtr<UBwayHeroDebugComponent> LocalPlayerInstance;
};

