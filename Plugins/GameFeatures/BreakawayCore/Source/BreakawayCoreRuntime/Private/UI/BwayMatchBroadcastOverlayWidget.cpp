// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayMatchBroadcastOverlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Buildable/BuildableBase.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Combat/BwayCombatNumberPopComponent.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/CanvasPanel.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "EnhancedInputSubsystems.h"
#include "GameModes/BwayMatchPhaseTypes.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Relic/RelicActor.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/BwayHUDHelpers.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchBroadcastOverlayWidget)

namespace BwayBroadcastOverlayConstants
{
	constexpr double EventToastDurationSeconds = 3.4;
	constexpr double HitConfirmFlashSeconds = 0.20;
}

UBwayMatchBroadcastOverlayWidget::UBwayMatchBroadcastOverlayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayMatchBroadcastOverlayWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(false);
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BroadcastOverlayCanvas"));
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UBwayMatchBroadcastOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BroadcastOverlayCanvas"));
	}
	RefreshBroadcastState();
}

void UBwayMatchBroadcastOverlayWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	(void)MyGeometry;
	(void)InDeltaTime;

	RefreshBroadcastState();
	InvalidateLayoutAndVolatility();
}

double UBwayMatchBroadcastOverlayWidget::GetOverlayClockSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetRealTimeSeconds();
	}
	return FPlatformTime::Seconds();
}

void UBwayMatchBroadcastOverlayWidget::SetInputDeviceOverrideForTest(const bool bEnableOverride, const bool bPreferGamepad)
{
	bTestOverrideInputDevice = bEnableOverride;
	bTestPreferGamepad = bPreferGamepad;
}

void UBwayMatchBroadcastOverlayWidget::PushEventToast(
	const FText& Headline,
	const FString& PlayerName,
	UTexture2D* PortraitTexture,
	const bool bIsFriendly)
{
	const double Now = GetOverlayClockSeconds();
	ActiveToast.Headline = Headline;
	ActiveToast.PlayerName = PlayerName;
	ActiveToast.PortraitTexture = PortraitTexture;
	ActiveToast.bIsFriendly = bIsFriendly;
	ActiveToast.StartRealTime = Now;
	ActiveToast.ExpireRealTime = Now + BwayBroadcastOverlayConstants::EventToastDurationSeconds;
	InvalidateLayoutAndVolatility();
}

bool UBwayMatchBroadcastOverlayWidget::IsEventToastActive() const
{
	const double Now = GetOverlayClockSeconds();
	return !ActiveToast.Headline.IsEmpty() && ActiveToast.ExpireRealTime > Now;
}

bool UBwayMatchBroadcastOverlayWidget::IsShopPromptVisible() const
{
	const APlayerController* OwningPC = GetOwningPlayer();
	if (!OwningPC || UBwayMatchHUDWidgetBase::IsLocalGameplayHUDOccludedByMenu(OwningPC))
	{
		return false;
	}

	if (const ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(OwningPC))
	{
		if (BwayPC->IsScoreboardVisible())
		{
			return false;
		}
	}

	const UWorld* World = GetWorld();
	const ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	const UBwayRoundManagementComponent* RoundMgmt = GS ? GS->GetRoundManagement() : nullptr;
	if (RoundMgmt && RoundMgmt->IsOrchestratingMatchFlow())
	{
		const EBwayMatchPhase Phase = RoundMgmt->GetCurrentMatchPhase();
		if (Phase != EBwayMatchPhase::Warmup && Phase != EBwayMatchPhase::Playing)
		{
			return false;
		}
	}

	const ABwayPlayerState* LocalPS = OwningPC->GetPlayerState<ABwayPlayerState>();
	return LocalPS && LocalPS->IsPlayerInHealingZone();
}

bool UBwayMatchBroadcastOverlayWidget::IsCrosshairVisible() const
{
	const APlayerController* OwningPC = GetOwningPlayer();
	if (!OwningPC || UBwayMatchHUDWidgetBase::IsLocalGameplayHUDOccludedByMenu(OwningPC))
	{
		return false;
	}

	if (const ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(OwningPC))
	{
		if (BwayPC->IsScoreboardVisible())
		{
			return false;
		}
	}

	const UWorld* World = GetWorld();
	const ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	const UBwayRoundManagementComponent* RoundMgmt = GS ? GS->GetRoundManagement() : nullptr;
	if (RoundMgmt && RoundMgmt->IsOrchestratingMatchFlow())
	{
		if (RoundMgmt->GetCurrentMatchPhase() != EBwayMatchPhase::Playing
			|| RoundMgmt->GetCurrentRoundState() != ERoundState::RoundActive)
		{
			return false;
		}
	}

	const ABwayPlayerState* LocalPS = OwningPC->GetPlayerState<ABwayPlayerState>();
	return LocalPS && UBwayHUDHelpers::IsPlayerAliveForHUD(LocalPS);
}

FText UBwayMatchBroadcastOverlayWidget::GetResolvedShopKeyLabel() const
{
	const UInputAction* ShopAction = CachedUpgradeShopAction;
	if (!ShopAction)
	{
		if (const ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(GetOwningPlayer()))
		{
			ShopAction = BwayPC->GetUpgradeShopInputAction();
		}
		if (!ShopAction)
		{
			ShopAction = LoadObject<UInputAction>(
				nullptr,
				TEXT("/BreakawayCore/Input/Actions/IA_BW_UpgradeShop.IA_BW_UpgradeShop"));
		}
	}
	if (!ShopAction)
	{
		return NSLOCTEXT("BwayBroadcastHUD", "ShopKeyFallback", "B");
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
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			BoundKeys = InputSubsystem->QueryKeysMappedToAction(ShopAction);
		}
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
		return NSLOCTEXT("BwayBroadcastHUD", "ShopKeyFallback", "B");
	}

	const FKey* PreferredKey = BoundKeys.FindByPredicate(
		[bPreferGamepad](const FKey& Key)
		{
			return Key.IsValid() && (Key.IsGamepadKey() == bPreferGamepad);
		});

	const FKey ChosenKey = PreferredKey ? *PreferredKey : BoundKeys[0];
	return ChosenKey.IsValid()
		? ChosenKey.GetDisplayName(/*bLongDisplayName*/ false)
		: NSLOCTEXT("BwayBroadcastHUD", "ShopKeyFallback", "B");
}

