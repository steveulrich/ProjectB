// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCaptureTheRelicScoreWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Blueprint/WidgetTree.h"
#include "CommonNumericTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCaptureTheRelicScoreWidget)

UBwayCaptureTheRelicScoreWidget::UBwayCaptureTheRelicScoreWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayCaptureTheRelicScoreWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureScorebugDecorationWidgets(FMath::Max(1, CachedPointsToWin > 0 ? CachedPointsToWin : 3));
}

void UBwayCaptureTheRelicScoreWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToLocalRespawnPlayerState();
	BindToRoundEndedEvents();
	RefreshScoreDisplay();
	RefreshRoundLabelDisplay();
	RefreshLocalRespawnIndicator();

	if (bShowRoundTimer)
	{
		RefreshTimerDisplay();

		if (TimerPollInterval > 0.0f)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					TimerPollHandle,
					this,
					&UBwayCaptureTheRelicScoreWidget::RefreshTimerDisplay,
					TimerPollInterval,
					true);
			}
		}
	}
}

void UBwayCaptureTheRelicScoreWidget::NativeDestruct()
{
	if (BoundLocalPlayerState.IsValid())
	{
		BoundLocalPlayerState->OnRespawnScheduleChanged.RemoveDynamic(
			this,
			&ThisClass::HandleLocalRespawnScheduleChanged);
	}
	BoundLocalPlayerState.Reset();

	if (BoundRoundManagement.IsValid())
	{
		BoundRoundManagement->OnRoundEnded.RemoveDynamic(
			this,
			&ThisClass::HandleRoundEnded);
	}
	BoundRoundManagement.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerPollHandle);
	}

	Super::NativeDestruct();
}

void UBwayCaptureTheRelicScoreWidget::RefreshScoreDisplay()
{
	BindToRoundEndedEvents();

	const int32 LocalTeam = GetLocalPlayerTeamForWidget(this);
	const int32 LeftScore = GetDisplayTeamScore(GetWorld(), 0, LocalTeam);
	const int32 RightScore = GetDisplayTeamScore(GetWorld(), 1, LocalTeam);
	const int32 PointsToWin = GetPointsToWinFromWorld(GetWorld());

	if (LeftScore == 0 && RightScore == 0)
	{
		Team0WinHistory.Reset();
		Team1WinHistory.Reset();
	}

	UpdateBoundScoreTexts(LeftScore, RightScore);
	UpdateRoundWinPips(LeftScore, RightScore, PointsToWin);
	OnScoreChanged(LeftScore, RightScore);
	InvalidateLayoutAndVolatility();
}

int32 UBwayCaptureTheRelicScoreWidget::ResolveAuthoritativeClockSeconds() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	const ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	const UBwayRoundManagementComponent* RoundMgmt = GameState ? GameState->GetRoundManagement() : nullptr;
	if (!RoundMgmt)
	{
		return FMath::Max(0, GetRoundTimeRemainingFromWorld(World));
	}

	if (RoundMgmt->GetCurrentRoundState() == ERoundState::RoundActive)
	{
		return FMath::Max(0, RoundMgmt->GetRoundTimeRemaining());
	}

	switch (RoundMgmt->GetCurrentMatchPhase())
	{
	case EBwayMatchPhase::Warmup:
		return FMath::Max(0, RoundMgmt->GetWarmupTimeRemaining());
	case EBwayMatchPhase::Prematch:
	{
		const int32 PrematchRemaining = RoundMgmt->GetPrematchTimeRemaining();
		return PrematchRemaining > 0
			? PrematchRemaining
			: FMath::Max(0, FMath::RoundToInt(RoundMgmt->RoundDuration));
	}
	case EBwayMatchPhase::PostRound:
	case EBwayMatchPhase::PostMatch:
		return 0;
	default:
		return FMath::Max(0, RoundMgmt->GetRoundTimeRemaining());
	}
}

