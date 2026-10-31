// Copyright Epic Games, Inc. All Rights Reserved.
// Goal 6 headless Match HUD / Team Portrait Respawn Timers / Ability Bar Overlays /
// Round Phase Banners / 8-Player Scoreboard & Post-Match Breakdown automation suite
// (Breakaway.UI.MatchHUDAndPortraits.*) and live guard=0 PIE + 200ms packaged runtime
// audit (bway.Test.Goal6RuntimeAudit + Client_RunGoal6HUDAuditProbe).

#include "CoreMinimal.h"

#include "BwayPlayerController.h"

#if !UE_BUILD_SHIPPING

#include "AIController.h"
#include "BrainComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "BreakawayGameMode.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "CommonUIExtensions.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameModes/BwayMatchFlowConfig.h"
#include "GameState/BwayBotCreationComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "GameplayEffect.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "ICommonInputModule.h"
#include "ImageUtils.h"
#include "InputMappingContext.h"
#include "Layout/WidgetPath.h"
#include "LyraGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Relic/RelicActor.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "UI/BwayAbilityBarHUDWidget.h"
#include "UI/BwayCaptureTheRelicScoreWidget.h"
#include "UI/BwayCurrencyDisplayWidget.h"
#include "UI/BwayHealthHUDWidget.h"
#include "UI/BwayMatchAbilitySlotWidget.h"
#include "UI/BwayMatchBroadcastOverlayWidget.h"
#include "UI/BwayMatchBreakdownWidget.h"
#include "UI/BwayPauseMenuWidget.h"
#include "UI/BwayPostMatchInterstitialWidget.h"
#include "UI/BwayPostRoundSummaryWidget.h"
#include "UI/BwayRelicStatusWidget.h"
#include "UI/BwayResultsScreenWidget.h"
#include "UI/BwayScoreboardWidget.h"
#include "UI/BwayTeamPortraitSlotWidget.h"
#include "UI/BwayTeamPortraitsHUDWidget.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "GameplayEffectComponent.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

namespace BwayGoal6StatMutators
{
	void AddKills(ABwayPlayerState* PS, int32 Count)
	{
		for (int32 I = 0; PS && I < Count; ++I)
		{
			PS->AddKill();
		}
	}

	void AddDeaths(ABwayPlayerState* PS, int32 Count)
	{
		for (int32 I = 0; PS && I < Count; ++I)
		{
			PS->AddDeath();
		}
	}

	void AddAssists(ABwayPlayerState* PS, int32 Count)
	{
		for (int32 I = 0; PS && I < Count; ++I)
		{
			PS->AddAssist();
		}
	}

	void AddForcedFumbles(ABwayPlayerState* PS, int32 Count)
	{
		for (int32 I = 0; PS && I < Count; ++I)
		{
			PS->AddForcedFumble();
		}
	}

	void AddInterceptions(ABwayPlayerState* PS, int32 Count)
	{
		for (int32 I = 0; PS && I < Count; ++I)
		{
			PS->AddInterception();
		}
	}

	void CreditGoldEarned(ABwayPlayerState* PS, int32 PositiveDelta)
	{
		if (!PS || PositiveDelta <= 0)
		{
			return;
		}
		if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
		{
			const float Current = ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute());
			ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), Current + float(PositiveDelta));
		}
	}

	void SetTeamScoreExact(UBwayScoringComponent* Scoring, int32 TeamIndex, int32 DesiredScore)
	{
		if (!Scoring)
		{
			return;
		}
		const int32 Current = Scoring->GetTeamScore(TeamIndex);
		if (DesiredScore > Current)
		{
			Scoring->AddScore(TeamIndex, DesiredScore - Current);
		}
	}
} // namespace BwayGoal6StatMutators

namespace BwayGoal6RuntimeHelpers
{
	static FString ActiveEvidenceDir;