void UBwayMatchBroadcastOverlayWidget::RefreshBroadcastState()
{
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

	PollMatchEventDeltas();
}

void UBwayMatchBroadcastOverlayWidget::PollMatchEventDeltas()
{
	UWorld* World = GetWorld();
	ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	if (!GS || GS->PlayerArray.IsEmpty())
	{
		return;
	}

	const UBwayRoundManagementComponent* RoundMgmt = GS->GetRoundManagement();
	const int32 CurrentRound = RoundMgmt ? RoundMgmt->GetCurrentRoundNumber() : 1;
	const EBwayMatchPhase Phase = RoundMgmt ? RoundMgmt->GetCurrentMatchPhase() : EBwayMatchPhase::Playing;

	if (LastTrackedRoundNumber != INDEX_NONE && CurrentRound != LastTrackedRoundNumber)
	{
		// Clear Relic possession baseline across round boundaries so initial center spawn doesn't false-fire,
		// while preserving cumulative stat baselines.
		for (TPair<int32, FPlayerStatSnapshot>& Pair : PlayerSnapshots)
		{
			Pair.Value.bHasRelic = false;
		}
	}
	LastTrackedRoundNumber = CurrentRound;

	const int32 LocalTeam = UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(this);

	int32 BestPriority = 99;
	FText BestHeadline;
	FString BestPlayerName;
	UTexture2D* BestPortrait = nullptr;
	bool bBestFriendly = true;

	for (APlayerState* Entry : GS->PlayerArray)
	{
		const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Entry);
		if (!BwayPS)
		{
			continue;
		}

		const int32 PlayerKey = BwayPS->GetPlayerId() != 0
			? BwayPS->GetPlayerId()
			: static_cast<int32>(BwayPS->GetUniqueID());

		FPlayerStatSnapshot CurrentSnap;
		CurrentSnap.ObjectiveScore = BwayPS->GetObjectiveScore();
		CurrentSnap.Interceptions = BwayPS->GetInterceptions();
		CurrentSnap.ForcedFumbles = BwayPS->GetForcedFumbles();
		CurrentSnap.Kills = BwayPS->GetKills();
		CurrentSnap.bHasRelic = BwayPS->bHasRelic;

		if (bHasBaselineSnapshots)
		{
			if (const FPlayerStatSnapshot* PrevSnap = PlayerSnapshots.Find(PlayerKey))
			{
				const bool bAllowEvents = (Phase == EBwayMatchPhase::Playing || Phase == EBwayMatchPhase::PostRound);
				if (bAllowEvents)
				{
					int32 EventPriority = 99;
					FText CandidateHeadline;

					if (CurrentSnap.ObjectiveScore > PrevSnap->ObjectiveScore)
					{
						EventPriority = 1;
						CandidateHeadline = NSLOCTEXT("BwayBroadcastHUD", "ToastGoalScored", "GOAL SCORED!");
					}
					else if (CurrentSnap.Interceptions > PrevSnap->Interceptions)
					{
						EventPriority = 2;
						CandidateHeadline = NSLOCTEXT("BwayBroadcastHUD", "ToastInterception", "INTERCEPTION!");
					}
					else if (CurrentSnap.ForcedFumbles > PrevSnap->ForcedFumbles)
					{
						EventPriority = 3;
						CandidateHeadline = NSLOCTEXT("BwayBroadcastHUD", "ToastFumbleForced", "FUMBLE FORCED!");
					}
					else if (CurrentSnap.bHasRelic && !PrevSnap->bHasRelic && Phase == EBwayMatchPhase::Playing)
					{
						EventPriority = 4;
						CandidateHeadline = NSLOCTEXT("BwayBroadcastHUD", "ToastRelicTaken", "RELIC TAKEN!");
					}
					else if (CurrentSnap.Kills > PrevSnap->Kills)
					{
						EventPriority = 5;
						CandidateHeadline = NSLOCTEXT("BwayBroadcastHUD", "ToastElimination", "ELIMINATION!");
					}

					if (EventPriority < BestPriority)
					{
						BestPriority = EventPriority;
						BestHeadline = CandidateHeadline;
						BestPlayerName = BwayPS->GetPlayerName();
						BestPortrait = UBwayHUDHelpers::GetPlayerHeroPortrait(BwayPS);
						const int32 EventTeam = GS->GetPlayerTeam(BwayPS);
						const int32 DisplaySlot = UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(EventTeam, LocalTeam);
						bBestFriendly = (DisplaySlot != 1);
					}
				}
			}
		}

		PlayerSnapshots.Add(PlayerKey, CurrentSnap);
	}

	bHasBaselineSnapshots = true;

	if (BestPriority < 99 && !BestHeadline.IsEmpty())
	{
		PushEventToast(BestHeadline, BestPlayerName, BestPortrait, bBestFriendly);
	}
}

int32 UBwayMatchBroadcastOverlayWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	int32 CurrentLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (LocalSize.X < 64.0f || LocalSize.Y < 64.0f)
	{
		return CurrentLayer;
	}

	const APlayerController* OwningPC = GetOwningPlayer();
	const bool bMenuOccluded = UBwayMatchHUDWidgetBase::IsLocalGameplayHUDOccludedByMenu(OwningPC);
	const ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(OwningPC);
	const bool bScoreboardOpen = BwayPC && BwayPC->IsScoreboardVisible();

	const UWorld* World = GetWorld();
	const ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	const UBwayRoundManagementComponent* RoundMgmt = GS ? GS->GetRoundManagement() : nullptr;
	const EBwayMatchPhase Phase = RoundMgmt ? RoundMgmt->GetCurrentMatchPhase() : EBwayMatchPhase::Playing;
	const bool bInPostOverlay = (Phase == EBwayMatchPhase::PostRound || Phase == EBwayMatchPhase::PostMatch);

	// 1. Always paint top-bar portrait backing wings & bottom hexagonal ability pedestal behind the HUD slot widgets
	if (!bInPostOverlay)
	{
		CurrentLayer = PaintTopPortraitBackings(AllottedGeometry, OutDrawElements, CurrentLayer, LocalSize);
		CurrentLayer = PaintBottomAbilityPedestal(AllottedGeometry, OutDrawElements, CurrentLayer, LocalSize);
	}

	if (bMenuOccluded || bScoreboardOpen || bInPostOverlay)
	{
		return CurrentLayer;
	}

	// 2. Upper-right live arena minimap
	CurrentLayer = PaintLiveMinimap(AllottedGeometry, OutDrawElements, CurrentLayer, LocalSize);

	// 3. Mid-left match event toast (INTERCEPTION!, GOAL SCORED!, etc.)
	if (IsEventToastActive())
	{
		CurrentLayer = PaintEventToast(AllottedGeometry, OutDrawElements, CurrentLayer, LocalSize);
	}

	// 4. Bottom-left UPGRADES [B] shop prompt (only visible near friendly team goal / healing zone)
	if (IsShopPromptVisible())
	{
		CurrentLayer = PaintShopPrompt(AllottedGeometry, OutDrawElements, CurrentLayer, LocalSize);
	}

	// 5. Center 3-spoke Breakaway crosshair
	if (IsCrosshairVisible())
	{
		CurrentLayer = PaintCrosshair(AllottedGeometry, OutDrawElements, CurrentLayer, LocalSize);
	}

	return CurrentLayer;
}

