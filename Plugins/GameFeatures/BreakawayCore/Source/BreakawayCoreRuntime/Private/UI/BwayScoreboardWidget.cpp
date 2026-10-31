// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayScoreboardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameState/BwayScoringComponent.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayScoreboardWidget)

namespace
{
FString FormatRomanRank(const int32 Rank)
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

void AddFixedScoreboardCell(
	UWidgetTree* WidgetTree,
	UHorizontalBox* RowBox,
	const float Width,
	const FText& Text,
	const FLinearColor& Color,
	const int32 FontSize,
	const ETextJustify::Type Justify = ETextJustify::Center)
{
	USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CellSize->SetWidthOverride(Width);

	UTextBlock* CellText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = CellText->GetFont();
	Font.Size = FontSize;
	CellText->SetFont(Font);
	CellText->SetText(Text);
	CellText->SetColorAndOpacity(FSlateColor(Color));
	CellText->SetJustification(Justify);
	CellText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	CellText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f));
	CellSize->SetContent(CellText);

	if (UHorizontalBoxSlot* HSlot = RowBox->AddChildToHorizontalBox(CellSize))
	{
		HSlot->SetVerticalAlignment(VAlign_Center);
		HSlot->SetPadding(FMargin(4.0f, 0.0f));
	}
}
} // namespace

UBwayScoreboardWidget::UBwayScoreboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

TSharedRef<SWidget> UBwayScoreboardWidget::RebuildWidget()
{
	EnsureScoreboardLayout();
	return Super::RebuildWidget();
}

void UBwayScoreboardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureScoreboardLayout();
}

void UBwayScoreboardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureScoreboardLayout();
	RefreshScoreboardData();
}

void UBwayScoreboardWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		RefreshScoreboardData();
		TimeSinceLastRefresh = 0.0f;
	}
}

const UBwayUpgradeCatalog* UBwayScoreboardWidget::ResolveUpgradeCatalog() const
{
	if (CachedUpgradeCatalog)
	{
		return CachedUpgradeCatalog;
	}

	if (const ABwayGameState* GS = GetBwayGameState())
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

	CachedUpgradeCatalog = LoadObject<UBwayUpgradeCatalog>(
		nullptr,
		TEXT("/BreakawayCore/Economy/DA_BW_MatchUpgrades.DA_BW_MatchUpgrades"));
	return CachedUpgradeCatalog;
}