void UBwayCaptureTheRelicScoreWidget::RefreshTimerDisplay()
{
	BindToLocalRespawnPlayerState();

	const ABwayGameState* GameState = GetBwayGameState();
	const UBwayRoundManagementComponent* RoundMgmt = GameState ? GameState->GetRoundManagement() : nullptr;
	const bool bRoundActive = RoundMgmt && RoundMgmt->GetCurrentRoundState() == ERoundState::RoundActive;

	const int32 SecondsRemaining = ResolveAuthoritativeClockSeconds();
	const int32 SuddenDeathThreshold = GetSuddenDeathWarningSecondsFromWorld(GetWorld());
	const bool bNewSuddenDeathWarning = bRoundActive
		&& SuddenDeathThreshold > 0
		&& SecondsRemaining > 0
		&& SecondsRemaining <= SuddenDeathThreshold;
	if (bNewSuddenDeathWarning != bSuddenDeathWarningShown)
	{
		bSuddenDeathWarningShown = bNewSuddenDeathWarning;
		OnSuddenDeathTimerStateChanged(bSuddenDeathWarningShown);
	}

	const int32 CurrentPointsToWin = GetPointsToWinFromWorld(GetWorld());
	if (CurrentPointsToWin != CachedPointsToWin)
	{
		RefreshScoreDisplay();
	}

	UpdateBoundTimerText(SecondsRemaining);
	RefreshRoundLabelDisplay();
	RefreshLocalRespawnIndicator();
	OnRoundTimeUpdated(SecondsRemaining, FormatRoundTime(SecondsRemaining));
}

void UBwayCaptureTheRelicScoreWidget::NotifyTeamScoreChanged(int32 TeamIndex, int32 NewScore)
{
	(void)TeamIndex;
	(void)NewScore;
	RefreshScoreDisplay();
}

void UBwayCaptureTheRelicScoreWidget::NotifyRoundTimeChanged(int32 SecondsRemaining)
{
	(void)SecondsRemaining;

	if (bShowRoundTimer)
	{
		RefreshTimerDisplay();
	}
}

void UBwayCaptureTheRelicScoreWidget::NotifyRoundStateChanged(FName NewState)
{
	(void)NewState;
	if (bSuddenDeathWarningShown)
	{
		bSuddenDeathWarningShown = false;
		OnSuddenDeathTimerStateChanged(false);
	}

	RefreshScoreDisplay();
	RefreshRoundLabelDisplay();
	RefreshLocalRespawnIndicator();

	if (bShowRoundTimer)
	{
		RefreshTimerDisplay();
	}
}

void UBwayCaptureTheRelicScoreWidget::RefreshRoundLabelDisplay()
{
	UpdateBoundRoundLabelText();
}

void UBwayCaptureTheRelicScoreWidget::UpdateBoundRoundLabelText()
{
	if (Text_RoundLabel)
	{
		const int32 RoundNumber = FMath::Max(GetCurrentRoundNumberFromWorld(GetWorld()), 1);
		Text_RoundLabel->SetText(FText::Format(
			NSLOCTEXT("BwayCTRScore", "RoundLabel", "ROUND {0}"),
			FText::AsNumber(RoundNumber)));
	}
}

void UBwayCaptureTheRelicScoreWidget::UpdateBoundScoreTexts(int32 Team1Score, int32 Team2Score)
{
	if (Text_Team1Score)
	{
		Text_Team1Score->SetCurrentValue(static_cast<float>(Team1Score));
		Text_Team1Score->SetText(FText::AsNumber(Team1Score));
	}

	if (Text_Team2Score)
	{
		Text_Team2Score->SetCurrentValue(static_cast<float>(Team2Score));
		Text_Team2Score->SetText(FText::AsNumber(Team2Score));
	}
}

