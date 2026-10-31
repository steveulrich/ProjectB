// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCurrencyDisplayWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetTree.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCurrencyDisplayWidget)

UBwayCurrencyDisplayWidget::UBwayCurrencyDisplayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayCurrencyDisplayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureWalletHUDWidgets();
	if (bIsWalletHUDWidget && !CurrencyIcon)
	{
		if (Image_CurrencyIcon)
		{
			CurrencyIcon = Cast<UTexture2D>(Image_CurrencyIcon->GetBrush().GetResourceObject());
		}
		if (!CurrencyIcon)
		{
			CurrencyIcon = LoadObject<UTexture2D>(
				nullptr,
				TEXT("/BreakawayCore/UI/Art/Match_HUD/icon_gold.icon_gold"));
		}
	}
	SetAmount(CurrentAmount);
	SetCurrencyIcon(CurrencyIcon);
	BindInputDelegates();
	TryBindToCurrentGold();
	RefreshWalletHUDState();
}

void UBwayCurrencyDisplayWidget::NativeDestruct()
{
	UnbindFromCurrentGold();
	UnbindInputDelegates();
	Super::NativeDestruct();
}

void UBwayCurrencyDisplayWidget::SetWalletHUDModeForTest(const bool bEnable)
{
	bForceWalletHUDMode = bEnable;
	EnsureWalletHUDWidgets();
	BindInputDelegates();
	TryBindToCurrentGold();
	RefreshWalletHUDState();
}

void UBwayCurrencyDisplayWidget::SetInputDeviceOverrideForTest(const bool bEnableOverride, const bool bPreferGamepad)
{
	bTestOverrideInputDevice = bEnableOverride;
	bTestPreferGamepad = bPreferGamepad;
	RefreshWalletHUDState();
}

void UBwayCurrencyDisplayWidget::EnsureWalletHUDWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!CachedGoldFrame)
	{
		CachedGoldFrame = Cast<UBorder>(WidgetTree->FindWidget(TEXT("GoldFrame")));
	}

	bIsWalletHUDWidget = (CachedGoldFrame != nullptr) || bForceWalletHUDMode;
	if (!bIsWalletHUDWidget)
	{
		return;
	}

	UHorizontalBox* GoldRow = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("GoldRow")));
	if (!GoldRow)
	{
		WidgetTree->ForEachWidget([&GoldRow](UWidget* Candidate)
		{
			if (!GoldRow)
			{
				GoldRow = Cast<UHorizontalBox>(Candidate);
			}
		});
	}
	if (!WidgetTree->RootWidget && !GoldRow)
	{
		CachedGoldFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GoldFrame"));
		GoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoldRow"));
		Text_Amount = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Amount"));
		Image_CurrencyIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_CurrencyIcon"));
		GoldRow->AddChildToHorizontalBox(Text_Amount);
		GoldRow->AddChildToHorizontalBox(Image_CurrencyIcon);
		CachedGoldFrame->SetContent(GoldRow);
		WidgetTree->RootWidget = CachedGoldFrame;
	}

	if (CachedGoldFrame)
	{
		// Broadcast HUD displays floating gold digits + crystal shard ("783 ◆") with no dark box frame
		FSlateBrush NoBrush;
		NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		CachedGoldFrame->SetBrush(NoBrush);
		CachedGoldFrame->SetBrushColor(FLinearColor::Transparent);
		CachedGoldFrame->SetPadding(FMargin(0.0f));
	}

	if (USizeBox* RootSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("RootSizeBox"))))
	{
		RootSizeBox->ClearWidthOverride();
		RootSizeBox->ClearHeightOverride();
	}

	if (Text_Amount)
	{
		Text_Amount->SetFont(UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(30, 4, 2));
		Text_Amount->SetColorAndOpacity(FSlateColor(FLinearColor(0.97f, 0.73f, 0.14f, 1.0f)));
		Text_Amount->SetShadowOffset(FVector2D(1.25f, 1.25f));
		Text_Amount->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f));
	}

	if (Image_CurrencyIcon)
	{
		Image_CurrencyIcon->SetDesiredSizeOverride(FVector2D(20.0f, 30.0f));
		// Hide low-fill texture padding in wallet HUD mode so NativePaint renders the crisp faceted Breakaway gold crystal shard
		Image_CurrencyIcon->SetRenderOpacity(0.0f);
		Image_CurrencyIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Ensure broadcast order in GoldRow: [Text_Amount ("783")] on LEFT + [Image_CurrencyIcon ("◆")] on RIGHT
	if (GoldRow && Text_Amount && Image_CurrencyIcon)
	{
		const TArray<UWidget*> RowChildren = GoldRow->GetAllChildren();
		if (RowChildren.Num() < 2 || RowChildren[0] != Text_Amount || RowChildren[1] != Image_CurrencyIcon)
		{
			Text_Amount->RemoveFromParent();
			Image_CurrencyIcon->RemoveFromParent();
			if (Text_MaxGoldCap)
			{
				Text_MaxGoldCap->RemoveFromParent();
			}
			if (Text_ShopPrompt)
			{
				Text_ShopPrompt->RemoveFromParent();
			}
			GoldRow->ClearChildren();

			if (UHorizontalBoxSlot* AmountSlot = GoldRow->AddChildToHorizontalBox(Text_Amount))
			{
				AmountSlot->SetPadding(FMargin(0.0f, -3.0f, 0.0f, 0.0f));
				AmountSlot->SetVerticalAlignment(VAlign_Center);
			}
			if (UHorizontalBoxSlot* IconSlot = GoldRow->AddChildToHorizontalBox(Image_CurrencyIcon))
			{
				IconSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
				IconSlot->SetVerticalAlignment(VAlign_Center);
			}
			if (Text_MaxGoldCap)
			{
				GoldRow->AddChildToHorizontalBox(Text_MaxGoldCap);
			}
			if (Text_ShopPrompt)
			{
				GoldRow->AddChildToHorizontalBox(Text_ShopPrompt);
			}
		}
	}

	if (GoldRow && !Text_MaxGoldCap)
	{
		Text_MaxGoldCap = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_MaxGoldCap"));
		Text_MaxGoldCap->SetVisibility(ESlateVisibility::Collapsed);
		GoldRow->AddChildToHorizontalBox(Text_MaxGoldCap);
	}

	if (GoldRow && !Text_ShopPrompt)
	{
		Text_ShopPrompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ShopPrompt"));
		Text_ShopPrompt->SetVisibility(ESlateVisibility::Collapsed);
		GoldRow->AddChildToHorizontalBox(Text_ShopPrompt);
	}
}