int32 UBwayMatchBroadcastOverlayWidget::PaintTopPortraitBackings(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FVector2D& LocalSize) const
{
	const double CenterX = LocalSize.X * 0.5;
	const FLinearColor PanelFill(0.03f, 0.09f, 0.14f, 0.84f);
	const FLinearColor FriendlyTrim(0.09f, 0.62f, 0.78f, 0.78f);
	const FLinearColor EnemyTrim(0.68f, 0.24f, 0.24f, 0.78f);

	// Friendly left portrait wing backing: angled inner right edge parallel to the center trapezoid's left slope
	const FVector2D LeftWing[4] = {
		FVector2D(CenterX - 696.0, 0.0),
		FVector2D(CenterX - 350.0, 0.0),
		FVector2D(CenterX - 332.0, 88.0),
		FVector2D(CenterX - 696.0, 88.0),
	};
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(AllottedGeometry, OutDrawElements, ++LayerId, LeftWing, PanelFill);
	const FVector2D LeftBottomTrim[3] = { LeftWing[3], LeftWing[2], LeftWing[1] };
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(AllottedGeometry, OutDrawElements, ++LayerId, LeftBottomTrim, FriendlyTrim, 1.5f, false);

	// Enemy right portrait wing backing: angled inner left edge parallel to the center trapezoid's right slope
	const FVector2D RightWing[4] = {
		FVector2D(CenterX + 350.0, 0.0),
		FVector2D(CenterX + 696.0, 0.0),
		FVector2D(CenterX + 696.0, 88.0),
		FVector2D(CenterX + 332.0, 88.0),
	};
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(AllottedGeometry, OutDrawElements, ++LayerId, RightWing, PanelFill);
	const FVector2D RightBottomTrim[3] = { RightWing[0], RightWing[3], RightWing[2] };
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(AllottedGeometry, OutDrawElements, ++LayerId, RightBottomTrim, EnemyTrim, 1.5f, false);

	return LayerId;
}

int32 UBwayMatchBroadcastOverlayWidget::PaintBottomAbilityPedestal(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FVector2D& LocalSize) const
{
	const double CenterX = LocalSize.X * 0.5;
	const double BottomY = LocalSize.Y;
	const double TopY = BottomY - 124.0;

	// Outer trapezoidal gunmetal pedestal flush to bottom of screen (/ left slope, \ right slope)
	const FVector2D OuterPedestal[4] = {
		FVector2D(CenterX - 284.0, TopY),
		FVector2D(CenterX + 284.0, TopY),
		FVector2D(CenterX + 346.0, BottomY),
		FVector2D(CenterX - 346.0, BottomY),
	};
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		OuterPedestal,
		FLinearColor(0.08f, 0.10f, 0.13f, 0.90f));

	// Darker recessed left alcove for the 1 (Buildable) hex slot
	const FVector2D BuildableAlcove[4] = {
		FVector2D(CenterX - 280.0, TopY + 4.0),
		FVector2D(CenterX - 186.0, TopY + 4.0),
		FVector2D(CenterX - 200.0, BottomY),
		FVector2D(CenterX - 340.0, BottomY),
	};
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		BuildableAlcove,
		FLinearColor(0.03f, 0.03f, 0.04f, 0.72f));

	// Brushed metallic upper-bevel highlight + angled wing trim
	const FVector2D PedestalRim[4] = {
		OuterPedestal[3],
		OuterPedestal[0],
		OuterPedestal[1],
		OuterPedestal[2],
	};
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		PedestalRim,
		FLinearColor(0.36f, 0.41f, 0.48f, 0.85f),
		1.8f,
		false);

	return LayerId;
}