void UBwayScoreboardWidget::EnsureScoreboardLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScoreboardRootCanvas"));

	UImage* Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ScoreboardDimBackdrop"));
	if (WhiteBrush)
	{
		Backdrop->SetBrush(*WhiteBrush);
	}
	Backdrop->SetColorAndOpacity(FLinearColor(0.01f, 0.02f, 0.04f, 0.68f));
	if (UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop))
	{
		BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackdropSlot->SetOffsets(FMargin(0.0f));
	}

	USizeBox* CenterBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScoreboardCenterSize"));
	CenterBox->SetWidthOverride(1640.0f);
	if (UCanvasPanelSlot* CenterSlot = RootCanvas->AddChildToCanvas(CenterBox))
	{
		CenterSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		CenterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CenterSlot->SetAutoSize(true);
	}

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ScoreboardPanelBorder"));
	if (WhiteBrush)
	{
		PanelBorder->SetBrush(*WhiteBrush);
	}
	PanelBorder->SetPadding(FMargin(24.0f, 18.0f));
	PanelBorder->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.94f));
	CenterBox->SetContent(PanelBorder);

	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ScoreboardMainVBox"));
	PanelBorder->SetContent(MainVBox);

	Text_ScoreboardHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ScoreboardHeader"));
	FSlateFontInfo TitleFont = Text_ScoreboardHeader->GetFont();
	TitleFont.Size = 22;
	Text_ScoreboardHeader->SetFont(TitleFont);
	Text_ScoreboardHeader->SetJustification(ETextJustify::Center);
	Text_ScoreboardHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.97f, 1.0f, 1.0f)));
	Text_ScoreboardHeader->SetText(NSLOCTEXT("Scoreboard", "HeaderTitle", "BREAKAWAY MATCH SCOREBOARD"));
	if (UVerticalBoxSlot* TitleSlot = MainVBox->AddChildToVerticalBox(Text_ScoreboardHeader))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	Text_ScoreboardSubheader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ScoreboardSubheader"));
	FSlateFontInfo SubFont = TitleFont;
	SubFont.Size = 14;
	Text_ScoreboardSubheader->SetFont(SubFont);
	Text_ScoreboardSubheader->SetJustification(ETextJustify::Center);
	Text_ScoreboardSubheader->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.88f, 0.96f, 0.95f)));
	if (UVerticalBoxSlot* SubSlot = MainVBox->AddChildToVerticalBox(Text_ScoreboardSubheader))
	{
		SubSlot->SetHorizontalAlignment(HAlign_Center);
		SubSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 12.0f));
	}

	// Column headers row
	UBorder* HeaderBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ScoreboardColumnHeaderBar"));
	if (WhiteBrush)
	{
		HeaderBar->SetBrush(*WhiteBrush);
	}
	HeaderBar->SetPadding(FMargin(10.0f, 6.0f));
	HeaderBar->SetBrushColor(FLinearColor(0.08f, 0.10f, 0.16f, 0.95f));
	UHorizontalBox* HeaderCols = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ScoreboardHeaderCols"));
	const FLinearColor HeaderTextColor(0.76f, 0.82f, 0.92f, 0.96f);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 190.0f, NSLOCTEXT("Scoreboard", "ColHero", "WARRIOR"), HeaderTextColor, 12, ETextJustify::Left);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 250.0f, NSLOCTEXT("Scoreboard", "ColPlayer", "PLAYER"), HeaderTextColor, 12, ETextJustify::Left);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 115.0f, NSLOCTEXT("Scoreboard", "ColKDA", "K / D / A"), HeaderTextColor, 12);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 80.0f, NSLOCTEXT("Scoreboard", "ColGoals", "GOALS"), HeaderTextColor, 12);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 95.0f, NSLOCTEXT("Scoreboard", "ColDmg", "DAMAGE"), HeaderTextColor, 12);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 95.0f, NSLOCTEXT("Scoreboard", "ColHeal", "HEALING"), HeaderTextColor, 12);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 85.0f, NSLOCTEXT("Scoreboard", "ColFmbl", "FUMBLES"), HeaderTextColor, 12);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 85.0f, NSLOCTEXT("Scoreboard", "ColInt", "INTERCEPT"), HeaderTextColor, 12);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 85.0f, NSLOCTEXT("Scoreboard", "ColBld", "BUILDABLES"), HeaderTextColor, 12);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 105.0f, NSLOCTEXT("Scoreboard", "ColGold", "GOLD"), HeaderTextColor, 12);
	AddFixedScoreboardCell(WidgetTree, HeaderCols, 260.0f, NSLOCTEXT("Scoreboard", "ColUpgrades", "EQUIPPED UPGRADES"), HeaderTextColor, 12, ETextJustify::Left);
	HeaderBar->SetContent(HeaderCols);
	MainVBox->AddChildToVerticalBox(HeaderBar);

	// Friendly Team Section
	UTextBlock* FriendlyTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_FriendlyTeamSection"));
	FSlateFontInfo TeamFont = SubFont;
	TeamFont.Size = 13;
	FriendlyTitle->SetFont(TeamFont);
	FriendlyTitle->SetText(NSLOCTEXT("Scoreboard", "FriendlyTeamTitle", "FRIENDLY TEAM (BLUE)"));
	FriendlyTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.32f, 0.76f, 1.0f, 1.0f)));
	if (UVerticalBoxSlot* FriendlyTitleSlot = MainVBox->AddChildToVerticalBox(FriendlyTitle))
	{
		FriendlyTitleSlot->SetPadding(FMargin(4.0f, 10.0f, 0.0f, 4.0f));
	}

	FriendlyRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FriendlyRowsBox"));
	MainVBox->AddChildToVerticalBox(FriendlyRowsBox);

	// Enemy Team Section
	UTextBlock* EnemyTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_EnemyTeamSection"));
	EnemyTitle->SetFont(TeamFont);
	EnemyTitle->SetText(NSLOCTEXT("Scoreboard", "EnemyTeamTitle", "ENEMY TEAM (RED)"));
	EnemyTitle->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.38f, 0.36f, 1.0f)));
	if (UVerticalBoxSlot* EnemyTitleSlot = MainVBox->AddChildToVerticalBox(EnemyTitle))
	{
		EnemyTitleSlot->SetPadding(FMargin(4.0f, 12.0f, 0.0f, 4.0f));
	}

	EnemyRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EnemyRowsBox"));
	MainVBox->AddChildToVerticalBox(EnemyRowsBox);

	WidgetTree->RootWidget = RootCanvas;
}