void UBwayCurrencyDisplayWidget::BindInputDelegates()
{
	if (!bIsWalletHUDWidget)
	{
		return;
	}

	UnbindInputDelegates();

	if (!CachedUpgradeShopAction)
	{
		if (const ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(GetOwningPlayer()))
		{
			CachedUpgradeShopAction = BwayPC->GetUpgradeShopInputAction();
		}
		if (!CachedUpgradeShopAction)
		{
			CachedUpgradeShopAction = LoadObject<UInputAction>(
				nullptr,
				TEXT("/BreakawayCore/Input/Actions/IA_BW_UpgradeShop.IA_BW_UpgradeShop"));
		}
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			BoundInputSubsystem = InputSubsystem;
			InputSubsystem->ControlMappingsRebuiltDelegate.AddUniqueDynamic(
				this,
				&ThisClass::HandleControlMappingsRebuilt);
		}

		if (UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(LocalPlayer))
		{
			InputMethodChangedHandle = CommonInput->OnInputMethodChangedNative.AddWeakLambda(
				this,
				[this](ECommonInputType /*NewInputType*/)
				{
					HandleControlMappingsRebuilt();
				});
		}
	}
}

void UBwayCurrencyDisplayWidget::UnbindInputDelegates()
{
	if (BoundInputSubsystem.IsValid())
	{
		BoundInputSubsystem->ControlMappingsRebuiltDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleControlMappingsRebuilt);
	}
	BoundInputSubsystem.Reset();

	if (InputMethodChangedHandle.IsValid())
	{
		if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			if (UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(LocalPlayer))
			{
				CommonInput->OnInputMethodChangedNative.Remove(InputMethodChangedHandle);
			}
		}
		InputMethodChangedHandle.Reset();
	}
}

void UBwayCurrencyDisplayWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeSinceLastBindingAttempt += InDeltaTime;
	if (TimeSinceLastBindingAttempt >= BindingRetryInterval)
	{
		TimeSinceLastBindingAttempt = 0.0f;
		if (!BoundAbilitySystemComponent.IsValid() || (bIsWalletHUDWidget && !BoundUpgradeComponent.IsValid()))
		{
			TryBindToCurrentGold();
		}
		if (bIsWalletHUDWidget)
		{
			if (!BoundInputSubsystem.IsValid())
			{
				BindInputDelegates();
			}
			RefreshWalletHUDState();
		}
	}

	if (bIsWalletHUDWidget)
	{
		if (bShopPromptShouldPulse)
		{
			PulseAccumulatedSeconds += InDeltaTime;
			const float PulseAlpha = 0.5f + 0.5f * FMath::Sin(PulseAccumulatedSeconds * 5.5f);
			ApplyPulseVisuals(PulseAlpha);
		}
		else if (PulseAccumulatedSeconds > 0.0f)
		{
			PulseAccumulatedSeconds = 0.0f;
			ApplyPulseVisuals(0.0f);
		}
	}
}

void UBwayCurrencyDisplayWidget::TryBindToCurrentGold()
{
	APlayerState* PlayerState = GetOwningPlayerState();
	UAbilitySystemComponent* ASC = PlayerState ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState) : nullptr;
	if (!ASC)
	{
		return;
	}

	if (BoundAbilitySystemComponent.Get() != ASC || !CurrentGoldChangedHandle.IsValid())
	{
		UnbindFromCurrentGold();
		BoundAbilitySystemComponent = ASC;
		CurrentGoldChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
			.AddUObject(this, &UBwayCurrencyDisplayWidget::HandleCurrentGoldChanged);
		MaxGoldChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetMaxGoldAttribute())
			.AddUObject(this, &UBwayCurrencyDisplayWidget::HandleMaxGoldChanged);

		const float CurrentGold = ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute());
		CurrentMaxGold = FMath::Max(0, FMath::RoundToInt(ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetMaxGoldAttribute())));
		SetAmount(FMath::RoundToInt(CurrentGold));
	}

	if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
	{
		if (UBwayUpgradeComponent* UpgradeComp = BwayPS->UpgradeComponent)
		{
			if (BoundUpgradeComponent.Get() != UpgradeComp)
			{
				if (BoundUpgradeComponent.IsValid())
				{
					BoundUpgradeComponent->OnUpgradesChanged.RemoveDynamic(this, &ThisClass::HandleOwnedUpgradesChanged);
				}
				BoundUpgradeComponent = UpgradeComp;
				UpgradeComp->OnUpgradesChanged.AddUniqueDynamic(this, &ThisClass::HandleOwnedUpgradesChanged);
			}
		}
	}

	if (bIsWalletHUDWidget)
	{
		RefreshWalletHUDState();
	}
}

void UBwayCurrencyDisplayWidget::UnbindFromCurrentGold()
{
	if (BoundAbilitySystemComponent.IsValid())
	{
		if (CurrentGoldChangedHandle.IsValid())
		{
			BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
				.Remove(CurrentGoldChangedHandle);
		}
		if (MaxGoldChangedHandle.IsValid())
		{
			BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetMaxGoldAttribute())
				.Remove(MaxGoldChangedHandle);
		}
	}

	if (BoundUpgradeComponent.IsValid())
	{
		BoundUpgradeComponent->OnUpgradesChanged.RemoveDynamic(this, &ThisClass::HandleOwnedUpgradesChanged);
	}

	CurrentGoldChangedHandle.Reset();
	MaxGoldChangedHandle.Reset();
	BoundUpgradeComponent.Reset();
	BoundAbilitySystemComponent.Reset();
}

void UBwayCurrencyDisplayWidget::HandleCurrentGoldChanged(const FOnAttributeChangeData& ChangeData)
{
	SetAmount(FMath::RoundToInt(ChangeData.NewValue));
}

void UBwayCurrencyDisplayWidget::HandleMaxGoldChanged(const FOnAttributeChangeData& ChangeData)
{
	CurrentMaxGold = FMath::Max(0, FMath::RoundToInt(ChangeData.NewValue));
	RefreshWalletHUDState();
}

