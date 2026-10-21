#pragma once

#include "CommonActivatableWidget.h"
#include "Components/ComboBoxString.h"
#include "BwayUpgradeShopWidget.generated.h"

class UBwayUpgradeComponent;
class UAbilitySystemComponent;
class UTextBlock;
class UButton;
class UVerticalBox;
class UBorder;

/** Local CommonUI view. All purchases resolve through the owning PlayerState component. */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayUpgradeShopWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	UBwayUpgradeShopWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Shop")
	void SelectUpgradeById(FName UpgradeId);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Shop")
	void SelectUpgradeByIndex(int32 CatalogIndex);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Shop")
	void SellSelected();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Shop")
	void UndoLastAction();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	UFUNCTION() void Refresh();
	UFUNCTION() void BuySelected();
	UFUNCTION() void CloseShop();
	UFUNCTION() void SelectionChanged(FString Selected, ESelectInfo::Type Type);
	UFUNCTION() void PurchaseResult(FName Id, bool bSuccess);
	UFUNCTION() void OnCatalogItem0Clicked();
	UFUNCTION() void OnCatalogItem1Clicked();
	UFUNCTION() void OnCatalogItem2Clicked();
	UFUNCTION() void OnCatalogItem3Clicked();
	UFUNCTION() void OnCatalogItem4Clicked();
	UFUNCTION() void OnCatalogItem5Clicked();
	UFUNCTION() void OnCatalogItem6Clicked();
	UFUNCTION() void OnCatalogItem7Clicked();
	UFUNCTION() void OnInventorySlot0Clicked();
	UFUNCTION() void OnInventorySlot1Clicked();
	UFUNCTION() void OnInventorySlot2Clicked();
	UFUNCTION() void OnInventorySlot3Clicked();
	void RefreshAccess();
	void RebuildCategoryGrid();
	UPROPERTY(Transient) TObjectPtr<UBwayUpgradeComponent> Shop;
	UPROPERTY(Transient) TObjectPtr<UAbilitySystemComponent> ASC;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> Selector;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PreRoundTimerText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> InspectorTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> InspectorCategoryText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Balance;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Details;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BuyLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SellLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UndoLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> BuyButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SellButton;
	UPROPERTY(Transient) TObjectPtr<UButton> UndoButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> AttackColumnBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> DefenseColumnBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> MiscColumnBox;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> CatalogCardButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> CatalogCardLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> InventorySlotButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> InventorySlotLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> TierRowBorders;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TierRowTexts;
	TArray<FName> DisplayIds;
	FDelegateHandle GoldHandle;
	FTimerHandle AccessTimer;
	bool bPending = false;
	bool bAccessWasOpen = true;
};