TArray<FScoreboardPlayerData> UBwayScoreboardWidget::GetAllScoreboardPlayers() const
{
	TArray<FScoreboardPlayerData> AllPlayers;
	ABwayGameState* GameState = GetBwayGameState();
	if (!GameState)
	{
		CachedMVPPlayerId = INDEX_NONE;
		CachedMVPPlayerName = FText::GetEmpty();
		return AllPlayers;
	}

	const int32 LocalTeam = UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(this);
	TSet<int32> SeenPlayerIds;

	for (int32 TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		const FTeamInfo& TeamInfo = GameState->GetTeamInfo(TeamIdx);
		for (APlayerState* PS : TeamInfo.TeamMembers)
		{
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
			{
				SeenPlayerIds.Add(BwayPS->GetPlayerId());
				AllPlayers.Add(BuildPlayerData(BwayPS, TeamIdx, LocalTeam));
			}
		}
	}

	for (const FBwayMatchBreakdownPlayerColumn& Disconnected : GameState->GetDisconnectedMatchParticipants())
	{
		if (Disconnected.PlayerId != INDEX_NONE && SeenPlayerIds.Contains(Disconnected.PlayerId))
		{
			continue;
		}
		if (Disconnected.GameTeamIndex == 0 || Disconnected.GameTeamIndex == 1)
		{
			AllPlayers.Add(BuildDisconnectedPlayerData(Disconnected, LocalTeam));
		}
	}

	// Deterministic MVP computation matching UBwayMatchStatsLibrary::PopulateMVPAndColumns (tie-break: lowest PlayerId).
	int32 BestIdx = INDEX_NONE;
	float BestScore = -1.0f;
	int32 BestPlayerId = MAX_int32;

	for (int32 Idx = 0; Idx < AllPlayers.Num(); ++Idx)
	{
		const FScoreboardPlayerData& Row = AllPlayers[Idx];
		const bool bBetter = (Row.MVPScore > BestScore + KINDA_SMALL_NUMBER)
			|| (FMath::IsNearlyEqual(Row.MVPScore, BestScore) && Row.PlayerId < BestPlayerId);
		if (bBetter)
		{
			BestScore = Row.MVPScore;
			BestIdx = Idx;
			BestPlayerId = Row.PlayerId;
		}
	}

	CachedMVPPlayerId = INDEX_NONE;
	CachedMVPPlayerName = FText::GetEmpty();
	if (BestIdx != INDEX_NONE && AllPlayers.IsValidIndex(BestIdx))
	{
		AllPlayers[BestIdx].bIsMVP = true;
		CachedMVPPlayerId = AllPlayers[BestIdx].PlayerId;
		CachedMVPPlayerName = AllPlayers[BestIdx].PlayerName;
	}

	AllPlayers.StableSort([](const FScoreboardPlayerData& A, const FScoreboardPlayerData& B)
	{
		if (A.DisplayTeamSlot != B.DisplayTeamSlot)
		{
			return A.DisplayTeamSlot < B.DisplayTeamSlot;
		}
		return A.PlayerId < B.PlayerId;
	});

	return AllPlayers;
}

TArray<FScoreboardPlayerData> UBwayScoreboardWidget::GetTeamPlayers(const int32 TeamIndex) const
{
	TArray<FScoreboardPlayerData> Filtered;
	for (const FScoreboardPlayerData& Row : GetAllScoreboardPlayers())
	{
		if (Row.TeamIndex == TeamIndex)
		{
			Filtered.Add(Row);
		}
	}
	return Filtered;
}

int32 UBwayScoreboardWidget::GetTeam1Score() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			return Scoring->GetTeamScore(0);
		}
	}
	return 0;
}

int32 UBwayScoreboardWidget::GetTeam2Score() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			return Scoring->GetTeamScore(1);
		}
	}
	return 0;
}

int32 UBwayScoreboardWidget::GetCurrentRound() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		return GameState->GetCurrentRoundNumber();
	}
	return 0;
}

FText UBwayScoreboardWidget::GetMatchTimeElapsed() const
{
	const int32 Remaining = UBwayMatchHUDWidgetBase::GetRoundTimeRemainingFromWorld(GetWorld());
	return UBwayMatchHUDWidgetBase::FormatRoundTime(FMath::Max(0, Remaining));
}