void UBwayCaptureTheRelicScoreWidget::EnsureScorebugDecorationWidgets(int32 PointsToWin)
{
	if (!WidgetTree)
	{
		return;
	}

	if (!WidgetTree->RootWidget)
	{
		USizeBox* FallbackRootSB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PrimaryWidthSB"));
		FallbackRootSB->SetWidthOverride(544.0f);
		FallbackRootSB->SetHeightOverride(86.0f);

		UOverlay* FallbackOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay"));
		if (!Text_Timer)
		{
			Text_Timer = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("Text_Timer"));
			FallbackOverlay->AddChildToOverlay(Text_Timer);
		}
		FallbackRootSB->SetContent(FallbackOverlay);
		WidgetTree->RootWidget = FallbackRootSB;
	}

	// Expand scorebug box to the 544x86 broadcast trapezoid footprint and suppress legacy UMG children visually
	// (while preserving their live values/visibility for view-model getters and automation tests).
	if (USizeBox* PrimaryWidthSB = WidgetTree->FindWidget<USizeBox>(TEXT("PrimaryWidthSB")))
	{
		PrimaryWidthSB->SetWidthOverride(544.0f);
		PrimaryWidthSB->SetHeightOverride(86.0f);
	}
	for (const TCHAR* LegacyVisualName : { TEXT("HUDTimerBG"), TEXT("AllyCounterSB"), TEXT("EnemyCounterSB"), TEXT("Overlay_1"), TEXT("Overlay_2") })
	{
		if (UWidget* LegacyWidget = WidgetTree->FindWidget(LegacyVisualName))
		{
			LegacyWidget->SetRenderOpacity(0.0f);
		}
	}
	if (Text_Timer)
	{
		Text_Timer->SetRenderOpacity(0.0f);
	}
	if (Text_RoundLabel)
	{
		Text_RoundLabel->SetRenderOpacity(0.0f);
	}

	const int32 ClampedPips = FMath::Clamp(PointsToWin, 1, 7);

	auto EnsurePipsInOverlay = [&](UOverlay* TargetOverlay, TObjectPtr<UHorizontalBox>& InOutRow, TArray<TObjectPtr<UBorder>>& InOutBorders, const TCHAR* RowName)
	{
		if (!TargetOverlay)
		{
			return;
		}
		if (!InOutRow)
		{
			InOutRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(RowName));
			InOutRow->SetRenderOpacity(0.0f);
			if (UOverlaySlot* OverlaySlot = TargetOverlay->AddChildToOverlay(InOutRow))
			{
				OverlaySlot->SetHorizontalAlignment(HAlign_Center);
				OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
				OverlaySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			}
		}

		if (InOutBorders.Num() != ClampedPips && InOutRow)
		{
			InOutRow->ClearChildren();
			InOutBorders.Reset();
			for (int32 PipIndex = 0; PipIndex < ClampedPips; ++PipIndex)
			{
				USizeBox* PipSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				PipSizeBox->SetWidthOverride(14.0f);
				PipSizeBox->SetHeightOverride(6.0f);

				UBorder* PipBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				PipBorder->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
				PipBorder->SetBrushColor(FLinearColor(0.24f, 0.27f, 0.35f, 0.90f));
				PipSizeBox->AddChild(PipBorder);

				if (UHorizontalBoxSlot* HSlot = InOutRow->AddChildToHorizontalBox(PipSizeBox))
				{
					HSlot->SetPadding(FMargin(2.5f, 0.0f));
				}
				InOutBorders.Add(PipBorder);
			}
		}
	};

	UOverlay* FriendlyOverlay = WidgetTree->FindWidget<UOverlay>(TEXT("Overlay_1"));
	UOverlay* EnemyOverlay = WidgetTree->FindWidget<UOverlay>(TEXT("Overlay_2"));
	EnsurePipsInOverlay(FriendlyOverlay, FriendlyPipsRow, FriendlyPipBorders, TEXT("FriendlyPipsRow"));
	EnsurePipsInOverlay(EnemyOverlay, EnemyPipsRow, EnemyPipBorders, TEXT("EnemyPipsRow"));

	if (!LocalRespawnBadgeBorder)
	{
		if (UOverlay* MainOverlay = WidgetTree->FindWidget<UOverlay>(TEXT("Overlay")))
		{
			LocalRespawnBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LocalRespawnBadgeBorder"));
			LocalRespawnBadgeBorder->SetBrushColor(FLinearColor(0.55f, 0.08f, 0.08f, 0.92f));
			LocalRespawnBadgeBorder->SetPadding(FMargin(10.0f, 1.0f));
			LocalRespawnBadgeBorder->SetRenderOpacity(0.0f);
			LocalRespawnBadgeBorder->SetVisibility(ESlateVisibility::Collapsed);

			Text_LocalRespawnBadge = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_LocalRespawnBadge"));
			FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
			BadgeFont.OutlineSettings.OutlineSize = 1;
			BadgeFont.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.9f);
			Text_LocalRespawnBadge->SetFont(BadgeFont);
			Text_LocalRespawnBadge->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.92f, 0.82f, 1.0f)));
			Text_LocalRespawnBadge->SetJustification(ETextJustify::Center);
			LocalRespawnBadgeBorder->AddChild(Text_LocalRespawnBadge);

			if (UOverlaySlot* BadgeSlot = MainOverlay->AddChildToOverlay(LocalRespawnBadgeBorder))
			{
				BadgeSlot->SetHorizontalAlignment(HAlign_Center);
				BadgeSlot->SetVerticalAlignment(VAlign_Bottom);
				BadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 1.0f));
			}
		}
	}
}

