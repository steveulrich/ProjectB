#include "UI/BwayMatchAbilitySlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchAbilitySlotWidget)

#define LOCTEXT_NAMESPACE "BwayMatchAbilitySlotWidget"

namespace
{
void ClipConvexPolygonY(
	const TConstArrayView<FVector2D> InPoly,
	const double MinY,
	const double MaxY,
	TArray<FVector2D>& OutPoly)
{
	OutPoly.Reset();
	if (InPoly.Num() < 3 || MaxY <= MinY + KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Pass 1: clip against Y >= MinY
	TArray<FVector2D> Stage1;
	Stage1.Reserve(InPoly.Num() + 2);
	for (int32 Idx = 0; Idx < InPoly.Num(); ++Idx)
	{
		const FVector2D& A = InPoly[Idx];
		const FVector2D& B = InPoly[(Idx + 1) % InPoly.Num()];
		const bool bAInside = (A.Y >= MinY);
		const bool bBInside = (B.Y >= MinY);
		if (bAInside && bBInside)
		{
			Stage1.Add(B);
		}
		else if (bAInside && !bBInside)
		{
			const double T = (MinY - A.Y) / (B.Y - A.Y);
			Stage1.Emplace(FMath::Lerp(A.X, B.X, T), MinY);
		}
		else if (!bAInside && bBInside)
		{
			const double T = (MinY - A.Y) / (B.Y - A.Y);
			Stage1.Emplace(FMath::Lerp(A.X, B.X, T), MinY);
			Stage1.Add(B);
		}
	}

	if (Stage1.Num() < 3)
	{
		return;
	}

	// Pass 2: clip against Y <= MaxY
	OutPoly.Reserve(Stage1.Num() + 2);
	for (int32 Idx = 0; Idx < Stage1.Num(); ++Idx)
	{
		const FVector2D& A = Stage1[Idx];
		const FVector2D& B = Stage1[(Idx + 1) % Stage1.Num()];
		const bool bAInside = (A.Y <= MaxY);
		const bool bBInside = (B.Y <= MaxY);
		if (bAInside && bBInside)
		{
			OutPoly.Add(B);
		}
		else if (bAInside && !bBInside)
		{
			const double T = (MaxY - A.Y) / (B.Y - A.Y);
			OutPoly.Emplace(FMath::Lerp(A.X, B.X, T), MaxY);
		}
		else if (!bAInside && bBInside)
		{
			const double T = (MaxY - A.Y) / (B.Y - A.Y);
			OutPoly.Emplace(FMath::Lerp(A.X, B.X, T), MaxY);
			OutPoly.Add(B);
		}
	}
}
} // namespace

void UBwayMatchAbilitySlotWidget::SetSlotViewModel(const FBwayMatchAbilitySlotViewModel& InViewModel)
{
	ViewModel = InViewModel;
	ApplyViewModel();
}

void UBwayMatchAbilitySlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureFallbackWidgets();
	ApplyViewModel();
}