void UBwayScoreboardWidget::BuildTeamRowsInContainer(
	UVerticalBox* Container,
	const TArray<FScoreboardPlayerData>& Rows,
	const bool bFriendlyTeam)
{
	if (!Container || !WidgetTree)
	{
		return;
	}

	Container->ClearChildren();
	const UBwayUpgradeCatalog* Catalog = ResolveUpgradeCatalog();
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	for (const FScoreboardPlayerData& Row : Rows)
	{
		UBorder* RowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		if (WhiteBrush)
		{
			RowBorder->SetBrush(*WhiteBrush);
		}
		RowBorder->SetPadding(FMargin(10.0f, 6.0f));

		FLinearColor RowBg = bFriendlyTeam
			? FLinearColor(0.05f, 0.13f, 0.24f, 0.86f)
			: FLinearColor(0.24f, 0.06f, 0.07f, 0.86f);
		if (Row.bIsMVP)
		{
			RowBg = FLinearColor(0.30f, 0.23f, 0.05f, 0.94f);
		}
		else if (!Row.bIsConnected)
		{
			RowBg.A = 0.56f;
		}
		RowBorder->SetBrushColor(RowBg);

		UHorizontalBox* RowHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// 1. Warrior cell (34x34 Portrait + Hero Name)
		{
			USizeBox* HeroCellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			HeroCellSize->SetWidthOverride(190.0f);
			UHorizontalBox* HeroHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

			USizeBox* PortraitSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			PortraitSize->SetWidthOverride(32.0f);
			PortraitSize->SetHeightOverride(32.0f);
			UImage* PortraitImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			if (Row.HeroPortrait)
			{
				PortraitImg->SetBrushFromTexture(Row.HeroPortrait);
				PortraitImg->SetColorAndOpacity(Row.bIsAlive ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.40f, 0.9f));
			}
			else
			{
				if (WhiteBrush)
				{
					PortraitImg->SetBrush(*WhiteBrush);
				}
				PortraitImg->SetColorAndOpacity(bFriendlyTeam
					? FLinearColor(0.18f, 0.46f, 0.82f, 0.85f)
					: FLinearColor(0.82f, 0.22f, 0.22f, 0.85f));
			}
			PortraitSize->SetContent(PortraitImg);
			if (UHorizontalBoxSlot* PSlot = HeroHBox->AddChildToHorizontalBox(PortraitSize))
			{
				PSlot->SetVerticalAlignment(VAlign_Center);
				PSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			}

			UTextBlock* HeroText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			FSlateFontInfo HFont = HeroText->GetFont();
			HFont.Size = 12;
			HeroText->SetFont(HFont);
			HeroText->SetText(Row.HeroName.IsEmpty() ? NSLOCTEXT("Scoreboard", "NoHero", "—") : Row.HeroName);
			HeroText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.93f, 0.98f, 0.96f)));
			if (UHorizontalBoxSlot* HTxtSlot = HeroHBox->AddChildToHorizontalBox(HeroText))
			{
				HTxtSlot->SetVerticalAlignment(VAlign_Center);
			}

			HeroCellSize->SetContent(HeroHBox);
			if (UHorizontalBoxSlot* CellSlot = RowHBox->AddChildToHorizontalBox(HeroCellSize))
			{
				CellSlot->SetVerticalAlignment(VAlign_Center);
				CellSlot->SetPadding(FMargin(4.0f, 0.0f));
			}
		}

		// 2. Player Name + [YOU] + (DC) + [MVP]
		{
			FString DecoratedName = Row.PlayerName.ToString();
			if (Row.bIsLocalPlayer)
			{
				DecoratedName += TEXT(" [YOU]");
			}
			if (!Row.bIsConnected)
			{
				DecoratedName += TEXT(" (DC)");
			}
			if (!Row.bIsAlive && Row.bIsConnected)
			{
				DecoratedName += TEXT(" [DEAD]");
			}
			if (Row.bIsMVP)
			{
				DecoratedName = FString::Printf(TEXT("★ MVP  %s"), *DecoratedName);
			}

			const FLinearColor NameColor = Row.bIsMVP
				? FLinearColor(1.0f, 0.86f, 0.24f, 1.0f)
				: (Row.bIsLocalPlayer
					? FLinearColor(0.48f, 0.92f, 1.0f, 1.0f)
					: FLinearColor(0.95f, 0.96f, 1.0f, 0.96f));
			AddFixedScoreboardCell(WidgetTree, RowHBox, 250.0f, FText::FromString(DecoratedName), NameColor, 13, ETextJustify::Left);
		}

		const FLinearColor StatColor(0.93f, 0.95f, 0.99f, 0.96f);
		AddFixedScoreboardCell(
			WidgetTree,
			RowHBox,
			115.0f,
			FText::FromString(FString::Printf(TEXT("%d / %d / %d"), Row.Kills, Row.Deaths, Row.Assists)),
			StatColor,
			13);
		AddFixedScoreboardCell(WidgetTree, RowHBox, 80.0f, FText::AsNumber(Row.ObjectiveScore), StatColor, 13);
		AddFixedScoreboardCell(WidgetTree, RowHBox, 95.0f, FText::AsNumber(Row.DamageDealt), StatColor, 13);
		AddFixedScoreboardCell(WidgetTree, RowHBox, 95.0f, FText::AsNumber(Row.HealingDone), StatColor, 13);
		AddFixedScoreboardCell(WidgetTree, RowHBox, 85.0f, FText::AsNumber(Row.ForcedFumbles), StatColor, 13);
		AddFixedScoreboardCell(WidgetTree, RowHBox, 85.0f, FText::AsNumber(Row.Interceptions), StatColor, 13);
		AddFixedScoreboardCell(WidgetTree, RowHBox, 85.0f, FText::AsNumber(Row.BuildablesDestroyed), StatColor, 13);
		AddFixedScoreboardCell(
			WidgetTree,
			RowHBox,
			105.0f,
			FText::AsNumber(Row.MatchGoldEarned),
			FLinearColor(0.98f, 0.84f, 0.28f, 0.98f),
			13);

		// Equipped Upgrades cell (Icon + Roman rank badge)
		{
			USizeBox* UpgradesCellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			UpgradesCellSize->SetWidthOverride(260.0f);
			UHorizontalBox* UpgBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

			int32 DisplayedUpgrades = 0;
			for (const FBwayOwnedUpgrade& Owned : Row.EquippedUpgrades)
			{
				if (Owned.Id.IsNone() || Owned.Rank <= 0)
				{
					continue;
				}

				UBorder* ChipBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				if (WhiteBrush)
				{
					ChipBorder->SetBrush(*WhiteBrush);
				}
				ChipBorder->SetPadding(FMargin(4.0f, 2.0f));
				ChipBorder->SetBrushColor(FLinearColor(0.10f, 0.14f, 0.22f, 0.95f));

				UHorizontalBox* ChipHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				UTexture2D* UpgradeIcon = nullptr;
				FString ShortLabel = Owned.Id.ToString().Left(3).ToUpper();

				if (Catalog)
				{
					if (const FBwayUpgradeDefinition* Def = Catalog->FindUpgrade(Owned.Id))
					{
						UpgradeIcon = Def->Icon.LoadSynchronous();
						if (!Def->DisplayName.IsEmpty())
						{
							ShortLabel = Def->DisplayName.ToString().Left(3).ToUpper();
						}
					}
				}

				if (UpgradeIcon)
				{
					USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
					IconSize->SetWidthOverride(20.0f);
					IconSize->SetHeightOverride(20.0f);
					UImage* IconImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
					IconImg->SetBrushFromTexture(UpgradeIcon);
					IconSize->SetContent(IconImg);
					if (UHorizontalBoxSlot* ISlot = ChipHBox->AddChildToHorizontalBox(IconSize))
					{
						ISlot->SetVerticalAlignment(VAlign_Center);
						ISlot->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 0.0f));
					}
				}

				UTextBlock* RankText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				FSlateFontInfo RFont = RankText->GetFont();
				RFont.Size = 11;
				RankText->SetFont(RFont);
				RankText->SetText(FText::FromString(
					UpgradeIcon
						? FormatRomanRank(Owned.Rank)
						: FString::Printf(TEXT("%s %s"), *ShortLabel, *FormatRomanRank(Owned.Rank))));
				RankText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.86f, 0.34f, 1.0f)));
				if (UHorizontalBoxSlot* RSlot = ChipHBox->AddChildToHorizontalBox(RankText))
				{
					RSlot->SetVerticalAlignment(VAlign_Center);
				}

				ChipBorder->SetContent(ChipHBox);
				if (UHorizontalBoxSlot* ChipSlot = UpgBox->AddChildToHorizontalBox(ChipBorder))
				{
					ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
					ChipSlot->SetVerticalAlignment(VAlign_Center);
				}
				++DisplayedUpgrades;
			}

			if (DisplayedUpgrades == 0)
			{
				UTextBlock* NoneText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				FSlateFontInfo NFont = NoneText->GetFont();
				NFont.Size = 11;
				NoneText->SetFont(NFont);
				NoneText->SetText(NSLOCTEXT("Scoreboard", "NoUpgrades", "—"));
				NoneText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.60f, 0.70f, 0.80f)));
				UpgBox->AddChildToHorizontalBox(NoneText);
			}

			UpgradesCellSize->SetContent(UpgBox);
			if (UHorizontalBoxSlot* UpgCellSlot = RowHBox->AddChildToHorizontalBox(UpgradesCellSize))
			{
				UpgCellSlot->SetVerticalAlignment(VAlign_Center);
				UpgCellSlot->SetPadding(FMargin(4.0f, 0.0f));
			}
		}

		RowBorder->SetContent(RowHBox);
		if (UVerticalBoxSlot* RowVSlot = Container->AddChildToVerticalBox(RowBorder))
		{
			RowVSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
	}
}