int32 UBwayMatchBroadcastOverlayWidget::PaintCrosshair(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FVector2D& LocalSize) const
{
	const FVector2D Center = LocalSize * 0.5;

	bool bHitConfirmActive = false;
	if (const ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(GetOwningPlayer()))
	{
		if (const UBwayCombatNumberPopComponent* NumberPop = BwayPC->CombatNumberPopComponent)
		{
			if (const UWorld* World = GetWorld())
			{
				bHitConfirmActive =
					(World->GetTimeSeconds() - NumberPop->GetLastOutgoingHitWorldTime()) <= BwayBroadcastOverlayConstants::HitConfirmFlashSeconds;
			}
		}
	}

	const FLinearColor OutlineColor(0.04f, 0.05f, 0.08f, 0.92f);
	const FLinearColor SpokeColor = bHitConfirmActive
		? FLinearColor(1.0f, 0.82f, 0.30f, 0.98f)
		: FLinearColor(0.96f, 0.98f, 1.0f, 0.94f);

	auto DrawOutlinedSpoke = [&](const FVector2D& Offset, const FVector2D& Size)
	{
		UBwayMatchHUDWidgetBase::DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			Offset - FVector2D(1.0, 1.0),
			Size + FVector2D(2.0, 2.0),
			OutlineColor);
		UBwayMatchHUDWidgetBase::DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			LayerId + 1,
			Offset,
			Size,
			SpokeColor);
	};

	// Left horizontal bar, right horizontal bar, and lower vertical bar (3-spoke Breakaway T-reticle)
	++LayerId;
	DrawOutlinedSpoke(FVector2D(Center.X - 21.0, Center.Y - 1.5), FVector2D(14.0, 3.0));
	DrawOutlinedSpoke(FVector2D(Center.X + 7.0, Center.Y - 1.5), FVector2D(14.0, 3.0));
	DrawOutlinedSpoke(FVector2D(Center.X - 1.5, Center.Y + 7.0), FVector2D(3.0, 13.0));
	++LayerId;

	// Tiny crisp center 'x' pip
	const double CrossHalf = bHitConfirmActive ? 4.2 : 2.6;
	const FLinearColor CenterPipColor = bHitConfirmActive
		? FLinearColor(1.0f, 0.42f, 0.24f, 1.0f)
		: FLinearColor(1.0f, 1.0f, 1.0f, 0.92f);
	const FVector2D DiagA[2] = { Center + FVector2D(-CrossHalf, -CrossHalf), Center + FVector2D(CrossHalf, CrossHalf) };
	const FVector2D DiagB[2] = { Center + FVector2D(-CrossHalf, CrossHalf), Center + FVector2D(CrossHalf, -CrossHalf) };
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(AllottedGeometry, OutDrawElements, ++LayerId, DiagA, OutlineColor, 2.6f, false);
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(AllottedGeometry, OutDrawElements, LayerId, DiagB, OutlineColor, 2.6f, false);
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(AllottedGeometry, OutDrawElements, ++LayerId, DiagA, CenterPipColor, 1.4f, false);
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(AllottedGeometry, OutDrawElements, LayerId, DiagB, CenterPipColor, 1.4f, false);

	return LayerId;
}

int32 UBwayMatchBroadcastOverlayWidget::PaintShopPrompt(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FVector2D& LocalSize) const
{
	// Bottom-left flush banner: X = 0, Y = H - 214, matching "Shop Prompt (only visible near team goal)"
	const double BaseY = FMath::Max(320.0, LocalSize.Y - 214.0);
	const double BarH = 54.0;

	// Outer warm bronze-slate angled plate
	const FVector2D OuterPlate[4] = {
		FVector2D(0.0, BaseY),
		FVector2D(356.0, BaseY),
		FVector2D(382.0, BaseY + BarH),
		FVector2D(0.0, BaseY + BarH),
	};
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		OuterPlate,
		FLinearColor(0.24f, 0.16f, 0.11f, 0.88f));

	// Inner dark obsidian trapezoid bar
	const FVector2D InnerBar[4] = {
		FVector2D(0.0, BaseY + 5.0),
		FVector2D(340.0, BaseY + 5.0),
		FVector2D(362.0, BaseY + BarH - 5.0),
		FVector2D(0.0, BaseY + BarH - 5.0),
	};
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		InnerBar,
		FLinearColor(0.03f, 0.03f, 0.05f, 0.94f));

	const FVector2D InnerTrim[3] = { InnerBar[0], InnerBar[1], InnerBar[2] };
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		InnerTrim,
		FLinearColor(0.58f, 0.37f, 0.16f, 0.85f),
		1.4f,
		false);

	// Pewter/silver Shield-and-Sword heraldic crest overlapping the left side of the bar
	const FVector2D CrestCenter(48.0, BaseY + BarH * 0.5);
	const FVector2D ShieldOuter[7] = {
		CrestCenter + FVector2D(-24.0, -29.0),
		CrestCenter + FVector2D(24.0, -29.0),
		CrestCenter + FVector2D(26.0, -2.0),
		CrestCenter + FVector2D(20.0, 16.0),
		CrestCenter + FVector2D(0.0, 32.0),
		CrestCenter + FVector2D(-20.0, 16.0),
		CrestCenter + FVector2D(-26.0, -2.0),
	};
	const FVector2D ShieldInner[7] = {
		CrestCenter + FVector2D(-20.0, -25.0),
		CrestCenter + FVector2D(20.0, -25.0),
		CrestCenter + FVector2D(22.0, -2.0),
		CrestCenter + FVector2D(16.5, 13.5),
		CrestCenter + FVector2D(0.0, 27.5),
		CrestCenter + FVector2D(-16.5, 13.5),
		CrestCenter + FVector2D(-22.0, -2.0),
	};
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		ShieldOuter,
		FLinearColor(0.68f, 0.66f, 0.62f, 0.98f));
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		ShieldInner,
		FLinearColor(0.46f, 0.44f, 0.41f, 0.98f));
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		ShieldOuter,
		FLinearColor(0.14f, 0.13f, 0.12f, 0.95f),
		1.5f,
		true);

	// Vertical broadsword emblem centered on the shield (pommel, hilt, crossguard, blade)
	const FLinearColor SwordMetal(0.84f, 0.82f, 0.78f, 0.98f);
	UBwayMatchHUDWidgetBase::DrawSolidBox(AllottedGeometry, OutDrawElements, ++LayerId, CrestCenter + FVector2D(-3.5, -35.0), FVector2D(7.0, 4.5), SwordMetal);
	UBwayMatchHUDWidgetBase::DrawSolidBox(AllottedGeometry, OutDrawElements, LayerId, CrestCenter + FVector2D(-2.0, -30.5), FVector2D(4.0, 8.5), SwordMetal);
	UBwayMatchHUDWidgetBase::DrawSolidBox(AllottedGeometry, OutDrawElements, LayerId, CrestCenter + FVector2D(-9.5, -22.5), FVector2D(19.0, 3.5), SwordMetal);
	const FVector2D SwordBlade[5] = {
		CrestCenter + FVector2D(-3.2, -19.0),
		CrestCenter + FVector2D(3.2, -19.0),
		CrestCenter + FVector2D(2.6, 12.0),
		CrestCenter + FVector2D(0.0, 19.5),
		CrestCenter + FVector2D(-2.6, 12.0),
	};
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(AllottedGeometry, OutDrawElements, LayerId, SwordBlade, SwordMetal);

	// Bold condensed gold-amber "UPGRADES" label
	const FString UpgradesLabel = TEXT("UPGRADES");
	const FSlateFontInfo UpgradesFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(24, 12, 1);
	const FVector2D LabelSize = UBwayMatchHUDWidgetBase::MeasureBroadcastText(UpgradesLabel, UpgradesFont);
	const FVector2D LabelPos(106.0, BaseY + (BarH - LabelSize.Y) * 0.5 - 1.0);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		static_cast<uint32>(++LayerId),
		AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(LabelSize.X + 20.0), static_cast<float>(LabelSize.Y + 8.0)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(LabelPos.X), static_cast<float>(LabelPos.Y)))),
		UpgradesLabel,
		UpgradesFont,
		ESlateDrawEffect::None,
		FLinearColor(0.95f, 0.61f, 0.12f, 1.0f));

	// Dynamic keycap badge (e.g. [B] on KBM or gamepad button)
	const FString KeyString = GetResolvedShopKeyLabel().ToString().ToUpper();
	const FSlateFontInfo KeyFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(17, 4, 1);
	const FVector2D KeyTextSize = UBwayMatchHUDWidgetBase::MeasureBroadcastText(KeyString, KeyFont);
	const double KeyBoxW = FMath::Max(28.0, KeyTextSize.X + 14.0);
	const double KeyBoxH = 28.0;
	const FVector2D KeyBoxPos(282.0, BaseY + (BarH - KeyBoxH) * 0.5);

	UBwayMatchHUDWidgetBase::DrawSolidBox(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		KeyBoxPos - FVector2D(1.5, 1.5),
		FVector2D(KeyBoxW + 3.0, KeyBoxH + 3.0),
		FLinearColor(0.32f, 0.34f, 0.38f, 0.92f));
	UBwayMatchHUDWidgetBase::DrawSolidBox(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		KeyBoxPos,
		FVector2D(KeyBoxW, KeyBoxH),
		FLinearColor(0.14f, 0.15f, 0.18f, 0.98f));

	const FVector2D KeyTextPos(
		KeyBoxPos.X + (KeyBoxW - KeyTextSize.X) * 0.5,
		KeyBoxPos.Y + (KeyBoxH - KeyTextSize.Y) * 0.5 - 0.5);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		static_cast<uint32>(++LayerId),
		AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(KeyTextSize.X + 10.0), static_cast<float>(KeyTextSize.Y + 6.0)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(KeyTextPos.X), static_cast<float>(KeyTextPos.Y)))),
		KeyString,
		KeyFont,
		ESlateDrawEffect::None,
		FLinearColor(0.84f, 0.86f, 0.90f, 1.0f));

	return LayerId;
}