void UBwayMatchAbilitySlotWidget::EnsureFallbackWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!WidgetTree->RootWidget)
	{
		USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSizeBox"));
		RootBox->SetWidthOverride(84.0f);
		RootBox->SetHeightOverride(98.0f);
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FallbackSlotOverlay"));
		Image_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Icon"));
		Image_DisabledOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_DisabledOverlay"));
		Image_CooldownOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_CooldownOverlay"));
		ProgressBar_Cooldown = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_Cooldown"));
		Image_RelicFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_RelicFrame"));
		Text_AbilityName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_AbilityName"));
		Text_Keybind = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Keybind"));
		Text_State = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_State"));
		Text_CooldownTimer = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_CooldownTimer"));

		Root->AddChildToOverlay(Image_Icon);
		Root->AddChildToOverlay(Image_DisabledOverlay);
		Root->AddChildToOverlay(Image_CooldownOverlay);
		Root->AddChildToOverlay(ProgressBar_Cooldown);
		Root->AddChildToOverlay(Image_RelicFrame);
		Root->AddChildToOverlay(Text_AbilityName);
		Root->AddChildToOverlay(Text_Keybind);
		Root->AddChildToOverlay(Text_State);
		Root->AddChildToOverlay(Text_CooldownTimer);
		Root->SetRenderOpacity(0.0f);
		RootBox->SetContent(Root);
		WidgetTree->RootWidget = RootBox;
		return;
	}

	// Suppress legacy square UMG slot art visually while preserving child widget state/visibility for queries and tests.
	WidgetTree->ForEachWidget([](UWidget* Candidate)
	{
		if (!Candidate)
		{
			return;
		}
		if (UBorder* Border = Cast<UBorder>(Candidate))
		{
			FSlateBrush NoBrush;
			NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			Border->SetBrush(NoBrush);
			Border->SetBrushColor(FLinearColor::Transparent);
		}
		else if (Cast<UOverlay>(Candidate) != nullptr
			|| Cast<UImage>(Candidate) != nullptr
			|| Cast<UProgressBar>(Candidate) != nullptr
			|| Cast<UTextBlock>(Candidate) != nullptr)
		{
			Candidate->SetRenderOpacity(0.0f);
		}
	});

	// W_BW_MatchAbilitySlot authors SlotOverlay + ProgressBar_Cooldown + Text_CooldownTimer
	// without an authored Image_CooldownOverlay child; construct and layer it under Text_CooldownTimer.
	if (!Image_CooldownOverlay)
	{
		UOverlay* TargetOverlay = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("SlotOverlay")));
		if (!TargetOverlay)
		{
			WidgetTree->ForEachWidget([&TargetOverlay](UWidget* Candidate)
			{
				if (!TargetOverlay)
				{
					TargetOverlay = Cast<UOverlay>(Candidate);
				}
			});
		}

		if (TargetOverlay)
		{
			Image_CooldownOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_CooldownOverlay"));
			if (UTexture2D* CooldownTex = LoadObject<UTexture2D>(
				nullptr, TEXT("/BreakawayCore/UI/Art/Match_HUD/hud_ability_cool_down.hud_ability_cool_down")))
			{
				Image_CooldownOverlay->SetBrushFromTexture(CooldownTex);
			}
			Image_CooldownOverlay->SetColorAndOpacity(FLinearColor(0.01f, 0.02f, 0.05f, 0.64f));
			Image_CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);

			if (UOverlaySlot* CooldownSlot = TargetOverlay->AddChildToOverlay(Image_CooldownOverlay))
			{
				CooldownSlot->SetHorizontalAlignment(HAlign_Fill);
				CooldownSlot->SetVerticalAlignment(VAlign_Fill);
			}

			// Re-append Text_CooldownTimer after Image_CooldownOverlay so countdown digits render crisp on top
			if (Text_CooldownTimer && Text_CooldownTimer->GetParent() == TargetOverlay)
			{
				EHorizontalAlignment HAlign = HAlign_Center;
				EVerticalAlignment VAlign = VAlign_Center;
				FMargin Pad(0.0f);
				if (const UOverlaySlot* ExistingTimerSlot = Cast<UOverlaySlot>(Text_CooldownTimer->Slot))
				{
					HAlign = ExistingTimerSlot->GetHorizontalAlignment();
					VAlign = ExistingTimerSlot->GetVerticalAlignment();
					Pad = ExistingTimerSlot->GetPadding();
				}
				Text_CooldownTimer->RemoveFromParent();
				if (UOverlaySlot* RestoredTimerSlot = TargetOverlay->AddChildToOverlay(Text_CooldownTimer))
				{
					RestoredTimerSlot->SetHorizontalAlignment(HAlign);
					RestoredTimerSlot->SetVerticalAlignment(VAlign);
					RestoredTimerSlot->SetPadding(Pad);
				}
			}
		}
	}
}