void UBwayScoreboardWidget::RebuildVisualRows(
	const TArray<FScoreboardPlayerData>& FriendlyPlayers,
	const TArray<FScoreboardPlayerData>& EnemyPlayers)
{
	if (Text_ScoreboardSubheader)
	{
		const int32 LocalTeam = UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(this);
		const int32 FriendlyWins = UBwayMatchHUDWidgetBase::GetDisplayTeamScore(GetWorld(), 0, LocalTeam);
		const int32 EnemyWins = UBwayMatchHUDWidgetBase::GetDisplayTeamScore(GetWorld(), 1, LocalTeam);
		const int32 TargetWins = UBwayMatchHUDWidgetBase::GetPointsToWinFromWorld(GetWorld());
		const int32 RoundNum = FMath::Max(1, GetCurrentRound());
		const FText ClockText = GetMatchTimeElapsed();

		Text_ScoreboardSubheader->SetText(FText::FromString(FString::Printf(
			TEXT("ROUND %d • SERIES SCORE: %d - %d (FIRST TO %d) • CLOCK %s"),
			RoundNum,
			FriendlyWins,
			EnemyWins,
			TargetWins,
			*ClockText.ToString())));
	}

	BuildTeamRowsInContainer(FriendlyRowsBox, FriendlyPlayers, /*bFriendlyTeam*/ true);
	BuildTeamRowsInContainer(EnemyRowsBox, EnemyPlayers, /*bFriendlyTeam*/ false);
}

