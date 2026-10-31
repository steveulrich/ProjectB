// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayMatchBreakdownWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "CommonButtonBase.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Texture2D.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "Styling/CoreStyle.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "UI/Foundation/LyraButtonBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchBreakdownWidget)

namespace
{
FString FormatUpgradeRomanRank(const int32 Rank)
{
	switch (Rank)
	{
	case 1:
		return TEXT("I");
	case 2:
		return TEXT("II");
	case 3:
		return TEXT("III");
	default:
		return FString::FromInt(Rank);
	}
}
} // namespace

UBwayMatchBreakdownWidget::UBwayMatchBreakdownWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UBwayMatchBreakdownWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateRoleActionPermissions();
}

void UBwayMatchBreakdownWidget::SetRoleOverrideForTest(const bool bEnableOverride, const bool bSimulateHost)
{
	bTestOverrideRole = bEnableOverride;
	bTestSimulateHost = bSimulateHost;
	UpdateRoleActionPermissions();
}

bool UBwayMatchBreakdownWidget::IsHostBreakdownRole() const
{
	if (bTestOverrideRole)
	{
		return bTestSimulateHost;
	}

	const UWorld* World = GetWorld();
	return !World || World->GetNetMode() != NM_Client;
}

UCommonButtonBase* UBwayMatchBreakdownWidget::FindPlayAgainButton() const
{
	return WidgetTree ? Cast<UCommonButtonBase>(WidgetTree->FindWidget(TEXT("Btn_PlayAgain"))) : nullptr;
}

UWidget* UBwayMatchBreakdownWidget::NativeGetDesiredFocusTarget() const
{
	if (UCommonButtonBase* PlayAgainBtn = FindPlayAgainButton())
	{
		if (PlayAgainBtn->GetIsEnabled() && PlayAgainBtn->IsVisible())
		{
			return PlayAgainBtn;
		}
	}

	if (Btn_ReturnToLobby && Btn_ReturnToLobby->GetIsEnabled() && Btn_ReturnToLobby->IsVisible())
	{
		return Btn_ReturnToLobby;
	}

	return Super::NativeGetDesiredFocusTarget();
}

const UBwayUpgradeCatalog* UBwayMatchBreakdownWidget::ResolveUpgradeCatalog() const
{
	if (CachedUpgradeCatalog)
	{
		return CachedUpgradeCatalog;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const ABwayGameState* GS = World->GetGameState<ABwayGameState>())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
				{
					if (BwayPS->UpgradeComponent && BwayPS->UpgradeComponent->GetCatalog())
					{
						CachedUpgradeCatalog = BwayPS->UpgradeComponent->GetCatalog();
						return CachedUpgradeCatalog;
					}
				}
			}
		}
	}

	CachedUpgradeCatalog = LoadObject<UBwayUpgradeCatalog>(
		nullptr,
		TEXT("/BreakawayCore/Economy/DA_BW_MatchUpgrades.DA_BW_MatchUpgrades"));
	return CachedUpgradeCatalog;
}

void UBwayMatchBreakdownWidget::UpdateRoleActionPermissions()
{
	const bool bIsHost = IsHostBreakdownRole();

	if (UCommonButtonBase* PlayAgainBtn = FindPlayAgainButton())
	{
		PlayAgainBtn->SetIsEnabled(bIsHost);
		PlayAgainBtn->SetVisibility(ESlateVisibility::Visible);
		if (ULyraButtonBase* LyraPlayAgain = Cast<ULyraButtonBase>(PlayAgainBtn))
		{
			LyraPlayAgain->SetButtonText(
				bIsHost
					? NSLOCTEXT("PostMatch", "RematchHost", "REMATCH")
					: NSLOCTEXT("PostMatch", "RematchHostOnly", "HOST ONLY: REMATCH"));
		}
	}

	if (Btn_ReturnToLobby)
	{
		Btn_ReturnToLobby->SetIsEnabled(true);
		Btn_ReturnToLobby->SetVisibility(ESlateVisibility::Visible);
		if (ULyraButtonBase* LyraReturnBtn = Cast<ULyraButtonBase>(Btn_ReturnToLobby.Get()))
		{
			LyraReturnBtn->SetButtonText(
				bIsHost
					? NSLOCTEXT("PostMatch", "ReturnToLobbyHost", "RETURN TO LOBBY")
					: NSLOCTEXT("PostMatch", "LeaveToLobbyClient", "LEAVE TO LOBBY"));
		}
	}
}