void UBwayCurrencyDisplayWidget::HandleOwnedUpgradesChanged()
{
	RefreshWalletHUDState();
}

void UBwayCurrencyDisplayWidget::HandleControlMappingsRebuilt()
{
	RefreshWalletHUDState();
}

FText UBwayCurrencyDisplayWidget::GetResolvedShopKeyLabel() const
{
	const UInputAction* ShopAction = CachedUpgradeShopAction;
	if (!ShopAction)
	{
		ShopAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/BreakawayCore/Input/Actions/IA_BW_UpgradeShop.IA_BW_UpgradeShop"));
	}
	if (!ShopAction)
	{
		return NSLOCTEXT("BwayCurrencyHUD", "ShopUnbound", "—");
	}

	bool bPreferGamepad = bTestPreferGamepad;
	if (!bTestOverrideInputDevice)
	{
		if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			if (const UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(LocalPlayer))
			{
				bPreferGamepad = (CommonInput->GetCurrentInputType() == ECommonInputType::Gamepad);
			}
		}
	}

	TArray<FKey> BoundKeys;
	if (BoundInputSubsystem.IsValid())
	{
		BoundKeys = BoundInputSubsystem->QueryKeysMappedToAction(ShopAction);
	}
	if (BoundKeys.IsEmpty())
	{
		static const TCHAR* ImcPaths[] = {
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_PlayerUI.IMC_BW_PlayerUI"),
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_Default.IMC_BW_Default"),
		};
		for (const TCHAR* ImcPath : ImcPaths)
		{
			if (const UInputMappingContext* Imc = LoadObject<UInputMappingContext>(nullptr, ImcPath))
			{
				for (const FEnhancedActionKeyMapping& Mapping : Imc->GetMappings())
				{
					if (Mapping.Action == ShopAction && Mapping.Key.IsValid())
					{
						BoundKeys.AddUnique(Mapping.Key);
					}
				}
			}
		}
	}

	if (BoundKeys.IsEmpty())
	{
		return NSLOCTEXT("BwayCurrencyHUD", "ShopUnbound", "—");
	}

	const FKey* PreferredKey = BoundKeys.FindByPredicate(
		[bPreferGamepad](const FKey& Key)
		{
			return Key.IsValid() && (Key.IsGamepadKey() == bPreferGamepad);
		});

	const FKey ChosenKey = PreferredKey ? *PreferredKey : BoundKeys[0];
	return ChosenKey.IsValid()
		? ChosenKey.GetDisplayName(/*bLongDisplayName*/ false)
		: NSLOCTEXT("BwayCurrencyHUD", "ShopUnbound", "—");
}

bool UBwayCurrencyDisplayWidget::EvaluateEligibleAffordablePurchase() const
{
	const UBwayUpgradeComponent* UpgradeComp = BoundUpgradeComponent.Get();
	if (!UpgradeComp)
	{
		if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(GetOwningPlayerState()))
		{
			UpgradeComp = BwayPS->UpgradeComponent;
		}
	}

	if (!UpgradeComp || !UpgradeComp->CanPurchaseUpgrades())
	{
		return false;
	}

	if (UBwayMatchHUDWidgetBase::IsLocalGameplayHUDOccludedByMenu(GetOwningPlayer()))
	{
		return false;
	}

	const UBwayUpgradeCatalog* Catalog = UpgradeComp->GetCatalog();
	if (!Catalog)
	{
		return false;
	}

	int32 DistinctOwnedCount = 0;
	for (const FBwayOwnedUpgrade& Owned : UpgradeComp->GetOwnedUpgrades())
	{
		if (Owned.Rank > 0)
		{
			++DistinctOwnedCount;
		}
	}

	for (const FBwayUpgradeDefinition& Def : Catalog->Upgrades)
	{
		if (Def.Id.IsNone() || Def.Ranks.IsEmpty())
		{
			continue;
		}

		const int32 OwnedRank = UpgradeComp->GetOwnedRank(Def.Id);
		if (OwnedRank < 0 || OwnedRank >= Def.Ranks.Num())
		{
			continue;
		}

		if (OwnedRank == 0 && DistinctOwnedCount >= Catalog->MaxOwnedUpgrades)
		{
			continue;
		}

		const int32 NextRankCost = Def.Ranks[OwnedRank].Cost;
		if (CurrentAmount >= NextRankCost)
		{
			return true;
		}
	}

	return false;
}