void UBwayScoreboardWidget::RefreshScoreboardData()
{
	EnsureScoreboardLayout();

	const TArray<FScoreboardPlayerData> AllPlayers = GetAllScoreboardPlayers();
	TArray<FScoreboardPlayerData> Team0Auth;
	TArray<FScoreboardPlayerData> Team1Auth;
	TArray<FScoreboardPlayerData> FriendlyDisplay;
	TArray<FScoreboardPlayerData> EnemyDisplay;

	for (const FScoreboardPlayerData& Row : AllPlayers)
	{
		if (Row.TeamIndex == 0)
		{
			Team0Auth.Add(Row);
		}
		else if (Row.TeamIndex == 1)
		{
			Team1Auth.Add(Row);
		}

		if (Row.DisplayTeamSlot == 0)
		{
			FriendlyDisplay.Add(Row);
		}
		else
		{
			EnemyDisplay.Add(Row);
		}
	}

	RebuildVisualRows(FriendlyDisplay, EnemyDisplay);
	OnScoreboardDataRefreshed(Team0Auth, Team1Auth);
}

ABwayGameState* UBwayScoreboardWidget::GetBwayGameState() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetGameState<ABwayGameState>();
	}
	return nullptr;
}

FScoreboardPlayerData UBwayScoreboardWidget::BuildPlayerData(
	ABwayPlayerState* PlayerState,
	const int32 TeamIndex,
	const int32 LocalTeam) const
{
	FScoreboardPlayerData Data;
	if (!PlayerState)
	{
		return Data;
	}

	Data.PlayerId = PlayerState->GetPlayerId();
	Data.PlayerName = FText::FromString(PlayerState->GetPlayerName());
	Data.TeamIndex = TeamIndex;
	Data.DisplayTeamSlot = UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(TeamIndex, LocalTeam);
	Data.Ping = static_cast<int32>(PlayerState->GetPingInMilliseconds());
	Data.bIsConnected = true;

	if (APlayerController* LocalPC = GetOwningPlayer())
	{
		Data.bIsLocalPlayer = (LocalPC->PlayerState == PlayerState);
	}

	Data.HeroId = PlayerState->GetSelectedHeroId();
	if (Data.HeroId.IsValid())
	{
		if (UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(Data.HeroId))
		{
			Data.HeroName = HeroData->DisplayName;
			Data.HeroPortrait = HeroData->Portrait;
		}
		else
		{
			Data.HeroName = FText::FromString(Data.HeroId.PrimaryAssetName.ToString());
		}
	}

	const FBwayPlayerMatchStats Stats = PlayerState->GetMatchStatsSnapshot();
	Data.Kills = Stats.Kills;
	Data.Deaths = Stats.Deaths;
	Data.Assists = Stats.Assists;
	Data.ObjectiveScore = Stats.RelicScores;
	Data.DamageDealt = Stats.DamageDealt;
	Data.HealingDone = Stats.HealingDone;
	Data.ForcedFumbles = Stats.ForcedFumbles;
	Data.Interceptions = Stats.Interceptions;
	Data.BuildablesDestroyed = Stats.BuildablesDestroyed;
	Data.MatchGoldEarned = Stats.GoldEarned;
	Data.EquippedUpgrades = PlayerState->GetReplicatedEquippedUpgrades();
	Data.MVPScore = UBwayMatchStatsLibrary::ComputePlayerMVPScore(Stats);

	bool bAlive = !PlayerState->GetRespawnSchedule().bIsWaitingToRespawn;
	if (APawn* Pawn = PlayerState->GetPawn())
	{
		if (const ULyraHealthComponent* HealthComp = ULyraHealthComponent::FindHealthComponent(Pawn))
		{
			if (HealthComp->IsDeadOrDying() || HealthComp->GetHealth() <= 0.0f)
			{
				bAlive = false;
			}
		}
	}
	else if (PlayerState->GetDeaths() > 0)
	{
		bAlive = false;
	}
	Data.bIsAlive = bAlive;

	return Data;
}