int32 UBwayMatchBroadcastOverlayWidget::PaintEventToast(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FVector2D& LocalSize) const
{
	(void)LocalSize;
	const double Now = GetOverlayClockSeconds();
	const double Remaining = ActiveToast.ExpireRealTime - Now;
	if (Remaining <= 0.0 || ActiveToast.Headline.IsEmpty())
	{
		return LayerId;
	}

	const double Elapsed = FMath::Max(0.0, Now - ActiveToast.StartRealTime);
	const float FadeIn = FMath::Clamp(static_cast<float>(Elapsed / 0.15), 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp(static_cast<float>(Remaining / 0.35), 0.0f, 1.0f);
	const float Alpha = FadeIn * FadeOut;

	const double ToastY = 212.0;
	const double ToastW = 352.0;
	const double ToastH = 74.0;

	const FLinearColor TeamAccent = ActiveToast.bIsFriendly
		? FLinearColor(0.09f, 0.84f, 0.92f, Alpha)
		: FLinearColor(0.89f, 0.25f, 0.23f, Alpha);

	// Translucent horizontal banner flush to X = 0 with subtle gradient fade on the right
	// Note: In Slate MakeGradient, Orient_Vertical means vertical stop lines (left-to-right X interpolation).
	TArray<FSlateGradientStop> BannerStops;
	BannerStops.Add(FSlateGradientStop(FVector2D(0.0f, 0.0f), FLinearColor(0.05f, 0.11f, 0.16f, 0.88f * Alpha)));
	BannerStops.Add(FSlateGradientStop(FVector2D(static_cast<float>(ToastW * 0.72), 0.0f), FLinearColor(0.11f, 0.20f, 0.26f, 0.74f * Alpha)));
	BannerStops.Add(FSlateGradientStop(FVector2D(static_cast<float>(ToastW), 0.0f), FLinearColor(0.16f, 0.22f, 0.26f, 0.32f * Alpha)));
	FSlateDrawElement::MakeGradient(
		OutDrawElements,
		static_cast<uint32>(++LayerId),
		AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(ToastW), static_cast<float>(ToastH)),
			FSlateLayoutTransform(FVector2f(0.0f, static_cast<float>(ToastY)))),
		MoveTemp(BannerStops),
		Orient_Vertical);

	// Subtle top & bottom bronze-slate border lines
	UBwayMatchHUDWidgetBase::DrawSolidBox(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		FVector2D(0.0, ToastY),
		FVector2D(ToastW, 1.5),
		FLinearColor(0.45f, 0.36f, 0.26f, 0.65f * Alpha));
	UBwayMatchHUDWidgetBase::DrawSolidBox(
		AllottedGeometry,
		OutDrawElements,
		LayerId,
		FVector2D(0.0, ToastY + ToastH - 1.5),
		FVector2D(ToastW, 1.5),
		FLinearColor(0.45f, 0.36f, 0.26f, 0.65f * Alpha));

	// Left 62x62 team-bordered Hero Portrait
	const FVector2D PortraitFramePos(6.0, ToastY + 6.0);
	const FVector2D PortraitFrameSize(62.0, 62.0);
	UBwayMatchHUDWidgetBase::DrawSolidBox(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		PortraitFramePos,
		PortraitFrameSize,
		TeamAccent);

	const FVector2D PortraitInnerPos = PortraitFramePos + FVector2D(2.0, 2.0);
	const FVector2D PortraitInnerSize = PortraitFrameSize - FVector2D(4.0, 4.0);
	if (UTexture2D* PortraitTex = ActiveToast.PortraitTexture.Get())
	{
		FSlateBrush PortraitBrush;
		PortraitBrush.SetResourceObject(PortraitTex);
		PortraitBrush.ImageSize = FVector2D(PortraitInnerSize.X, PortraitInnerSize.Y);
		PortraitBrush.DrawAs = ESlateBrushDrawType::Image;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			static_cast<uint32>(++LayerId),
			AllottedGeometry.ToPaintGeometry(
				FVector2f(static_cast<float>(PortraitInnerSize.X), static_cast<float>(PortraitInnerSize.Y)),
				FSlateLayoutTransform(FVector2f(static_cast<float>(PortraitInnerPos.X), static_cast<float>(PortraitInnerPos.Y)))),
			&PortraitBrush,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 1.0f, 1.0f, Alpha));
	}
	else
	{
		UBwayMatchHUDWidgetBase::DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			++LayerId,
			PortraitInnerPos,
			PortraitInnerSize,
			FLinearColor(0.10f, 0.14f, 0.19f, Alpha));
	}

	// Upper-right bold condensed white headline (e.g. "INTERCEPTION!")
	const FString HeadlineStr = ActiveToast.Headline.ToString().ToUpper();
	const FSlateFontInfo HeadlineFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(22, 6, 1);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		static_cast<uint32>(++LayerId),
		AllottedGeometry.ToPaintGeometry(
			FVector2f(264.0f, 30.0f),
			FSlateLayoutTransform(FVector2f(78.0f, static_cast<float>(ToastY + 8.0)))),
		HeadlineStr,
		HeadlineFont,
		ESlateDrawEffect::None,
		FLinearColor(1.0f, 1.0f, 1.0f, Alpha));

	// Lower-right glowing horizontal team-colored accent bar + player display name
	TArray<FSlateGradientStop> NameBarStops;
	FLinearColor BarStart = TeamAccent;
	BarStart.A = 0.68f * Alpha;
	FLinearColor BarEnd = TeamAccent;
	BarEnd.A = 0.05f * Alpha;
	NameBarStops.Add(FSlateGradientStop(FVector2D(0.0f, 0.0f), BarStart));
	NameBarStops.Add(FSlateGradientStop(FVector2D(248.0f, 0.0f), BarEnd));
	FSlateDrawElement::MakeGradient(
		OutDrawElements,
		static_cast<uint32>(++LayerId),
		AllottedGeometry.ToPaintGeometry(
			FVector2f(248.0f, 22.0f),
			FSlateLayoutTransform(FVector2f(76.0f, static_cast<float>(ToastY + 42.0)))),
		MoveTemp(NameBarStops),
		Orient_Vertical);

	if (!ActiveToast.PlayerName.IsEmpty())
	{
		const FSlateFontInfo NameFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(16, 4, 1);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			static_cast<uint32>(++LayerId),
			AllottedGeometry.ToPaintGeometry(
				FVector2f(242.0f, 22.0f),
				FSlateLayoutTransform(FVector2f(82.0f, static_cast<float>(ToastY + 43.0)))),
			ActiveToast.PlayerName,
			NameFont,
			ESlateDrawEffect::None,
			FLinearColor(0.96f, 0.99f, 1.0f, Alpha));
	}

	return LayerId;
}