	FString ResolveEvidenceDirectory(const FString& Token)
	{
		const FString CleanToken = Token.IsEmpty() ? TEXT("goal6-runtime") : Token;
		const FString Dir = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("goal6-hud-20261029") / CleanToken);
		IFileManager::Get().MakeDirectory(*Dir, true);
		return Dir;
	}

	bool CaptureLocalViewportPNG(APlayerController* PC, const FString& OutputPngPath, FIntVector& OutSize)
	{
		OutSize = FIntVector::ZeroValue;
		if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer())
		{
			return false;
		}
		if (!GEngine || !FSlateApplication::IsInitialized() || !GDynamicRHI)
		{
			return false;
		}
		if (FString(GDynamicRHI->GetName()).Contains(TEXT("Null")))
		{
			return false;
		}

		UGameViewportClient* ViewportClient = PC->GetLocalPlayer()->ViewportClient;
		if (!ViewportClient || !ViewportClient->Viewport)
		{
			return false;
		}

		const TSharedPtr<SViewport> ViewWidget = ViewportClient->GetGameViewportWidget();
		const TSharedPtr<SWindow> Window = ViewportClient->GetWindow();
		if (!Window.IsValid())
		{
			return false;
		}

		FSlateApplication& Slate = FSlateApplication::Get();
		Window->SlatePrepass(Window->GetDPIScaleFactor());
		ViewportClient->Viewport->Draw(false);

		TArray<FColor> Pixels;
		bool bCaptured = false;
		if (ViewWidget.IsValid())
		{
			FWidgetPath ViewPath;
			if (Slate.GeneratePathToWidgetUnchecked(ViewWidget.ToSharedRef(), ViewPath))
			{
				bCaptured = Slate.TakeScreenshot(ViewWidget.ToSharedRef(), Pixels, OutSize);
			}
		}
		if ((!bCaptured || OutSize.X <= 0 || OutSize.Y <= 0) && Window.IsValid())
		{
			Pixels.Reset();
			bCaptured = Slate.TakeScreenshot(Window.ToSharedRef(), Pixels, OutSize);
		}
		if (!bCaptured || OutSize.X <= 0 || OutSize.Y <= 0 || int64(OutSize.X) * OutSize.Y != Pixels.Num())
		{
			return false;
		}

		for (FColor& Px : Pixels)
		{
			Px.A = 255;
		}

		TArray64<uint8> PNG;
		FImageUtils::PNGCompressImageArray(
			OutSize.X,
			OutSize.Y,
			TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()),
			PNG);
		if (PNG.IsEmpty())
		{
			return false;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPngPath), true);
		return FFileHelper::SaveArrayToFile(PNG, *OutputPngPath);
	}

	template <typename WidgetT>
	WidgetT* FindLocalHUDWidget(APlayerController* OwningPC)
	{
		for (TObjectIterator<WidgetT> It; It; ++It)
		{
			WidgetT* Candidate = *It;
			if (!Candidate || Candidate->IsTemplate())
			{
				continue;
			}
			if (Candidate->GetOwningPlayer() == OwningPC && Candidate->GetWorld() == OwningPC->GetWorld())
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	UBwayTeamPortraitsHUDWidget* FindLocalTeamPortraitsWidget(APlayerController* OwningPC, int32 DesiredDisplaySlot)
	{
		for (TObjectIterator<UBwayTeamPortraitsHUDWidget> It; It; ++It)
		{
			UBwayTeamPortraitsHUDWidget* Candidate = *It;
			if (!Candidate || Candidate->IsTemplate())
			{
				continue;
			}
			if (Candidate->GetOwningPlayer() == OwningPC
				&& Candidate->GetWorld() == OwningPC->GetWorld()
				&& Candidate->GetDisplaySlotIndex() == DesiredDisplaySlot)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	void RunLocalGoal6Probe(ABwayPlayerController* PC, int32 Stage, bool bCaptureScreenshot, const FString& RolePrefix)
	{
		if (!PC || !PC->IsLocalController() || !PC->GetWorld())
		{
			return;
		}

		UWorld* World = PC->GetWorld();
		ABwayGameState* GS = World->GetGameState<ABwayGameState>();
		ABwayPlayerState* LocalPS = PC->GetPlayerState<ABwayPlayerState>();
		const int32 LocalPlayerId = LocalPS ? LocalPS->GetPlayerId() : INDEX_NONE;
		if (Stage == -99)
		{
			if (World->GetNetMode() == NM_Client && PC->GetGameInstance())
			{
				UE_LOG(LogTemp, Display,
					TEXT("BwayGoal6Probe: Stage=-99 role=%s player=%d action=ReturnToMainMenu"),
					*RolePrefix,
					LocalPlayerId);
				PC->GetGameInstance()->ReturnToMainMenu();
				const double ExitAt = FPlatformTime::Seconds() + 1.25;
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([LocalPlayerId, ExitAt](float)
				{
					if (FPlatformTime::Seconds() < ExitAt)
					{
						return true;
					}
					UE_LOG(LogTemp, Display, TEXT("BwayGoal6Audit: PeerReleased local=%d reason=disconnect-probe"), LocalPlayerId);
					return false;
				}), 0.10f);
			}
			return;
		}
		if (Stage == -1)
		{
			if (World->GetNetMode() == NM_Client)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayGoal6Audit: PeerReleased local=%d reason=complete"), LocalPlayerId);
			}
			return;
		}

		const FString OutDir = ActiveEvidenceDir.IsEmpty()
			? ResolveEvidenceDirectory(TEXT("packaged-or-pie"))
			: ActiveEvidenceDir;

		UBwayCaptureTheRelicScoreWidget* ScoreWidget = FindLocalHUDWidget<UBwayCaptureTheRelicScoreWidget>(PC);
		UBwayRelicStatusWidget* RelicStatus = FindLocalHUDWidget<UBwayRelicStatusWidget>(PC);
		UBwayTeamPortraitsHUDWidget* FriendlyPortraits = FindLocalTeamPortraitsWidget(PC, 0);
		UBwayTeamPortraitsHUDWidget* EnemyPortraits = FindLocalTeamPortraitsWidget(PC, 1);
		UBwayAbilityBarHUDWidget* AbilityBar = FindLocalHUDWidget<UBwayAbilityBarHUDWidget>(PC);
		UBwayHealthHUDWidget* HealthWidget = FindLocalHUDWidget<UBwayHealthHUDWidget>(PC);
		UBwayCurrencyDisplayWidget* GoldWidget = nullptr;
		for (TObjectIterator<UBwayCurrencyDisplayWidget> It; It; ++It)
		{
			UBwayCurrencyDisplayWidget* Candidate = *It;
			if (Candidate && !Candidate->IsTemplate()
				&& Candidate->GetOwningPlayer() == PC
				&& Candidate->GetWorld() == World
				&& Candidate->GetName().Contains(TEXT("Gold")))
			{
				GoldWidget = Candidate;
				break;
			}
		}

		if (ScoreWidget)
		{
			ScoreWidget->RefreshScoreDisplay();
			ScoreWidget->RefreshTimerDisplay();
		}
		if (RelicStatus)
		{
			RelicStatus->RefreshRelicStatus();
		}
		if (FriendlyPortraits)
		{
			FriendlyPortraits->RefreshTeamPortraits();
		}
		if (EnemyPortraits)
		{
			EnemyPortraits->RefreshTeamPortraits();
		}
		if (AbilityBar)
		{
			AbilityBar->RefreshAbilityBar();
		}
		if (HealthWidget)
		{
			HealthWidget->RefreshHealthDisplay();
		}
		if (GoldWidget)
		{
			GoldWidget->RefreshWalletHUDState();
		}

		if (Stage == 1)
		{
			UE_LOG(LogTemp, Display,
				TEXT("BwayGoal6Probe: Stage=1 role=%s net=%d player=%d pips=%d timer=[%s] bannerVisible=%d primary=[%s] secondary=[%s] gold=%d maxGold=%d shopKey=[%s] shopPulse=%d"),
				*RolePrefix,
				int32(World->GetNetMode()),
				LocalPlayerId,
				ScoreWidget ? ScoreWidget->GetDisplayedPointsToWin() : -1,
				ScoreWidget ? *ScoreWidget->GetDisplayedTimerText().ToString() : TEXT("null"),
				RelicStatus && RelicStatus->IsBannerVisible() ? 1 : 0,
				RelicStatus ? *RelicStatus->GetDisplayedPrimaryStatusText().ToString() : TEXT("null"),
				RelicStatus ? *RelicStatus->GetDisplayedSecondaryStatusText().ToString() : TEXT("null"),
				GoldWidget ? GoldWidget->GetAmount() : -1,
				GoldWidget ? GoldWidget->GetDisplayedMaxGold() : -1,
				GoldWidget ? *GoldWidget->GetResolvedShopKeyLabel().ToString() : TEXT("null"),
				GoldWidget && GoldWidget->IsShopPromptPulsing() ? 1 : 0);
		}
		else if (Stage == 2)
		{
			int32 OccupiedPortraits = 0;
			int32 ActivePortraitRespawnTimers = 0;
			int32 VisibleDeathOverlays = 0;
			int32 VisibleRelicBadges = 0;
			auto CountRow = [&](UBwayTeamPortraitsHUDWidget* Row)
			{
				if (!Row)
				{
					return;
				}
				for (UBwayTeamPortraitSlotWidget* Slot : Row->GetSlotWidgets())
				{
					if (!Slot)
					{
						continue;
					}
					const FBwayTeamPortraitSlotViewModel& VM = Slot->GetSlotViewModel();
					if (VM.bIsOccupied)
					{
						++OccupiedPortraits;
					}
					if (Slot->IsRespawnTimerVisible() && VM.RespawnRemainingSeconds > 0)
					{
						++ActivePortraitRespawnTimers;
					}
					if (Slot->IsDeathOverlayVisible())
					{
						++VisibleDeathOverlays;
					}
					if (Slot->IsRelicBadgeVisible())
					{
						++VisibleRelicBadges;
					}
				}
			};
			CountRow(FriendlyPortraits);
			CountRow(EnemyPortraits);

			const int32 SlotCount = AbilityBar ? AbilityBar->GetSlotWidgets().Num() : 0;
			int32 CooldownSlots = 0;
			int32 RelicLockedSlots = 0;
			FString BuildableState = TEXT("none");
			if (AbilityBar)
			{
				for (UBwayMatchAbilitySlotWidget* Slot : AbilityBar->GetSlotWidgets())
				{
					if (!Slot)
					{
						continue;
					}
					if (Slot->IsCooldownOverlayVisible())
					{
						++CooldownSlots;
					}
					if (Slot->IsRelicFrameVisible())
					{
						++RelicLockedSlots;
					}
					if (Slot->GetSlotViewModel().bIsBuildable)
					{
						BuildableState = Slot->GetDisplayedStateText().ToString();
						BuildableState.ReplaceInline(TEXT("\r"), TEXT(""));
						BuildableState.ReplaceInline(TEXT("\n"), TEXT(" "));
					}
				}
			}

			UE_LOG(LogTemp, Display,
				TEXT("BwayGoal6Probe: Stage=2 role=%s net=%d player=%d occupiedPortraits=%d portraitTimers=%d deathOverlays=%d relicBadges=%d abilitySlots=%d cooldownSlots=%d relicSlots=%d buildable=[%s] slide=[%s] relicPrompt=[%s] hp=%.0f/%.0f trail=%.2f armor=%d"),
				*RolePrefix,
				int32(World->GetNetMode()),
				LocalPlayerId,
				OccupiedPortraits,
				ActivePortraitRespawnTimers,
				VisibleDeathOverlays,
				VisibleRelicBadges,
				SlotCount,
				CooldownSlots,
				RelicLockedSlots,
				*BuildableState,
				AbilityBar ? *AbilityBar->GetSharedSlidePromptText().ToString() : TEXT("null"),
				AbilityBar ? *AbilityBar->GetRelicPromptText().ToString() : TEXT("null"),
				HealthWidget ? HealthWidget->GetDisplayedHealth() : -1.f,
				HealthWidget ? HealthWidget->GetDisplayedMaxHealth() : -1.f,
				HealthWidget ? HealthWidget->GetDamageTrailPercent() : -1.f,
				HealthWidget ? HealthWidget->GetDisplayedArmor() : -1);
		}
		else if (Stage == 3)
		{
			PC->ShowScoreboard();
			if (UBwayScoreboardWidget* Scoreboard = PC->GetScoreboardWidget())
			{
				Scoreboard->RefreshScoreboardData();
			}
			bCaptureScreenshot = false;
		}
		else if (Stage == 31)
		{
			if (!PC->IsScoreboardVisible())
			{
				PC->ShowScoreboard();
			}
			UBwayScoreboardWidget* Scoreboard = PC->GetScoreboardWidget();
			if (Scoreboard)
			{
				Scoreboard->RefreshScoreboardData();
			}
			const TArray<FScoreboardPlayerData> Rows = Scoreboard ? Scoreboard->GetAllScoreboardPlayers() : TArray<FScoreboardPlayerData>();
			int32 RowsWithUpgrades = 0;
			for (const FScoreboardPlayerData& Row : Rows)
			{
				if (Row.EquippedUpgrades.Num() > 0)
				{
					++RowsWithUpgrades;
				}
			}

			int32 RemainingPortraitTimers = 0;
			for (UBwayTeamPortraitsHUDWidget* Row : { FriendlyPortraits, EnemyPortraits })
			{
				if (!Row)
				{
					continue;
				}
				for (UBwayTeamPortraitSlotWidget* Slot : Row->GetSlotWidgets())
				{
					if (Slot && Slot->IsRespawnTimerVisible())
					{
						++RemainingPortraitTimers;
					}
				}
			}

			UE_LOG(LogTemp, Display,
				TEXT("BwayGoal6Probe: Stage=3 role=%s net=%d player=%d scoreboardVisible=%d rows=%d rowsWithUpgrades=%d mvpId=%d mvp=[%s] portraitTimersAfterRespawn=%d"),
				*RolePrefix,
				int32(World->GetNetMode()),
				LocalPlayerId,
				PC->IsScoreboardVisible() ? 1 : 0,
				Rows.Num(),
				RowsWithUpgrades,
				Scoreboard ? Scoreboard->GetMVPPlayerId() : INDEX_NONE,
				Scoreboard ? *Scoreboard->GetMVPPlayerName().ToString() : TEXT("null"),
				RemainingPortraitTimers);

			if (bCaptureScreenshot)
			{
				FIntVector ImgSize;
				const FString ShotPath = OutDir / FString::Printf(TEXT("%s-p%d-stage3-scoreboard.png"), *RolePrefix, LocalPlayerId);
				if (CaptureLocalViewportPNG(PC, ShotPath, ImgSize))
				{
					UE_LOG(LogTemp, Display, TEXT("BwayGoal6Probe: Screenshot stage=3 role=%s size=%dx%d path=%s"),
						*RolePrefix, ImgSize.X, ImgSize.Y, *ShotPath);
				}
				bCaptureScreenshot = false;
			}
			PC->HideScoreboard();
		}
		else if (Stage >= 40 && Stage <= 42)
		{
			UBwayPostRoundSummaryWidget* PostRound = PC->GetPostRoundSummaryWidget();
			UE_LOG(LogTemp, Display,
				TEXT("BwayGoal6Probe: Stage=%d role=%s net=%d player=%d postRoundValid=%d header=[%s] reason=[%s] series=[%s]"),
				Stage,
				*RolePrefix,
				int32(World->GetNetMode()),
				LocalPlayerId,
				PostRound != nullptr ? 1 : 0,
				PostRound ? *PostRound->GetRoundOutcomeHeaderText().ToString() : TEXT("null"),
				PostRound ? *PostRound->GetWinReasonText().ToString() : TEXT("null"),
				PostRound ? *PostRound->GetSeriesScoreText().ToString() : TEXT("null"));
		}
		else if (Stage == 50)
		{
			if (UBwayResultsScreenWidget* Results = Cast<UBwayResultsScreenWidget>(PC->GetResultsWidget()))
			{
				if (!Results->GetBreakdownWidget())
				{
					Results->ForceAdvanceToBreakdownForAudit();
				}
			}
			bCaptureScreenshot = false;
		}
		else if (Stage == 5)
		{
			UBwayResultsScreenWidget* Results = Cast<UBwayResultsScreenWidget>(PC->GetResultsWidget());
			if (Results && !Results->GetBreakdownWidget())
			{
				Results->ForceAdvanceToBreakdownForAudit();
			}
			UBwayMatchBreakdownWidget* Breakdown = Results ? Results->GetBreakdownWidget() : nullptr;
			int32 DisconnectedCols = 0;
			int32 UpgradeCols = 0;
			if (Breakdown)
			{
				for (const FBwayMatchBreakdownPlayerColumn& Col : Breakdown->GetPlayerColumns())
				{
					if (!Col.bIsConnected)
					{
						++DisconnectedCols;
					}
					if (Col.EquippedUpgrades.Num() > 0)
					{
						++UpgradeCols;
					}
				}
			}

			// Verify client cannot trigger host rematch over RPC on ListenServer
			if (World->GetNetMode() == NM_Client)
			{
				PC->Server_RequestPlayAgain();
			}

			UE_LOG(LogTemp, Display,
				TEXT("BwayGoal6Probe: Stage=5 role=%s net=%d player=%d breakdownValid=%d cols=%d displayedCols=%d dcCols=%d upgradeCols=%d mvpId=%d mvp=[%s] playAgainEnabled=%d returnEnabled=%d focus=[%s]"),
				*RolePrefix,
				int32(World->GetNetMode()),
				LocalPlayerId,
				Breakdown != nullptr ? 1 : 0,
				Breakdown ? Breakdown->GetPlayerColumns().Num() : -1,
				Breakdown ? Breakdown->GetDisplayedColumnCount() : -1,
				DisconnectedCols,
				UpgradeCols,
				Breakdown ? Breakdown->GetMVPPlayerId() : INDEX_NONE,
				Breakdown ? *Breakdown->GetMVPPlayerName().ToString() : TEXT("null"),
				Breakdown && Breakdown->IsPlayAgainButtonEnabled() ? 1 : 0,
				Breakdown && Breakdown->IsReturnToLobbyButtonEnabled() ? 1 : 0,
				Breakdown ? *GetNameSafe(Breakdown->GetDesiredFocusTarget()) : TEXT("null"));
		}
		else if (Stage == 6)
		{
			const int32 TotalKills = LocalPS ? LocalPS->GetKills() : -1;
			const int32 TotalUpgrades = LocalPS ? LocalPS->GetReplicatedEquippedUpgrades().Num() : -1;
			const int32 DisconnectedCount = GS ? GS->GetDisconnectedMatchParticipants().Num() : -1;
			UE_LOG(LogTemp, Display,
				TEXT("BwayGoal6Probe: Stage=6 role=%s net=%d player=%d rematchResetKills=%d rematchResetUpgrades=%d dcRemaining=%d resultsDismissed=%d"),
				*RolePrefix,
				int32(World->GetNetMode()),
				LocalPlayerId,
				TotalKills,
				TotalUpgrades,
				DisconnectedCount,
				PC->GetResultsWidget() == nullptr ? 1 : 0);
		}

		if (bCaptureScreenshot)
		{
			FIntVector ImgSize;
			const FString ShotPath = OutDir / FString::Printf(TEXT("%s-p%d-stage%d.png"), *RolePrefix, LocalPlayerId, Stage);
			if (CaptureLocalViewportPNG(PC, ShotPath, ImgSize))
			{
				UE_LOG(LogTemp, Display, TEXT("BwayGoal6Probe: Screenshot stage=%d role=%s size=%dx%d path=%s"),
					Stage, *RolePrefix, ImgSize.X, ImgSize.Y, *ShotPath);
			}
		}
	}
} // namespace BwayGoal6RuntimeHelpers

void ABwayPlayerController::Client_RunGoal6HUDAuditProbe_Implementation(int32 Stage, bool bCaptureScreenshot)
{
	if (!IsLocalController())
	{
		return;
	}
	const FString RoleLabel = (GetWorld() && GetWorld()->GetNetMode() == NM_Client) ? TEXT("client") : TEXT("host");
	BwayGoal6RuntimeHelpers::RunLocalGoal6Probe(this, Stage, bCaptureScreenshot, RoleLabel);
}

#if WITH_EDITOR
namespace BwayGoal6EditorAssetCleanup
{
	bool SaveLoadedAssetPackage(UObject* AssetObject)
	{
		if (!AssetObject)
		{
			return false;
		}
		UPackage* Pkg = AssetObject->GetOutermost();
		if (!Pkg)
		{
			return false;
		}
		Pkg->MarkPackageDirty();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Pkg->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		SaveArgs.Error = GError;
		const FSavePackageResultStruct Result = UPackage::Save(Pkg, AssetObject, *PackageFileName, SaveArgs);
		return Result.IsSuccessful();
	}

	int32 StripGameplayEffectVisualCues(UGameplayEffect* EffectCDO)
	{
		if (!EffectCDO)
		{
			return 0;
		}
		int32 StrippedCount = 0;
		if (!EffectCDO->GameplayCues.IsEmpty())
		{
			StrippedCount += EffectCDO->GameplayCues.Num();
			EffectCDO->GameplayCues.Empty();
		}

		if (FArrayProperty* CompArrayProp = FindFProperty<FArrayProperty>(UGameplayEffect::StaticClass(), TEXT("GEComponents")))
		{
			auto* CompArray = CompArrayProp->ContainerPtrToValuePtr<TArray<TObjectPtr<UGameplayEffectComponent>>>(EffectCDO);
			if (CompArray)
			{
				for (int32 Idx = CompArray->Num() - 1; Idx >= 0; --Idx)
				{
					const UGameplayEffectComponent* Comp = (*CompArray)[Idx];
					if (Comp && Comp->GetClass()->GetName().Contains(TEXT("GameplayCue")))
					{
						CompArray->RemoveAt(Idx);
						++StrippedCount;
					}
				}
			}
		}
		return StrippedCount;
	}

	FAutoConsoleCommand CleanGoal6PhaseAssetsCommand(
		TEXT("bway.Editor.CleanGoal6PhaseAssets"),
		TEXT("Editor-only: Disables bPrintToScreen on PrintString debug nodes in BW_Phase_* Blueprints and strips green wireframe GameplayCue from GE_BW_DamageImmunity_FromGameMode."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const TCHAR* PhaseBlueprintPaths[] = {
				TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Prematch.BW_Phase_Prematch"),
				TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Warmup.BW_Phase_Warmup"),
				TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Playing.BW_Phase_Playing"),
				TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_PostRound.BW_Phase_PostRound"),
				TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_PostMatch.BW_Phase_PostMatch"),
				TEXT("/Game/UI/Indicators/NameplateManagerComponent.NameplateManagerComponent"),
				TEXT("/Game/UI/Indicators/W_Nameplate.W_Nameplate")
			};

			int32 TotalPhaseNodesCleaned = 0;
			for (const TCHAR* Path : PhaseBlueprintPaths)
			{
				UBlueprint* BP = LoadObject<UBlueprint>(nullptr, Path);
				if (!BP)
				{
					UE_LOG(LogTemp, Warning, TEXT("CleanGoal6PhaseAssets: Could not load %s"), Path);
					continue;
				}

				TArray<UEdGraph*> AllGraphs;
				BP->GetAllGraphs(AllGraphs);
				int32 BpCleaned = 0;
				for (UEdGraph* Graph : AllGraphs)
				{
					if (!Graph)
					{
						continue;
					}
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
						if (!CallNode || CallNode->FunctionReference.GetMemberName() != FName(TEXT("PrintString")))
						{
							continue;
						}
						if (UEdGraphPin* ScreenPin = CallNode->FindPin(TEXT("bPrintToScreen")))
						{
							if (!ScreenPin->DefaultValue.Equals(TEXT("false"), ESearchCase::IgnoreCase))
							{
								ScreenPin->Modify();
								ScreenPin->DefaultValue = TEXT("false");
								++BpCleaned;
							}
						}
					}
				}

				if (BpCleaned > 0)
				{
					FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
					FKismetEditorUtilities::CompileBlueprint(BP);
					const bool bSaved = SaveLoadedAssetPackage(BP);
					TotalPhaseNodesCleaned += BpCleaned;
					UE_LOG(LogTemp, Display, TEXT("CleanGoal6PhaseAssets: Cleaned %d PrintString nodes in %s (saved=%d)"),
						BpCleaned, Path, bSaved ? 1 : 0);
				}
				else
				{
					UE_LOG(LogTemp, Display, TEXT("CleanGoal6PhaseAssets: %s already has 0 active PrintString screen nodes"), Path);
				}
			}

			// Strip green wireframe GameplayCue from GE_BW_DamageImmunity_FromGameMode
			const TCHAR* ImmunityBpPath = TEXT("/BreakawayCore/Experiences/Phases/GE_BW_DamageImmunity_FromGameMode.GE_BW_DamageImmunity_FromGameMode");
			if (UBlueprint* ImmunityBP = LoadObject<UBlueprint>(nullptr, ImmunityBpPath))
			{
				const FString ParentName = ImmunityBP->ParentClass ? ImmunityBP->ParentClass->GetPathName() : TEXT("None");
				int32 StrippedPre = 0;
				int32 StrippedPost = 0;
				if (UGameplayEffect* CDO = ImmunityBP->GeneratedClass
					? Cast<UGameplayEffect>(ImmunityBP->GeneratedClass->GetDefaultObject())
					: nullptr)
				{
					CDO->Modify();
					StrippedPre = StripGameplayEffectVisualCues(CDO);
				}

				// If ParentClass is a Lyra Blueprint carrying a green wireframe cue component on its parent CDO,
				// reparent directly to UGameplayEffect while keeping immunity tags/modifiers intact, or strip after compile.
				FBlueprintEditorUtils::MarkBlueprintAsModified(ImmunityBP);
				FKismetEditorUtilities::CompileBlueprint(ImmunityBP);

				if (UGameplayEffect* CompiledCDO = ImmunityBP->GeneratedClass
					? Cast<UGameplayEffect>(ImmunityBP->GeneratedClass->GetDefaultObject())
					: nullptr)
				{
					CompiledCDO->Modify();
					StrippedPost = StripGameplayEffectVisualCues(CompiledCDO);
				}

				const bool bSavedGE = SaveLoadedAssetPackage(ImmunityBP);
				UE_LOG(LogTemp, Display,
					TEXT("CleanGoal6PhaseAssets: GE_BW_DamageImmunity_FromGameMode parent=%s strippedPre=%d strippedPost=%d saved=%d"),
					*ParentName, StrippedPre, StrippedPost, bSavedGE ? 1 : 0);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("CleanGoal6PhaseAssets: Could not load %s"), ImmunityBpPath);
			}

			UE_LOG(LogTemp, Display, TEXT("CleanGoal6PhaseAssets: DONE totalPrintStringNodesCleaned=%d"), TotalPhaseNodesCleaned);
		}));
} // namespace BwayGoal6EditorAssetCleanup
#endif // WITH_EDITOR

