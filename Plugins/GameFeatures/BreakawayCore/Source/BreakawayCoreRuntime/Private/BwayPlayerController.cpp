// Copyright Epic Games, Inc. All Rights Reserved.

#include "BwayPlayerController.h"
#include "UI/BwayUpgradeShopWidget.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "UI/BwayResultsScreenWidget.h"
#include "UI/BwayPostRoundSummaryWidget.h"
#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Development/BwayCheatManager.h"
#include "Combat/BwayCombatNumberPopComponent.h"
#include "Combat/BwayNameplatePolicyComponent.h"
#include "GameplayTagContainer.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayPlayerController)

ABwayPlayerController::ABwayPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if USING_CHEAT_MANAGER
	// Override the cheat manager with our Breakaway-specific one
	CheatClass = UBwayCheatManager::StaticClass();
#endif

	CombatNumberPopComponent = CreateDefaultSubobject<UBwayCombatNumberPopComponent>(TEXT("CombatNumberPopComponent"));
	NameplatePolicyComponent = CreateDefaultSubobject<UBwayNameplatePolicyComponent>(TEXT("NameplatePolicyComponent"));
}

void ABwayPlayerController::ShowHeroSelect()
{
#if USING_CHEAT_MANAGER
	if (UBwayCheatManager* BwayCheatMgr = Cast<UBwayCheatManager>(CheatManager))
	{
		BwayCheatMgr->ShowHeroSelect();
	}
#endif
}

void ABwayPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent) InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ThisClass::ToggleUpgradeShop);
}

void ABwayPlayerController::ToggleUpgradeShop()
{
	if (!IsLocalController() || !GetLocalPlayer()) return;
	if (UpgradeShopWidget && UpgradeShopWidget->IsActivated())
	{
		UpgradeShopWidget->DeactivateWidget();
		return;
	}
	const ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>();
	if (!GS || !GS->GetRoundManagement() || (HeroSelectionWidget && HeroSelectionWidget->IsActivated()) || ResultsWidget) return;
	const EBwayMatchPhase Phase = GS->GetRoundManagement()->GetCurrentMatchPhase();
	if (Phase == EBwayMatchPhase::None || Phase == EBwayMatchPhase::Prematch || Phase == EBwayMatchPhase::PostMatch) return;
	UpgradeShopWidget = Cast<UBwayUpgradeShopWidget>(UCommonUIExtensions::PushContentToLayer_ForPlayer(
		GetLocalPlayer(), FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), UBwayUpgradeShopWidget::StaticClass()));
}

void ABwayPlayerController::HideHeroSelect()
{
#if USING_CHEAT_MANAGER
	if (UBwayCheatManager* BwayCheatMgr = Cast<UBwayCheatManager>(CheatManager))
	{
		BwayCheatMgr->HideHeroSelect();
	}
#endif
}

void ABwayPlayerController::ToggleHeroSelect()
{
#if USING_CHEAT_MANAGER
	if (UBwayCheatManager* BwayCheatMgr = Cast<UBwayCheatManager>(CheatManager))
	{
		BwayCheatMgr->ToggleHeroSelect();
	}
#endif
}

void ABwayPlayerController::Client_RequestPreSelectedHero_Implementation()
{
	FPrimaryAssetId PreSelectedHeroId;

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UBwayFrontendStateSubsystem* FrontendState = LocalPlayer->GetSubsystem<UBwayFrontendStateSubsystem>())
		{
			PreSelectedHeroId = FrontendState->GetSelectedHeroId();
		}
	}

	if (PreSelectedHeroId.IsValid())
	{
		const bool bLockSelection = !UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this);
		Server_SubmitPreSelectedHero(PreSelectedHeroId, bLockSelection);
	}
}