void UBwayCaptureTheRelicScoreWidget::UpdateRoundWinPips(int32 FriendlyScore, int32 EnemyScore, int32 PointsToWin)
{
	CachedPointsToWin = FMath::Max(1, PointsToWin);
	CachedFriendlyScore = FMath::Max(0, FriendlyScore);
	CachedEnemyScore = FMath::Max(0, EnemyScore);

	EnsureScorebugDecorationWidgets(CachedPointsToWin);

	const FLinearColor FriendlyFilledColor(0.24f, 0.72f, 1.0f, 1.0f);
	const FLinearColor EnemyFilledColor(1.0f, 0.36f, 0.20f, 1.0f);
	const FLinearColor EmptyPipColor(0.24f, 0.27f, 0.35f, 0.90f);

	for (int32 Index = 0; Index < FriendlyPipBorders.Num(); ++Index)
	{
		if (FriendlyPipBorders[Index])
		{
			FriendlyPipBorders[Index]->SetBrushColor(Index < CachedFriendlyScore ? FriendlyFilledColor : EmptyPipColor);
		}
	}

	for (int32 Index = 0; Index < EnemyPipBorders.Num(); ++Index)
	{
		if (EnemyPipBorders[Index])
		{
			EnemyPipBorders[Index]->SetBrushColor(Index < CachedEnemyScore ? EnemyFilledColor : EmptyPipColor);
		}
	}
}

void UBwayCaptureTheRelicScoreWidget::BindToLocalRespawnPlayerState()
{
	ABwayPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (BoundLocalPlayerState.Get() == LocalPS)
	{
		return;
	}

	if (BoundLocalPlayerState.IsValid())
	{
		BoundLocalPlayerState->OnRespawnScheduleChanged.RemoveDynamic(
			this,
			&ThisClass::HandleLocalRespawnScheduleChanged);
	}

	BoundLocalPlayerState = LocalPS;
	if (LocalPS)
	{
		LocalPS->OnRespawnScheduleChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleLocalRespawnScheduleChanged);
	}
}

void UBwayCaptureTheRelicScoreWidget::HandleLocalRespawnScheduleChanged()
{
	RefreshLocalRespawnIndicator();
}