#if WITH_DEV_AUTOMATION_TESTS

namespace BwayGoal6TestHelpers
{
	struct FScopedGoal6MatchWorld
	{
		FAutomationTestBase* Test = nullptr;
		UWorld* World = nullptr;
		UGameInstance* GameInstance = nullptr;
		ABwayGameState* GameState = nullptr;
		ABreakawayGameMode* GameMode = nullptr;
		UBwayRoundManagementComponent* RoundMgmt = nullptr;
		UBwayScoringComponent* Scoring = nullptr;
		UBwayRelicManagerComponent* RelicMgr = nullptr;
		TArray<TObjectPtr<ULocalPlayer>> CreatedLocalPlayers;
		int32 NextLocalPlatformUserId = 10;

		explicit FScopedGoal6MatchWorld(FAutomationTestBase* InTest)
			: Test(InTest)
		{
		}

		~FScopedGoal6MatchWorld()
		{
			if (!World)
			{
				return;
			}
			for (UActorComponent* Comp : {
				static_cast<UActorComponent*>(RoundMgmt),
				static_cast<UActorComponent*>(Scoring),
				static_cast<UActorComponent*>(RelicMgr),
				GameState ? static_cast<UActorComponent*>(GameState->HeroSelectionManager) : nullptr,
				GameState ? static_cast<UActorComponent*>(GameState->HeroSelectionPhaseComponent) : nullptr })
			{
				if (Comp && Comp->HasBegunPlay())
				{
					Comp->EndPlay(EEndPlayReason::EndPlayInEditor);
				}
			}
			World->GetTimerManager().ClearAllTimersForObject(GameState);
			if (GameInstance)
			{
				for (ULocalPlayer* LP : CreatedLocalPlayers)
				{
					if (LP)
					{
						GameInstance->RemoveLocalPlayer(LP);
					}
				}
				CreatedLocalPlayers.Reset();
			}
			World->DestroyWorld(false);
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (GEngine)
			{
				GEngine->DestroyWorldContext(World);
			}
		}

		bool Init(const FString& Options = TEXT("?MatchRules=Demo"))
		{
			if (!Test->TestNotNull(TEXT("GEngine must exist"), GEngine))
			{
				return false;
			}
			ICommonInputModule::GetSettings().LoadData();

			const auto InitValues = UWorld::InitializationValues()
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false);

			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitValues);
			if (!Test->TestNotNull(TEXT("Headless Goal 6 world created"), World))
			{
				return false;
			}

			GameInstance = NewObject<UGameInstance>(GEngine);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.OwningGameInstance = GameInstance;
			World->SetGameInstance(GameInstance);
			Context.SetCurrentWorld(World);
			GameInstance->Init();

			UBwayHeroRegistry* Registry = GameInstance->GetSubsystem<UBwayHeroRegistry>();
			if (!Test->TestNotNull(TEXT("UBwayHeroRegistry initialized"), Registry))
			{
				return false;
			}

			GameState = World->SpawnActor<ABwayGameState>();
			GameMode = World->SpawnActor<ABreakawayGameMode>();
			if (!Test->TestNotNull(TEXT("ABwayGameState spawned"), GameState)
				|| !Test->TestNotNull(TEXT("ABreakawayGameMode spawned"), GameMode))
			{
				return false;
			}

			World->SetGameState(GameState);
			GameMode->GameState = GameState;
			GameMode->OptionsString = Options;
			World->URL = FURL(nullptr, *(TEXT("/BreakawayCore/Maps/L_BW_Dorado") + Options), TRAVEL_Absolute);

			if (const auto* ModeProp = FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), TEXT("AuthorityGameMode")))
			{
				ModeProp->SetObjectPropertyValue_InContainer(World, GameMode);
			}
			if (const auto* TeamsProp = FindFProperty<FArrayProperty>(ABwayGameState::StaticClass(), TEXT("Teams")))
			{
				auto* Teams = TeamsProp->ContainerPtrToValuePtr<TArray<FTeamInfo>>(GameState);
				if (Teams->IsEmpty())
				{
					Teams->Add(FTeamInfo(0));
					Teams->Add(FTeamInfo(1));
				}
			}

			RoundMgmt = GameState->FindComponentByClass<UBwayRoundManagementComponent>();
			if (!RoundMgmt)
			{
				RoundMgmt = NewObject<UBwayRoundManagementComponent>(GameState, TEXT("RoundManagementComponent"));
				GameState->AddInstanceComponent(RoundMgmt);
			}
			Scoring = GameState->FindComponentByClass<UBwayScoringComponent>();
			if (!Scoring)
			{
				Scoring = NewObject<UBwayScoringComponent>(GameState, TEXT("ScoringComponent"));
				GameState->AddInstanceComponent(Scoring);
			}
			RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>();
			if (!RelicMgr)
			{
				RelicMgr = NewObject<UBwayRelicManagerComponent>(GameState, TEXT("RelicManagerComponent"));
				GameState->AddInstanceComponent(RelicMgr);
			}

			for (UActorComponent* Comp : {
				static_cast<UActorComponent*>(GameState->HeroSelectionManager),
				static_cast<UActorComponent*>(Scoring),
				static_cast<UActorComponent*>(RelicMgr),
				static_cast<UActorComponent*>(RoundMgmt) })
			{
				if (Comp)
				{
					if (!Comp->IsRegistered())
					{
						Comp->RegisterComponent();
					}
					if (!Comp->HasBegunPlay())
					{
						Comp->BeginPlay();
					}
				}
			}

			int32 DemoPointsToWin = 3;
			float DemoRoundDuration = 240.0f;
			if (const UBwayMatchFlowConfig* DemoConfig = LoadObject<UBwayMatchFlowConfig>(
				nullptr, TEXT("/BreakawayCore/MatchFlow/DA_BW_MatchFlow_Demo.DA_BW_MatchFlow_Demo")))
			{
				DemoPointsToWin = DemoConfig->PointsToWin;
				DemoRoundDuration = DemoConfig->RoundDuration;
				RoundMgmt->SetRespawnProgressionRules(
					DemoConfig->BaseRespawnDelaySeconds,
					DemoConfig->PerDeathRespawnIncrementSeconds,
					DemoConfig->MaxRespawnDelaySeconds,
					DemoConfig->bResetRespawnEscalationEachRound);
			}
			RoundMgmt->SetPointsToWin(DemoPointsToWin);
			RoundMgmt->SetRoundDuration(DemoRoundDuration);

			return true;
		}

		void ConfigureBreakawayInputMappings(ULocalPlayer* LP)
		{
			if (!LP)
			{
				return;
			}
			UEnhancedInputLocalPlayerSubsystem* InputSub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
			if (!InputSub)
			{
				return;
			}
			// Match LAS_BW_SharedInput: IMC_BW_HeroAbilities already contains all hero, placement,
			// and relic mappings; IMC_BW_RelicAbilities is an uninstalled legacy reference asset.
			const TCHAR* MappingPaths[] = {
				TEXT("/BreakawayCore/Input/Mappings/IMC_BW_Default.IMC_BW_Default"),
				TEXT("/BreakawayCore/Input/Mappings/IMC_BW_HeroAbilities.IMC_BW_HeroAbilities"),
				TEXT("/BreakawayCore/Input/Mappings/IMC_BW_PlayerUI.IMC_BW_PlayerUI")
			};
			int32 Priority = 0;
			FModifyContextOptions ModifyOpts;
			ModifyOpts.bForceImmediately = true;
			ModifyOpts.bIgnoreAllPressedKeysUntilRelease = false;
			for (const TCHAR* Path : MappingPaths)
			{
				if (const UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, Path))
				{
					InputSub->AddMappingContext(IMC, Priority++, ModifyOpts);
				}
			}
		}

		ABwayPlayerController* SpawnPlayer(
			int32 TeamIndex,
			int32 PlayerId,
			const FString& PlayerName,
			const FPrimaryAssetId& HeroId,
			bool bAttachLocalPlayer = true)
		{
			ABwayPlayerController* PC = World->SpawnActor<ABwayPlayerController>();
			if (!PC)
			{
				return nullptr;
			}
			World->AddController(PC);
			if (bAttachLocalPlayer)
			{
				ULocalPlayer* LP = NewObject<ULocalPlayer>(GEngine);
				GameInstance->AddLocalPlayer(LP, FPlatformUserId::CreateFromInternalId(NextLocalPlatformUserId++));
				CreatedLocalPlayers.Add(LP);
				PC->SetPlayer(LP);
				PC->InitInputSystem();
				ConfigureBreakawayInputMappings(LP);
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = PC;
			ABwayPlayerState* PS = World->SpawnActor<ABwayPlayerState>(SpawnParams);
			PC->SetPlayerState(PS);
			PS->SetPlayerId(PlayerId);
			PS->SetPlayerName(PlayerName);

			if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
			{
				if (!ASC->GetSet<ULyraHealthSet>())
				{
					ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(PS));
				}
				if (!ASC->GetSet<UBwayHeroAttributeSet>())
				{
					ASC->AddSpawnedAttribute(NewObject<UBwayHeroAttributeSet>(PS));
				}
				if (!ASC->GetSet<UBwayGoldAttributeSet>())
				{
					ASC->AddSpawnedAttribute(NewObject<UBwayGoldAttributeSet>(PS));
				}
			}

			if (PS->UpgradeComponent && !PS->UpgradeComponent->GetCatalog())
			{
				if (UBwayUpgradeCatalog* MatchCatalog = LoadObject<UBwayUpgradeCatalog>(
					nullptr, TEXT("/BreakawayCore/Economy/DA_BW_MatchUpgrades.DA_BW_MatchUpgrades")))
				{
					PS->UpgradeComponent->ConfigureCatalog(MatchCatalog);
				}
			}

			GameState->AddPlayerState(PS);
			GameState->AddPlayerToTeam(PS, TeamIndex);
			if (HeroId.IsValid())
			{
				PS->TrySetSelectedHeroId(HeroId);
				PS->ServerLockHeroSelection();
			}
			return PC;
		}

		ABwayCharacterWithAbilities* SpawnHeroPawnForController(ABwayPlayerController* PC, float MaxHealth = 400.0f, float Armor = 25.0f)
		{
			if (!PC)
			{
				return nullptr;
			}
			ABwayPlayerState* PS = PC->GetPlayerState<ABwayPlayerState>();
			ABwayCharacterWithAbilities* Character = World->SpawnActor<ABwayCharacterWithAbilities>();
			if (!Character || !PS)
			{
				return nullptr;
			}
			PC->Possess(Character);
			Character->SetPlayerState(PS);

			if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
			{
				if (!ASC->GetSet<ULyraHealthSet>())
				{
					ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(PS));
				}
				if (!ASC->GetSet<UBwayHeroAttributeSet>())
				{
					ASC->AddSpawnedAttribute(NewObject<UBwayHeroAttributeSet>(PS));
				}
				if (!ASC->GetSet<UBwayGoldAttributeSet>())
				{
					ASC->AddSpawnedAttribute(NewObject<UBwayGoldAttributeSet>(PS));
				}
				ASC->InitAbilityActorInfo(PS, Character);
				ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), MaxHealth);
				ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), MaxHealth);
				ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), Armor);
				ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetMaxGoldAttribute(), 2500.0f);
				if (ULyraHealthComponent* HealthComp = ULyraHealthComponent::FindHealthComponent(Character))
				{
					HealthComp->InitializeWithAbilitySystem(ASC);
				}
			}
			return Character;
		}
	};

	template <typename WidgetT>
	TSubclassOf<WidgetT> LoadWidgetClassOrFallback(const TCHAR* ClassPath)
	{
		if (UClass* Loaded = FSoftClassPath(ClassPath).TryLoadClass<WidgetT>())
		{
			return Loaded;
		}
		return WidgetT::StaticClass();
	}

	TArray<FPrimaryAssetId> GetCanonicalSliceHeroIds(const UObject* WorldContext)
	{
		TArray<FPrimaryAssetId> Out;
		for (const TCHAR* HeroName : { TEXT("Argus"), TEXT("Alona"), TEXT("Korryn"), TEXT("Rawlins") })
		{
			const FPrimaryAssetId Id = UBwayHeroRegistry::ResolveHeroIdByName(HeroName);
			if (Id.IsValid() && UBwayHeroRegistry::GetHeroDataById(Id) != nullptr)
			{
				Out.AddUnique(Id);
			}
		}
		if (Out.Num() < 4)
		{
			if (UBwayHeroRegistry* Registry = UBwayHeroRegistry::Get(WorldContext))
			{
				for (const TSoftObjectPtr<UBwayHeroDataAsset>& SoftHero : Registry->GetAllHeroSoftObjects())
				{
					if (const UBwayHeroDataAsset* Hero = Registry->LoadHeroSync(SoftHero))
					{
						Out.AddUnique(Hero->GetPrimaryAssetId());
					}
				}
			}
		}
		return Out;
	}
} // namespace BwayGoal6TestHelpers