void UBwayMatchBreakdownWidget::ApplyBreakdownData(FBwayPostMatchSummaryData SummaryData)
{
	for (FBwayMatchBreakdownPlayerColumn& Column : SummaryData.PlayerColumns)
	{
		if (!Column.HeroPortrait && Column.HeroId.IsValid())
		{
			if (const UBwayHeroDataAsset* Hero = UBwayHeroRegistry::GetHeroDataById(Column.HeroId))
			{
				Column.HeroPortrait = Hero->Portrait;
				if (Column.HeroName.IsEmpty())
				{
					Column.HeroName = Hero->DisplayName;
				}
			}
		}
	}

	const int32 LocalTeam = SummaryData.LocalPlayerTeamIndex;
	CachedSummary = UBwayMatchStatsLibrary::RemapPostMatchSummaryForDisplay(SummaryData, LocalTeam);
	OnBreakdownReady(CachedSummary);
	PolishBreakdownLayoutAndColumns();
	UpdateRoleActionPermissions();
}

void UBwayMatchBreakdownWidget::PolishBreakdownLayoutAndColumns()
{
	if (!WidgetTree)
	{
		return;
	}

	if (UImage* Backdrop = Cast<UImage>(WidgetTree->FindWidget(TEXT("DimOverlay"))))
	{
		if (UCanvasPanelSlot* BackdropSlot = Cast<UCanvasPanelSlot>(Backdrop->Slot))
		{
			BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BackdropSlot->SetOffsets(FMargin(0.0f));
		}
		if (Backdrop->GetBrush().GetResourceObject() == nullptr)
		{
			Backdrop->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		}
		Backdrop->SetColorAndOpacity(FLinearColor(0.01f, 0.02f, 0.04f, 0.78f));
	}

	if (UBorder* PanelBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("PanelBorder"))))
	{
		PanelBorder->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.95f));
	}

	// Expand center panel from 1200x720 to 1720x850 so all 8 columns fit simultaneously at 1920x1080 without clipping.
	if (USizeBox* CenterPanelSize = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("CenterPanelSize"))))
	{
		CenterPanelSize->SetWidthOverride(1720.0f);
		CenterPanelSize->SetHeightOverride(850.0f);
	}

	if (UTextBlock* HeaderText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Header"))))
	{
		const FString OutcomeWord = (CachedSummary.LocalPlayerTeamIndex == INDEX_NONE)
			? TEXT("MATCH COMPLETE")
			: (CachedSummary.bLocalPlayerWon ? TEXT("VICTORY") : TEXT("DEFEAT"));
		const FString MVPString = CachedSummary.MVPPlayerName.IsEmpty()
			? TEXT("")
			: FString::Printf(
				TEXT("  •  ★ MVP: %s (%s)"),
				*CachedSummary.MVPPlayerName.ToString(),
				*CachedSummary.MVPHeroName.ToString());

		HeaderText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  •  SERIES %d - %d%s"),
			*OutcomeWord,
			CachedSummary.Team0Score,
			CachedSummary.Team1Score,
			*MVPString)));
		HeaderText->SetColorAndOpacity(FSlateColor(
			CachedSummary.bLocalPlayerWon
				? FLinearColor(0.36f, 0.80f, 1.0f, 1.0f)
				: FLinearColor(1.0f, 0.42f, 0.38f, 1.0f)));
	}

	// Ensure UPGRADES row label exists in StatLabelsColumn aligned with the bottom upgrades row on each player column.
	if (UVerticalBox* StatLabelsColumn = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("StatLabelsColumn"))))
	{
		if (!WidgetTree->FindWidget(TEXT("Row_Label_Upgrades")))
		{
			UHorizontalBox* UpgradesLabelRow = WidgetTree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(),
				TEXT("Row_Label_Upgrades"));
			UTextBlock* UpgradesLabelText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				TEXT("Text_Label_Upgrades"));

			if (const UTextBlock* ExistingLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Label_Buildables"))))
			{
				UpgradesLabelText->SetFont(ExistingLabel->GetFont());
				UpgradesLabelText->SetColorAndOpacity(ExistingLabel->GetColorAndOpacity());
			}
			else
			{
				FSlateFontInfo Font = UpgradesLabelText->GetFont();
				Font.Size = 16;
				UpgradesLabelText->SetFont(Font);
				UpgradesLabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.88f, 0.96f, 0.95f)));
			}

			UpgradesLabelText->SetText(NSLOCTEXT("PostMatch", "UpgradesRowLabel", "Upgrades"));
			UpgradesLabelRow->AddChildToHorizontalBox(UpgradesLabelText);

			if (UVerticalBoxSlot* VSlot = StatLabelsColumn->AddChildToVerticalBox(UpgradesLabelRow))
			{
				VSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
			}
		}
	}

	if (UHorizontalBox* ColumnsHost = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("PlayerColumnsHost"))))
	{
		const TArray<UWidget*> ColumnWidgets = ColumnsHost->GetAllChildren();
		const int32 PairCount = FMath::Min(ColumnWidgets.Num(), CachedSummary.PlayerColumns.Num());
		for (int32 Idx = 0; Idx < PairCount; ++Idx)
		{
			if (UHorizontalBoxSlot* HostSlot = Cast<UHorizontalBoxSlot>(ColumnWidgets[Idx] ? ColumnWidgets[Idx]->Slot : nullptr))
			{
				HostSlot->SetPadding(FMargin(2.0f, 0.0f));
			}
			if (UUserWidget* ColumnUserWidget = Cast<UUserWidget>(ColumnWidgets[Idx]))
			{
				DecoratePlayerColumnWidget(ColumnUserWidget, CachedSummary.PlayerColumns[Idx]);
			}
		}
	}
}