void UBwayCaptureTheRelicScoreWidget::RefreshLocalRespawnIndicator()
{
	EnsureScorebugDecorationWidgets(FMath::Max(1, CachedPointsToWin > 0 ? CachedPointsToWin : GetPointsToWinFromWorld(GetWorld())));
	CachedLocalRespawnSeconds = 0;

	APlayerController* OwningPC = GetOwningPlayer();
	ABwayPlayerState* LocalPS = BoundLocalPlayerState.Get();
	ABwayGameState* GameState = GetBwayGameState();
	const UBwayRoundManagementComponent* RoundMgmt = GameState ? GameState->GetRoundManagement() : nullptr;

	bool bShowLocalRespawn = false;
	if (LocalPS && GameState && !IsLocalGameplayHUDOccludedByMenu(OwningPC))
	{
		const bool bRoundActive = !RoundMgmt || (
			(!RoundMgmt->IsOrchestratingMatchFlow() || RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::Playing)
			&& RoundMgmt->GetCurrentRoundState() == ERoundState::RoundActive);
		const APawn* LocalPawn = LocalPS->GetPawn();
		const ULyraHealthComponent* Health = LocalPawn ? ULyraHealthComponent::FindHealthComponent(LocalPawn) : nullptr;
		const bool bIsAlive = LocalPawn && (!Health || (!Health->IsDeadOrDying() && Health->GetHealth() > 0.0f));
		const FBwayRespawnSchedule Schedule = LocalPS->GetRespawnSchedule();
		const int32 CurrentRound = RoundMgmt ? RoundMgmt->GetCurrentRoundNumber() : 0;
		const bool bRoundMatches = (CurrentRound <= 0 || Schedule.RoundNumber == CurrentRound);
		const int32 Remaining = (bRoundActive && !bIsAlive && bRoundMatches)
			? Schedule.GetRemainingSeconds(GameState->GetServerWorldTimeSeconds())
			: 0;
		if (Schedule.bIsWaitingToRespawn && Remaining > 0)
		{
			CachedLocalRespawnSeconds = Remaining;
			bShowLocalRespawn = true;
		}
	}

	if (LocalRespawnBadgeBorder && Text_LocalRespawnBadge)
	{
		if (bShowLocalRespawn)
		{
			Text_LocalRespawnBadge->SetText(FText::Format(
				NSLOCTEXT("BwayCTRScore", "LocalRespawnFmt", "RESPAWN {0}s"),
				FText::AsNumber(CachedLocalRespawnSeconds)));
			LocalRespawnBadgeBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_LocalRespawnBadge->SetText(FText::GetEmpty());
			LocalRespawnBadgeBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UBwayCaptureTheRelicScoreWidget::UpdateBoundTimerText(int32 SecondsRemaining)
{
	if (Text_Timer)
	{
		Text_Timer->SetText(FormatRoundTime(FMath::Max(0, SecondsRemaining)));

		const FLinearColor TimerColor = bSuddenDeathWarningShown
			? FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)
			: FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		Text_Timer->SetColorAndOpacity(TimerColor);
	}
	InvalidateLayoutAndVolatility();
}

void UBwayCaptureTheRelicScoreWidget::BindToRoundEndedEvents()
{
	ABwayGameState* GameState = GetBwayGameState();
	UBwayRoundManagementComponent* RoundMgmt = GameState ? GameState->GetRoundManagement() : nullptr;
	if (BoundRoundManagement.Get() == RoundMgmt)
	{
		return;
	}

	if (BoundRoundManagement.IsValid())
	{
		BoundRoundManagement->OnRoundEnded.RemoveDynamic(this, &ThisClass::HandleRoundEnded);
	}

	BoundRoundManagement = RoundMgmt;
	if (RoundMgmt)
	{
		RoundMgmt->OnRoundEnded.AddUniqueDynamic(this, &ThisClass::HandleRoundEnded);
	}
}

void UBwayCaptureTheRelicScoreWidget::HandleRoundEnded(
	const int32 WinningTeam,
	const EBwayWinCondition WinCondition,
	const int32 RoundNumber)
{
	(void)RoundNumber;
	if (WinningTeam == 0)
	{
		Team0WinHistory.Add(WinCondition);
	}
	else if (WinningTeam == 1)
	{
		Team1WinHistory.Add(WinCondition);
	}
	InvalidateLayoutAndVolatility();
}

UTexture2D* UBwayCaptureTheRelicScoreWidget::ResolveRoundWinConditionIcon(const EBwayWinCondition Condition) const
{
	if (!CachedRoundIconScore)
	{
		CachedRoundIconScore = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/BreakawayCore/UI/Art/Match_HUD/icon_round_type_score.icon_round_type_score"));
	}
	if (!CachedRoundIconTeamWipe)
	{
		CachedRoundIconTeamWipe = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/BreakawayCore/UI/Art/Match_HUD/icon_round_type_team_wipe.icon_round_type_team_wipe"));
	}
	if (!CachedRoundIconTerritory)
	{
		CachedRoundIconTerritory = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/BreakawayCore/UI/Art/Match_HUD/icon_round_type_territory.icon_round_type_territory"));
	}

	switch (Condition)
	{
	case EBwayWinCondition::TeamEliminated:
		return CachedRoundIconTeamWipe ? CachedRoundIconTeamWipe.Get() : CachedRoundIconScore.Get();
	case EBwayWinCondition::TimeExpired:
	case EBwayWinCondition::SuddenDeath:
		return CachedRoundIconTerritory ? CachedRoundIconTerritory.Get() : CachedRoundIconScore.Get();
	case EBwayWinCondition::GoalScored:
	default:
		return CachedRoundIconScore.Get();
	}
}

int32 UBwayCaptureTheRelicScoreWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	int32 CurrentLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 60.0f || Size.Y < 20.0f)
	{
		return CurrentLayer;
	}

	const double W = Size.X;
	const double H = Size.Y;
	const double SlopeInset = FMath::Min(W * 0.16, H * 0.98); // ~84px inward slope over 86px height

	// 1. Main dark translucent navy-slate trapezoid body flush to Y = 0
	const FVector2D OuterTrapezoid[4] = {
		FVector2D(0.0, 0.0),
		FVector2D(W, 0.0),
		FVector2D(W - SlopeInset, H),
		FVector2D(SlopeInset, H),
	};
	DrawConvexPolygon(AllottedGeometry, OutDrawElements, ++CurrentLayer, OuterTrapezoid, FLinearColor(0.03f, 0.09f, 0.14f, 0.88f));

	// Center timer bay recessed trapezoid
	const double CenterTopHalfW = 104.0;
	const double CenterBottomHalfW = 86.0;
	const FVector2D CenterBay[4] = {
		FVector2D(W * 0.5 - CenterTopHalfW, 0.0),
		FVector2D(W * 0.5 + CenterTopHalfW, 0.0),
		FVector2D(W * 0.5 + CenterBottomHalfW, H),
		FVector2D(W * 0.5 - CenterBottomHalfW, H),
	};
	DrawConvexPolygon(AllottedGeometry, OutDrawElements, ++CurrentLayer, CenterBay, FLinearColor(0.06f, 0.15f, 0.22f, 0.58f));

	// Crisp outer cyan-slate bevel trim + angled inner wing dividers
	const FVector2D OuterBevel[4] = {
		OuterTrapezoid[0],
		OuterTrapezoid[3],
		OuterTrapezoid[2],
		OuterTrapezoid[1],
	};
	DrawPolygonOutline(AllottedGeometry, OutDrawElements, ++CurrentLayer, OuterBevel, FLinearColor(0.18f, 0.54f, 0.70f, 0.92f), 2.0f, false);

	const FVector2D LeftDivider[2] = { CenterBay[0], CenterBay[3] };
	const FVector2D RightDivider[2] = { CenterBay[1], CenterBay[2] };
	DrawPolygonOutline(AllottedGeometry, OutDrawElements, CurrentLayer, LeftDivider, FLinearColor(0.16f, 0.44f, 0.58f, 0.65f), 1.3f, false);
	DrawPolygonOutline(AllottedGeometry, OutDrawElements, CurrentLayer, RightDivider, FLinearColor(0.16f, 0.44f, 0.58f, 0.65f), 1.3f, false);

	// 2. Vertically stacked slanted parallelogram round-win bars inside left (Friendly) & right (Enemy) trapezoid wings
	const int32 NumBars = FMath::Clamp(CachedPointsToWin > 0 ? CachedPointsToWin : 3, 1, 5);
	const double TopMargin = 6.0;
	const double BottomMargin = 7.0;
	const double BarGap = 4.5;
	const double AvailableH = FMath::Max(18.0, H - TopMargin - BottomMargin - BarGap * (NumBars - 1));
	const double BarH = AvailableH / static_cast<double>(NumBars);
	const double BarW = 74.0;
	const double OuterEdgePad = 12.0;
	const double SlopePerY = SlopeInset / FMath::Max(1.0, H);

	const int32 LocalTeam = GetLocalPlayerTeamForWidget(this);
	const TArray<EBwayWinCondition>& FriendlyHistory = ShouldSwapTeamsForDisplay(LocalTeam) ? Team1WinHistory : Team0WinHistory;
	const TArray<EBwayWinCondition>& EnemyHistory = ShouldSwapTeamsForDisplay(LocalTeam) ? Team0WinHistory : Team1WinHistory;

	const FLinearColor EmptyBarFill(0.04f, 0.12f, 0.18f, 0.88f);
	const FLinearColor EmptyBarOutline(0.15f, 0.42f, 0.54f, 0.76f);
	const FLinearColor FriendlyLitFill(0.09f, 0.83f, 0.91f, 0.96f);
	const FLinearColor FriendlyLitOutline(0.58f, 0.98f, 1.0f, 0.96f);
	const FLinearColor EnemyLitFill(0.87f, 0.31f, 0.27f, 0.96f);
	const FLinearColor EnemyLitOutline(1.0f, 0.62f, 0.58f, 0.94f);

	++CurrentLayer;
	for (int32 Row = 0; Row < NumBars; ++Row)
	{
		// Top-to-bottom fill order (Row 0 = top bar, Row 1 = middle bar, Row 2 = bottom bar)
		const double Y0 = TopMargin + Row * (BarH + BarGap);
		const double Y1 = Y0 + BarH;

		// Left Wing (Friendly — slanted \ parallel to left trapezoid edge)
		const double LeftOuterX0 = OuterEdgePad + Y0 * SlopePerY;
		const double LeftOuterX1 = OuterEdgePad + Y1 * SlopePerY;
		const FVector2D LeftBar[4] = {
			FVector2D(LeftOuterX0, Y0),
			FVector2D(LeftOuterX0 + BarW, Y0),
			FVector2D(LeftOuterX1 + BarW, Y1),
			FVector2D(LeftOuterX1, Y1),
		};
		const bool bFriendlyLit = (Row < CachedFriendlyScore);
		DrawConvexPolygon(AllottedGeometry, OutDrawElements, CurrentLayer, LeftBar, bFriendlyLit ? FriendlyLitFill : EmptyBarFill);
		DrawPolygonOutline(AllottedGeometry, OutDrawElements, CurrentLayer + 1, LeftBar, bFriendlyLit ? FriendlyLitOutline : EmptyBarOutline, 1.3f, true);

		if (bFriendlyLit)
		{
			const EBwayWinCondition Cond = FriendlyHistory.IsValidIndex(Row) ? FriendlyHistory[Row] : EBwayWinCondition::GoalScored;
			if (UTexture2D* WinIcon = ResolveRoundWinConditionIcon(Cond))
			{
				const double IconSize = FMath::Clamp(BarH - 5.0, 10.0, 16.0);
				const FVector2D BarCenter(
					(LeftBar[0].X + LeftBar[1].X + LeftBar[2].X + LeftBar[3].X) * 0.25,
					(Y0 + Y1) * 0.5);
				FSlateBrush IconBrush;
				IconBrush.SetResourceObject(WinIcon);
				IconBrush.ImageSize = FVector2D(IconSize, IconSize);
				IconBrush.DrawAs = ESlateBrushDrawType::Image;
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					static_cast<uint32>(CurrentLayer + 2),
					AllottedGeometry.ToPaintGeometry(
						FVector2f(static_cast<float>(IconSize), static_cast<float>(IconSize)),
						FSlateLayoutTransform(FVector2f(
							static_cast<float>(BarCenter.X - IconSize * 0.5),
							static_cast<float>(BarCenter.Y - IconSize * 0.5)))),
					&IconBrush,
					ESlateDrawEffect::None,
					FLinearColor(1.0f, 1.0f, 1.0f, 0.96f));
			}
		}

		// Right Wing (Enemy — slanted / parallel to right trapezoid edge)
		const double RightOuterX0 = W - OuterEdgePad - Y0 * SlopePerY;
		const double RightOuterX1 = W - OuterEdgePad - Y1 * SlopePerY;
		const FVector2D RightBar[4] = {
			FVector2D(RightOuterX0 - BarW, Y0),
			FVector2D(RightOuterX0, Y0),
			FVector2D(RightOuterX1, Y1),
			FVector2D(RightOuterX1 - BarW, Y1),
		};
		const bool bEnemyLit = (Row < CachedEnemyScore);
		DrawConvexPolygon(AllottedGeometry, OutDrawElements, CurrentLayer, RightBar, bEnemyLit ? EnemyLitFill : EmptyBarFill);
		DrawPolygonOutline(AllottedGeometry, OutDrawElements, CurrentLayer + 1, RightBar, bEnemyLit ? EnemyLitOutline : EmptyBarOutline, 1.3f, true);

		if (bEnemyLit)
		{
			const EBwayWinCondition Cond = EnemyHistory.IsValidIndex(Row) ? EnemyHistory[Row] : EBwayWinCondition::GoalScored;
			if (UTexture2D* WinIcon = ResolveRoundWinConditionIcon(Cond))
			{
				const double IconSize = FMath::Clamp(BarH - 5.0, 10.0, 16.0);
				const FVector2D BarCenter(
					(RightBar[0].X + RightBar[1].X + RightBar[2].X + RightBar[3].X) * 0.25,
					(Y0 + Y1) * 0.5);
				FSlateBrush IconBrush;
				IconBrush.SetResourceObject(WinIcon);
				IconBrush.ImageSize = FVector2D(IconSize, IconSize);
				IconBrush.DrawAs = ESlateBrushDrawType::Image;
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					static_cast<uint32>(CurrentLayer + 2),
					AllottedGeometry.ToPaintGeometry(
						FVector2f(static_cast<float>(IconSize), static_cast<float>(IconSize)),
						FSlateLayoutTransform(FVector2f(
							static_cast<float>(BarCenter.X - IconSize * 0.5),
							static_cast<float>(BarCenter.Y - IconSize * 0.5)))),
					&IconBrush,
					ESlateDrawEffect::None,
					FLinearColor(1.0f, 1.0f, 1.0f, 0.96f));
			}
		}
	}
	CurrentLayer += 3;

	// 3. Large bold condensed opaque white Match Timer ("02:27") centered in the trapezoid
	if (bShowRoundTimer)
	{
		const FString TimerStr = Text_Timer
			? Text_Timer->GetText().ToString()
			: FormatRoundTime(ResolveAuthoritativeClockSeconds()).ToString();
		if (!TimerStr.IsEmpty())
		{
			const FSlateFontInfo TimerFont = GetBroadcastCondensedFont(44, 14, 1);
			const FVector2D TextSize = MeasureBroadcastText(TimerStr, TimerFont);
			const FVector2D TextPos((W - TextSize.X) * 0.5, (H - TextSize.Y) * 0.5 - 2.0);
			const FLinearColor TimerColor = bSuddenDeathWarningShown
				? FLinearColor(1.0f, 0.26f, 0.24f, 1.0f)
				: FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

			FSlateDrawElement::MakeText(
				OutDrawElements,
				static_cast<uint32>(++CurrentLayer),
				AllottedGeometry.ToPaintGeometry(
					FVector2f(static_cast<float>(TextSize.X + 24.0), static_cast<float>(TextSize.Y + 12.0)),
					FSlateLayoutTransform(FVector2f(static_cast<float>(TextPos.X), static_cast<float>(TextPos.Y)))),
				TimerStr,
				TimerFont,
				ESlateDrawEffect::None,
				TimerColor);
		}
	}

	return CurrentLayer;
}

FText UBwayCaptureTheRelicScoreWidget::GetDisplayedTimerText() const
{
	return Text_Timer ? Text_Timer->GetText() : FText::GetEmpty();
}