void UBwayMatchAbilitySlotWidget::ApplyViewModel()
{
	EnsureFallbackWidgets();

	if (WidgetTree)
	{
		USizeBox* RootSizeBox = WidgetTree->FindWidget<USizeBox>(TEXT("RootSizeBox"));
		if (!RootSizeBox)
		{
			RootSizeBox = Cast<USizeBox>(WidgetTree->RootWidget);
		}
		if (RootSizeBox)
		{
			if (ViewModel.Position == 5)
			{
				// Larger center Ultimate ("R") hexagon
				RootSizeBox->SetWidthOverride(98.0f);
				RootSizeBox->SetHeightOverride(112.0f);
			}
			else if (ViewModel.bIsBuildable || ViewModel.Position == 0)
			{
				// Recessed left Buildable ("1") hexagon
				RootSizeBox->SetWidthOverride(78.0f);
				RootSizeBox->SetHeightOverride(92.0f);
			}
			else
			{
				// Standard Combat ("[Mouse]", "F", "Q", "E") hexagons
				RootSizeBox->SetWidthOverride(84.0f);
				RootSizeBox->SetHeightOverride(98.0f);
			}
		}
	}

	const bool bDisabled = !ViewModel.bIsAvailable || ViewModel.bIsConsumed;
	const bool bOnCooldown = ViewModel.bIsOnCooldown && !ViewModel.bIsConsumed;

	if (Image_Icon)
	{
		UTexture2D* IconTexture = ViewModel.DisplayInfo.Icon.LoadSynchronous();
		Image_Icon->SetBrushFromTexture(IconTexture);

		const FLinearColor IconTint = (bDisabled || bOnCooldown)
			? FLinearColor(0.24f, 0.24f, 0.28f, 1.0f)
			: FLinearColor::White;
		Image_Icon->SetColorAndOpacity(IconTint);
	}

	if (Image_DisabledOverlay)
	{
		if (UOverlaySlot* DisabledSlot = Cast<UOverlaySlot>(Image_DisabledOverlay->Slot))
		{
			DisabledSlot->SetHorizontalAlignment(HAlign_Fill);
			DisabledSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (Image_DisabledOverlay->GetBrush().GetResourceObject() == nullptr)
		{
			Image_DisabledOverlay->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		}
		if (Image_DisabledOverlay->GetColorAndOpacity().A <= KINDA_SMALL_NUMBER)
		{
			Image_DisabledOverlay->SetColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.04f, 0.58f));
		}
		Image_DisabledOverlay->SetVisibility(bDisabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Image_CooldownOverlay)
	{
		if (UOverlaySlot* CooldownSlot = Cast<UOverlaySlot>(Image_CooldownOverlay->Slot))
		{
			CooldownSlot->SetHorizontalAlignment(HAlign_Fill);
			CooldownSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (Image_CooldownOverlay->GetBrush().GetResourceObject() == nullptr)
		{
			Image_CooldownOverlay->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		}
		if (Image_CooldownOverlay->GetColorAndOpacity().A <= KINDA_SMALL_NUMBER)
		{
			Image_CooldownOverlay->SetColorAndOpacity(FLinearColor(0.01f, 0.02f, 0.05f, 0.68f));
		}
		Image_CooldownOverlay->SetVisibility(bOnCooldown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ProgressBar_Cooldown)
	{
		if (bOnCooldown)
		{
			ProgressBar_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
			ProgressBar_Cooldown->SetPercent(FMath::Clamp(ViewModel.CooldownPercent, 0.f, 1.f));
		}
		else
		{
			ProgressBar_Cooldown->SetPercent(0.f);
			ProgressBar_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Image_RelicFrame)
	{
		if (UOverlaySlot* RelicFrameSlot = Cast<UOverlaySlot>(Image_RelicFrame->Slot))
		{
			RelicFrameSlot->SetHorizontalAlignment(HAlign_Fill);
			RelicFrameSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (Image_RelicFrame->GetBrush().GetResourceObject() == nullptr)
		{
			if (UMaterialInterface* FocusBorderMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/BreakawayCore/UI/Materials/M_UI_FocusBorder.M_UI_FocusBorder")))
			{
				FSlateBrush BorderBrush = Image_RelicFrame->GetBrush();
				BorderBrush.SetResourceObject(FocusBorderMat);
				BorderBrush.DrawAs = ESlateBrushDrawType::Box;
				BorderBrush.Margin = FMargin(0.25f);
				Image_RelicFrame->SetBrush(BorderBrush);
			}
			else
			{
				Image_RelicFrame->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
			}
		}
		if (ViewModel.bIsRelicAbility)
		{
			Image_RelicFrame->SetColorAndOpacity(
				ViewModel.bIsAvailable
					? FLinearColor(0.96f, 0.78f, 0.18f, 0.95f)
					: FLinearColor(0.76f, 0.48f, 0.14f, 0.85f));
			Image_RelicFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_RelicFrame->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Text_AbilityName)
	{
		Text_AbilityName->SetText(
			ViewModel.DisplayInfo.AbilityName.IsEmpty()
				? LOCTEXT("UnresolvedAbility", "Unresolved")
				: ViewModel.DisplayInfo.AbilityName);
	}

	if (Text_Keybind)
	{
		Text_Keybind->SetText(ViewModel.KeyLabel);
	}

	if (Text_State)
	{
		FSlateFontInfo StateFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
		StateFont.OutlineSettings.OutlineSize = 1;
		StateFont.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.94f);
		Text_State->SetFont(StateFont);
		Text_State->SetJustification(ETextJustify::Center);

		FText StateText;
		FLinearColor StateColor(0.92f, 0.94f, 0.98f, 0.96f);

		if (ViewModel.bIsBuildable)
		{
			const bool bHasQuotaMax = ViewModel.MaxActiveBuildables > 0;
			const FString StructureRatio = bHasQuotaMax
				? FString::Printf(TEXT("%d/%d"), FMath::Max(0, ViewModel.ActiveBuildableCount), ViewModel.MaxActiveBuildables)
				: FString();

			if (ViewModel.bIsConsumed)
			{
				StateText = FText::FromString(
					bHasQuotaMax
						? FString::Printf(TEXT("Placed\n(%s)"), *StructureRatio)
						: TEXT("Placed"));
				StateColor = FLinearColor(0.92f, 0.66f, 0.26f, 0.98f);
			}
			else if (ViewModel.bIsAvailable)
			{
				StateText = FText::FromString(
					bHasQuotaMax
						? FString::Printf(TEXT("1/Round\n%s"), *StructureRatio)
						: TEXT("1/Round"));
				StateColor = FLinearColor(0.45f, 0.92f, 0.65f, 0.98f);
			}
			else
			{
				StateText = LOCTEXT("AbilityUnavailable", "—");
			}
		}
		else if (ViewModel.bIsRelicAbility)
		{
			if (!ViewModel.bIsAvailable)
			{
				StateText = LOCTEXT("RelicCarrierLock", "RELIC\nLOCK");
				StateColor = FLinearColor(0.96f, 0.62f, 0.20f, 0.98f);
			}
			else
			{
				StateText = LOCTEXT("RelicControlBadge", "RELIC");
				StateColor = FLinearColor(0.98f, 0.84f, 0.25f, 0.98f);
			}
		}
		else if (!ViewModel.bIsAvailable)
		{
			StateText = LOCTEXT("AbilityUnavailable", "—");
		}

		Text_State->SetText(StateText);
		Text_State->SetColorAndOpacity(FSlateColor(StateColor));
		Text_State->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text_State->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f));
		Text_State->SetVisibility(StateText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Text_CooldownTimer)
	{
		if (bOnCooldown)
		{
			FText CooldownText;
			if (ViewModel.CountdownTime < 1.0f)
			{
				const float ClampedTenths = FMath::Max(0.1f, ViewModel.CountdownTime);
				CooldownText = FText::FromString(FString::Printf(TEXT("%.1f"), ClampedTenths));
			}
			else
			{
				const int32 SecondsRemaining = FMath::Max(1, FMath::CeilToInt(ViewModel.CountdownTime));
				CooldownText = FText::AsNumber(SecondsRemaining);
			}

			Text_CooldownTimer->SetText(CooldownText);
			Text_CooldownTimer->SetShadowOffset(FVector2D(1.25f, 1.25f));
			Text_CooldownTimer->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f));
			Text_CooldownTimer->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_CooldownTimer->SetText(FText::GetEmpty());
			Text_CooldownTimer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	InvalidateLayoutAndVolatility();
	BP_OnSlotViewModelChanged(ViewModel);
}

int32 UBwayMatchAbilitySlotWidget::NativePaint(
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

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 16.0f || Size.Y <= 24.0f)
	{
		return CurrentLayer;
	}

	const double W = Size.X;
	const double H = Size.Y;
	const double MidX = W * 0.5;
	const double TopY = 2.0;
	const double HexH = FMath::Max(24.0, H - 13.0);

	const bool bDisabled = !ViewModel.bIsAvailable || ViewModel.bIsConsumed;
	const bool bOnCooldown = ViewModel.bIsOnCooldown && !ViewModel.bIsConsumed;
	const bool bIsUltimate = (ViewModel.Position == 5);
	const bool bIsBuildable = ViewModel.bIsBuildable || (ViewModel.Position == 0);

	// Pointy-top hexagon vertices
	const FVector2D HexShadow[6] = {
		FVector2D(MidX, TopY - 1.2),
		FVector2D(W - 0.8, TopY + HexH * 0.25),
		FVector2D(W - 0.8, TopY + HexH * 0.75),
		FVector2D(MidX, TopY + HexH + 1.4),
		FVector2D(0.8, TopY + HexH * 0.75),
		FVector2D(0.8, TopY + HexH * 0.25),
	};
	const FVector2D Hex[6] = {
		FVector2D(MidX, TopY),
		FVector2D(W - 2.0, TopY + HexH * 0.25),
		FVector2D(W - 2.0, TopY + HexH * 0.75),
		FVector2D(MidX, TopY + HexH),
		FVector2D(2.0, TopY + HexH * 0.75),
		FVector2D(2.0, TopY + HexH * 0.25),
	};
	const FVector2D HexInset[6] = {
		FVector2D(MidX, TopY + 2.5),
		FVector2D(W - 4.2, TopY + HexH * 0.25 + 1.2),
		FVector2D(W - 4.2, TopY + HexH * 0.75 - 1.2),
		FVector2D(MidX, TopY + HexH - 2.5),
		FVector2D(4.2, TopY + HexH * 0.75 - 1.2),
		FVector2D(4.2, TopY + HexH * 0.25 + 1.2),
	};

	// 1. Outer drop shadow + dark charcoal hexagon interior
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		HexShadow,
		FLinearColor(0.01f, 0.02f, 0.03f, 0.88f));

	const FLinearColor BaseHexFill = bIsBuildable
		? FLinearColor(0.05f, 0.05f, 0.06f, 0.94f)
		: (bIsUltimate ? FLinearColor(0.10f, 0.12f, 0.15f, 0.96f) : FLinearColor(0.08f, 0.10f, 0.13f, 0.95f));
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		Hex,
		BaseHexFill);

	// Subtle lower-half metallic sheen inside ready hexagons
	if (!bDisabled && !bOnCooldown)
	{
		TArray<FVector2D> LowerHalfPoly;
		ClipConvexPolygonY(Hex, TopY + HexH * 0.50, TopY + HexH, LowerHalfPoly);
		if (LowerHalfPoly.Num() >= 3)
		{
			const FLinearColor SheenColor = (ViewModel.bIsRelicAbility || bIsUltimate)
				? FLinearColor(0.38f, 0.30f, 0.12f, 0.26f)
				: (bIsBuildable ? FLinearColor(0.20f, 0.16f, 0.10f, 0.18f) : FLinearColor(0.18f, 0.22f, 0.27f, 0.28f));
			UBwayMatchHUDWidgetBase::DrawConvexPolygon(
				AllottedGeometry,
				OutDrawElements,
				++CurrentLayer,
				LowerHalfPoly,
				SheenColor);
		}
	}

	// 2. Centered Ability Icon (or clean golden-amber placeholder emblem if unauthored)
	const double IconSide = bIsUltimate ? 58.0 : (bIsBuildable ? 42.0 : 48.0);
	const FVector2D IconCenter(MidX, TopY + HexH * 0.47);
	const FVector2D IconOffset(IconCenter.X - IconSide * 0.5, IconCenter.Y - IconSide * 0.5);
	const FSlateBrush* IconBrush = (Image_Icon && Image_Icon->GetBrush().GetResourceObject() != nullptr)
		? &Image_Icon->GetBrush()
		: nullptr;

	if (IconBrush)
	{
		const FLinearColor IconTint = (bDisabled || bOnCooldown)
			? FLinearColor(0.28f, 0.30f, 0.35f, 0.92f)
			: (bIsBuildable ? FLinearColor(0.76f, 0.58f, 0.26f, 0.94f) : FLinearColor::White);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			static_cast<uint32>(++CurrentLayer),
			AllottedGeometry.ToPaintGeometry(
				FVector2f(static_cast<float>(IconSide), static_cast<float>(IconSide)),
				FSlateLayoutTransform(FVector2f(static_cast<float>(IconOffset.X), static_cast<float>(IconOffset.Y)))),
			IconBrush,
			ESlateDrawEffect::None,
			IconTint);
	}
	else
	{
		const double EmblemR = IconSide * 0.34;
		const FVector2D EmblemPoly[4] = {
			IconCenter + FVector2D(0.0, -EmblemR),
			IconCenter + FVector2D(EmblemR * 0.85, 0.0),
			IconCenter + FVector2D(0.0, EmblemR),
			IconCenter + FVector2D(-EmblemR * 0.85, 0.0),
		};
		const FLinearColor EmblemTint = (bDisabled || bOnCooldown)
			? FLinearColor(0.26f, 0.27f, 0.31f, 0.82f)
			: (bIsBuildable ? FLinearColor(0.56f, 0.42f, 0.18f, 0.88f) : FLinearColor(0.95f, 0.72f, 0.16f, 0.94f));
		UBwayMatchHUDWidgetBase::DrawConvexPolygon(AllottedGeometry, OutDrawElements, ++CurrentLayer, EmblemPoly, EmblemTint);
	}

	// 3. Hexagon-clipped disabled veil & bottom-to-top cooldown sweep
	if (bDisabled)
	{
		UBwayMatchHUDWidgetBase::DrawConvexPolygon(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			Hex,
			FLinearColor(0.02f, 0.02f, 0.04f, 0.52f));
	}

	if (bOnCooldown)
	{
		UBwayMatchHUDWidgetBase::DrawConvexPolygon(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			Hex,
			FLinearColor(0.01f, 0.02f, 0.05f, 0.64f));

		const double ClampedPct = FMath::Clamp(static_cast<double>(ViewModel.CooldownPercent), 0.0, 1.0);
		if (ClampedPct > KINDA_SMALL_NUMBER)
		{
			const double SweepTopY = TopY + HexH * (1.0 - ClampedPct);
			TArray<FVector2D> SweepPoly;
			ClipConvexPolygonY(Hex, SweepTopY, TopY + HexH, SweepPoly);
			if (SweepPoly.Num() >= 3)
			{
				UBwayMatchHUDWidgetBase::DrawConvexPolygon(
					AllottedGeometry,
					OutDrawElements,
					++CurrentLayer,
					SweepPoly,
					FLinearColor(0.14f, 0.78f, 0.94f, 0.38f));
			}
		}
	}

	// 4. Double-bevel metallic / Relic-gold hexagon border
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		HexInset,
		FLinearColor(0.02f, 0.03f, 0.05f, 0.82f),
		1.0f,
		true);

	FLinearColor HexBorderColor(0.56f, 0.61f, 0.68f, 0.94f);
	float HexBorderThickness = 1.8f;
	if (ViewModel.bIsRelicAbility)
	{
		HexBorderColor = ViewModel.bIsAvailable
			? FLinearColor(0.96f, 0.78f, 0.20f, 0.98f)
			: FLinearColor(0.74f, 0.48f, 0.16f, 0.88f);
		HexBorderThickness = 2.2f;
	}
	else if (bIsUltimate && !bDisabled)
	{
		HexBorderColor = FLinearColor(0.76f, 0.73f, 0.62f, 0.96f);
		HexBorderThickness = 2.1f;
	}
	else if (bIsBuildable)
	{
		HexBorderColor = FLinearColor(0.34f, 0.32f, 0.29f, 0.86f);
		HexBorderThickness = 1.6f;
	}
	else if (bDisabled)
	{
		HexBorderColor = FLinearColor(0.32f, 0.35f, 0.40f, 0.80f);
	}

	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		Hex,
		HexBorderColor,
		HexBorderThickness,
		true);

	// 5. Cooldown countdown digits centered over hexagon (or subtle buildable active count pip when > 0)
	if (bOnCooldown)
	{
		const FString CooldownStr = GetDisplayedCooldownText().ToString();
		if (!CooldownStr.IsEmpty())
		{
			const FSlateFontInfo TimerFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(24, 0, 2);
			const FVector2D TimerSize = UBwayMatchHUDWidgetBase::MeasureBroadcastText(CooldownStr, TimerFont);
			const FVector2D TimerPos(MidX - TimerSize.X * 0.5, TopY + HexH * 0.47 - TimerSize.Y * 0.5);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				static_cast<uint32>(++CurrentLayer),
				AllottedGeometry.ToPaintGeometry(
					FVector2f(static_cast<float>(TimerSize.X + 4.0), static_cast<float>(TimerSize.Y + 2.0)),
					FSlateLayoutTransform(FVector2f(static_cast<float>(TimerPos.X), static_cast<float>(TimerPos.Y)))),
				CooldownStr,
				TimerFont,
				ESlateDrawEffect::None,
				FLinearColor::White);
		}
	}
	else if (bIsBuildable && ViewModel.MaxActiveBuildables > 0 && ViewModel.ActiveBuildableCount > 0)
	{
		const FString RatioStr = FString::Printf(TEXT("%d/%d"), ViewModel.ActiveBuildableCount, ViewModel.MaxActiveBuildables);
		const FSlateFontInfo RatioFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(11, 0, 1);
		const FVector2D RatioSize = UBwayMatchHUDWidgetBase::MeasureBroadcastText(RatioStr, RatioFont);
		const FVector2D RatioPos(MidX - RatioSize.X * 0.5, TopY + 9.0);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			static_cast<uint32>(++CurrentLayer),
			AllottedGeometry.ToPaintGeometry(
				FVector2f(static_cast<float>(RatioSize.X + 2.0), static_cast<float>(RatioSize.Y + 2.0)),
				FSlateLayoutTransform(FVector2f(static_cast<float>(RatioPos.X), static_cast<float>(RatioPos.Y)))),
			RatioStr,
			RatioFont,
			ESlateDrawEffect::None,
			FLinearColor(0.86f, 0.78f, 0.54f, 0.90f));
	}

	// 6. Bottom-vertex input prompt badge: 2-button Mouse icon for LMB/RMB, or crisp dark keycap for 1 / F / R / Q / E / Gamepad
	const FString KeyStr = GetDisplayedKeybindText().ToString().TrimStartAndEnd();
	const FVector2D BadgeCenter(MidX, H - 11.0);
	const bool bIsLMB = KeyStr.Equals(TEXT("LMB"), ESearchCase::IgnoreCase);
	const bool bIsRMB = KeyStr.Equals(TEXT("RMB"), ESearchCase::IgnoreCase);

	if (bIsLMB || bIsRMB)
	{
		const double HalfW = 7.0;
		const double HalfH = 10.0;
		const FVector2D MouseOuter[8] = {
			BadgeCenter + FVector2D(-HalfW + 2.5, -HalfH - 1.0),
			BadgeCenter + FVector2D(HalfW - 2.5, -HalfH - 1.0),
			BadgeCenter + FVector2D(HalfW + 1.0, -HalfH + 3.0),
			BadgeCenter + FVector2D(HalfW + 1.0, HalfH - 3.0),
			BadgeCenter + FVector2D(HalfW - 2.5, HalfH + 1.0),
			BadgeCenter + FVector2D(-HalfW + 2.5, HalfH + 1.0),
			BadgeCenter + FVector2D(-HalfW - 1.0, HalfH - 3.0),
			BadgeCenter + FVector2D(-HalfW - 1.0, -HalfH + 3.0),
		};
		const FVector2D MouseBody[8] = {
			BadgeCenter + FVector2D(-HalfW + 2.2, -HalfH),
			BadgeCenter + FVector2D(HalfW - 2.2, -HalfH),
			BadgeCenter + FVector2D(HalfW, -HalfH + 2.6),
			BadgeCenter + FVector2D(HalfW, HalfH - 2.6),
			BadgeCenter + FVector2D(HalfW - 2.2, HalfH),
			BadgeCenter + FVector2D(-HalfW + 2.2, HalfH),
			BadgeCenter + FVector2D(-HalfW, HalfH - 2.6),
			BadgeCenter + FVector2D(-HalfW, -HalfH + 2.6),
		};

		UBwayMatchHUDWidgetBase::DrawConvexPolygon(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			MouseOuter,
			FLinearColor(0.03f, 0.04f, 0.06f, 0.96f));
		UBwayMatchHUDWidgetBase::DrawConvexPolygon(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			MouseBody,
			FLinearColor(0.56f, 0.60f, 0.66f, 0.98f));

		// Highlighted active mouse button (upper-left for LMB, upper-right for RMB)
		if (bIsLMB)
		{
			const FVector2D LeftBtn[5] = {
				BadgeCenter + FVector2D(-HalfW + 2.2, -HalfH),
				BadgeCenter + FVector2D(-0.8, -HalfH),
				BadgeCenter + FVector2D(-0.8, -1.0),
				BadgeCenter + FVector2D(-HalfW, -1.0),
				BadgeCenter + FVector2D(-HalfW, -HalfH + 2.6),
			};
			UBwayMatchHUDWidgetBase::DrawConvexPolygon(AllottedGeometry, OutDrawElements, ++CurrentLayer, LeftBtn, FLinearColor::White);
		}
		else
		{
			const FVector2D RightBtn[5] = {
				BadgeCenter + FVector2D(0.8, -HalfH),
				BadgeCenter + FVector2D(HalfW - 2.2, -HalfH),
				BadgeCenter + FVector2D(HalfW, -HalfH + 2.6),
				BadgeCenter + FVector2D(HalfW, -1.0),
				BadgeCenter + FVector2D(0.8, -1.0),
			};
			UBwayMatchHUDWidgetBase::DrawConvexPolygon(AllottedGeometry, OutDrawElements, ++CurrentLayer, RightBtn, FLinearColor::White);
		}

		// Dark cross seam separating left button, right button, and palm body
		UBwayMatchHUDWidgetBase::DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			BadgeCenter + FVector2D(-HalfW, -1.2),
			FVector2D(HalfW * 2.0, 1.6),
			FLinearColor(0.06f, 0.07f, 0.10f, 0.95f));
		UBwayMatchHUDWidgetBase::DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			CurrentLayer,
			BadgeCenter + FVector2D(-0.8, -HalfH),
			FVector2D(1.6, HalfH - 0.5),
			FLinearColor(0.06f, 0.07f, 0.10f, 0.95f));
	}
	else if (!KeyStr.IsEmpty())
	{
		const FSlateFontInfo KeyFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(15, 2, 1);
		const FVector2D KeyTextSize = UBwayMatchHUDWidgetBase::MeasureBroadcastText(KeyStr, KeyFont);
		const double CapH = 21.0;
		const double CapW = FMath::Max(21.0, KeyTextSize.X + 10.0);
		const FVector2D CapOffset(BadgeCenter.X - CapW * 0.5, BadgeCenter.Y - CapH * 0.5);

		UBwayMatchHUDWidgetBase::DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			CapOffset,
			FVector2D(CapW, CapH),
			FLinearColor(0.04f, 0.05f, 0.07f, 0.98f));

		const FVector2D CapOutline[4] = {
			CapOffset,
			CapOffset + FVector2D(CapW, 0.0),
			CapOffset + FVector2D(CapW, CapH),
			CapOffset + FVector2D(0.0, CapH),
		};
		UBwayMatchHUDWidgetBase::DrawPolygonOutline(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			CapOutline,
			FLinearColor(0.46f, 0.50f, 0.56f, 0.92f),
			1.2f,
			true);

		const FVector2D KeyPos(
			BadgeCenter.X - KeyTextSize.X * 0.5,
			BadgeCenter.Y - KeyTextSize.Y * 0.5 - 0.5);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			static_cast<uint32>(++CurrentLayer),
			AllottedGeometry.ToPaintGeometry(
				FVector2f(static_cast<float>(KeyTextSize.X + 4.0), static_cast<float>(KeyTextSize.Y + 2.0)),
				FSlateLayoutTransform(FVector2f(static_cast<float>(KeyPos.X), static_cast<float>(KeyPos.Y)))),
			KeyStr,
			KeyFont,
			ESlateDrawEffect::None,
			FLinearColor::White);
	}

	return CurrentLayer;
}

FText UBwayMatchAbilitySlotWidget::GetDisplayedCooldownText() const
{
	return Text_CooldownTimer ? Text_CooldownTimer->GetText() : FText::GetEmpty();
}

FText UBwayMatchAbilitySlotWidget::GetDisplayedStateText() const
{
	return Text_State ? Text_State->GetText() : FText::GetEmpty();
}

FText UBwayMatchAbilitySlotWidget::GetDisplayedKeybindText() const
{
	return Text_Keybind ? Text_Keybind->GetText() : ViewModel.KeyLabel;
}

FText UBwayMatchAbilitySlotWidget::GetDisplayedAbilityNameText() const
{
	return Text_AbilityName ? Text_AbilityName->GetText() : ViewModel.DisplayInfo.AbilityName;
}

bool UBwayMatchAbilitySlotWidget::IsCooldownOverlayVisible() const
{
	return Image_CooldownOverlay && Image_CooldownOverlay->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UBwayMatchAbilitySlotWidget::IsRelicFrameVisible() const
{
	return Image_RelicFrame && Image_RelicFrame->GetVisibility() != ESlateVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