void UBwayMatchBreakdownWidget::DecoratePlayerColumnWidget(
	UUserWidget* ColumnWidget,
	const FBwayMatchBreakdownPlayerColumn& ColumnData)
{
	if (!ColumnWidget || !ColumnWidget->WidgetTree)
	{
		return;
	}

	UWidgetTree* ColTree = ColumnWidget->WidgetTree;
	if (USizeBox* RootColSize = Cast<USizeBox>(ColTree->FindWidget(TEXT("SizeBox_397"))))
	{
		RootColSize->SetWidthOverride(172.0f);
	}

	if (UBorder* ColumnBorder = Cast<UBorder>(ColTree->FindWidget(TEXT("ColumnBorder_MVPTint"))))
	{
		FLinearColor ColTint = (ColumnData.DisplayColumnIndex == 0)
			? FLinearColor(0.06f, 0.16f, 0.30f, 0.88f)
			: FLinearColor(0.30f, 0.07f, 0.08f, 0.88f);
		if (ColumnData.bIsMVP)
		{
			ColTint = FLinearColor(0.36f, 0.28f, 0.06f, 0.96f);
		}
		else if (!ColumnData.bIsConnected)
		{
			ColTint.A = 0.58f;
		}
		ColumnBorder->SetBrushColor(ColTint);
	}

	if (UTextBlock* NameText = Cast<UTextBlock>(ColTree->FindWidget(TEXT("Text_PlayerName"))))
	{
		FString DisplayName = ColumnData.PlayerName.ToString();
		if (!ColumnData.bIsConnected)
		{
			DisplayName += TEXT(" (DC)");
		}
		if (ColumnData.bIsMVP)
		{
			DisplayName = FString::Printf(TEXT("★ MVP %s"), *DisplayName);
			NameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.88f, 0.26f, 1.0f)));
		}
		else
		{
			NameText->SetColorAndOpacity(FSlateColor(
				ColumnData.DisplayColumnIndex == 0
					? FLinearColor(0.68f, 0.88f, 1.0f, 1.0f)
					: FLinearColor(1.0f, 0.72f, 0.70f, 1.0f)));
		}
		NameText->SetText(FText::FromString(DisplayName));
	}

	if (UVerticalBox* ColumnVBox = Cast<UVerticalBox>(ColTree->FindWidget(TEXT("ColumnVBox"))))
	{
		if (UWidget* ExistingUpgrades = ColTree->FindWidget(TEXT("Row_EquippedUpgrades")))
		{
			ColumnVBox->RemoveChild(ExistingUpgrades);
		}

		USizeBox* RowHeightBox = ColTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Row_EquippedUpgrades"));
		RowHeightBox->SetMinDesiredHeight(26.0f);
		UHorizontalBox* UpgRow = ColTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row_EquippedUpgradesHBox"));

		const UBwayUpgradeCatalog* Catalog = ResolveUpgradeCatalog();
		int32 CountAdded = 0;

		for (const FBwayOwnedUpgrade& Owned : ColumnData.EquippedUpgrades)
		{
			if (Owned.Id.IsNone() || Owned.Rank <= 0)
			{
				continue;
			}

			UBorder* Chip = ColTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Chip->SetPadding(FMargin(3.0f, 1.0f));
			Chip->SetBrushColor(FLinearColor(0.08f, 0.12f, 0.20f, 0.94f));

			UHorizontalBox* ChipRow = ColTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTexture2D* IconTex = nullptr;
			FString ShortCode = Owned.Id.ToString().Left(2).ToUpper();
			if (Catalog)
			{
				if (const FBwayUpgradeDefinition* Def = Catalog->FindUpgrade(Owned.Id))
				{
					IconTex = Def->Icon.LoadSynchronous();
					if (!Def->DisplayName.IsEmpty())
					{
						ShortCode = Def->DisplayName.ToString().Left(2).ToUpper();
					}
				}
			}

			if (IconTex)
			{
				USizeBox* IconSize = ColTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				IconSize->SetWidthOverride(18.0f);
				IconSize->SetHeightOverride(18.0f);
				UImage* IconImg = ColTree->ConstructWidget<UImage>(UImage::StaticClass());
				IconImg->SetBrushFromTexture(IconTex);
				IconSize->SetContent(IconImg);
				if (UHorizontalBoxSlot* ISlot = ChipRow->AddChildToHorizontalBox(IconSize))
				{
					ISlot->SetVerticalAlignment(VAlign_Center);
					ISlot->SetPadding(FMargin(0.0f, 0.0f, 2.0f, 0.0f));
				}
			}

			UTextBlock* RankTxt = ColTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			FSlateFontInfo Font = RankTxt->GetFont();
			Font.Size = 10;
			RankTxt->SetFont(Font);
			RankTxt->SetText(FText::FromString(
				IconTex
					? FormatUpgradeRomanRank(Owned.Rank)
					: FString::Printf(TEXT("%s%s"), *ShortCode, *FormatUpgradeRomanRank(Owned.Rank))));
			RankTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.86f, 0.30f, 1.0f)));
			if (UHorizontalBoxSlot* RSlot = ChipRow->AddChildToHorizontalBox(RankTxt))
			{
				RSlot->SetVerticalAlignment(VAlign_Center);
			}

			Chip->SetContent(ChipRow);
			if (UHorizontalBoxSlot* CSlot = UpgRow->AddChildToHorizontalBox(Chip))
			{
				CSlot->SetPadding(FMargin(1.0f, 0.0f));
				CSlot->SetVerticalAlignment(VAlign_Center);
			}
			++CountAdded;
		}

		if (CountAdded == 0)
		{
			UTextBlock* DashTxt = ColTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			FSlateFontInfo Font = DashTxt->GetFont();
			Font.Size = 14;
			DashTxt->SetFont(Font);
			DashTxt->SetText(NSLOCTEXT("PostMatch", "NoEquippedUpgrades", "—"));
			DashTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.70f, 0.78f, 0.85f)));
			UpgRow->AddChildToHorizontalBox(DashTxt);
		}

		RowHeightBox->SetContent(UpgRow);
		if (UVerticalBoxSlot* VSlot = ColumnVBox->AddChildToVerticalBox(RowHeightBox))
		{
			VSlot->SetHorizontalAlignment(HAlign_Center);
			VSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
		}
	}
}

bool UBwayMatchBreakdownWidget::IsPlayAgainButtonEnabled() const
{
	if (const UCommonButtonBase* PlayAgainBtn = FindPlayAgainButton())
	{
		return PlayAgainBtn->GetIsEnabled();
	}
	return IsHostBreakdownRole();
}

bool UBwayMatchBreakdownWidget::IsReturnToLobbyButtonEnabled() const
{
	return Btn_ReturnToLobby ? Btn_ReturnToLobby->GetIsEnabled() : true;
}

int32 UBwayMatchBreakdownWidget::GetDisplayedColumnCount() const
{
	if (WidgetTree)
	{
		if (const UHorizontalBox* ColumnsHost = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("PlayerColumnsHost"))))
		{
			return ColumnsHost->GetChildrenCount();
		}
	}
	return CachedSummary.PlayerColumns.Num();
}

void UBwayMatchBreakdownWidget::RequestReturnToLobby()
{
	OnReturnToLobbyRequested.Broadcast();
}

void UBwayMatchBreakdownWidget::RequestPlayAgain()
{
	if (!IsHostBreakdownRole())
	{
		return;
	}
	OnPlayAgainRequested.Broadcast();
}
