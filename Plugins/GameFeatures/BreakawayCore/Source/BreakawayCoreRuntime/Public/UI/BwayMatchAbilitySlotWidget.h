#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "BwayMatchAbilitySlotWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;

/**
 * Complete presentation state for one stable in-match ability-bar position.
 * The slot is display-only; gameplay input remains owned by Lyra Enhanced Input.
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayMatchAbilitySlotViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	int32 Position = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	FAbilityDisplayInfo DisplayInfo;

	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	FText KeyLabel;

	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	bool bIsBuildable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	bool bIsConsumed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	bool bIsRelicAbility = false;

	/** True while the ability's cooldown gameplay effect is active. */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	bool bIsOnCooldown = false;

	/**
	 * Cooldown fill progress in [0,1].
	 * 0 = just entered cooldown, 1 = about to become ready.
	 * Computed as 1 - (Remaining / Duration).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CooldownPercent = 0.f;

	/** Seconds remaining on cooldown. Meaningful only while bIsOnCooldown is true. */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	float CountdownTime = 0.f;
};

/**
 * Non-interactive visual for one match ability. Blueprint owns only layout/style.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Match Ability Slot"))
class BREAKAWAYCORERUNTIME_API UBwayMatchAbilitySlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ability Bar")
	void SetSlotViewModel(const FBwayMatchAbilitySlotViewModel& InViewModel);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Bar")
	const FBwayMatchAbilitySlotViewModel& GetSlotViewModel() const { return ViewModel; }

protected:
	virtual void NativePreConstruct() override;

	void ApplyViewModel();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Bar", meta = (DisplayName = "On Slot View Model Changed"))
	void BP_OnSlotViewModelChanged(const FBwayMatchAbilitySlotViewModel& NewViewModel);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UImage> Image_DisabledOverlay;

	/** Grey-out overlay shown while the ability is on cooldown. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UImage> Image_CooldownOverlay;

	/** Vertical cooldown fill (bottom→top). Hidden when ready. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UProgressBar> ProgressBar_Cooldown;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UImage> Image_RelicFrame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UTextBlock> Text_AbilityName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UTextBlock> Text_Keybind;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UTextBlock> Text_State;

	/** Seconds remaining; shown only while on cooldown. Collapsed when ready. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Ability Bar|Widgets")
	TObjectPtr<UTextBlock> Text_CooldownTimer;

	UPROPERTY(BlueprintReadOnly, Category = "Ability Bar")
	FBwayMatchAbilitySlotViewModel ViewModel;
};