void ABwayPlayerController::Server_SubmitPreSelectedHero_Implementation(FPrimaryAssetId PreSelectedHeroId, bool bLockSelection)
{
	if (UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidWorld(this))
	{
		return;
	}

	if (!PreSelectedHeroId.IsValid())
	{
		return;
	}

	if (ABwayPlayerState* BwayPS = GetPlayerState<ABwayPlayerState>())
	{
		if (!BwayPS->IsHeroLocked())
		{
			BwayPS->ServerSetSelectedHeroId(PreSelectedHeroId);

			if (bLockSelection)
			{
				BwayPS->ServerLockHeroSelection();
			}
			else if (ABwayGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
			{
				if (UBwayHeroSelectionManager* SelectionManager = GameState->FindComponentByClass<UBwayHeroSelectionManager>())
				{
					SelectionManager->SynchronizePlayerSelectionState(BwayPS);
				}
			}
		}

		if (bLockSelection && GetPawn())
		{
			if (ABreakawayGameMode* BreakawayGameMode = GetWorld() ? Cast<ABreakawayGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
			{
				BreakawayGameMode->ApplyHeroDataToNewPawn(this);
			}
		}
	}
}

void ABwayPlayerController::Client_ShowHeroSelection_Implementation(const TSoftClassPtr<UCommonActivatableWidget>& WidgetClass)
{
	if (WidgetClass.IsNull())
	{
		return;
	}

	TSubclassOf<UCommonActivatableWidget> LoadedClass = WidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerController: Failed to load hero selection widget"));
		return;
	}

	if (HeroSelectionWidget)
	{
		UCommonUIExtensions::PopContentFromLayer(HeroSelectionWidget);
		HeroSelectionWidget = nullptr;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerController: Cannot show hero selection without a local player"));
		return;
	}

	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	HeroSelectionWidget = UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, MenuLayer, LoadedClass);
	if (!HeroSelectionWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerController: Failed to push hero selection widget to the CommonUI layer"));
		return;
	}
}

void ABwayPlayerController::Client_HideHeroSelection_Implementation()
{
	if (HeroSelectionWidget)
	{
		UCommonUIExtensions::PopContentFromLayer(HeroSelectionWidget);
		HeroSelectionWidget = nullptr;
	}

	RestoreGameplayInputMode();
}

void ABwayPlayerController::RestoreGameplayInputMode()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);
}

void ABwayPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		BindPostRoundSummaryListeners();
	}
}

void ABwayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalPlayerController())
	{
		UnbindPostRoundSummaryListeners();
		DismissPostRoundSummary();
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayPlayerController::BindPostRoundSummaryListeners()
{
	UnbindPostRoundSummaryListeners();

	if (UWorld* World = GetWorld())
	{
		if (ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			if (UBwayRoundManagementComponent* RoundMgmt = GameState->FindComponentByClass<UBwayRoundManagementComponent>())
			{
				BoundRoundManagementComponent = RoundMgmt;
				RoundMgmt->OnPostRoundSummaryStarted.AddDynamic(this, &ABwayPlayerController::HandlePostRoundSummaryStarted);
				RoundMgmt->OnMatchPhaseChanged.AddDynamic(this, &ABwayPlayerController::HandleMatchPhaseChanged);

				if (RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound && RoundMgmt->HasActivePostRoundSummary())
				{
					ShowPostRoundSummary(RoundMgmt->GetActivePostRoundSummary());
				}
			}
		}
	}
}

void ABwayPlayerController::UnbindPostRoundSummaryListeners()
{
	if (UBwayRoundManagementComponent* RoundMgmt = BoundRoundManagementComponent.Get())
	{
		RoundMgmt->OnPostRoundSummaryStarted.RemoveDynamic(this, &ABwayPlayerController::HandlePostRoundSummaryStarted);
		RoundMgmt->OnMatchPhaseChanged.RemoveDynamic(this, &ABwayPlayerController::HandleMatchPhaseChanged);
	}

	BoundRoundManagementComponent.Reset();
}

void ABwayPlayerController::HandlePostRoundSummaryStarted(FBwayPostRoundSummaryData SummaryData)
{
	if (UBwayRoundManagementComponent* RoundMgmt = BoundRoundManagementComponent.Get())
	{
		if (RoundMgmt->GetCurrentMatchPhase() != EBwayMatchPhase::PostRound)
		{
			return;
		}
	}

	ShowPostRoundSummary(SummaryData);
}

void ABwayPlayerController::HandleMatchPhaseChanged(EBwayMatchPhase NewPhase)
{
	if (UpgradeShopWidget && UpgradeShopWidget->IsActivated()) UpgradeShopWidget->DeactivateWidget();
	if (NewPhase != EBwayMatchPhase::PostRound)
	{
		DismissPostRoundSummary();
	}
}

TSubclassOf<UUserWidget> ABwayPlayerController::ResolvePostRoundSummaryWidgetClass() const
{
	if (const ABwayGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
	{
		if (!GameState->PostRoundSummaryWidgetClass.IsNull())
		{
			if (UClass* LoadedClass = GameState->PostRoundSummaryWidgetClass.LoadSynchronous())
			{
				return LoadedClass;
			}
		}
	}

	const TSoftClassPtr<UBwayPostRoundSummaryWidget> DefaultWidgetClass(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Match/WBP_BW_PostRoundSummary.WBP_BW_PostRoundSummary_C")));

	return DefaultWidgetClass.LoadSynchronous();
}

void ABwayPlayerController::ShowPostRoundSummary(const FBwayPostRoundSummaryData& SummaryData)
{
	const TSubclassOf<UUserWidget> WidgetClass = ResolvePostRoundSummaryWidgetClass();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerController: PostRound summary widget class not configured"));
		return;
	}

	if (!PostRoundSummaryWidget)
	{
		PostRoundSummaryWidget = CreateWidget<UBwayPostRoundSummaryWidget>(this, WidgetClass);
		if (!PostRoundSummaryWidget)
		{
			return;
		}

		PostRoundSummaryWidget->AddToViewport(150);
	}

	FBwayPostRoundSummaryData ResolvedSummary = SummaryData;
	if (UBwayRoundManagementComponent* RoundMgmt = BoundRoundManagementComponent.Get())
	{
		if (RoundMgmt->HasActivePostRoundSummary())
		{
			ResolvedSummary = RoundMgmt->GetActivePostRoundSummary();
		}
	}

	ResolvedSummary = UBwayMatchStatsLibrary::BuildPostRoundSummaryData(
		this,
		ResolvedSummary.CompletedRoundNumber,
		ResolvedSummary.RoundWinningTeam,
		ResolvedSummary.DisplayDurationSeconds);

	PostRoundSummaryWidget->ApplySummaryData(ResolvedSummary);

	UE_LOG(LogTemp, Log, TEXT("BwayPlayerController: PostRound summary shown (round %d, winner team %d)"),
		ResolvedSummary.CompletedRoundNumber, ResolvedSummary.RoundWinningTeam + 1);
}

void ABwayPlayerController::DismissPostRoundSummary()
{
	if (PostRoundSummaryWidget)
	{
		PostRoundSummaryWidget->RemoveFromParent();
		PostRoundSummaryWidget = nullptr;
	}
}

void ABwayPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!IsLocalPlayerController() || !InPawn)
	{
		return;
	}

	if (ResultsWidget && ResultsWidget->IsInViewport())
	{
		return;
	}

	RestoreGameplayInputMode();
}