void UBwayCurrencyDisplayWidget::ApplyPulseVisuals(const float PulseAlpha)
{
	if (CachedGoldFrame)
	{
		CachedGoldFrame->SetBrushColor(FLinearColor::Transparent);
	}

	if (Text_Amount && bIsWalletHUDWidget)
	{
		const FLinearColor BaseGoldColor(0.97f, 0.73f, 0.14f, 1.0f);
		const FLinearColor BrightGoldColor(1.0f, 0.92f, 0.44f, 1.0f);
		Text_Amount->SetColorAndOpacity(FSlateColor(
			bShopPromptShouldPulse
				? FMath::Lerp(BaseGoldColor, BrightGoldColor, PulseAlpha)
				: BaseGoldColor));
	}

	if (Text_ShopPrompt)
	{
		const FLinearColor IdleColor(0.72f, 0.76f, 0.84f, 0.84f);
		const FLinearColor EligibleLowColor(0.95f, 0.78f, 0.20f, 0.92f);
		const FLinearColor EligibleHighColor(1.0f, 0.96f, 0.56f, 1.0f);
		Text_ShopPrompt->SetColorAndOpacity(FSlateColor(
			bShopPromptShouldPulse
				? FMath::Lerp(EligibleLowColor, EligibleHighColor, PulseAlpha)
				: IdleColor));
	}
}

void UBwayCurrencyDisplayWidget::RefreshWalletHUDState()
{
	EnsureWalletHUDWidgets();
	if (!bIsWalletHUDWidget)
	{
		return;
	}

	if (!BoundInputSubsystem.IsValid())
	{
		BindInputDelegates();
	}

	UAbilitySystemComponent* LiveASC = BoundAbilitySystemComponent.Get();
	if (!LiveASC)
	{
		if (APlayerState* PS = GetOwningPlayerState())
		{
			LiveASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
		}
	}

	if (LiveASC)
	{
		const int32 LiveGold = FMath::RoundToInt(LiveASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()));
		if (LiveGold != CurrentAmount)
		{
			SetAmount(LiveGold);
		}
		CurrentMaxGold = FMath::Max(
			0,
			FMath::RoundToInt(LiveASC->GetNumericAttribute(UBwayGoldAttributeSet::GetMaxGoldAttribute())));
	}

	if (Text_MaxGoldCap)
	{
		if (CurrentMaxGold > 0)
		{
			FNumberFormattingOptions Options;
			Options.UseGrouping = true;
			Text_MaxGoldCap->SetText(
				FText::FromString(FString::Printf(TEXT("/ %s"), *FText::AsNumber(CurrentMaxGold, &Options).ToString())));
		}
		else
		{
			Text_MaxGoldCap->SetText(FText::GetEmpty());
		}
		// Keep cap text queryable via GetDisplayedCapText() while hidden in the broadcast wallet HUD ("783 ◆").
		Text_MaxGoldCap->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Text_ShopPrompt)
	{
		const FText ShopKey = GetResolvedShopKeyLabel();
		Text_ShopPrompt->SetText(
			FText::FromString(FString::Printf(TEXT("[%s] SHOP"), *ShopKey.ToString())));
		// Dedicated bottom-left "UPGRADES [B]" banner renders on UBwayMatchBroadcastOverlayWidget when near team goal.
		Text_ShopPrompt->SetVisibility(ESlateVisibility::Collapsed);
	}

	const bool bNewPulse = EvaluateEligibleAffordablePurchase();
	if (bShopPromptShouldPulse != bNewPulse)
	{
		bShopPromptShouldPulse = bNewPulse;
		if (!bShopPromptShouldPulse)
		{
			PulseAccumulatedSeconds = 0.0f;
			ApplyPulseVisuals(0.0f);
		}
		else
		{
			ApplyPulseVisuals(0.8f);
		}
	}
}

