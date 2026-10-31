// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayMatchHUDWidgetBase.h"
#include "UI/BwayMatchBroadcastOverlayWidget.h"
#include "BwayGameState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayCharacterWithAbilities.h"
#include "Relic/RelicActor.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "PrimaryGameLayout.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchHUDWidgetBase)

ABwayGameState* UBwayMatchHUDWidgetBase::ResolveBwayGameState(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	if (UWorld* World = Widget->GetWorld())
	{
		return World->GetGameState<ABwayGameState>();
	}

	return nullptr;
}

FText UBwayMatchHUDWidgetBase::FormatRoundTime(const int32 TotalSeconds)
{
	const int32 ClampedSeconds = FMath::Max(0, TotalSeconds);
	const int32 Minutes = ClampedSeconds / 60;
	const int32 Seconds = ClampedSeconds % 60;

	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds));
}

int32 UBwayMatchHUDWidgetBase::GetTeamScoreFromWorld(const UWorld* World, const int32 TeamIndex)
{
	if (!World || TeamIndex < 0)
	{
		return 0;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			return Scoring->GetTeamScore(TeamIndex);
		}
	}

	return 0;
}

int32 UBwayMatchHUDWidgetBase::GetRoundTimeRemainingFromWorld(const UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		return GameState->GetRoundTimeRemaining();
	}

	return 0;
}

int32 UBwayMatchHUDWidgetBase::GetRelicPossessingTeamFromWorld(const UWorld* World)
{
	if (!World)
	{
		return -1;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			return RelicMgr->GetRelicPossessingTeam();
		}
	}

	return -1;
}

bool UBwayMatchHUDWidgetBase::IsRelicCarriedInWorld(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			if (const ARelicActor* Relic = RelicMgr->GetRelicActor())
			{
				return Relic->GetCurrentState() == ERelicState::Carried;
			}
		}
	}

	return false;
}

FText UBwayMatchHUDWidgetBase::GetRelicCarrierNameFromWorld(const UWorld* World)
{
	if (!World)
	{
		return FText::GetEmpty();
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			if (const ARelicActor* Relic = RelicMgr->GetRelicActor())
			{
				if (const ABwayCharacterWithAbilities* Carrier = Relic->CurrentCarrier)
				{
					if (const APlayerState* CarrierPS = Carrier->GetPlayerState())
					{
						return FText::FromString(CarrierPS->GetPlayerName());
					}
				}
			}
		}
	}

	return FText::GetEmpty();
}

int32 UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForPlayerController(const APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return -1;
	}

	const APlayerState* PS = PlayerController->GetPlayerState<APlayerState>();
	if (!PS || PS->IsOnlyASpectator())
	{
		return -1;
	}

	if (const UWorld* World = PlayerController->GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			return GameState->GetPlayerTeam(PS);
		}
	}

	return -1;
}

int32 UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return -1;
	}

	return GetLocalPlayerTeamForPlayerController(Widget->GetOwningPlayer());
}

bool UBwayMatchHUDWidgetBase::ShouldSwapTeamsForDisplay(const int32 LocalPlayerTeamIndex)
{
	return LocalPlayerTeamIndex == 1;
}

int32 UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(const int32 GameTeamIndex, const int32 LocalPlayerTeamIndex)
{
	if (GameTeamIndex < 0)
	{
		return -1;
	}

	if (!ShouldSwapTeamsForDisplay(LocalPlayerTeamIndex))
	{
		return GameTeamIndex;
	}

	return GameTeamIndex == 0 ? 1 : 0;
}

int32 UBwayMatchHUDWidgetBase::MapDisplaySlotToGameTeam(const int32 DisplaySlotIndex, const int32 LocalPlayerTeamIndex)
{
	if (DisplaySlotIndex < 0 || DisplaySlotIndex > 1)
	{
		return -1;
	}

	if (!ShouldSwapTeamsForDisplay(LocalPlayerTeamIndex))
	{
		return DisplaySlotIndex;
	}

	return DisplaySlotIndex == 0 ? 1 : 0;
}

int32 UBwayMatchHUDWidgetBase::GetDisplayTeamScore(
	const UWorld* World,
	const int32 DisplaySlotIndex,
	const int32 LocalPlayerTeamIndex)
{
	const int32 GameTeamIndex = MapDisplaySlotToGameTeam(DisplaySlotIndex, LocalPlayerTeamIndex);
	return GetTeamScoreFromWorld(World, GameTeamIndex);
}

