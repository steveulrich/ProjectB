#pragma once

#include "CommonActivatableWidget.h"
#include "Components/ComboBoxString.h"
#include "BwayUpgradeShopWidget.generated.h"

class UBwayUpgradeComponent;
class UAbilitySystemComponent;
class UTextBlock;
class UButton;

/** Local CommonUI view. All purchases resolve through the owning PlayerState component. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayUpgradeShopWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	UFUNCTION() void Refresh();
	UFUNCTION() void BuySelected();
	UFUNCTION() void CloseShop();
	UFUNCTION() void SelectionChanged(FString Selected, ESelectInfo::Type Type);
	UFUNCTION() void PurchaseResult(FName Id, bool bSuccess);
	void RefreshAccess();
	UPROPERTY(Transient) TObjectPtr<UBwayUpgradeComponent> Shop;
	UPROPERTY(Transient) TObjectPtr<UAbilitySystemComponent> ASC;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> Selector;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Balance;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Details;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BuyLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> BuyButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
	TArray<FName> DisplayIds;
	FDelegateHandle GoldHandle;
	FTimerHandle AccessTimer;
	bool bPending = false;
	bool bAccessWasOpen = true;
};