void UBwayCurrencyDisplayWidget::SetAmount(int32 Amount)
{
	CurrentAmount = Amount;

	// Broadcast HUD wallet displays plain integer gold ("783"); shop price tags retain thousands grouping.
	FNumberFormattingOptions Options;
	Options.UseGrouping = !bIsWalletHUDWidget;
	FText FormattedText = FText::AsNumber(Amount, &Options);

	if (Text_Amount)
	{
		Text_Amount->SetText(FormattedText);
	}

	if (bIsWalletHUDWidget)
	{
		RefreshWalletHUDState();
	}

	OnAmountChanged(CurrentAmount, FormattedText);
}

void UBwayCurrencyDisplayWidget::SetCurrencyIcon(UTexture2D* Icon)
{
	CurrencyIcon = Icon;

	if (Image_CurrencyIcon)
	{
		Image_CurrencyIcon->SetBrushFromTexture(CurrencyIcon);
		Image_CurrencyIcon->SetVisibility(
			(bIsWalletHUDWidget || CurrencyIcon) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	OnCurrencyIconChanged(CurrencyIcon);
}

int32 UBwayCurrencyDisplayWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	int32 CurrentLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	if (!bIsWalletHUDWidget)
	{
		return CurrentLayer;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 12.0 || Size.Y <= 12.0)
	{
		return CurrentLayer;
	}

	FVector2D IconCenter(Size.X - 10.0, Size.Y * 0.5);
	double HalfW = 8.5;
	double HalfH = 13.5;
	if (Image_CurrencyIcon)
	{
		const FGeometry& IconGeo = Image_CurrencyIcon->GetCachedGeometry();
		if (IconGeo.GetLocalSize().X > 4.0 && IconGeo.GetLocalSize().Y > 4.0)
		{
			const FVector2D TopLeft = AllottedGeometry.AbsoluteToLocal(IconGeo.GetAbsolutePosition());
			const FVector2D BottomRight = AllottedGeometry.AbsoluteToLocal(
				IconGeo.GetAbsolutePosition() + IconGeo.GetLocalSize() * IconGeo.Scale);
			IconCenter = (TopLeft + BottomRight) * 0.5;
			HalfW = FMath::Clamp((BottomRight.X - TopLeft.X) * 0.46, 6.0, 11.0);
			HalfH = FMath::Clamp((BottomRight.Y - TopLeft.Y) * 0.48, 10.0, 15.0);
		}
	}

	// Faceted tilted Breakaway gold crystal shard matching the reference HUD ("783 ◆")
	const FVector2D TopTip(IconCenter.X + 2.5, IconCenter.Y - HalfH);
	const FVector2D RightMid(IconCenter.X + HalfW, IconCenter.Y - 1.5);
	const FVector2D BottomTip(IconCenter.X - 2.5, IconCenter.Y + HalfH);
	const FVector2D LeftMid(IconCenter.X - HalfW, IconCenter.Y + 1.5);

	const FVector2D OuterShard[4] = { TopTip, RightMid, BottomTip, LeftMid };
	const FVector2D LeftLitFacet[3] = { TopTip, BottomTip, LeftMid };
	const FVector2D RightWarmFacet[3] = { TopTip, RightMid, BottomTip };

	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		LeftLitFacet,
		FLinearColor(1.0f, 0.84f, 0.24f, 1.0f));
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		CurrentLayer,
		RightWarmFacet,
		FLinearColor(0.92f, 0.58f, 0.08f, 1.0f));
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		OuterShard,
		FLinearColor(0.06f, 0.04f, 0.01f, 0.90f),
		1.25f,
		true);

	return CurrentLayer;
}

void UBwayCurrencyDisplayWidget::SetCanAfford(bool bCanAfford)
{
	if (bPlayerCanAfford != bCanAfford)
	{
		bPlayerCanAfford = bCanAfford;
		OnAffordabilityChanged(bPlayerCanAfford);
	}
}

FText UBwayCurrencyDisplayWidget::GetDisplayedCapText() const
{
	return Text_MaxGoldCap ? Text_MaxGoldCap->GetText() : FText::GetEmpty();
}

FText UBwayCurrencyDisplayWidget::GetDisplayedShopPromptText() const
{
	return Text_ShopPrompt ? Text_ShopPrompt->GetText() : FText::GetEmpty();
}