void UBwayMatchHUDWidgetBase::RemapGameTeamScoresForDisplay(
	const int32 LocalPlayerTeamIndex,
	const int32 GameTeam0Score,
	const int32 GameTeam1Score,
	int32& OutDisplayLeftScore,
	int32& OutDisplayRightScore)
{
	if (ShouldSwapTeamsForDisplay(LocalPlayerTeamIndex))
	{
		OutDisplayLeftScore = GameTeam1Score;
		OutDisplayRightScore = GameTeam0Score;
	}
	else
	{
		OutDisplayLeftScore = GameTeam0Score;
		OutDisplayRightScore = GameTeam1Score;
	}
}

FBwayPostRoundSummaryData UBwayMatchHUDWidgetBase::RemapPostRoundSummaryForDisplay(
	const FBwayPostRoundSummaryData& Summary,
	const int32 LocalPlayerTeamIndex)
{
	FBwayPostRoundSummaryData DisplaySummary = Summary;

	if (ShouldSwapTeamsForDisplay(LocalPlayerTeamIndex))
	{
		Swap(DisplaySummary.Team0Score, DisplaySummary.Team1Score);
		Swap(DisplaySummary.Team0Stats, DisplaySummary.Team1Stats);
		Swap(DisplaySummary.Team0RoundWinConditions, DisplaySummary.Team1RoundWinConditions);
	}

	return DisplaySummary;
}

int32 UBwayMatchHUDWidgetBase::GetDisplayRelicPossessingTeam(const UWorld* World, const int32 LocalPlayerTeamIndex)
{
	const int32 GameTeamIndex = GetRelicPossessingTeamFromWorld(World);
	return MapGameTeamToDisplaySlot(GameTeamIndex, LocalPlayerTeamIndex);
}

int32 UBwayMatchHUDWidgetBase::GetCurrentRoundNumberFromWorld(const UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		return GameState->GetCurrentRoundNumber();
	}

	return 0;
}

int32 UBwayMatchHUDWidgetBase::GetSuddenDeathWarningSecondsFromWorld(const UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRoundManagementComponent* RoundMgmt = GameState->FindComponentByClass<UBwayRoundManagementComponent>())
		{
			return RoundMgmt->SuddenDeathWarningSeconds;
		}
	}

	return 0;
}

int32 UBwayMatchHUDWidgetBase::GetPointsToWinFromWorld(const UWorld* World)
{
	if (!World)
	{
		return 3;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRoundManagementComponent* RoundMgmt = GameState->FindComponentByClass<UBwayRoundManagementComponent>())
		{
			return FMath::Max(1, RoundMgmt->PointsToWin);
		}
	}

	return 3;
}

int32 UBwayMatchHUDWidgetBase::GetDisplayRelicTerritoryHalf(const UWorld* World, const int32 LocalPlayerTeamIndex)
{
	if (!World)
	{
		return -1;
	}

	float Tolerance = 1.0f;
	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRoundManagementComponent* RoundMgmt = GameState->FindComponentByClass<UBwayRoundManagementComponent>())
		{
			Tolerance = RoundMgmt->MidfieldTolerance;
		}
	}

	const int32 GameHalfTeam = UBwayMidfieldRulesLibrary::GetRelicHalfFromWorld(World, Tolerance);
	if (GameHalfTeam < 0)
	{
		return -1;
	}

	return MapGameTeamToDisplaySlot(GameHalfTeam, LocalPlayerTeamIndex);
}

bool UBwayMatchHUDWidgetBase::IsLocalGameplayHUDOccludedByMenu(const APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	if (UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(const_cast<APlayerController*>(PlayerController)))
	{
		for (const TCHAR* LayerName : { TEXT("UI.Layer.Menu"), TEXT("UI.Layer.GameMenu"), TEXT("UI.Layer.Modal") })
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(LayerName), false);
			if (UCommonActivatableWidgetContainerBase* Layer = Tag.IsValid() ? Layout->GetLayerWidget(Tag) : nullptr)
			{
				if (const UCommonActivatableWidget* Active = Layer->GetActiveWidget();
					Active && Active->IsActivated() && Active->IsVisible())
				{
					return true;
				}
			}
		}
	}

	return false;
}