int32 UBwayMatchBroadcastOverlayWidget::PaintLiveMinimap(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FVector2D& LocalSize) const
{
	UWorld* World = GetWorld();
	const ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	if (!World || !GS)
	{
		return LayerId;
	}

	// Upper-right vertical rounded-stadium arena minimap directly below Enemy Team Portraits
	const double MapW = 166.0;
	const double MapH = 376.0;
	const FVector2D MapOrigin(LocalSize.X - 246.0, 106.0);
	const FVector2D MapCenter = MapOrigin + FVector2D(MapW * 0.5, MapH * 0.5);
	const double StadiumRadius = 68.0;
	const double StraightHalfH = (MapH * 0.5) - StadiumRadius - 12.0; // ~108px

	auto BuildStadiumPerimeter = [&](const double Radius, const double HalfStraight, const int32 ArcSteps, TArray<FVector2D>& OutPts)
	{
		OutPts.Reset();
		const FVector2D TopArcCenter(MapCenter.X, MapCenter.Y - HalfStraight);
		const FVector2D BottomArcCenter(MapCenter.X, MapCenter.Y + HalfStraight);

		// Top semicircle: left (-PI) -> top (-PI/2) -> right (0)
		for (int32 Step = 0; Step <= ArcSteps; ++Step)
		{
			const double Angle = -UE_PI + (UE_PI * static_cast<double>(Step) / static_cast<double>(ArcSteps));
			OutPts.Emplace(TopArcCenter.X + FMath::Cos(Angle) * Radius, TopArcCenter.Y + FMath::Sin(Angle) * Radius);
		}
		// Bottom semicircle: right (0) -> bottom (+PI/2) -> left (+PI)
		for (int32 Step = 0; Step <= ArcSteps; ++Step)
		{
			const double Angle = (UE_PI * static_cast<double>(Step) / static_cast<double>(ArcSteps));
			OutPts.Emplace(BottomArcCenter.X + FMath::Cos(Angle) * Radius, BottomArcCenter.Y + FMath::Sin(Angle) * Radius);
		}
	};

	// 1. Cerulean shrine water/sky outer accents at top and bottom of the minimap panel
	// Note: In Slate MakeGradient, Orient_Horizontal means horizontal stop lines (top-to-bottom Y interpolation).
	TArray<FSlateGradientStop> BackdropStops;
	BackdropStops.Add(FSlateGradientStop(FVector2D(0.0f, 0.0f), FLinearColor(0.11f, 0.56f, 0.84f, 0.72f)));
	BackdropStops.Add(FSlateGradientStop(FVector2D(0.0f, static_cast<float>(MapH * 0.25)), FLinearColor(0.09f, 0.30f, 0.46f, 0.52f)));
	BackdropStops.Add(FSlateGradientStop(FVector2D(0.0f, static_cast<float>(MapH * 0.75)), FLinearColor(0.09f, 0.30f, 0.46f, 0.52f)));
	BackdropStops.Add(FSlateGradientStop(FVector2D(0.0f, static_cast<float>(MapH)), FLinearColor(0.11f, 0.56f, 0.84f, 0.72f)));
	FSlateDrawElement::MakeGradient(
		OutDrawElements,
		static_cast<uint32>(++LayerId),
		AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(MapW), static_cast<float>(MapH)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(MapOrigin.X), static_cast<float>(MapOrigin.Y)))),
		MoveTemp(BackdropStops),
		Orient_Horizontal,
		ESlateDrawEffect::None,
		FVector4f(14.0f, 14.0f, 14.0f, 14.0f));

	// 2. Dark slate rounded-stadium arena floor + inner recessed combat court
	TArray<FVector2D> OuterStadiumPts;
	BuildStadiumPerimeter(StadiumRadius, StraightHalfH, 16, OuterStadiumPts);
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		OuterStadiumPts,
		FLinearColor(0.15f, 0.23f, 0.31f, 0.90f));

	TArray<FVector2D> InnerCourtPts;
	BuildStadiumPerimeter(StadiumRadius - 12.0, StraightHalfH - 4.0, 14, InnerCourtPts);
	UBwayMatchHUDWidgetBase::DrawConvexPolygon(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		InnerCourtPts,
		FLinearColor(0.24f, 0.35f, 0.44f, 0.82f));

	// Center Relic dais circle + top/bottom goal shrine circles + lane pillars
	auto DrawFilledCircle = [&](const FVector2D& Center, const double Radius, const FLinearColor& FillColor, const int32 Segments = 18)
	{
		TArray<FVector2D> CirclePts;
		CirclePts.Reserve(Segments);
		for (int32 Idx = 0; Idx < Segments; ++Idx)
		{
			const double Angle = UE_TWO_PI * static_cast<double>(Idx) / static_cast<double>(Segments);
			CirclePts.Emplace(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius);
		}
		UBwayMatchHUDWidgetBase::DrawConvexPolygon(AllottedGeometry, OutDrawElements, LayerId, CirclePts, FillColor);
	};

	++LayerId;
	DrawFilledCircle(MapCenter, 26.0, FLinearColor(0.35f, 0.48f, 0.58f, 0.72f), 20);
	DrawFilledCircle(MapCenter, 13.0, FLinearColor(0.18f, 0.27f, 0.35f, 0.88f), 16);
	DrawFilledCircle(FVector2D(MapCenter.X, MapCenter.Y - StraightHalfH + 4.0), 22.0, FLinearColor(0.18f, 0.28f, 0.36f, 0.78f), 18);
	DrawFilledCircle(FVector2D(MapCenter.X, MapCenter.Y + StraightHalfH - 4.0), 22.0, FLinearColor(0.18f, 0.28f, 0.36f, 0.78f), 18);

	// Horizontal midfield divider line
	const FVector2D MidLine[2] = {
		FVector2D(MapCenter.X - (StadiumRadius - 10.0), MapCenter.Y),
		FVector2D(MapCenter.X + (StadiumRadius - 10.0), MapCenter.Y),
	};
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		MidLine,
		FLinearColor(0.62f, 0.74f, 0.84f, 0.45f),
		1.4f,
		false);

	// 3. Signature ~28-segment light-grey dashed stadium outer track
	TArray<FVector2D> TrackPts;
	BuildStadiumPerimeter(StadiumRadius - 5.0, StraightHalfH, 14, TrackPts);
	++LayerId;
	for (int32 Idx = 0; Idx < TrackPts.Num(); Idx += 2)
	{
		const FVector2D& P0 = TrackPts[Idx];
		const FVector2D& P1 = TrackPts[(Idx + 1) % TrackPts.Num()];
		const FVector2D Seg[2] = { P0, FMath::Lerp(P0, P1, 0.76) };
		UBwayMatchHUDWidgetBase::DrawPolygonOutline(
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			Seg,
			FLinearColor(0.72f, 0.77f, 0.82f, 0.84f),
			4.2f,
			false);
	}

	// 4. Resolve arena coordinate frame so Friendly Goal is always at bottom (+Y) and Enemy Goal is always at top (-Y)
	FVector ArenaCenter(0.0, 0.0, 0.0);
	FVector TeamOneDir(0.0, -1.0, 0.0);
	if (!UBwayMidfieldRulesLibrary::TryGetMidfieldFrame(World, ArenaCenter, TeamOneDir))
	{
		ArenaCenter = FVector::ZeroVector;
		TeamOneDir = FVector(0.0, -1.0, 0.0);
	}
	TeamOneDir = TeamOneDir.GetSafeNormal2D();
	if (TeamOneDir.IsNearlyZero())
	{
		TeamOneDir = FVector(0.0, -1.0, 0.0);
	}

	const int32 LocalTeam = UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(this);
	// TeamOneDir points from Team 0's goal toward Team 1's goal.
	// For Team 0 (or spectator), Enemy Goal is in +TeamOneDir; for Team 1, Enemy Goal is in -TeamOneDir.
	const FVector EnemyForwardDir = (LocalTeam == 1) ? -TeamOneDir : TeamOneDir;
	const FVector RightFromLocalDir = FVector::CrossProduct(FVector::UpVector, EnemyForwardDir).GetSafeNormal2D();

	constexpr double GoalHalfSpan = 4250.0;
	constexpr double LateralHalfSpan = 2350.0;
	const double MapPlayHalfW = StadiumRadius - 14.0;
	const double MapPlayHalfH = StraightHalfH + StadiumRadius - 22.0;

	auto ProjectWorldToMinimap = [&](const FVector& WorldLoc) -> FVector2D
	{
		const FVector Rel = WorldLoc - ArenaCenter;
		const double NormX = FMath::Clamp(FVector::DotProduct(Rel, RightFromLocalDir) / LateralHalfSpan, -0.95, 0.95);
		const double NormY = FMath::Clamp(-FVector::DotProduct(Rel, EnemyForwardDir) / GoalHalfSpan, -0.95, 0.95);
		return FVector2D(MapCenter.X + NormX * MapPlayHalfW, MapCenter.Y + NormY * MapPlayHalfH);
	};

	// 5. Vivid horizontal red Relic progress line + Relic emblem at live ARelicActor position
	FVector RelicWorldLoc = ArenaCenter;
	bool bFoundRelic = false;
	if (const UBwayRelicManagerComponent* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>())
	{
		if (const ARelicActor* Relic = RelicMgr->GetRelicActor())
		{
			RelicWorldLoc = (Relic->GetCurrentState() == ERelicState::Carried && Relic->CurrentCarrier)
				? Relic->CurrentCarrier->GetActorLocation()
				: Relic->GetActorLocation();
			bFoundRelic = true;
		}
	}

	const FVector2D RelicPt = bFoundRelic ? ProjectWorldToMinimap(RelicWorldLoc) : MapCenter;
	const FVector2D RelicProgressLine[2] = {
		FVector2D(MapOrigin.X + 8.0, RelicPt.Y),
		FVector2D(MapOrigin.X + MapW - 8.0, RelicPt.Y),
	};
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		RelicProgressLine,
		FLinearColor(0.05f, 0.01f, 0.01f, 0.75f),
		4.8f,
		false);
	UBwayMatchHUDWidgetBase::DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++LayerId,
		RelicProgressLine,
		FLinearColor(0.96f, 0.11f, 0.11f, 0.98f),
		3.0f,
		false);

	++LayerId;
	DrawFilledCircle(RelicPt, 6.5, FLinearColor(0.78f, 0.08f, 0.10f, 0.96f), 14);
	DrawFilledCircle(RelicPt, 3.2, FLinearColor(1.0f, 0.76f, 0.24f, 0.98f), 10);

	// 6. Friendly (cyan) and Enemy (red) active buildable pips
	if (GS->BuildableRegistryComponent)
	{
		++LayerId;
		const TArray<ABuildableActor*> AllBuildables = GS->BuildableRegistryComponent->GetAllBuildables();
		for (const ABuildableActor* Buildable : AllBuildables)
		{
			if (!IsValid(Buildable))
			{
				continue;
			}
			const int32 OwnerTeam = GS->GetPlayerTeam(Buildable->GetPlacingPlayerState());
			const int32 DisplaySlot = UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(OwnerTeam, LocalTeam);
			const FLinearColor PipColor = (DisplaySlot == 1)
				? FLinearColor(0.95f, 0.18f, 0.16f, 0.96f)
				: FLinearColor(0.10f, 0.90f, 0.96f, 0.96f);
			DrawFilledCircle(ProjectWorldToMinimap(Buildable->GetActorLocation()), 3.4, PipColor, 10);
		}
	}

	// 7. 8 Player markers: cyan/red 'X' for allies/enemies and rotated white directional chevron for local player
	const APlayerController* OwningPC = GetOwningPlayer();
	const APawn* LocalPawn = OwningPC ? OwningPC->GetPawn() : nullptr;

	++LayerId;
	for (APlayerState* Entry : GS->PlayerArray)
	{
		const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Entry);
		if (!BwayPS || !UBwayHUDHelpers::IsPlayerAliveForHUD(BwayPS))
		{
			continue;
		}
		const APawn* Pawn = BwayPS->GetPawn();
		if (!Pawn || Pawn == LocalPawn)
		{
			continue;
		}

		const FVector2D MarkerPt = ProjectWorldToMinimap(Pawn->GetActorLocation());
		const int32 PlayerTeam = GS->GetPlayerTeam(BwayPS);
		const int32 DisplaySlot = UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(PlayerTeam, LocalTeam);
		const FLinearColor MarkerColor = (DisplaySlot == 1)
			? FLinearColor(0.96f, 0.16f, 0.15f, 1.0f)
			: FLinearColor(0.10f, 0.94f, 0.98f, 1.0f);

		constexpr double Arm = 4.8;
		const FVector2D CrossA[2] = { MarkerPt + FVector2D(-Arm, -Arm), MarkerPt + FVector2D(Arm, Arm) };
		const FVector2D CrossB[2] = { MarkerPt + FVector2D(-Arm, Arm), MarkerPt + FVector2D(Arm, -Arm) };
		UBwayMatchHUDWidgetBase::DrawPolygonOutline(
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			CrossA,
			FLinearColor(0.02f, 0.04f, 0.06f, 0.85f),
			3.8f,
			false);
		UBwayMatchHUDWidgetBase::DrawPolygonOutline(
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			CrossB,
			FLinearColor(0.02f, 0.04f, 0.06f, 0.85f),
			3.8f,
			false);
		UBwayMatchHUDWidgetBase::DrawPolygonOutline(AllottedGeometry, OutDrawElements, LayerId + 1, CrossA, MarkerColor, 2.3f, false);
		UBwayMatchHUDWidgetBase::DrawPolygonOutline(AllottedGeometry, OutDrawElements, LayerId + 1, CrossB, MarkerColor, 2.3f, false);
	}
	++LayerId;

	if (LocalPawn)
	{
		const ABwayPlayerState* LocalPS = OwningPC ? OwningPC->GetPlayerState<ABwayPlayerState>() : nullptr;
		if (!LocalPS || UBwayHUDHelpers::IsPlayerAliveForHUD(LocalPS))
		{
			const FVector2D SelfPt = ProjectWorldToMinimap(LocalPawn->GetActorLocation());
			FRotator ViewRot = OwningPC ? OwningPC->GetControlRotation() : LocalPawn->GetActorRotation();
			const FVector WorldLookDir = FRotationMatrix(FRotator(0.0f, ViewRot.Yaw, 0.0f)).GetUnitAxis(EAxis::X).GetSafeNormal2D();

			FVector2D MapDir(
				FVector::DotProduct(WorldLookDir, RightFromLocalDir),
				-FVector::DotProduct(WorldLookDir, EnemyForwardDir));
			if (!MapDir.Normalize())
			{
				MapDir = FVector2D(0.0, -1.0);
			}
			const FVector2D MapPerp(-MapDir.Y, MapDir.X);

			const FVector2D Chevron[3] = {
				SelfPt + MapDir * 7.5,
				SelfPt - MapDir * 4.5 + MapPerp * 5.0,
				SelfPt - MapDir * 4.5 - MapPerp * 5.0,
			};
			UBwayMatchHUDWidgetBase::DrawConvexPolygon(
				AllottedGeometry,
				OutDrawElements,
				++LayerId,
				Chevron,
				FLinearColor(1.0f, 1.0f, 1.0f, 0.98f));
			UBwayMatchHUDWidgetBase::DrawPolygonOutline(
				AllottedGeometry,
				OutDrawElements,
				++LayerId,
				Chevron,
				FLinearColor(0.04f, 0.07f, 0.11f, 0.92f),
				1.3f,
				true);
		}
	}

	return LayerId;
}