FScoreboardPlayerData UBwayScoreboardWidget::BuildDisconnectedPlayerData(
	const FBwayMatchBreakdownPlayerColumn& Snapshot,
	const int32 LocalTeam) const
{
	FScoreboardPlayerData Data;
	Data.PlayerId = Snapshot.PlayerId;
	Data.PlayerName = Snapshot.PlayerName;
	Data.TeamIndex = Snapshot.GameTeamIndex;
	Data.DisplayTeamSlot = UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(Snapshot.GameTeamIndex, LocalTeam);
	Data.Kills = Snapshot.Stats.Kills;
	Data.Deaths = Snapshot.Stats.Deaths;
	Data.Assists = Snapshot.Stats.Assists;
	Data.ObjectiveScore = Snapshot.Stats.RelicScores;
	Data.DamageDealt = Snapshot.Stats.DamageDealt;
	Data.HealingDone = Snapshot.Stats.HealingDone;
	Data.ForcedFumbles = Snapshot.Stats.ForcedFumbles;
	Data.Interceptions = Snapshot.Stats.Interceptions;
	Data.BuildablesDestroyed = Snapshot.Stats.BuildablesDestroyed;
	Data.MatchGoldEarned = Snapshot.Stats.GoldEarned;
	Data.HeroId = Snapshot.HeroId;
	Data.HeroName = Snapshot.HeroName;
	if (Data.HeroId.IsValid())
	{
		if (UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(Data.HeroId))
		{
			Data.HeroPortrait = HeroData->Portrait;
			if (Data.HeroName.IsEmpty())
			{
				Data.HeroName = HeroData->DisplayName;
			}
		}
	}
	Data.EquippedUpgrades = Snapshot.EquippedUpgrades;
	Data.MVPScore = UBwayMatchStatsLibrary::ComputePlayerMVPScore(Snapshot.Stats);
	Data.bIsConnected = false;
	Data.bIsAlive = false;
	Data.Ping = 0;
	return Data;
}