FSlateFontInfo UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(
	const int32 PointSize,
	const int32 LetterSpacing,
	const int32 OutlineSize,
	const FLinearColor& OutlineColor)
{
	static TWeakObjectPtr<const UObject> CachedCondensedFontObject;
	static bool bAttemptedFontLoad = false;

	if (!CachedCondensedFontObject.IsValid() && !bAttemptedFontLoad)
	{
		bAttemptedFontLoad = true;
		CachedCondensedFontObject = LoadObject<UObject>(
			nullptr,
			TEXT("/BreakawayCore/UI/Fonts/refrigeratordeluxe-heavy_Font.refrigeratordeluxe-heavy_Font"));
	}

	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(6, PointSize));
	if (CachedCondensedFontObject.IsValid())
	{
		FontInfo.FontObject = CachedCondensedFontObject.Get();
		FontInfo.TypefaceFontName = NAME_None;
	}
	FontInfo.Size = FMath::Max(6, PointSize);
	FontInfo.LetterSpacing = LetterSpacing;
	FontInfo.OutlineSettings.OutlineSize = FMath::Max(0, OutlineSize);
	FontInfo.OutlineSettings.OutlineColor = OutlineColor;
	return FontInfo;
}

FVector2D UBwayMatchHUDWidgetBase::MeasureBroadcastText(const FString& Text, const FSlateFontInfo& FontInfo)
{
	if (Text.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}

	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FVector2D(FontMeasure->Measure(Text, FontInfo));
	}

	return FVector2D(static_cast<float>(Text.Len()) * FontInfo.Size * 0.56f, FontInfo.Size * 1.15f);
}

void UBwayMatchHUDWidgetBase::DrawConvexPolygon(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const TConstArrayView<FVector2D> Points,
	const FLinearColor& Color)
{
	if (Points.Num() < 3 || Color.A <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	if (!WhiteBrush || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		return;
	}

	const FSlateResourceHandle ResourceHandle = WhiteBrush->GetRenderingResource();
	const FSlateRenderTransform& RenderTransform = AllottedGeometry.GetAccumulatedRenderTransform();
	FColor VertexColor = Color.ToFColor(true);
	VertexColor.A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Color.A * 255.0f), 0, 255));

	TArray<FSlateVertex> Verts;
	Verts.Reserve(Points.Num());
	for (const FVector2D& Pt : Points)
	{
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			RenderTransform,
			FVector2f(static_cast<float>(Pt.X), static_cast<float>(Pt.Y)),
			FVector2f(0.5f, 0.5f),
			VertexColor));
	}

	TArray<SlateIndex> Indices;
	Indices.Reserve((Points.Num() - 2) * 3);
	for (int32 Idx = 1; Idx + 1 < Points.Num(); ++Idx)
	{
		Indices.Add(0);
		Indices.Add(static_cast<SlateIndex>(Idx));
		Indices.Add(static_cast<SlateIndex>(Idx + 1));
	}

	FSlateDrawElement::MakeCustomVerts(
		OutDrawElements,
		static_cast<uint32>(FMath::Max(0, LayerId)),
		ResourceHandle,
		Verts,
		Indices,
		nullptr,
		0,
		0);
}

void UBwayMatchHUDWidgetBase::DrawPolygonOutline(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const TConstArrayView<FVector2D> Points,
	const FLinearColor& Color,
	const float Thickness,
	const bool bClosed)
{
	if (Points.Num() < 2 || Color.A <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TArray<FVector2f> LinePoints;
	LinePoints.Reserve(Points.Num() + (bClosed ? 1 : 0));
	for (const FVector2D& Pt : Points)
	{
		LinePoints.Emplace(static_cast<float>(Pt.X), static_cast<float>(Pt.Y));
	}
	if (bClosed && Points.Num() >= 3)
	{
		LinePoints.Emplace(static_cast<float>(Points[0].X), static_cast<float>(Points[0].Y));
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		static_cast<uint32>(FMath::Max(0, LayerId)),
		AllottedGeometry.ToPaintGeometry(),
		MoveTemp(LinePoints),
		ESlateDrawEffect::None,
		Color,
		true,
		Thickness);
}

void UBwayMatchHUDWidgetBase::DrawSolidBox(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FVector2D& Offset,
	const FVector2D& Size,
	const FLinearColor& Color)
{
	if (Size.X <= 0.0f || Size.Y <= 0.0f || Color.A <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	if (!WhiteBrush)
	{
		return;
	}

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		static_cast<uint32>(FMath::Max(0, LayerId)),
		AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(Size.X), static_cast<float>(Size.Y)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(Offset.X), static_cast<float>(Offset.Y)))),
		WhiteBrush,
		ESlateDrawEffect::None,
		Color);
}