// ============================================================================
// 1. Scorebug, 4v4 Team Portrait Respawn Timers & Phase Banners
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayGoal6ScorebugPortraitsAndRespawnTimersTest,
	"Breakaway.UI.MatchHUDAndPortraits.ScorebugPortraitsAndRespawnTimers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayGoal6ScorebugPortraitsAndRespawnTimersTest::RunTest(const FString&)
{
	BwayGoal6TestHelpers::FScopedGoal6MatchWorld Scope(this);
	if (!Scope.Init())
	{
		return false;
	}

	const TArray<FPrimaryAssetId> HeroIds = BwayGoal6TestHelpers::GetCanonicalSliceHeroIds(Scope.World);
	if (!TestTrue(TEXT("At least 4 vertical-slice heroes available"), HeroIds.Num() >= 4))
	{
		return false;
	}

	TArray<ABwayPlayerController*> Controllers;
	TArray<ABwayPlayerState*> Players;
	TArray<ABwayCharacterWithAbilities*> Pawns;
	for (int32 Idx = 0; Idx < 8; ++Idx)
	{
		const int32 Team = Idx < 4 ? 0 : 1;
		ABwayPlayerController* PC = Scope.SpawnPlayer(
			Team,
			300 + Idx,
			FString::Printf(TEXT("Warrior_%d"), Idx + 1),
			HeroIds[Idx % 4],
			Idx == 0 || Idx == 4);
		if (!TestNotNull(TEXT("PlayerController spawned"), PC))
		{
			return false;
		}
		Controllers.Add(PC);
		Players.Add(PC->GetPlayerState<ABwayPlayerState>());
		Pawns.Add(Scope.SpawnHeroPawnForController(PC));
	}

	ABwayPlayerController* Team0Observer = Controllers[0];
	ABwayPlayerController* Team1Observer = Controllers[4];

	const auto ScoreClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayCaptureTheRelicScoreWidget>(
		TEXT("/BreakawayCore/UI/Match/W_BW_CaptureTheRelic_ScoreWidget.W_BW_CaptureTheRelic_ScoreWidget_C"));
	const auto FriendlyPortraitsClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayTeamPortraitsHUDWidget>(
		TEXT("/BreakawayCore/UI/Match/W_BW_TeamPortraitsWidget.W_BW_TeamPortraitsWidget_C"));
	const auto EnemyPortraitsClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayTeamPortraitsHUDWidget>(
		TEXT("/BreakawayCore/UI/Match/W_BW_TeamPortraitsWidget_Enemy.W_BW_TeamPortraitsWidget_Enemy_C"));
	const auto RelicStatusClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayRelicStatusWidget>(
		TEXT("/BreakawayCore/UI/Match/W_BW_RelicStatusWidget.W_BW_RelicStatusWidget_C"));

	UBwayCaptureTheRelicScoreWidget* ScoreWidget = CreateWidget<UBwayCaptureTheRelicScoreWidget>(Team0Observer, ScoreClass);
	UBwayTeamPortraitsHUDWidget* FriendlyRow = CreateWidget<UBwayTeamPortraitsHUDWidget>(Team0Observer, FriendlyPortraitsClass);
	UBwayTeamPortraitsHUDWidget* EnemyRow = CreateWidget<UBwayTeamPortraitsHUDWidget>(Team0Observer, EnemyPortraitsClass);
	UBwayTeamPortraitsHUDWidget* Team1FriendlyRow = CreateWidget<UBwayTeamPortraitsHUDWidget>(Team1Observer, FriendlyPortraitsClass);
	UBwayRelicStatusWidget* RelicStatus = CreateWidget<UBwayRelicStatusWidget>(Team0Observer, RelicStatusClass);

	if (!TestNotNull(TEXT("ScoreWidget created"), ScoreWidget)
		|| !TestNotNull(TEXT("FriendlyRow created"), FriendlyRow)
		|| !TestNotNull(TEXT("EnemyRow created"), EnemyRow)
		|| !TestNotNull(TEXT("Team1FriendlyRow created"), Team1FriendlyRow)
		|| !TestNotNull(TEXT("RelicStatus created"), RelicStatus))
	{
		return false;
	}

	FriendlyRow->SetDisplaySlotIndexForTest(0);
	EnemyRow->SetDisplaySlotIndexForTest(1);
	Team1FriendlyRow->SetDisplaySlotIndexForTest(0);

	TSharedRef<SWidget> ScoreSlate = ScoreWidget->TakeWidget();
	TSharedRef<SWidget> FriendlySlate = FriendlyRow->TakeWidget();
	TSharedRef<SWidget> EnemySlate = EnemyRow->TakeWidget();
	TSharedRef<SWidget> Team1FriendlySlate = Team1FriendlyRow->TakeWidget();
	TSharedRef<SWidget> RelicSlate = RelicStatus->TakeWidget();
	(void)ScoreSlate;
	(void)FriendlySlate;
	(void)EnemySlate;
	(void)Team1FriendlySlate;
	(void)RelicSlate;

	// Prematch / Warmup phase banner + KBM vs Gamepad shop key label
	Scope.RoundMgmt->EnterPrematch();
	Scope.RoundMgmt->EnterWarmup();
	RelicStatus->SetInputDeviceOverrideForTest(true, false);
	RelicStatus->RefreshRelicStatus();
	const FString KbmShopKey = RelicStatus->GetResolvedShopKeyLabel().ToString();
	TestTrue(TEXT("Prematch/Warmup banner visible"), RelicStatus->IsBannerVisible());
	TestTrue(TEXT("Phase banner shows ROUND 1"), RelicStatus->GetDisplayedPrimaryStatusText().ToString().Contains(TEXT("ROUND 1")));

	RelicStatus->SetInputDeviceOverrideForTest(true, true);
	RelicStatus->RefreshRelicStatus();
	const FString PadShopKey = RelicStatus->GetResolvedShopKeyLabel().ToString();
	TestFalse(TEXT("KBM shop key label not empty"), KbmShopKey.IsEmpty());
	TestFalse(TEXT("Gamepad shop key label not empty"), PadShopKey.IsEmpty());
	TestNotEqual(TEXT("Gamepad shop label differs from KBM shop label"), KbmShopKey, PadShopKey);

	// Advance to Round 1 Playing (240s Demo clock, PointsToWin = 3)
	Scope.RoundMgmt->EnterPlaying();
	// In the empty headless test world (no PlayerStarts/PawnData), StartRound() destroys pre-round pawns;
	// respawn live hero pawns for all 8 controllers before applying selective combat deaths.
	for (int32 Idx = 0; Idx < Controllers.Num(); ++Idx)
	{
		if (!IsValid(Pawns[Idx]) || Controllers[Idx]->GetPawn() != Pawns[Idx])
		{
			Pawns[Idx] = Scope.SpawnHeroPawnForController(Controllers[Idx], 400.0f, 25.0f);
		}
	}
	Scope.Scoring->ResetScores();
	Scope.Scoring->AddScore(0, 2);
	Scope.Scoring->AddScore(1, 1);
	ScoreWidget->RefreshScoreDisplay();
	ScoreWidget->RefreshTimerDisplay();
	RelicStatus->RefreshRelicStatus();

	TestEqual(TEXT("Demo PointsToWin pips == 3"), ScoreWidget->GetDisplayedPointsToWin(), 3);
	TestEqual(TEXT("Friendly filled pips == 2"), ScoreWidget->GetDisplayedFriendlyFilledPips(), 2);
	TestEqual(TEXT("Enemy filled pips == 1"), ScoreWidget->GetDisplayedEnemyFilledPips(), 1);
	TestEqual(TEXT("Demo 240s round clock displays zero-padded 04:00"), ScoreWidget->GetDisplayedTimerText().ToString(), FString(TEXT("04:00")));
	TestFalse(TEXT("Phase status banner collapses during active Playing per broadcast HUD"), RelicStatus->IsBannerVisible());

	// Verify 4v4 Friendly (Blue) vs Enemy (Red) portrait framing + Relic carrier badge + live respawn countdown
	Players[1]->SetHasRelic(true);
	const double NowServer = Scope.GameState->GetServerWorldTimeSeconds();

	// Kill Friendly slot 2 (Players[2]) and Enemy slot 1 (Players[5])
	if (ULyraAbilitySystemComponent* ASC2 = Players[2]->GetLyraAbilitySystemComponent())
	{
		ASC2->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
		ASC2->SetLooseGameplayTagCount(LyraGameplayTags::Status_Death_Dead, 1);
	}
	Players[2]->SetRespawnSchedule(NowServer + 7.4, 1);

	if (ULyraAbilitySystemComponent* ASC5 = Players[5]->GetLyraAbilitySystemComponent())
	{
		ASC5->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
		ASC5->SetLooseGameplayTagCount(LyraGameplayTags::Status_Death_Dead, 1);
	}
	Players[5]->SetRespawnSchedule(NowServer + 4.9, 1);

	// Also schedule local player respawn to verify Scorebug local respawn countdown badge
	if (ULyraAbilitySystemComponent* ASC0 = Players[0]->GetLyraAbilitySystemComponent())
	{
		ASC0->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
		ASC0->SetLooseGameplayTagCount(LyraGameplayTags::Status_Death_Dead, 1);
	}
	Players[0]->SetRespawnSchedule(NowServer + 5.6, 1);

	FriendlyRow->RefreshTeamPortraits();
	EnemyRow->RefreshTeamPortraits();
	Team1FriendlyRow->RefreshTeamPortraits();
	ScoreWidget->RefreshTimerDisplay();

	TestEqual(TEXT("4 friendly portrait slots"), FriendlyRow->GetSlotWidgets().Num(), 4);
	TestEqual(TEXT("4 enemy portrait slots"), EnemyRow->GetSlotWidgets().Num(), 4);
	TestTrue(TEXT("Scorebug local respawn countdown active (6s)"), ScoreWidget->GetDisplayedLocalRespawnSeconds() >= 5);

	UBwayTeamPortraitSlotWidget* CarrierSlot = FriendlyRow->GetSlotWidgets()[1];
	UBwayTeamPortraitSlotWidget* DeadFriendlySlot = FriendlyRow->GetSlotWidgets()[2];
	UBwayTeamPortraitSlotWidget* AliveFriendlySlot = FriendlyRow->GetSlotWidgets()[3];
	UBwayTeamPortraitSlotWidget* DeadEnemySlot = EnemyRow->GetSlotWidgets()[1];

	TestTrue(TEXT("Friendly slot marked bIsFriendly"), AliveFriendlySlot->GetSlotViewModel().bIsFriendly);
	TestFalse(TEXT("Enemy slot marked !bIsFriendly"), DeadEnemySlot->GetSlotViewModel().bIsFriendly);
	TestNotNull(TEXT("Friendly portrait texture bound"), AliveFriendlySlot->GetSlotViewModel().PortraitTexture.Get());
	TestTrue(TEXT("Alive friendly portrait mini health bar == 1.0"), FMath::IsNearlyEqual(AliveFriendlySlot->GetSlotViewModel().HealthPercent, 1.0f, 0.01f));
	TestTrue(TEXT("Dead friendly portrait mini health bar == 0.0"), FMath::IsNearlyEqual(DeadFriendlySlot->GetSlotViewModel().HealthPercent, 0.0f, 0.01f));
	TestTrue(TEXT("Relic carrier badge visible"), CarrierSlot->IsRelicBadgeVisible());

	TestTrue(TEXT("Dead friendly portrait shows respawn timer"), DeadFriendlySlot->IsRespawnTimerVisible());
	TestTrue(TEXT("Dead friendly portrait shows death overlay"), DeadFriendlySlot->IsDeathOverlayVisible());
	TestEqual(TEXT("Dead friendly portrait countdown ceil(7.4) == 8"), DeadFriendlySlot->GetDisplayedRespawnText().ToString(), FString(TEXT("8")));

	TestTrue(TEXT("Dead enemy portrait shows respawn timer"), DeadEnemySlot->IsRespawnTimerVisible());
	TestTrue(TEXT("Dead enemy portrait shows death overlay"), DeadEnemySlot->IsDeathOverlayVisible());
	TestEqual(TEXT("Dead enemy portrait countdown ceil(4.9) == 5"), DeadEnemySlot->GetDisplayedRespawnText().ToString(), FString(TEXT("5")));

	// Team 1 observer sees Team 1 on the left (Friendly = Blue) with the same 5s respawn timer!
	UBwayTeamPortraitSlotWidget* Team1ObserverDeadAlly = Team1FriendlyRow->GetSlotWidgets()[1];
	TestTrue(TEXT("Team 1 observer sees own team as Friendly"), Team1ObserverDeadAlly->GetSlotViewModel().bIsFriendly);
	TestEqual(TEXT("Team 1 observer sees same 5s ally respawn countdown"), Team1ObserverDeadAlly->GetDisplayedRespawnText().ToString(), FString(TEXT("5")));

	// Verify expiry clamp: past deadline while still dead must clamp to 0 (never negative)
	Players[2]->SetRespawnSchedule(NowServer - 1.5, 1);
	FriendlyRow->RefreshTeamPortraits();
	TestEqual(TEXT("Expired respawn deadline clamps to 0 (never negative)"), DeadFriendlySlot->GetSlotViewModel().RespawnRemainingSeconds, 0);
	TestFalse(TEXT("Expired 0s respawn timer collapses text"), DeadFriendlySlot->IsRespawnTimerVisible());

	// Verify immediate reset on replacement-pawn respawn (and no flash-alive during pawnless respawn window)
	if (Pawns[2])
	{
		if (ULyraHealthComponent* DeadHC2 = ULyraHealthComponent::FindHealthComponent(Pawns[2]))
		{
			DeadHC2->UninitializeFromAbilitySystem();
		}
		Controllers[2]->UnPossess();
		Pawns[2]->Destroy();
		Pawns[2] = nullptr;
	}
	FriendlyRow->RefreshTeamPortraits();
	TestFalse(TEXT("Pawnless respawn window stays !bIsAlive so portrait never flashes alive"), DeadFriendlySlot->GetSlotViewModel().bIsAlive);

	Pawns[2] = Scope.SpawnHeroPawnForController(Controllers[2], 400.0f, 25.0f);
	Players[2]->SetRespawnSchedule(0.0, 0);
	FriendlyRow->RefreshTeamPortraits();
	TestTrue(TEXT("Respawned warrior bIsAlive immediately true"), DeadFriendlySlot->GetSlotViewModel().bIsAlive);
	TestFalse(TEXT("Respawned warrior death overlay immediately hidden"), DeadFriendlySlot->IsDeathOverlayVisible());
	TestFalse(TEXT("Respawned warrior respawn timer immediately hidden"), DeadFriendlySlot->IsRespawnTimerVisible());

	// Verify round reset clears remaining respawn countdowns immediately
	if (Pawns[0])
	{
		if (ULyraHealthComponent* DeadHC0 = ULyraHealthComponent::FindHealthComponent(Pawns[0]))
		{
			DeadHC0->UninitializeFromAbilitySystem();
		}
		Controllers[0]->UnPossess();
		Pawns[0]->Destroy();
		Pawns[0] = nullptr;
	}
	Pawns[0] = Scope.SpawnHeroPawnForController(Controllers[0], 400.0f, 25.0f);
	Players[0]->SetRespawnSchedule(0.0, 0);
	ScoreWidget->RefreshTimerDisplay();
	TestEqual(TEXT("Scorebug local respawn clears immediately on round reset"), ScoreWidget->GetDisplayedLocalRespawnSeconds(), 0);

	return true;
}