void ABwayPlayerController::Client_ShowResults_Implementation(
	int32 WinningTeam,
	int32 Team1Score,
	int32 Team2Score,
	int32 TotalRounds,
	const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	DismissPostRoundSummary();

	if (WidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerController: Client_ShowResults called with null widget class"));
		return;
	}

	UClass* LoadedClass = WidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerController: Client_ShowResults failed to load widget class"));
		return;
	}

	// Clear any previous results widget before creating a new one.
	if (ResultsWidget)
	{
		ResultsWidget->RemoveFromParent();
		ResultsWidget = nullptr;
	}

	ResultsWidget = CreateWidget<UUserWidget>(this, LoadedClass);
	if (!ResultsWidget)
	{
		return;
	}

	ResultsWidget->AddToViewport(200);

	SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);

	// The collapsed orchestration widget is not an input target. Its visible
	// interstitial and breakdown take focus after their data and controls exist.
	if (UBwayResultsScreenWidget* ResultsScreen = Cast<UBwayResultsScreenWidget>(ResultsWidget))
	{
		ResultsScreen->ApplyAuthoritativeResults(WinningTeam, Team1Score, Team2Score, TotalRounds);
	}
	else if (ResultsWidget->IsFocusable())
	{
		ResultsWidget->SetUserFocus(this);
	}

	UE_LOG(LogTemp, Log, TEXT("BwayPlayerController: Results screen shown — Team %d wins (%d-%d, %d rounds)"),
		WinningTeam + 1, Team1Score, Team2Score, TotalRounds);
}

void ABwayPlayerController::Server_RequestReturnToFrontEnd_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (ABwayGameState* GS = World->GetGameState<ABwayGameState>())
		{
			const UBwayRoundManagementComponent* RoundMgmt = GS->GetRoundManagement();
			if (!RoundMgmt || RoundMgmt->GetCurrentMatchPhase() != EBwayMatchPhase::PostMatch)
			{
				UE_LOG(LogTemp, Warning, TEXT("BwayPlayerController: Ignoring results return request outside PostMatch"));
				return;
			}
			GS->ReturnToFrontEnd();
		}
	}
}

void ABwayPlayerController::Server_RequestPlayAgain_Implementation()
{
	if (ABwayGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
	{
		GS->RestartMatchFromResults();
	}
}

void ABwayPlayerController::Client_DismissResultsScreen_Implementation()
{
	if (ResultsWidget)
	{
		ResultsWidget->RemoveFromParent();
		ResultsWidget = nullptr;
	}

	RestoreGameplayInputMode();
}