void UBwayMatchHUDWidgetBase::ApplyBroadcastHUDLayoutOverrides(APlayerController* OwningPlayer)
{
	if (!OwningPlayer || !OwningPlayer->IsLocalController())
	{
		return;
	}

	UUserWidget* MatchHUDLayout = nullptr;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Candidate = *It;
		if (!IsValid(Candidate)
			|| Candidate->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
			|| Candidate->GetOwningPlayer() != OwningPlayer
			|| !Candidate->WidgetTree)
		{
			continue;
		}

		if (Candidate->WidgetTree->FindWidget(TEXT("ExtensionPoint_TeamScore")) != nullptr)
		{
			MatchHUDLayout = Candidate;
			break;
		}
	}

	if (!MatchHUDLayout || !MatchHUDLayout->WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(MatchHUDLayout->WidgetTree->FindWidget(TEXT("CanvasPanel_0")));
	if (!RootCanvas)
	{
		MatchHUDLayout->WidgetTree->ForEachWidget([&RootCanvas](UWidget* Widget)
		{
			if (!RootCanvas)
			{
				RootCanvas = Cast<UCanvasPanel>(Widget);
			}
		});
	}
	if (!RootCanvas)
	{
		return;
	}

	auto ConfigureExtensionSlot = [&](
		const TCHAR* WidgetName,
		const FAnchors& Anchors,
		const FVector2D& Alignment,
		const FVector2D& Position,
		const bool bAutoSize,
		const int32 ZOrder,
		const bool bCollapseWidget = false)
	{
		UWidget* Target = MatchHUDLayout->WidgetTree->FindWidget(WidgetName);
		if (!Target)
		{
			return;
		}

		if (bCollapseWidget)
		{
			Target->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Target->Slot))
		{
			CanvasSlot->SetAnchors(Anchors);
			CanvasSlot->SetAlignment(Alignment);
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetAutoSize(bAutoSize);
			CanvasSlot->SetZOrder(ZOrder);
		}
	};

	// Top broadcast bar: center trapezoid flush to Y=0, flanked symmetrically by Friendly & Enemy 4v4 portrait wings
	ConfigureExtensionSlot(TEXT("ExtensionPoint_TeamScore"), FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 0.0f), true, 2);
	ConfigureExtensionSlot(TEXT("ExtensionPoint_Team1Portraits"), FAnchors(0.5f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-364.0f, 4.0f), true, 2);
	ConfigureExtensionSlot(TEXT("ExtensionPoint_Team2Portraits"), FAnchors(0.5f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(364.0f, 4.0f), true, 2);
	ConfigureExtensionSlot(TEXT("ExtensionPoint_ModeStatus"), FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 94.0f), true, 2);

	// Suppress ShooterCore's generic elimination feed and weapon reticle host in favor of Breakaway's Match Event Toast & Crosshair
	ConfigureExtensionSlot(TEXT("ExtensionPoint_ModeStatus_1"), FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D::ZeroVector, true, 0, true);
	ConfigureExtensionSlot(TEXT("ExtensionPoint_Reticle"), FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, true, 0, true);

	// Bottom-center vitals row ([500] [=========Health Bar=========] [783 ◆]) directly above hexagonal Ability Bar pedestal
	ConfigureExtensionSlot(TEXT("ExtensionPoint_Health"), FAnchors(0.5f, 1.0f), FVector2D(1.0f, 0.5f), FVector2D(172.0f, -145.0f), true, 2);
	ConfigureExtensionSlot(TEXT("GoldExtensionPoint"), FAnchors(0.5f, 1.0f), FVector2D(0.0f, 0.5f), FVector2D(186.0f, -145.0f), true, 2);
	ConfigureExtensionSlot(TEXT("AbilityBarExtensionPoint"), FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(-4.0f, -6.0f), true, 2);

	// Mount UBwayMatchBroadcastOverlayWidget once onto CanvasPanel_0 at ZOrder=0 (behind slot widgets at ZOrder=2)
	bool bOverlayAlreadyMounted = false;
	const TArray<UWidget*> CanvasChildren = RootCanvas->GetAllChildren();
	for (UWidget* Child : CanvasChildren)
	{
		if (Cast<UBwayMatchBroadcastOverlayWidget>(Child) != nullptr)
		{
			bOverlayAlreadyMounted = true;
			break;
		}
	}

	if (!bOverlayAlreadyMounted)
	{
		if (UBwayMatchBroadcastOverlayWidget* BroadcastOverlay = CreateWidget<UBwayMatchBroadcastOverlayWidget>(
			OwningPlayer,
			UBwayMatchBroadcastOverlayWidget::StaticClass()))
		{
			BroadcastOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(BroadcastOverlay))
			{
				OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				OverlaySlot->SetOffsets(FMargin(0.0f));
				OverlaySlot->SetZOrder(0);
			}
		}
	}
}

void UBwayMatchHUDWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyBroadcastHUDLayoutOverrides(GetOwningPlayer());
	BindToMatchData();
}

void UBwayMatchHUDWidgetBase::NativeDestruct()
{
	UnbindFromMatchData();
	Super::NativeDestruct();
}

ABwayGameState* UBwayMatchHUDWidgetBase::GetBwayGameState() const
{
	if (CachedGameState.IsValid())
	{
		return CachedGameState.Get();
	}

	return ResolveBwayGameState(this);
}

UBwayScoringComponent* UBwayMatchHUDWidgetBase::GetScoringComponent() const
{
	if (CachedScoringComponent.IsValid())
	{
		return CachedScoringComponent.Get();
	}

	if (const ABwayGameState* GameState = GetBwayGameState())
	{
		return GameState->FindComponentByClass<UBwayScoringComponent>();
	}

	return nullptr;
}

UBwayRelicManagerComponent* UBwayMatchHUDWidgetBase::GetRelicManagerComponent() const
{
	if (const ABwayGameState* GameState = GetBwayGameState())
	{
		return GameState->FindComponentByClass<UBwayRelicManagerComponent>();
	}

	return nullptr;
}

void UBwayMatchHUDWidgetBase::BindToMatchData()
{
	if (bBoundToMatchData)
	{
		return;
	}

	if (ABwayGameState* GameState = ResolveBwayGameState(this))
	{
		CachedGameState = GameState;

		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			CachedScoringComponent = Scoring;
			Scoring->OnTeamScoreChanged.AddDynamic(this, &UBwayMatchHUDWidgetBase::HandleTeamScoreChanged);
		}

		GameState->OnRoundTimeChanged.AddDynamic(this, &UBwayMatchHUDWidgetBase::HandleRoundTimeChanged);
		GameState->OnRoundStateChanged.AddDynamic(this, &UBwayMatchHUDWidgetBase::HandleRoundStateChanged);
		bBoundToMatchData = true;
	}
}

void UBwayMatchHUDWidgetBase::UnbindFromMatchData()
{
	if (!bBoundToMatchData)
	{
		return;
	}

	if (CachedScoringComponent.IsValid())
	{
		CachedScoringComponent->OnTeamScoreChanged.RemoveDynamic(this, &UBwayMatchHUDWidgetBase::HandleTeamScoreChanged);
	}

	if (CachedGameState.IsValid())
	{
		CachedGameState->OnRoundTimeChanged.RemoveDynamic(this, &UBwayMatchHUDWidgetBase::HandleRoundTimeChanged);
		CachedGameState->OnRoundStateChanged.RemoveDynamic(this, &UBwayMatchHUDWidgetBase::HandleRoundStateChanged);
	}

	CachedScoringComponent.Reset();
	CachedGameState.Reset();
	bBoundToMatchData = false;
}

void UBwayMatchHUDWidgetBase::HandleTeamScoreChanged(int32 TeamIndex, int32 NewScore)
{
	NotifyTeamScoreChanged(TeamIndex, NewScore);
}

void UBwayMatchHUDWidgetBase::HandleRoundTimeChanged(int32 SecondsRemaining)
{
	NotifyRoundTimeChanged(SecondsRemaining);
}

void UBwayMatchHUDWidgetBase::HandleRoundStateChanged(FName NewState)
{
	NotifyRoundStateChanged(NewState);
}