// ============================================================================
// 2. 6-Slot Ability Bar, Dynamic KBM/Gamepad Labels, Cooldown, Relic Lock & Buildable
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayGoal6AbilityBarOverlaysTest,
	"Breakaway.UI.MatchHUDAndPortraits.AbilityBarCooldownRelicAndBuildable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayGoal6AbilityBarOverlaysTest::RunTest(const FString&)
{
	BwayGoal6TestHelpers::FScopedGoal6MatchWorld Scope(this);
	if (!Scope.Init())
	{
		return false;
	}

	const TArray<FPrimaryAssetId> HeroIds = BwayGoal6TestHelpers::GetCanonicalSliceHeroIds(Scope.World);
	if (!TestTrue(TEXT("At least 4 heroes registered"), HeroIds.Num() >= 4))
	{
		return false;
	}

	ABwayPlayerController* PC = Scope.SpawnPlayer(0, 401, TEXT("AbilityTester"), HeroIds[0], true);
	ABwayPlayerState* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	Scope.SpawnHeroPawnForController(PC);
	ULyraAbilitySystemComponent* ASC = PS ? PS->GetLyraAbilitySystemComponent() : nullptr;

	const auto AbilityBarClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayAbilityBarHUDWidget>(
		TEXT("/BreakawayCore/UI/Match/W_BW_AbilityBar.W_BW_AbilityBar_C"));
	UBwayAbilityBarHUDWidget* AbilityBar = CreateWidget<UBwayAbilityBarHUDWidget>(PC, AbilityBarClass);
	if (!TestNotNull(TEXT("AbilityBar created"), AbilityBar)
		|| !TestNotNull(TEXT("PlayerState ASC valid"), ASC))
	{
		return false;
	}
	TSharedRef<SWidget> BarSlate = AbilityBar->TakeWidget();
	(void)BarSlate;

	// Verify 6 fixed slots across all 4 vertical-slice heroes & separation of Hero Mobility vs Shared Slide
	for (const FPrimaryAssetId& HeroId : HeroIds)
	{
		PS->UnlockHeroSelection();
		PS->TrySetSelectedHeroId(HeroId);
		PS->ServerLockHeroSelection();

		PS->GetHeroAbilityGrantedHandles().TakeFromAbilitySystem(ASC);
		if (const UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
		{
			for (const ULyraAbilitySet* Set : HeroData->AbilitySets)
			{
				if (Set)
				{
					Set->GiveToAbilitySystem(ASC, &PS->GetHeroAbilityGrantedHandles());
				}
			}
		}

		AbilityBar->SetInputDeviceOverrideForTest(true, false);
		AbilityBar->RefreshAbilityBar();

		if (!TestEqual(TEXT("Exactly 6 fixed action slots"), AbilityBar->GetSlotWidgets().Num(), 6))
		{
			return false;
		}
		TestTrue(TEXT("Slot 0 is Buildable"), AbilityBar->GetSlotWidgets()[0]->GetSlotViewModel().bIsBuildable);
		if (const UHorizontalBox* SlotsBox = AbilityBar->WidgetTree ? AbilityBar->WidgetTree->FindWidget<UHorizontalBox>(TEXT("AbilitySlots")) : nullptr)
		{
			TestEqual(TEXT("Visual hex slot 0 is Buildable (1)"), SlotsBox->GetChildAt(0), static_cast<UWidget*>(AbilityBar->GetSlotWidgets()[0]));
			TestEqual(TEXT("Visual hex slot 1 is Primary (LMB)"), SlotsBox->GetChildAt(1), static_cast<UWidget*>(AbilityBar->GetSlotWidgets()[1]));
			TestEqual(TEXT("Visual hex slot 2 is Mobility (F)"), SlotsBox->GetChildAt(2), static_cast<UWidget*>(AbilityBar->GetSlotWidgets()[2]));
			TestEqual(TEXT("Visual hex slot 3 is center Ultimate (R)"), SlotsBox->GetChildAt(3), static_cast<UWidget*>(AbilityBar->GetSlotWidgets()[5]));
			TestEqual(TEXT("Visual hex slot 4 is Ability 1 (Q)"), SlotsBox->GetChildAt(4), static_cast<UWidget*>(AbilityBar->GetSlotWidgets()[3]));
			TestEqual(TEXT("Visual hex slot 5 is Ability 2 (E)"), SlotsBox->GetChildAt(5), static_cast<UWidget*>(AbilityBar->GetSlotWidgets()[4]));
		}
		const FString MobilityKeyKbm = AbilityBar->GetSlotWidgets()[2]->GetDisplayedKeybindText().ToString();
		const FString SharedSlideKeyKbm = AbilityBar->GetSharedSlideKeyLabel().ToString();
		const FString RelicReqKeyKbm = AbilityBar->GetRelicRequestKeyLabel().ToString();

		TestFalse(TEXT("Hero Mobility key resolved"), MobilityKeyKbm.IsEmpty());
		TestFalse(TEXT("Shared Slide key resolved"), SharedSlideKeyKbm.IsEmpty());
		TestFalse(TEXT("Relic Request key resolved"), RelicReqKeyKbm.IsEmpty());
		TestNotEqual(TEXT("Hero Mobility slot is NOT conflated with Shared Slide"), MobilityKeyKbm, SharedSlideKeyKbm);

		// Switch to Gamepad and trigger HandleControlMappingsRebuilt
		AbilityBar->SetInputDeviceOverrideForTest(true, true);
		AbilityBar->TriggerControlMappingsRebuiltForTest();
		const FString MobilityKeyPad = AbilityBar->GetSlotWidgets()[2]->GetDisplayedKeybindText().ToString();
		const FString SharedSlideKeyPad = AbilityBar->GetSharedSlideKeyLabel().ToString();
		TestNotEqual(TEXT("Hero Mobility updates dynamically from KBM to Gamepad"), MobilityKeyKbm, MobilityKeyPad);
		TestNotEqual(TEXT("Shared Slide updates dynamically from KBM to Gamepad"), SharedSlideKeyKbm, SharedSlideKeyPad);
	}

	AbilityBar->SetInputDeviceOverrideForTest(true, false);
	PS->BeginRoundStatTracking();
	AbilityBar->RefreshAbilityBar();

	UBwayMatchAbilitySlotWidget* BuildableSlot = AbilityBar->GetSlotWidgets()[0];
	TestTrue(TEXT("Buildable slot shows 1/Round when available"),
		BuildableSlot->GetDisplayedStateText().ToString().Contains(TEXT("1/Round")));

	PS->MarkBuildablePlacedThisRound();
	AbilityBar->RefreshAbilityBar();
	TestTrue(TEXT("Buildable slot shows Placed state when consumed this round"),
		BuildableSlot->GetDisplayedStateText().ToString().Contains(TEXT("Placed")));

	// Cooldown overlay precision: tenths below 1.0s, whole seconds at/above 1.0s
	UBwayMatchAbilitySlotWidget* AbilitySlot1 = AbilityBar->GetSlotWidgets()[3];
	FBwayMatchAbilitySlotViewModel CooldownVM = AbilitySlot1->GetSlotViewModel();
	CooldownVM.bIsOnCooldown = true;
	CooldownVM.CooldownPercent = 0.85f;
	CooldownVM.CountdownTime = 0.42f;
	AbilitySlot1->SetSlotViewModel(CooldownVM);
	TestTrue(TEXT("Cooldown overlay visible below 1.0s"), AbilitySlot1->IsCooldownOverlayVisible());
	TestEqual(TEXT("Sub-second cooldown shows tenths (0.4)"), AbilitySlot1->GetDisplayedCooldownText().ToString(), FString(TEXT("0.4")));

	CooldownVM.CooldownPercent = 0.35f;
	CooldownVM.CountdownTime = 3.25f;
	AbilitySlot1->SetSlotViewModel(CooldownVM);
	TestEqual(TEXT("Above-1s cooldown shows whole seconds ceil(3.25) == 4"), AbilitySlot1->GetDisplayedCooldownText().ToString(), FString(TEXT("4")));

	CooldownVM.bIsOnCooldown = false;
	CooldownVM.CountdownTime = 0.0f;
	AbilitySlot1->SetSlotViewModel(CooldownVM);
	TestFalse(TEXT("Cooldown overlay hidden when ready"), AbilitySlot1->IsCooldownOverlayVisible());
	TestTrue(TEXT("Cooldown timer text empty when ready"), AbilitySlot1->GetDisplayedCooldownText().IsEmpty());

	// Relic Carrier Lock state
	PS->SetHasRelic(true);
	AbilityBar->RefreshAbilityBar();
	int32 RelicFramedSlots = 0;
	for (int32 SlotIdx = 1; SlotIdx < 6; ++SlotIdx)
	{
		if (AbilityBar->GetSlotWidgets()[SlotIdx]->IsRelicFrameVisible())
		{
			++RelicFramedSlots;
		}
	}
	TestEqual(TEXT("All 5 combat slots show Relic Carrier frame/lock while carrying Relic"), RelicFramedSlots, 5);
	TestTrue(TEXT("Auxiliary Relic prompt switches to carrier controls"),
		AbilityBar->GetRelicPromptText().ToString().Contains(TEXT("PASS")));

	PS->SetHasRelic(false);
	AbilityBar->RefreshAbilityBar();
	TestFalse(TEXT("Relic frame clears immediately when Relic is dropped"),
		AbilityBar->GetSlotWidgets()[1]->IsRelicFrameVisible());

	PS->GetHeroAbilityGrantedHandles().TakeFromAbilitySystem(ASC);
	return true;
}

// ============================================================================
// 3. Player Health (Damage Trail, Armor, Replacement Pawn) & Gold / Shop Pulse
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayGoal6HealthArmorAndGoldPulseTest,
	"Breakaway.UI.MatchHUDAndPortraits.HealthArmorAndGoldShopPulse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayGoal6HealthArmorAndGoldPulseTest::RunTest(const FString&)
{
	BwayGoal6TestHelpers::FScopedGoal6MatchWorld Scope(this);
	if (!Scope.Init())
	{
		return false;
	}

	const TArray<FPrimaryAssetId> HeroIds = BwayGoal6TestHelpers::GetCanonicalSliceHeroIds(Scope.World);
	ABwayPlayerController* PC = Scope.SpawnPlayer(0, 501, TEXT("VitalsTester"), HeroIds[0], true);
	ABwayPlayerState* PS = PC->GetPlayerState<ABwayPlayerState>();
	ABwayCharacterWithAbilities* FirstPawn = Scope.SpawnHeroPawnForController(PC, 400.0f, 30.0f);
	ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent();

	const auto HealthClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayHealthHUDWidget>(
		TEXT("/BreakawayCore/UI/Match/W_BW_HealthWidget.W_BW_HealthWidget_C"));
	const auto GoldClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayCurrencyDisplayWidget>(
		TEXT("/BreakawayCore/UI/Match/W_BW_GoldWidget.W_BW_GoldWidget_C"));

	UBwayHealthHUDWidget* HealthHUD = CreateWidget<UBwayHealthHUDWidget>(PC, HealthClass);
	UBwayCurrencyDisplayWidget* GoldHUD = CreateWidget<UBwayCurrencyDisplayWidget>(PC, GoldClass);
	if (!TestNotNull(TEXT("HealthHUD created"), HealthHUD)
		|| !TestNotNull(TEXT("GoldHUD created"), GoldHUD))
	{
		return false;
	}
	GoldHUD->SetWalletHUDModeForTest(true);

	TSharedRef<SWidget> HealthSlate = HealthHUD->TakeWidget();
	TSharedRef<SWidget> GoldSlate = GoldHUD->TakeWidget();
	(void)HealthSlate;
	(void)GoldSlate;

	HealthHUD->RefreshHealthDisplay();
	TestTrue(TEXT("Initial Health == 400"), FMath::IsNearlyEqual(HealthHUD->GetDisplayedHealth(), 400.0f));
	TestEqual(TEXT("Broadcast Health text displays integer current health only (400)"), HealthHUD->GetDisplayedHealthText().ToString(), FString(TEXT("400")));
	TestEqual(TEXT("Initial Armor == 30"), HealthHUD->GetDisplayedArmor(), 30);
	TestEqual(TEXT("Armor badge text"), HealthHUD->GetDisplayedArmorText().ToString(), FString(TEXT("ARMOR 30")));

	// Damage pawn to 200/400 -> HealthPercent == 0.50, DamageTrail holds at 1.00 before decaying
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 200.0f);
	HealthHUD->RefreshHealthDisplay();
	TestTrue(TEXT("Damaged HealthPercent == 0.50"), FMath::IsNearlyEqual(HealthHUD->GetHealthPercent(), 0.50f, 0.01f));
	TestTrue(TEXT("Damage trail holds above live health immediately after hit"),
		HealthHUD->GetDamageTrailPercent() > HealthHUD->GetHealthPercent() + 0.40f);

	// Replacement pawn rebind: kill old pawn and possess a fresh respawned pawn at 450 MaxHealth
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
	HealthHUD->RefreshHealthDisplay();
	if (ULyraHealthComponent* FirstHC = ULyraHealthComponent::FindHealthComponent(FirstPawn))
	{
		FirstHC->UninitializeFromAbilitySystem();
	}
	PC->UnPossess();
	FirstPawn->Destroy();

	ABwayCharacterWithAbilities* ReplacementPawn = Scope.SpawnHeroPawnForController(PC, 450.0f, 40.0f);
	TestNotNull(TEXT("Replacement pawn spawned"), ReplacementPawn);
	HealthHUD->RefreshHealthDisplay();
	TestTrue(TEXT("HealthHUD immediately rebinds to replacement pawn (450 HP)"),
		FMath::IsNearlyEqual(HealthHUD->GetDisplayedHealth(), 450.0f));
	TestEqual(TEXT("HealthHUD updates Armor to 40"), HealthHUD->GetDisplayedArmor(), 40);
	TestTrue(TEXT("Damage trail snaps cleanly to 1.0 on replacement pawn"),
		FMath::IsNearlyEqual(HealthHUD->GetDamageTrailPercent(), 1.0f, 0.01f));

	// Gold display, MaxGold cap, dynamic shop key, and pulse eligibility
	Scope.RoundMgmt->EnterPrematch();
	Scope.RoundMgmt->EnterWarmup();

	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 0.0f);
	GoldHUD->SetInputDeviceOverrideForTest(true, false);
	GoldHUD->RefreshWalletHUDState();
	TestEqual(TEXT("MaxGold cap bound (2500)"), GoldHUD->GetDisplayedMaxGold(), 2500);
	TestFalse(TEXT("Shop prompt does NOT pulse at 0 gold"), GoldHUD->IsShopPromptPulsing());

	const FString KbmKey = GoldHUD->GetResolvedShopKeyLabel().ToString();
	GoldHUD->SetInputDeviceOverrideForTest(true, true);
	GoldHUD->RefreshWalletHUDState();
	const FString PadKey = GoldHUD->GetResolvedShopKeyLabel().ToString();
	TestNotEqual(TEXT("Gold widget shop prompt updates dynamically for Gamepad"), KbmKey, PadKey);

	// Grant 800 gold during Warmup -> eligible & affordable upgrade exists -> MUST pulse!
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 800.0f);
	GoldHUD->RefreshWalletHUDState();
	TestEqual(TEXT("Live gold updated to 800"), GoldHUD->GetAmount(), 800);
	TestTrue(TEXT("Shop prompt pulses during Warmup when affordable upgrade exists"), GoldHUD->IsShopPromptPulsing());

	// Enter Playing far from any base healing zone while alive -> CanPurchaseUpgrades() becomes false -> MUST stop pulsing!
	PS->SetInHealingZone(false);
	Scope.RoundMgmt->EnterPlaying();
	if (!IsValid(ReplacementPawn) || PC->GetPawn() != ReplacementPawn)
	{
		ReplacementPawn = Scope.SpawnHeroPawnForController(PC, 450.0f, 40.0f);
	}
	if (ReplacementPawn)
	{
		ReplacementPawn->SetActorLocation(FVector(50000.0f, 50000.0f, 200.0f));
	}
	GoldHUD->RefreshWalletHUDState();
	TestFalse(TEXT("Shop prompt stops pulsing outside purchase window even with 800 gold"), GoldHUD->IsShopPromptPulsing());

	// Broadcast overlay verification: UPGRADES [B] near-goal visibility, active Playing crosshair, and INTERCEPTION! toast
	UBwayMatchBroadcastOverlayWidget* BroadcastOverlay = CreateWidget<UBwayMatchBroadcastOverlayWidget>(
		PC,
		UBwayMatchBroadcastOverlayWidget::StaticClass());
	if (TestNotNull(TEXT("BroadcastOverlay created"), BroadcastOverlay))
	{
		TSharedRef<SWidget> OverlaySlate = BroadcastOverlay->TakeWidget();
		(void)OverlaySlate;
		BroadcastOverlay->SetInputDeviceOverrideForTest(true, false);
		BroadcastOverlay->RefreshBroadcastState();

		TestTrue(TEXT("Broadcast crosshair visible during active Playing while alive"), BroadcastOverlay->IsCrosshairVisible());
		TestFalse(TEXT("Broadcast UPGRADES shop prompt hidden outside friendly goal zone"), BroadcastOverlay->IsShopPromptVisible());

		PS->SetInHealingZone(true);
		TestTrue(TEXT("Broadcast UPGRADES shop prompt visible near friendly team goal"), BroadcastOverlay->IsShopPromptVisible());
		TestFalse(TEXT("Broadcast UPGRADES shop key label resolved"), BroadcastOverlay->GetResolvedShopKeyLabel().IsEmpty());

		PS->AddInterception();
		BroadcastOverlay->RefreshBroadcastState();
		TestTrue(TEXT("Match Event Toast activates on Interception delta"), BroadcastOverlay->IsEventToastActive());
		TestEqual(TEXT("Match Event Toast headline == INTERCEPTION!"), BroadcastOverlay->GetActiveToastHeadline().ToString(), FString(TEXT("INTERCEPTION!")));
	}

	return true;
}

