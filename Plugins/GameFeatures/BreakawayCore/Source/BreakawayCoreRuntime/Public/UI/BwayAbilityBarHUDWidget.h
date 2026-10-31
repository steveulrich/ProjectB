#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "InputCoreTypes.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "UI/BwayMatchAbilitySlotWidget.h"
#include "BwayAbilityBarHUDWidget.generated.h"

class ABwayPlayerState;
class UAbilitySystemComponent;
class UBorder;
class UEnhancedInputLocalPlayerSubsystem;
class UHorizontalBox;
class UInputAction;
class ULyraGameplayAbility;
class UTextBlock;
class UVerticalBox;

/**
 * Lyra HUD-slot ability bar. It renders six stable, non-interactive positions:
 * buildable, primary, hero mobility, two abilities, and ultimate, plus separate
 * compact prompts for Shared Slide (IA_BW_Slide) and Relic Request / Controls.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Ability Bar HUD Widget"))
class BREAKAWAYCORERUNTIME_API UBwayAbilityBarHUDWidget : public UBwayMatchHUDWidgetBase
{
	GENERATED_BODY()

public:
	UBwayAbilityBarHUDWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "HUD|Ability Bar")
	void RefreshAbilityBar();

	const TArray<TObjectPtr<UBwayMatchAbilitySlotWidget>>& GetSlotWidgets() const { return SlotWidgets; }

	UFUNCTION(BlueprintPure, Category = "HUD|Ability Bar")
	FText GetSharedSlideKeyLabel() const { return CachedSharedSlideKeyLabel; }

	UFUNCTION(BlueprintPure, Category = "HUD|Ability Bar")
	FText GetSharedSlidePromptText() const;

	UFUNCTION(BlueprintPure, Category = "HUD|Ability Bar")
	FText GetRelicRequestKeyLabel() const { return CachedRelicRequestKeyLabel; }

	UFUNCTION(BlueprintPure, Category = "HUD|Ability Bar")
	FText GetRelicPromptText() const;

	/** Deterministic KBM vs Gamepad override for headless view-model verification. */
	void SetInputDeviceOverrideForTest(bool bEnableOverride, bool bPreferGamepad);

	/** Public entry point for tests to verify HandleControlMappingsRebuilt updates labels live. */
	void TriggerControlMappingsRebuiltForTest() { HandleControlMappingsRebuilt(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NotifyRoundStateChanged(FName NewState) override;

	void RebuildSlotWidgets();
	void BindToLocalPlayerState();
	void UnbindFromLocalPlayerState();
	void RequestAbilityBarRefresh();
	void RefreshAuxiliaryPrompts(bool bIsRelicCarrier);
	bool ShouldPreferGamepadLabels() const;

	TArray<FGameplayTag> GetFixedHeroSlotTags() const;
	FText GetHeroFallbackLabel(int32 Position) const;
	const UInputAction* ResolveInputAction(
		FGameplayTag InputTag,
		const TSoftObjectPtr<const UInputAction>& FallbackInputAction) const;
	FText ResolveKeyLabel(const UInputAction* InputAction) const;
	FText CompactKeyDisplayName(const FKey& Key) const;

	/** Fills cooldown fields on the view model from the live ASC + ability CDO. */
	void FillCooldownState(
		UAbilitySystemComponent* ASC,
		TSubclassOf<ULyraGameplayAbility> AbilityClass,
		FBwayMatchAbilitySlotViewModel& InOutViewModel) const;

	UFUNCTION()
	void HandleRelicPossessionChanged(bool bHasRelic);

	UFUNCTION()
	void HandleBuildableStateChanged(bool bHasPlacedBuildable);

	UFUNCTION()
	void HandleSelectedHeroChanged(FPrimaryAssetId NewHeroId);

	UFUNCTION()
	void HandleControlMappingsRebuilt();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Ability Bar")
	TObjectPtr<UHorizontalBox> AbilitySlots;

	/** Visual Blueprint class created once for each of the six fixed positions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Ability Bar")
	TSubclassOf<UBwayMatchAbilitySlotWidget> AbilitySlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Ability Bar", meta = (ClampMin = "0.05"))
	float AbilityRefreshInterval = 0.25f;

	/** Faster refresh while any ability slot is cooling down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Ability Bar", meta = (ClampMin = "0.016"))
	float CooldownRefreshInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Ability Bar")
	FMargin AbilitySlotPadding = FMargin(3.0f, 0.0f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBwayMatchAbilitySlotWidget>> SlotWidgets;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ABwayPlayerState> BoundPlayerState;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundInputSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<const UInputAction> CachedSharedSlideAction;

	UPROPERTY(Transient)
	TObjectPtr<const UInputAction> CachedRelicRequestAction;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> AuxiliaryPromptsBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_SharedSlidePrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_RelicPrompt;

	FDelegateHandle InputMethodChangedHandle;
	FText CachedSharedSlideKeyLabel;
	FText CachedRelicRequestKeyLabel;

	float TimeSinceLastAbilityRefresh = 0.0f;
	bool bRefreshRequested = true;
	bool bAnySlotOnCooldown = false;
	bool bTestOverrideInputDevice = false;
	bool bTestPreferGamepad = false;
};