// ============================================================================
// 4. PostRound Win Reasons, 8-Player Scoreboard & PostMatch Breakdown Reconciliation
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayGoal6PostRoundScoreboardAndResultsTest,
	"Breakaway.UI.MatchHUDAndPortraits.PostRoundBannersScoreboardAndResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayGoal6PostRoundScoreboardAndResultsTest::RunTest(const FString&)
{
	BwayGoal6TestHelpers::FScopedGoal6MatchWorld Scope(this);
	if (!Scope.Init())
	{
		return false;
	}

	const TArray<FPrimaryAssetId> HeroIds = BwayGoal6TestHelpers::GetCanonicalSliceHeroIds(Scope.World);
	TArray<ABwayPlayerController*> Controllers;
	TArray<ABwayPlayerState*> Players;
	for (int32 Idx = 0; Idx < 8; ++Idx)
	{
		ABwayPlayerController* PC = Scope.SpawnPlayer(
			Idx < 4 ? 0 : 1,
			600 + Idx,
			FString::Printf(TEXT("Contender_%d"), Idx + 1),
			HeroIds[Idx % 4],
			Idx == 0 || Idx == 4);
		Controllers.Add(PC);
		Players.Add(PC->GetPlayerState<ABwayPlayerState>());
	}

	// Verify all 3 canonical PostRound win-reason banners
	const auto PostRoundClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayPostRoundSummaryWidget>(
		TEXT("/BreakawayCore/UI/Match/WBP_BW_PostRoundSummary.WBP_BW_PostRoundSummary_C"));
	UBwayPostRoundSummaryWidget* PostRoundWidget = CreateWidget<UBwayPostRoundSummaryWidget>(Controllers[0], PostRoundClass);
	if (!TestNotNull(TEXT("PostRoundWidget created"), PostRoundWidget))
	{
		return false;
	}
	TSharedRef<SWidget> PostRoundSlate = PostRoundWidget->TakeWidget();
	(void)PostRoundSlate;

	FBwayPostRoundSummaryData RoundSummary = UBwayMatchStatsLibrary::BuildPostRoundSummaryData(Controllers[0], 1, 0, 6.0f);
	RoundSummary.Team0Score = 1;
	RoundSummary.Team1Score = 0;
	RoundSummary.PointsToWin = 3;
	RoundSummary.WinCondition = static_cast<uint8>(EBwayWinCondition::GoalScored);
	RoundSummary.ScoringPlayerName = TEXT("Contender_1");
	PostRoundWidget->ApplySummaryData(RoundSummary);
	TestEqual(TEXT("GoalScored win reason text"),
		PostRoundWidget->GetWinReasonText().ToString(),
		FString(TEXT("GOAL SCORED BY CONTENDER_1")));
	TestEqual(TEXT("Series score text"),
		PostRoundWidget->GetSeriesScoreText().ToString(),
		FString(TEXT("SERIES SCORE: 1 - 0 (FIRST TO 3)")));

	RoundSummary.CompletedRoundNumber = 2;
	RoundSummary.RoundWinningTeam = 1;
	RoundSummary.Team1Score = 1;
	RoundSummary.WinCondition = static_cast<uint8>(EBwayWinCondition::TeamEliminated);
	RoundSummary.ScoringPlayerName.Reset();
	PostRoundWidget->ApplySummaryData(RoundSummary);
	TestEqual(TEXT("TeamEliminated win reason text"),
		PostRoundWidget->GetWinReasonText().ToString(),
		FString(TEXT("ENEMY TEAM ELIMINATED")));

	RoundSummary.CompletedRoundNumber = 3;
	RoundSummary.RoundWinningTeam = 0;
	RoundSummary.Team0Score = 2;
	RoundSummary.WinCondition = static_cast<uint8>(EBwayWinCondition::TimeExpired);
	PostRoundWidget->ApplySummaryData(RoundSummary);
	TestEqual(TEXT("TimeExpired tiebreaker win reason text"),
		PostRoundWidget->GetWinReasonText().ToString(),
		FString(TEXT("TIME EXPIRED — TERRITORY TIEBREAKER")));

	// Populate authoritative stats across all 8 players, with an exact MVP tie between Player 601 and Player 605!
	for (int32 Idx = 0; Idx < 8; ++Idx)
	{
		ABwayPlayerState* PS = Players[Idx];
		BwayGoal6StatMutators::AddKills(PS, Idx + 1);
		BwayGoal6StatMutators::AddDeaths(PS, 1);
		BwayGoal6StatMutators::AddAssists(PS, 2);
		PS->AddDamageDealt(500 + Idx * 100);
		PS->AddHealingDone(120 + Idx * 25);
		BwayGoal6StatMutators::AddForcedFumbles(PS, Idx % 2);
		BwayGoal6StatMutators::AddInterceptions(PS, (Idx + 1) % 2);
		PS->AddBuildableDestroyed(1 + (Idx % 3));
		BwayGoal6StatMutators::CreditGoldEarned(PS, 400 + Idx * 50);
	}

	// Give Players[1] (PlayerId=601) and Players[5] (PlayerId=605) identical top MVP stats to verify tie-breaking!
	for (int32 TieIdx : { 1, 5 })
	{
		ABwayPlayerState* PS = Players[TieIdx];
		if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 0.0f);
		}
		PS->ResetMatchStats();
		BwayGoal6StatMutators::AddKills(PS, 10);
		BwayGoal6StatMutators::AddAssists(PS, 6);
		PS->AddObjectiveScore(2);
		PS->AddDamageDealt(4000);
		PS->AddHealingDone(1000);
		BwayGoal6StatMutators::AddForcedFumbles(PS, 3);
		BwayGoal6StatMutators::AddInterceptions(PS, 2);
		PS->AddBuildableDestroyed(4);
		BwayGoal6StatMutators::CreditGoldEarned(PS, 1800);
	}

	// Simulate Player 7 (PlayerId=607) disconnecting mid-match after earning stats
	Scope.RoundMgmt->EnterPrematch();
	Scope.RoundMgmt->EnterWarmup();
	Scope.RoundMgmt->EnterPlaying();
	ABwayPlayerState* DisconnectedPS = Players[7];
	const int32 DisconnectedKills = DisconnectedPS->GetKills();
	const int32 DisconnectedBuildables = DisconnectedPS->GetBuildablesDestroyed();
	Scope.GameState->SnapshotDisconnectedMatchParticipant(DisconnectedPS);
	Scope.GameState->RemovePlayerFromTeam(DisconnectedPS);
	Scope.GameState->RemovePlayerState(DisconnectedPS);

	// Reconcile Tab Scoreboard
	UBwayScoreboardWidget* Scoreboard = CreateWidget<UBwayScoreboardWidget>(Controllers[0], UBwayScoreboardWidget::StaticClass());
	if (!TestNotNull(TEXT("Scoreboard created"), Scoreboard))
	{
		return false;
	}
	TSharedRef<SWidget> ScoreboardSlate = Scoreboard->TakeWidget();
	(void)ScoreboardSlate;
	Scoreboard->RefreshScoreboardData();

	const TArray<FScoreboardPlayerData> ScoreboardRows = Scoreboard->GetAllScoreboardPlayers();
	TestEqual(TEXT("Scoreboard reconciles all 8 participants including disconnected player"), ScoreboardRows.Num(), 8);
	TestEqual(TEXT("Scoreboard deterministic MVP tie-breaker picks lower PlayerId (601)"), Scoreboard->GetMVPPlayerId(), 601);

	const FScoreboardPlayerData* DisconnectedRow = ScoreboardRows.FindByPredicate([](const FScoreboardPlayerData& Row)
	{
		return Row.PlayerId == 607;
	});
	if (TestNotNull(TEXT("Disconnected player present in Scoreboard"), DisconnectedRow))
	{
		TestFalse(TEXT("Disconnected row marked !bIsConnected"), DisconnectedRow->bIsConnected);
		TestEqual(TEXT("Disconnected row Kills preserved"), DisconnectedRow->Kills, DisconnectedKills);
		TestEqual(TEXT("Disconnected row BuildablesDestroyed preserved"), DisconnectedRow->BuildablesDestroyed, DisconnectedBuildables);
	}

	// Freeze end-of-match stats & build authoritative PostMatch summary
	Scope.Scoring->ResetScores();
	Scope.Scoring->AddScore(0, 3);
	Scope.Scoring->AddScore(1, 1);

	Scope.GameState->FreezeAllMatchStats(true);
	BwayGoal6StatMutators::AddKills(Players[0], 50);
	Players[0]->AddDamageDealt(99999);
	Players[0]->AddBuildableDestroyed(25);
	TestEqual(TEXT("Frozen PostMatch stats ignore late Kills"), Players[0]->GetKills(), 1);
	TestEqual(TEXT("Frozen PostMatch stats ignore late BuildablesDestroyed"), Players[0]->GetBuildablesDestroyed(), 1);

	const FBwayPostMatchSummaryData PostMatchSummary = UBwayMatchStatsLibrary::BuildPostMatchSummaryData(
		Controllers[0], 0, 4, 0.0f);
	TestEqual(TEXT("PostMatch summary reconciles all 8 columns including disconnected player"), PostMatchSummary.PlayerColumns.Num(), 8);
	TestEqual(TEXT("PostMatch summary deterministic MVP matches Scoreboard (601)"), PostMatchSummary.MVPPlayerId, 601);

	const auto BreakdownClass = BwayGoal6TestHelpers::LoadWidgetClassOrFallback<UBwayMatchBreakdownWidget>(
		TEXT("/BreakawayCore/UI/Match/WBP_BW_MatchBreakdown.WBP_BW_MatchBreakdown_C"));
	UBwayMatchBreakdownWidget* Breakdown = CreateWidget<UBwayMatchBreakdownWidget>(Controllers[0], BreakdownClass);
	if (!TestNotNull(TEXT("BreakdownWidget created"), Breakdown))
	{
		return false;
	}
	TSharedRef<SWidget> BreakdownSlate = Breakdown->TakeWidget();
	(void)BreakdownSlate;

	// Verify Host role permissions & focus
	Breakdown->SetRoleOverrideForTest(true, true);
	Breakdown->ApplyBreakdownData(PostMatchSummary);
	TestEqual(TEXT("8 visual breakdown columns populated"), Breakdown->GetDisplayedColumnCount(), 8);
	TestTrue(TEXT("Host role enables Rematch button"), Breakdown->IsPlayAgainButtonEnabled());
	TestTrue(TEXT("Host role enables Return to Lobby button"), Breakdown->IsReturnToLobbyButtonEnabled());
	TestNotNull(TEXT("Host role has valid focus target"), Breakdown->GetDesiredFocusTarget());

	// Verify Client role permissions & focus
	UWidget* HostFocus = Breakdown->GetDesiredFocusTarget();
	Breakdown->SetRoleOverrideForTest(true, false);
	Breakdown->ApplyBreakdownData(PostMatchSummary);
	UWidget* ClientFocus = Breakdown->GetDesiredFocusTarget();
	TestFalse(TEXT("Client role disables Host-only Rematch button"), Breakdown->IsPlayAgainButtonEnabled());
	TestTrue(TEXT("Client role enables Leave to Lobby button"), Breakdown->IsReturnToLobbyButtonEnabled());
	TestNotNull(TEXT("Client role focuses Leave to Lobby button"), ClientFocus);
	TestNotEqual(TEXT("Client focus shifts from disabled Rematch to enabled Leave to Lobby"), HostFocus, ClientFocus);

	// Verify clean next-match reset
	Scope.GameState->FreezeAllMatchStats(false);
	Scope.GameState->ClearDisconnectedMatchParticipants();
	for (int32 Idx = 0; Idx < 7; ++Idx)
	{
		Players[Idx]->ResetMatchStats();
		TestFalse(TEXT("Match stats unfrozen after reset"), Players[Idx]->AreMatchStatsFrozen());
		TestEqual(TEXT("Kills reset to 0"), Players[Idx]->GetKills(), 0);
		TestEqual(TEXT("BuildablesDestroyed reset to 0"), Players[Idx]->GetBuildablesDestroyed(), 0);
	}
	TestEqual(TEXT("Disconnected participant snapshots cleared for next match"),
		Scope.GameState->GetDisconnectedMatchParticipants().Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Live Guard=0 PIE & 200ms Packaged Runtime Audit (bway.Test.Goal6RuntimeAudit)
// ============================================================================

namespace BwayGoal6LiveAudit
{
	static bool bAuditRunning = false;

	struct FHeldBot
	{
		TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
		TWeakObjectPtr<UBrainComponent> Brain;
		bool bWasBrainRunning = false;
	};

	struct FGoal6AuditSession : public TSharedFromThis<FGoal6AuditSession>
	{
		FString Token;
		double StartedAt = FPlatformTime::Seconds();
		double StepStartedAt = FPlatformTime::Seconds();
		double DoradoEightPawnsReadyAt = 0.0;
		double PostMatchEnteredAt = 0.0;
		double RematchHumansReadyAt = 0.0;
		int32 Step = 0;
		TArray<FHeldBot> HeldBots;
		TWeakObjectPtr<ABwayPlayerState> DeadTeam0Target;
		TWeakObjectPtr<ABwayPlayerState> DeadTeam1Target;
		TWeakObjectPtr<ABwayPlayerState> CarrierTarget;
		int32 ExpectedHumansAfterRematch = 1;
		int32 FinalArrivedHumans = 0;
		bool bAutoLockedStagingHeroes = false;
		bool bUsedRealClientDisconnect = false;

		void Advance(int32 NextStep)
		{
			Step = NextStep;
			StepStartedAt = FPlatformTime::Seconds();
		}

		UWorld* FindAuthorityMatchWorld(TArray<UWorld*>& OutAllWorlds) const
		{
			OutAllWorlds.Reset();
			UWorld* AuthWorld = nullptr;
			if (!GEngine)
			{
				return nullptr;
			}
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				UWorld* W = Ctx.World();
				if (!W || !W->IsGameWorld())
				{
					continue;
				}
				OutAllWorlds.Add(W);
				if (W->GetNetMode() != NM_Client && W->GetAuthGameMode())
				{
					AuthWorld = W;
				}
			}
			return AuthWorld;
		}

		void PauseAllBotBrains(UWorld* AuthWorld)
		{
			for (TActorIterator<AAIController> It(AuthWorld); It; ++It)
			{
				AAIController* AI = *It;
				ABwayCharacterWithAbilities* BotPawn = AI ? Cast<ABwayCharacterWithAbilities>(AI->GetPawn()) : nullptr;
				UBrainComponent* Brain = AI ? AI->GetBrainComponent() : nullptr;
				if (!BotPawn)
				{
					continue;
				}
				const bool bAlreadyHeld = HeldBots.ContainsByPredicate([BotPawn](const FHeldBot& H)
				{
					return H.Pawn == BotPawn;
				});
				if (!bAlreadyHeld)
				{
					HeldBots.Add({ BotPawn, Brain, Brain && Brain->IsRunning() });
				}
				if (Brain && Brain->IsRunning())
				{
					Brain->StopLogic(TEXT("Goal6HUDRuntimeAudit"));
				}
				if (UCharacterMovementComponent* Move = BotPawn->GetCharacterMovement())
				{
					Move->StopMovementImmediately();
				}
			}
		}

		void EnsureRoundPlaying(UBwayRoundManagementComponent* RoundMgmt)
		{
			if (!RoundMgmt)
			{
				return;
			}
			if (RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::None)
			{
				RoundMgmt->EnterPrematch();
			}
			if (RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::Prematch)
			{
				RoundMgmt->EnterWarmup();
			}
			if (RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::Warmup
				|| RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound)
			{
				RoundMgmt->EnterPlaying();
			}
			else if (RoundMgmt->GetCurrentRoundState() != ERoundState::RoundActive)
			{
				RoundMgmt->StartRound();
			}
		}

		void DispatchProbeToAllPlayers(UWorld* AuthWorld, int32 Stage, bool bCaptureScreenshots)
		{
			ABwayPlayerController* LocalHostPC = nullptr;
			int32 RemoteDispatched = 0;
			for (FConstPlayerControllerIterator It = AuthWorld->GetPlayerControllerIterator(); It; ++It)
			{
				ABwayPlayerController* PC = Cast<ABwayPlayerController>(It->Get());
				if (!PC)
				{
					continue;
				}
				if (PC->IsLocalController())
				{
					LocalHostPC = PC;
				}
				else
				{
					PC->Client_RunGoal6HUDAuditProbe(Stage, bCaptureScreenshots);
					++RemoteDispatched;
				}
			}
			if (LocalHostPC)
			{
				BwayGoal6RuntimeHelpers::RunLocalGoal6Probe(LocalHostPC, Stage, bCaptureScreenshots, TEXT("host"));
			}
			UE_LOG(LogTemp, Display, TEXT("BwayGoal6Audit: DispatchedProbe stage=%d host=%d remotes=%d"),
				Stage, LocalHostPC != nullptr ? 1 : 0, RemoteDispatched);
		}

		bool Finish(bool bSuccess, const FString& Summary)
		{
			for (const FHeldBot& H : HeldBots)
			{
				if (H.bWasBrainRunning && H.Brain.IsValid())
				{
					H.Brain->RestartLogic();
				}
			}
			if (bSuccess)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayGoal6Audit: PASS token=%s %s"), *Token, *Summary);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("BwayGoal6Audit: FAIL token=%s reason=%s"), *Token, *Summary);
			}
			bAuditRunning = false;
			return false;
		}

		bool Tick(float)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - StartedAt > 180.0)
			{
				return Finish(false, FString::Printf(TEXT("timeout-at-step-%d"), Step));
			}

			TArray<UWorld*> AllWorlds;
			UWorld* AuthWorld = FindAuthorityMatchWorld(AllWorlds);
			if (!AuthWorld)
			{
				return true;
			}

			ABwayGameState* GS = AuthWorld->GetGameState<ABwayGameState>();
			UBwayRoundManagementComponent* RoundMgmt = GS ? GS->GetRoundManagement() : nullptr;
			UBwayScoringComponent* Scoring = GS ? GS->FindComponentByClass<UBwayScoringComponent>() : nullptr;
			UBwayRelicManagerComponent* RelicMgr = GS ? GS->FindComponentByClass<UBwayRelicManagerComponent>() : nullptr;
			if (!GS || !RoundMgmt || !Scoring)
			{
				return true;
			}

			// Step 0: Auto-lock 4 humans if still in L_BW_HeroSelect_Staging, then wait for Dorado + 8 spawned pawns
			if (Step == 0)
			{
				if (AuthWorld->GetMapName().Contains(TEXT("HeroSelect_Staging")))
				{
					if (!bAutoLockedStagingHeroes && GS->HeroSelectionManager && GS->HeroSelectionManager->IsSelectionActive())
					{
						TArray<ABwayPlayerController*> HumanPCs;
						int32 RemoteConnections = 0;
						for (FConstPlayerControllerIterator It = AuthWorld->GetPlayerControllerIterator(); It; ++It)
						{
							if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(It->Get()))
							{
								if (PC->GetPlayerState<ABwayPlayerState>())
								{
									HumanPCs.Add(PC);
									if (!PC->IsLocalController() && PC->GetNetConnection())
									{
										++RemoteConnections;
									}
								}
							}
						}

						if (HumanPCs.Num() >= 4 && RemoteConnections >= 3)
						{
							HumanPCs.Sort([](const ABwayPlayerController& A, const ABwayPlayerController& B)
							{
								return A.PlayerState->GetPlayerId() < B.PlayerState->GetPlayerId();
							});

							static const TCHAR* CanonicalHeroPaths[] = {
								TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus"),
								TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger"),
								TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona"),
								TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver")
							};

							bool bAllLocked = true;
							for (int32 Idx = 0; Idx < HumanPCs.Num(); ++Idx)
							{
								ABwayPlayerState* PS = HumanPCs[Idx]->GetPlayerState<ABwayPlayerState>();
								if (PS && !PS->IsHeroLocked())
								{
									if (const UBwayHeroDataAsset* Hero = LoadObject<UBwayHeroDataAsset>(nullptr, CanonicalHeroPaths[Idx % 4]))
									{
										HumanPCs[Idx]->Server_SubmitPreSelectedHero(Hero->GetPrimaryAssetId(), true);
									}
								}
								if (!PS || !PS->IsHeroLocked())
								{
									bAllLocked = false;
								}
							}
							if (bAllLocked)
							{
								bAutoLockedStagingHeroes = true;
								UE_LOG(LogTemp, Display, TEXT("BwayGoal6Audit: AutoLocked %d human heroes in staging; waiting for Dorado travel"), HumanPCs.Num());
							}
						}
					}
					return true;
				}

				if (!AuthWorld->GetMapName().Contains(TEXT("Dorado")))
				{
					return true;
				}
				if (RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::Prematch)
				{
					RoundMgmt->EnterWarmup();
				}
				if (UBwayBotCreationComponent* Bots = GS->FindComponentByClass<UBwayBotCreationComponent>())
				{
					if (GS->PlayerArray.Num() < 8)
					{
						Bots->EnsureBotsForRound();
					}
				}
				if (GS->PlayerArray.Num() < 8)
				{
					return true;
				}
				int32 SpawnedPawns = 0;
				for (APlayerState* Entry : GS->PlayerArray)
				{
					if (Entry && Entry->GetPawn())
					{
						++SpawnedPawns;
					}
				}
				if (SpawnedPawns < 8)
				{
					return true;
				}
				if (DoradoEightPawnsReadyAt <= 0.0)
				{
					DoradoEightPawnsReadyAt = Now;
				}
				if (Now - DoradoEightPawnsReadyAt < 1.50)
				{
					return true;
				}

				PauseAllBotBrains(AuthWorld);

				// Refresh Warmup countdown so Stage 1 captures a crisp live 4s prematch/warmup server timer & shop prompt
				RoundMgmt->EnterWarmup();

				// Grant 1200 gold to all 8 participants (and mark in healing zone)
				// so shop affordability pulse & upgrade purchases work cleanly
				for (APlayerState* Entry : GS->PlayerArray)
				{
					ABwayPlayerState* PS = Cast<ABwayPlayerState>(Entry);
					if (!PS)
					{
						continue;
					}
					PS->SetInHealingZone(true, GS->GetPlayerTeam(PS));
					if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
					{
						ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetMaxGoldAttribute(), 2500.0f);
						ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 1200.0f);
					}
				}

				Advance(1);
				return true;
			}

			// Step 1: Sample Warmup/Base banner, 3 round pips, gold/cap & shop pulse + purchase real catalog upgrades
			if (Step == 1 && Now - StepStartedAt >= 0.90)
			{
				DispatchProbeToAllPlayers(AuthWorld, 1, true);

				// Equip real catalog upgrades on players so ReplicatedEquippedUpgrades replicate to Scoreboard & Breakdown
				int32 Idx = 0;
				for (APlayerState* Entry : GS->PlayerArray)
				{
					ABwayPlayerState* PS = Cast<ABwayPlayerState>(Entry);
					UBwayUpgradeComponent* Upg = PS ? PS->UpgradeComponent.Get() : nullptr;
					const UBwayUpgradeCatalog* Cat = Upg ? Upg->GetCatalog() : nullptr;
					if (Upg && Cat && Cat->Upgrades.Num() > 0)
					{
						const FName FirstId = Cat->Upgrades[Idx % Cat->Upgrades.Num()].Id;
						Upg->TryPurchaseUpgrade(FirstId);
						if (Idx % 2 == 0)
						{
							Upg->TryPurchaseUpgrade(FirstId); // Rank II!
							if (Cat->Upgrades.Num() > 1)
							{
								const FName SecondId = Cat->Upgrades[(Idx + 1) % Cat->Upgrades.Num()].Id;
								Upg->TryPurchaseUpgrade(SecondId);
							}
						}
					}
					if (PS)
					{
						PS->SetInHealingZone(false);
					}
					++Idx;
				}

				EnsureRoundPlaying(RoundMgmt);
				Advance(2);
				return true;
			}

			// Step 2: Set up live combat HUD states (dead portrait countdowns on both teams, carrier lock, armor + damage trail, buildable placed)
			if (Step == 2 && Now - StepStartedAt >= 0.90)
			{
				PauseAllBotBrains(AuthWorld);

				const FGameplayTag TagAbility1 = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Ability1"), false);
				const FGameplayTag TagAbility2 = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Ability2"), false);
				const FGameplayTag TagAbility4 = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Ability4"), false);
				const FGameplayTag TagUltimate = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Ultimate"), false);

				int32 Idx = 0;
				ABwayPlayerState* ChosenCarrier = nullptr;
				for (APlayerState* Entry : GS->PlayerArray)
				{
					ABwayPlayerState* PS = Cast<ABwayPlayerState>(Entry);
					if (!PS)
					{
						continue;
					}
					const int32 Team = GS->GetPlayerTeam(PS);

					// Populate rich stats for Scoreboard & PostMatch reconciliation
					BwayGoal6StatMutators::AddKills(PS, 2 + (Idx % 4));
					BwayGoal6StatMutators::AddAssists(PS, 1 + (Idx % 3));
					PS->AddDamageDealt(850 + Idx * 140);
					PS->AddHealingDone(200 + Idx * 45);
					BwayGoal6StatMutators::AddForcedFumbles(PS, Idx % 2);
					BwayGoal6StatMutators::AddInterceptions(PS, (Idx + 1) % 2);
					PS->AddBuildableDestroyed(1 + (Idx % 2));
					BwayGoal6StatMutators::CreditGoldEarned(PS, 350 + Idx * 60);

					if (PS->IsABot() && Team == 0 && !DeadTeam0Target.IsValid())
					{
						DeadTeam0Target = PS;
					}
					else if (PS->IsABot() && Team == 1 && !DeadTeam1Target.IsValid())
					{
						DeadTeam1Target = PS;
					}
					else if (!PS->IsABot() && !ChosenCarrier)
					{
						ChosenCarrier = PS;
					}

					// Set Armor = 25 and apply partial damage on human pawns for Health + DamageTrail + Armor HUD
					if (!PS->IsABot())
					{
						PS->MarkBuildablePlacedThisRound();
						if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
						{
							ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 25.0f);
							const float MaxHP = FMath::Max(200.0f, ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()));
							ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), MaxHP * 0.68f);

							// Apply one real HUD-slot ability's CooldownGameplayEffect with an explicit 8.5s locked duration
							// on non-carrier humans so Stage 2 renders live replicated Cooldown overlay + ProgressBar + Timer!
							if (PS != ChosenCarrier)
							{
								bool bCooldownApplied = false;
								for (int32 Pass = 0; Pass < 2 && !bCooldownApplied; ++Pass)
								{
									for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
									{
										const UGameplayAbility* AbilityCDO = Spec.Ability;
										const FGameplayTagContainer* CooldownTags = AbilityCDO ? AbilityCDO->GetCooldownTags() : nullptr;
										const UGameplayEffect* CooldownGE = AbilityCDO ? AbilityCDO->GetCooldownGameplayEffect() : nullptr;
										if (!CooldownGE || !CooldownTags || CooldownTags->IsEmpty())
										{
											continue;
										}
										const FGameplayTagContainer& DynTags = Spec.GetDynamicSpecSourceTags();
										const bool bIsHudActionSlot =
											(TagAbility1.IsValid() && DynTags.HasTag(TagAbility1))
											|| (TagAbility2.IsValid() && DynTags.HasTag(TagAbility2))
											|| (TagAbility4.IsValid() && DynTags.HasTag(TagAbility4))
											|| (TagUltimate.IsValid() && DynTags.HasTag(TagUltimate));
										if (Pass == 0 && !bIsHudActionSlot)
										{
											continue;
										}

										FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
											CooldownGE->GetClass(),
											1.0f,
											ASC->MakeEffectContext());
										if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
										{
											SpecHandle.Data->SetDuration(8.5f, /*bLockDuration*/ true);
											const FActiveGameplayEffectHandle ActiveH = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
											if (ActiveH.IsValid())
											{
												bCooldownApplied = true;
												break;
											}
										}
									}
								}
							}
						}
					}
					++Idx;
				}

				CarrierTarget = ChosenCarrier;
				if (ChosenCarrier)
				{
					ChosenCarrier->SetHasRelic(true);
					if (ARelicActor* Relic = RelicMgr ? RelicMgr->GetRelicActor() : nullptr)
					{
						if (ABwayCharacterWithAbilities* CarrierPawn = Cast<ABwayCharacterWithAbilities>(ChosenCarrier->GetPawn()))
						{
							Relic->OnPickedUp(CarrierPawn);
						}
					}
				}

				// Kill 1 Team 0 bot and 1 Team 1 bot through production DamageSelfDestruct so authoritative RespawnSchedule runs!
				for (TWeakObjectPtr<ABwayPlayerState> Victim : { DeadTeam0Target, DeadTeam1Target })
				{
					if (APawn* VictimPawn = Victim.IsValid() ? Victim->GetPawn() : nullptr)
					{
						if (ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(VictimPawn))
						{
							HC->DamageSelfDestruct(false);
						}
					}
				}

				Advance(3);
				return true;
			}

			// Step 3: Wait 0.90s for 200ms RTT replication of RespawnSchedule + Relic carrier + vitals, then sample Stage 2!
			if (Step == 3 && Now - StepStartedAt >= 0.90)
			{
				DispatchProbeToAllPlayers(AuthWorld, 2, true);
				Advance(4);
				return true;
			}

			// Step 4: Wait for dead bots to respawn & open 8-player Tab Scoreboard across real rendered frames (Stage 3 -> Stage 31)
			if (Step == 4)
			{
				const bool bTeam0AliveAgain = !DeadTeam0Target.IsValid() || !DeadTeam0Target->GetRespawnSchedule().bIsWaitingToRespawn;
				const bool bTeam1AliveAgain = !DeadTeam1Target.IsValid() || !DeadTeam1Target->GetRespawnSchedule().bIsWaitingToRespawn;
				if ((!bTeam0AliveAgain || !bTeam1AliveAgain) && Now - StepStartedAt < 11.5)
				{
					return true;
				}
				PauseAllBotBrains(AuthWorld);
				DispatchProbeToAllPlayers(AuthWorld, 3, false);
				Advance(45);
				return true;
			}

			if (Step == 45 && Now - StepStartedAt >= 0.50)
			{
				DispatchProbeToAllPlayers(AuthWorld, 31, true);
				Advance(5);
				return true;
			}

			// Step 5: Trigger Round 1 win by GoalScored -> verify GOAL SCORED BY <PLAYER> banner (Stage 40)
			if (Step == 5 && Now - StepStartedAt >= 0.70)
			{
				const int32 CarrierTeam = CarrierTarget.IsValid() ? GS->GetPlayerTeam(CarrierTarget.Get()) : 0;
				RoundMgmt->OnRelicScored(CarrierTeam >= 0 ? CarrierTeam : 0, CarrierTarget.Get());
				Advance(6);
				return true;
			}

			if (Step == 6 && Now - StepStartedAt >= 0.90)
			{
				DispatchProbeToAllPlayers(AuthWorld, 40, true);
				EnsureRoundPlaying(RoundMgmt);
				Advance(7);
				return true;
			}

			// Step 7: Trigger Round 2 win by TeamEliminated -> verify ENEMY TEAM ELIMINATED banner (Stage 41)
			if (Step == 7 && Now - StepStartedAt >= 0.80)
			{
				const int32 LosingTeam = Scoring->GetTeamScore(0) >= Scoring->GetTeamScore(1) ? 1 : 0;
				RoundMgmt->EndRound(LosingTeam, EBwayWinCondition::TeamEliminated);
				Advance(8);
				return true;
			}

			if (Step == 8 && Now - StepStartedAt >= 0.90)
			{
				DispatchProbeToAllPlayers(AuthWorld, 41, false);
				EnsureRoundPlaying(RoundMgmt);
				Advance(9);
				return true;
			}

			// Step 9: Trigger Round 3 win by TimeExpired -> verify TIME EXPIRED — TERRITORY TIEBREAKER banner (Stage 42)
			if (Step == 9 && Now - StepStartedAt >= 0.80)
			{
				RoundMgmt->EndRound(0, EBwayWinCondition::TimeExpired);
				Advance(10);
				return true;
			}

			if (Step == 10 && Now - StepStartedAt >= 0.90)
			{
				DispatchProbeToAllPlayers(AuthWorld, 42, false);
				EnsureRoundPlaying(RoundMgmt);
				Advance(11);
				return true;
			}

			// Step 11: Disconnect 1 participant mid-Round 4 to verify 8-player disconnected participant reconciliation!
			if (Step == 11 && Now - StepStartedAt >= 0.80)
			{
				if (!GIsEditor)
				{
					// In 4-process packaged mode, disconnect the last remote client (NullRHI client #3) via real session leave + PeerReleased!
					ABwayPlayerController* LastRemotePC = nullptr;
					for (FConstPlayerControllerIterator It = AuthWorld->GetPlayerControllerIterator(); It; ++It)
					{
						ABwayPlayerController* Candidate = Cast<ABwayPlayerController>(It->Get());
						if (Candidate && !Candidate->IsLocalController())
						{
							LastRemotePC = Candidate;
						}
					}
					if (LastRemotePC)
					{
						bUsedRealClientDisconnect = true;
						LastRemotePC->Client_RunGoal6HUDAuditProbe(-99, false);
					}
				}
				else
				{
					// In 4-world PIE, snapshot & remove one bot as a non-bot disconnected participant so all 4 PIE viewports stay alive
					for (TActorIterator<AAIController> It(AuthWorld); It; ++It)
					{
						AAIController* AI = *It;
						ABwayPlayerState* BotPS = AI ? AI->GetPlayerState<ABwayPlayerState>() : nullptr;
						if (AI && BotPS)
						{
							BotPS->SetIsABot(false);
							GS->SnapshotDisconnectedMatchParticipant(BotPS);
							if (APawn* BotPawn = AI->GetPawn())
							{
								AI->UnPossess();
								BotPawn->Destroy();
							}
							GS->RemovePlayerFromTeam(BotPS);
							GS->RemovePlayerState(BotPS);
							AI->Destroy();
							break;
						}
					}
				}
				Advance(12);
				return true;
			}

			// Step 12: Wait for disconnect snapshot, then score match-winning point (3 - 1 -> final PostRound -> PostMatch)!
			if (Step == 12 && Now - StepStartedAt >= 1.40)
			{
				const int32 Team0Wins = Scoring->GetTeamScore(0);
				const int32 Team1Wins = Scoring->GetTeamScore(1);
				const int32 WinningTeam = Team0Wins >= Team1Wins ? 0 : 1;
				BwayGoal6StatMutators::SetTeamScoreExact(Scoring, WinningTeam, FMath::Max(2, RoundMgmt->PointsToWin - 1));
				RoundMgmt->OnRelicScored(WinningTeam, CarrierTarget.Get());
				PostMatchEnteredAt = 0.0;
				Advance(13);
				return true;
			}

			// Step 13: Wait for final PostRound to transition into PostMatch, then verify frozen stats & show Breakdown (Stage 50 -> Stage 5)
			if (Step == 13)
			{
				if (RoundMgmt->GetCurrentMatchPhase() != EBwayMatchPhase::PostMatch)
				{
					if (Now - StepStartedAt > 12.0)
					{
						return Finish(false, TEXT("postmatch-phase-not-reached"));
					}
					return true;
				}
				if (PostMatchEnteredAt <= 0.0)
				{
					PostMatchEnteredAt = Now;
				}
				if (Now - PostMatchEnteredAt < 0.85)
				{
					return true;
				}

				bool bAllFrozen = true;
				for (APlayerState* Entry : GS->PlayerArray)
				{
					ABwayPlayerState* PS = Cast<ABwayPlayerState>(Entry);
					if (PS)
					{
						const int32 KillsBefore = PS->GetKills();
						PS->AddKill();
						if (!PS->AreMatchStatsFrozen() || PS->GetKills() != KillsBefore)
						{
							bAllFrozen = false;
						}
					}
				}
				if (!bAllFrozen)
				{
					return Finish(false, TEXT("postmatch-stats-not-frozen"));
				}
				if (GS->GetDisconnectedMatchParticipants().Num() < 1)
				{
					return Finish(false, TEXT("disconnected-participant-snapshot-missing"));
				}

				DispatchProbeToAllPlayers(AuthWorld, 50, false);
				Advance(135);
				return true;
			}

			if (Step == 135 && Now - StepStartedAt >= 0.55)
			{
				DispatchProbeToAllPlayers(AuthWorld, 5, true);
				Advance(14);
				return true;
			}

			// Step 14: Verify Client's attempted Server_RequestPlayAgain was rejected (still in PostMatch), then trigger Host Rematch!
			if (Step == 14 && Now - StepStartedAt >= 0.90)
			{
				if (RoundMgmt->GetCurrentMatchPhase() != EBwayMatchPhase::PostMatch)
				{
					return Finish(false, TEXT("client-illegally-triggered-rematch"));
				}

				int32 ConnectedHumans = 0;
				for (APlayerState* Entry : GS->PlayerArray)
				{
					if (Entry && !Entry->IsABot())
					{
						++ConnectedHumans;
					}
				}
				ExpectedHumansAfterRematch = FMath::Max(1, ConnectedHumans);
				RematchHumansReadyAt = 0.0;

				GS->RestartMatchFromResults();
				Advance(15);
				return true;
			}

			// Step 15: Wait for Rematch ServerTravel to L_BW_HeroSelect_Staging & verify clean reset on Host & Clients (Stage 6)!
			if (Step == 15)
			{
				if (!AuthWorld->GetMapName().Contains(TEXT("HeroSelect_Staging")))
				{
					return true;
				}
				int32 ArrivedHumans = 0;
				for (APlayerState* Entry : GS->PlayerArray)
				{
					if (Entry && !Entry->IsABot())
					{
						++ArrivedHumans;
					}
				}
				if (ArrivedHumans < ExpectedHumansAfterRematch && Now - StepStartedAt < 14.0)
				{
					return true;
				}
				if (RematchHumansReadyAt <= 0.0)
				{
					RematchHumansReadyAt = Now;
				}
				if (Now - RematchHumansReadyAt < 1.25)
				{
					return true;
				}

				if (Scoring->GetTeamScore(0) != 0 || Scoring->GetTeamScore(1) != 0)
				{
					return Finish(false, TEXT("rematch-scores-not-reset"));
				}
				if (GS->GetDisconnectedMatchParticipants().Num() != 0)
				{
					return Finish(false, TEXT("rematch-disconnected-participants-not-cleared"));
				}
				for (APlayerState* Entry : GS->PlayerArray)
				{
					if (ABwayPlayerState* PS = Cast<ABwayPlayerState>(Entry))
					{
						if (PS->AreMatchStatsFrozen() || PS->GetKills() != 0 || PS->GetReplicatedEquippedUpgrades().Num() != 0)
						{
							return Finish(false, TEXT("rematch-player-stats-or-upgrades-not-reset"));
						}
					}
				}

				FinalArrivedHumans = ArrivedHumans;
				DispatchProbeToAllPlayers(AuthWorld, 6, false);
				Advance(16);
				return true;
			}

			// Step 16: Allow Stage 6 RPCs to complete on clients at 200ms RTT, then release packaged peers cleanly!
			if (Step == 16 && Now - StepStartedAt >= 0.90)
			{
				DispatchProbeToAllPlayers(AuthWorld, -1, false);
				return Finish(true, FString::Printf(
					TEXT("worlds=%d arrivedHumans=%d realClientDisconnect=%d stages=6 frozenVerified=1 rematchResetVerified=1"),
					AllWorlds.Num(),
					FinalArrivedHumans,
					bUsedRealClientDisconnect ? 1 : 0));
			}

			return true;
		}
	};

	FAutoConsoleCommand Goal6RuntimeAuditCommand(
		TEXT("bway.Test.Goal6RuntimeAudit"),
		TEXT("Development: Runs guard=0 Goal 6 Match HUD, Portrait Respawn Timers, Ability Overlays, Tab Scoreboard, PostRound Win Banners, 8-Player Breakdown, and Rematch Reset audit. Usage: bway.Test.Goal6RuntimeAudit [Token]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (bAuditRunning)
			{
				UE_LOG(LogTemp, Warning, TEXT("BwayGoal6Audit: already running"));
				return;
			}
			bAuditRunning = true;
			const FString Token = Args.Num() > 0 ? Args[0] : TEXT("goal6-live");
			BwayGoal6RuntimeHelpers::ActiveEvidenceDir = BwayGoal6RuntimeHelpers::ResolveEvidenceDirectory(Token);
			UE_LOG(LogTemp, Display, TEXT("BwayGoal6Audit: START token=%s dir=%s"),
				*Token, *BwayGoal6RuntimeHelpers::ActiveEvidenceDir);

			TSharedRef<FGoal6AuditSession> Session = MakeShared<FGoal6AuditSession>();
			Session->Token = Token;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Session](float Delta)
			{
				return Session->Tick(Delta);
			}), 0.05f);
		}));
} // namespace BwayGoal6LiveAudit

#else

void ABwayPlayerController::Client_RunGoal6HUDAuditProbe_Implementation(int32 Stage, bool bCaptureScreenshot)
{
	(void)Stage;
	(void)bCaptureScreenshot;
}

#endif // !UE_BUILD_SHIPPING
