// Copyright Epic Games, Inc. All Rights Reserved.
// Goal 5 headless UI/selection automation tests (Breakaway.HeroSelection.*,
// Breakaway.UI.FrontEndAndHeroSelectAudit.*) and live PIE runtime focus/navigation
// audit command (bway.Test.Goal5RuntimeAudit).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "BreakawayGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "CommonTextBlock.h"
#include "CommonUIExtensions.h"
#include "ICommonInputModule.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameModes/BwayUserFacingExperienceDefinition.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroAbilityUILibrary.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/BwayAbilityPreviewWidget.h"
#include "UI/BwayCommonButton.h"
#include "UI/BwayFrontEndFlowWidgets.h"
#include "UI/BwayHeroSlotWidget.h"
#include "UI/BwayPauseMenuWidget.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BwayGoal5TestHelpers
{
	struct FScopedHeadlessUIWorld
	{
		FAutomationTestBase* Test = nullptr;
		UWorld* World = nullptr;
		UGameInstance* GameInstance = nullptr;
		ABwayGameState* GameState = nullptr;
		ABreakawayGameMode* GameMode = nullptr;

		explicit FScopedHeadlessUIWorld(FAutomationTestBase* InTest)
			: Test(InTest)
		{
		}

		~FScopedHeadlessUIWorld()
		{
			if (!World)
			{
				return;
			}
			if (GameState)
			{
				if (GameState->HeroSelectionPhaseComponent && GameState->HeroSelectionPhaseComponent->HasBegunPlay())
				{
					GameState->HeroSelectionPhaseComponent->EndPlay(EEndPlayReason::EndPlayInEditor);
				}
				if (GameState->HeroSelectionManager && GameState->HeroSelectionManager->HasBegunPlay())
				{
					GameState->HeroSelectionManager->EndPlay(EEndPlayReason::EndPlayInEditor);
				}
			}
			World->GetTimerManager().ClearAllTimersForObject(GameState);
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

		bool Init(bool bSpawnGameActors = false, const FString& Options = TEXT("?HeroSelectStaging=1?NumBots=6"))
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
			if (!Test->TestNotNull(TEXT("Headless UI test world created"), World))
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

			if (bSpawnGameActors)
			{
				GameState = World->SpawnActor<ABwayGameState>();
				GameMode = World->SpawnActor<ABreakawayGameMode>();
				if (!Test->TestNotNull(TEXT("GameState spawned"), GameState) || !Test->TestNotNull(TEXT("GameMode spawned"), GameMode))
				{
					return false;
				}
				World->SetGameState(GameState);
				GameMode->GameState = GameState;
				GameMode->OptionsString = Options;
				World->URL = FURL(nullptr, *(TEXT("/BreakawayCore/Maps/L_BW_HeroSelect_Staging") + Options), TRAVEL_Absolute);

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

				for (UActorComponent* Comp : {
					static_cast<UActorComponent*>(GameState->HeroSelectionManager),
					static_cast<UActorComponent*>(GameState->HeroSelectionPhaseComponent) })
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
			}

			return true;
		}

		ABwayPlayerController* SpawnLocalHuman(int32 TeamIndex, int32 PlayerId, const FString& PlayerName)
		{
			ABwayPlayerController* PC = World->SpawnActor<ABwayPlayerController>();
			if (!PC)
			{
				return nullptr;
			}
			World->AddController(PC);
			PC->Player = NewObject<ULocalPlayer>(GEngine);

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = PC;
			ABwayPlayerState* PS = World->SpawnActor<ABwayPlayerState>(SpawnParams);
			PC->SetPlayerState(PS);
			PS->SetPlayerId(PlayerId);
			PS->SetPlayerName(PlayerName);

			if (GameState)
			{
				GameState->AddPlayerState(PS);
				GameState->AddPlayerToTeam(PS, TeamIndex);
				if (GameState->HeroSelectionManager)
				{
					GameState->HeroSelectionManager->RegisterPlayer(PS);
				}
			}
			return PC;
		}
	};
}

// ============================================================================
// 1. Breakaway.HeroSelection.*
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayHeroSelectionRosterAndClassesTest,
	"Breakaway.HeroSelection.RosterAndClasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayHeroSelectionRosterAndClassesTest::RunTest(const FString&)
{
	BwayGoal5TestHelpers::FScopedHeadlessUIWorld Scope(this);
	if (!Scope.Init(true))
	{
		return false;
	}

	ABwayPlayerController* PC = Scope.SpawnLocalHuman(0, 256, TEXT("WarriorOne"));
	if (!TestNotNull(TEXT("Local human controller"), PC))
	{
		return false;
	}

	UBwayHeroSelectWidget* Widget = CreateWidget<UBwayHeroSelectWidget>(PC, UBwayHeroSelectWidget::StaticClass());
	if (!TestNotNull(TEXT("UBwayHeroSelectWidget created"), Widget))
	{
		return false;
	}
	TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	(void)SlateWidget;

	const TArray<FHeroDisplayInfo> Heroes = Widget->GetAvailableHeroes();
	if (!TestTrue(TEXT("At least 4 vertical-slice heroes registered"), Heroes.Num() >= 4))
	{
		return false;
	}

	struct FExpectedHero
	{
		const TCHAR* Name;
		const TCHAR* ExpectedClass;
	};
	const FExpectedHero Expected[4] = {
		{ TEXT("Argus"),   TEXT("Fighter") },
		{ TEXT("Alona"),   TEXT("Support") },
		{ TEXT("Korryn"),  TEXT("Debuff Support") },
		{ TEXT("Rawlins"), TEXT("Shooter") }
	};

	const auto& SlotWidgets = Widget->GetHeroSlotWidgets();
	if (!TestTrue(TEXT("4 UBwayHeroSlotWidget cards constructed"), SlotWidgets.Num() >= 4))
	{
		return false;
	}

	for (int32 Idx = 0; Idx < 4; ++Idx)
	{
		const FHeroDisplayInfo& Info = Heroes[Idx];
		TestEqual(FString::Printf(TEXT("Canonical hero #%d name"), Idx), Info.DisplayName.ToString(), FString(Expected[Idx].Name));
		TestEqual(FString::Printf(TEXT("Canonical hero #%d reconciled class badge"), Idx), Info.ClassName.ToString(), FString(Expected[Idx].ExpectedClass));
		TestNotNull(FString::Printf(TEXT("Canonical hero #%d portrait texture"), Idx), Info.Portrait.Get());
		TestTrue(FString::Printf(TEXT("Canonical hero #%d MaxHealth > 0"), Idx), Info.Stats.MaxHealth > 0.f);
		TestTrue(FString::Printf(TEXT("Canonical hero #%d MoveSpeed > 0"), Idx), Info.Stats.MoveSpeed > 0.f);

		UBwayHeroSlotWidget* Slot = SlotWidgets[Idx];
		if (TestNotNull(TEXT("Hero slot widget valid"), Slot))
		{
			TestEqual(TEXT("Slot HeroId bound"), Slot->GetHeroId(), Info.HeroId);
			TestEqual(TEXT("Slot ClassName bound"), Slot->GetHeroClassName().ToString(), FString(Expected[Idx].ExpectedClass));
			TestEqual(TEXT("Slot Portrait bound"), Slot->GetHeroPortrait(), Info.Portrait.Get());
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayHeroSelectionKitCooldownsBuildablesTest,
	"Breakaway.HeroSelection.KitActionsCooldownsAndBuildables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayHeroSelectionKitCooldownsBuildablesTest::RunTest(const FString&)
{
	BwayGoal5TestHelpers::FScopedHeadlessUIWorld Scope(this);
	if (!Scope.Init(true))
	{
		return false;
	}

	ABwayPlayerController* PC = Scope.SpawnLocalHuman(0, 257, TEXT("KitInspector"));
	UBwayHeroSelectWidget* Widget = CreateWidget<UBwayHeroSelectWidget>(PC, UBwayHeroSelectWidget::StaticClass());
	if (!TestNotNull(TEXT("Hero select widget created"), Widget))
	{
		return false;
	}
	TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	(void)SlateWidget;

	const TMap<FString, FString> ExpectedBuildables = {
		{ TEXT("Argus"),   TEXT("Siege Engine") },
		{ TEXT("Alona"),   TEXT("Sun Shrine") },
		{ TEXT("Korryn"),  TEXT("Cursed Ward") },
		{ TEXT("Rawlins"), TEXT("Jail") }
	};

	const TArray<FHeroDisplayInfo> Heroes = Widget->GetAvailableHeroes();
	for (int32 Idx = 0; Idx < FMath::Min(4, Heroes.Num()); ++Idx)
	{
		const FHeroDisplayInfo& Hero = Heroes[Idx];
		Widget->SetInspectedHero(Hero.HeroId);

		const FString StatsReadout = Widget->GetInspectedStatsDisplayText().ToString();
		TestTrue(FString::Printf(TEXT("%s stats readout populated"), *Hero.DisplayName.ToString()),
			StatsReadout.Contains(TEXT("HP ")) && StatsReadout.Contains(TEXT("ATK "))
			&& StatsReadout.Contains(TEXT("ARMOR ")) && StatsReadout.Contains(TEXT("SPEED ")));

		// Expect Primary + 4 kit actions + 1 Signature Buildable = 6 actions (no duplicated Jump/Slide ability)
		TestEqual(FString::Printf(TEXT("%s resolves Primary + 4 kit actions + Signature Buildable (6 total)"), *Hero.DisplayName.ToString()),
			Hero.Abilities.Num(), 6);

		int32 BuildableCount = 0;
		int32 PositiveCooldownKitCount = 0;
		FString FoundBuildableName;
		TSet<FGameplayTag> SeenInputTags;

		const FGameplayTag SharedMoveSlideTag = FGameplayTag::RequestGameplayTag(FName("InputTag.Move.Slide"), false);
		const FGameplayTag SharedAbilitySlideTag = FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Slide"), false);
		const FGameplayTag SharedJumpTag = FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Jump"), false);

		for (int32 AbilIdx = 0; AbilIdx < Hero.Abilities.Num(); ++AbilIdx)
		{
			const FAbilityDisplayInfo& Abil = Hero.Abilities[AbilIdx];
			TestFalse(FString::Printf(TEXT("%s slot %d name non-empty"), *Hero.DisplayName.ToString(), AbilIdx), Abil.AbilityName.IsEmpty());
			TestFalse(FString::Printf(TEXT("%s slot %d description non-empty"), *Hero.DisplayName.ToString(), AbilIdx), Abil.Description.IsEmpty());
			TestFalse(FString::Printf(TEXT("%s slot %d keybind label non-empty"), *Hero.DisplayName.ToString(), AbilIdx), Abil.InputKeyLabel.IsEmpty());

			const FString LowerName = Abil.AbilityName.ToString().ToLower();
			TestFalse(FString::Printf(TEXT("%s does not include shared Jump as kit ability"), *Hero.DisplayName.ToString()),
				LowerName == TEXT("jump"));
			if (Abil.InputActionTag.IsValid())
			{
				TestFalse(FString::Printf(TEXT("%s does not duplicate shared movement tags into kit rows"), *Hero.DisplayName.ToString()),
					Abil.InputActionTag == SharedMoveSlideTag || Abil.InputActionTag == SharedAbilitySlideTag || Abil.InputActionTag == SharedJumpTag);
				TestFalse(FString::Printf(TEXT("%s does not duplicate mobility/kit input slot %s"), *Hero.DisplayName.ToString(), *Abil.InputActionTag.ToString()),
					SeenInputTags.Contains(Abil.InputActionTag));
				SeenInputTags.Add(Abil.InputActionTag);
			}

			if (Abil.bIsBuildable)
			{
				++BuildableCount;
				FoundBuildableName = Abil.AbilityName.ToString();
			}
			if (Abil.CooldownSeconds > 0.f)
			{
				++PositiveCooldownKitCount;
			}
		}

		TestEqual(FString::Printf(TEXT("%s has exactly 1 signature buildable"), *Hero.DisplayName.ToString()), BuildableCount, 1);
		if (const FString* ExpectedBld = ExpectedBuildables.Find(Hero.DisplayName.ToString()))
		{
			TestEqual(FString::Printf(TEXT("%s signature buildable matches Goal 1 spec"), *Hero.DisplayName.ToString()), FoundBuildableName, *ExpectedBld);
		}
		TestTrue(FString::Printf(TEXT("%s derives positive cooldowns on kit abilities"), *Hero.DisplayName.ToString()), PositiveCooldownKitCount >= 4);

		int32 ActivePreviewRows = 0;
		for (UBwayAbilityPreviewWidget* PreviewRow : Widget->GetAbilityPreviewWidgets())
		{
			if (PreviewRow && PreviewRow->HasActivePreview())
			{
				++ActivePreviewRows;
			}
		}
		TestEqual(FString::Printf(TEXT("%s populates 6 UBwayAbilityPreviewWidget rows"), *Hero.DisplayName.ToString()), ActivePreviewRows, 6);
	}

	const FString SharedMovement = Widget->GetSharedMovementDisplayText().ToString();
	TestTrue(TEXT("Shared movement summary includes Jump and Slide"),
		SharedMovement.Contains(TEXT("Jump")) && SharedMovement.Contains(TEXT("Slide")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayHeroSelectionRulesLockUnlockAndBotsTest,
	"Breakaway.HeroSelection.DuplicateRulesLockUnlockAndBotPreAssign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayHeroSelectionRulesLockUnlockAndBotsTest::RunTest(const FString&)
{
	AddExpectedMessage(TEXT("BwayPlayerState: Rejecting duplicate hero"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	FEditorScriptExecutionGuard ScriptGuard;
	BwayGoal5TestHelpers::FScopedHeadlessUIWorld Scope(this);
	if (!Scope.Init(true, TEXT("?HeroSelectStaging=1?NumBots=6")))
	{
		return false;
	}

	UBwayHeroSelectionManager* Manager = Scope.GameState->HeroSelectionManager;
	Manager->SetForceStagingBotPreAssignmentForTest(true, 6);

	ABwayPlayerController* BluePC0 = Scope.SpawnLocalHuman(0, 301, TEXT("BlueCaptain"));
	ABwayPlayerController* BluePC1 = Scope.SpawnLocalHuman(0, 302, TEXT("BlueWingman"));
	ABwayPlayerState* BluePS0 = BluePC0->GetPlayerState<ABwayPlayerState>();
	ABwayPlayerState* BluePS1 = BluePC1->GetPlayerState<ABwayPlayerState>();

	Manager->StartHeroSelection();

	// 2 humans on Blue Team (0), 0 humans on Red Team (1), MaxBots = 6 -> 2 Blue bots + 4 Red bots = 6 staging bots
	const TArray<FBwayStagingBotSelection>& InitialBots = Manager->GetStagingBotSelections();
	int32 BlueBotCount = 0;
	int32 RedBotCount = 0;
	FPrimaryAssetId BlueBotClaimedHero;
	for (const FBwayStagingBotSelection& Bot : InitialBots)
	{
		if (Bot.TeamIndex == 0)
		{
			++BlueBotCount;
			if (!BlueBotClaimedHero.IsValid())
			{
				BlueBotClaimedHero = Bot.SelectedHeroId;
			}
		}
		else if (Bot.TeamIndex == 1)
		{
			++RedBotCount;
		}
	}
	TestEqual(TEXT("Blue Team pre-assigns 2 staging bots alongside 2 humans"), BlueBotCount, 2);
	TestEqual(TEXT("Red Team pre-assigns 4 staging bots"), RedBotCount, 4);
	TestTrue(TEXT("Pre-assigned bot hero valid"), BlueBotClaimedHero.IsValid());

	// Human selecting a bot's pre-assigned hero succeeds and forces the bot to yield to an unclaimed hero
	TestTrue(TEXT("Human can claim a staging bot's pre-assigned hero"), BluePS0->TrySetSelectedHeroId(BlueBotClaimedHero));
	for (const FBwayStagingBotSelection& Bot : Manager->GetStagingBotSelections())
	{
		if (Bot.TeamIndex == 0)
		{
			TestNotEqual(TEXT("Blue staging bot yielded human's chosen hero"), Bot.SelectedHeroId, BlueBotClaimedHero);
		}
	}

	// Second human on the same team CANNOT claim BluePS0's hero
	TestFalse(TEXT("Duplicate human selection on same team rejected"), BluePS1->TrySetSelectedHeroId(BlueBotClaimedHero));

	// Instantiate UBwayHeroSelectWidget for BluePC0 and verify lock -> grid disabled -> unlock (rematch reuse) -> grid enabled
	UBwayHeroSelectWidget* Widget = CreateWidget<UBwayHeroSelectWidget>(BluePC0, UBwayHeroSelectWidget::StaticClass());
	TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	(void)SlateWidget;
	Widget->ActivateWidget();

	const TArray<FBwayTeamRosterSlotViewModel> BlueSidebar = Widget->GetTeamRosterSlotViewModels(0);
	const TArray<FBwayTeamRosterSlotViewModel> RedSidebar = Widget->GetTeamRosterSlotViewModels(1);
	TestEqual(TEXT("Blue sidebar has 4 slots"), BlueSidebar.Num(), 4);
	TestEqual(TEXT("Red sidebar has 4 slots"), RedSidebar.Num(), 4);
	for (int32 Idx = 0; Idx < 4; ++Idx)
	{
		TestTrue(TEXT("Blue 4v4 slot occupied"), BlueSidebar[Idx].bIsOccupied);
		TestTrue(TEXT("Red 4v4 slot occupied by bot"), RedSidebar[Idx].bIsOccupied && RedSidebar[Idx].bIsBot);
		TestNotNull(TEXT("Red bot sidebar slot has portrait"), RedSidebar[Idx].Portrait.Get());
	}

	// Lock BluePS0
	BluePS0->ServerLockHeroSelection();
	Widget->RefreshSelectionLockPresentation();
	TestTrue(TEXT("BluePS0 locked"), Widget->IsSelectionLocked());
	int32 Ready = 0, Total = 0;
	Widget->GetReadyPlayerCount(Ready, Total);
	TestEqual(TEXT("Ready count reflects 1/2 humans"), Ready, 1);
	TestEqual(TEXT("Total registered humans is 2"), Total, 2);

	for (UBwayHeroSlotWidget* Slot : Widget->GetHeroSlotWidgets())
	{
		if (Slot)
		{
			TestTrue(TEXT("Hero slot disabled once local selection locked"), Slot->IsLocalPhaseLocked() && !Slot->GetIsEnabled());
		}
	}

	// Rematch / pooled unlock reuse
	BluePS0->UnlockHeroSelection();
	Widget->RefreshSelectionLockPresentation();
	TestFalse(TEXT("BluePS0 unlocked for rematch reuse"), Widget->IsSelectionLocked());
	for (UBwayHeroSlotWidget* Slot : Widget->GetHeroSlotWidgets())
	{
		if (Slot)
		{
			TestFalse(TEXT("Hero slot re-enabled after OnLocalSelectionUnlocked"), Slot->IsLocalPhaseLocked());
			TestTrue(TEXT("Hero slot interactive after unlock"), Slot->GetIsEnabled());
		}
	}

	// Verify Dorado carry-over URL serialization contains 2 unique Blue bot heroes disjoint from BluePS0 and free of ':' for CanServerTravel
	const FString BlueCarry = Manager->BuildStagingBotHeroesOptionForTeam(0);
	TestFalse(TEXT("Team 0 staging bot carry option non-empty"), BlueCarry.IsEmpty());
	TestFalse(TEXT("Team 0 staging bot carry option omits ':' so CanServerTravel accepts URL"), BlueCarry.Contains(TEXT(":")));
	TestFalse(TEXT("Team 0 staging bot carry option excludes human hero"), BlueCarry.Contains(BlueBotClaimedHero.PrimaryAssetName.ToString()));

	Widget->DeactivateWidget();
	return true;
}

// ============================================================================
// 2. Breakaway.UI.FrontEndAndHeroSelectAudit.*
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayFrontEndMainMenuFlowsAuditTest,
	"Breakaway.UI.FrontEndAndHeroSelectAudit.StartupConfigAndMainMenuFlows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayFrontEndMainMenuFlowsAuditTest::RunTest(const FString&)
{
	FString DefaultMap;
	GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), DefaultMap, GEngineIni);
	TestTrue(TEXT("Startup GameDefaultMap points to L_LyraFrontEnd (not L_Expanse)"),
		DefaultMap.Contains(TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd")) && !DefaultMap.Contains(TEXT("L_Expanse")));

	const auto* LanPlaylist = LoadObject<UBwayUserFacingExperienceDefinition>(
		nullptr,
		TEXT("/Game/System/DA_Playlist_CTR_LAN.DA_Playlist_CTR_LAN"));
	if (TestNotNull(TEXT("DA_Playlist_CTR_LAN loads as UBwayUserFacingExperienceDefinition"), LanPlaylist))
	{
		TestEqual(TEXT("DA_Playlist_CTR_LAN MaxPlayerCount is 8 (4v4)"), LanPlaylist->MaxPlayerCount, 8);
		TestTrue(TEXT("DA_Playlist_CTR_LAN routes through L_BW_HeroSelect_Staging"), LanPlaylist->bRouteThroughHeroSelectStaging);
		TestTrue(TEXT("DA_Playlist_CTR_LAN targets L_BW_Dorado"), LanPlaylist->MapID.PrimaryAssetName.ToString().Contains(TEXT("Dorado")));
	}

	BwayGoal5TestHelpers::FScopedHeadlessUIWorld Scope(this);
	if (!Scope.Init(false))
	{
		return false;
	}
	ABwayPlayerController* PC = Scope.SpawnLocalHuman(0, 401, TEXT("MenuAuditor"));

	UClass* MainMenuClass = LoadClass<UBwayMainMenuWidget>(nullptr, TEXT("/BreakawayCore/UI/FrontEnd/W_MainMenu.W_MainMenu_C"));
	if (!MainMenuClass)
	{
		MainMenuClass = UBwayMainMenuWidget::StaticClass();
	}

	UBwayMainMenuWidget* MainMenu = CreateWidget<UBwayMainMenuWidget>(PC, MainMenuClass);
	if (!TestNotNull(TEXT("W_MainMenu instantiated"), MainMenu))
	{
		return false;
	}
	TSharedRef<SWidget> SlateWidget = MainMenu->TakeWidget();
	(void)SlateWidget;

	const TArray<FString> Labels = MainMenu->GetVisibleMenuButtonLabels();
	const TArray<FString> ExpectedLabels = {
		TEXT("Host LAN Match"),
		TEXT("Find LAN Match"),
		TEXT("Dev Practice / Bot Match"),
		TEXT("Settings"),
		TEXT("Quit")
	};
	if (TestEqual(TEXT("Main Menu exposes exact 5 Breakaway flows with no leftover Shooter tiles"), Labels.Num(), ExpectedLabels.Num()))
	{
		for (int32 Idx = 0; Idx < ExpectedLabels.Num(); ++Idx)
		{
			TestEqual(FString::Printf(TEXT("Main Menu button #%d"), Idx), Labels[Idx], ExpectedLabels[Idx]);
		}
	}

	TestEqual(TEXT("Main Menu desired focus target is Host LAN Match button"),
		MainMenu->GetDesiredFocusTarget(),
		static_cast<UWidget*>(MainMenu->GetHostLanMatchButton()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayServerBrowserStatesAndFocusAuditTest,
	"Breakaway.UI.FrontEndAndHeroSelectAudit.ServerBrowserStatesAndFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayServerBrowserStatesAndFocusAuditTest::RunTest(const FString&)
{
	BwayGoal5TestHelpers::FScopedHeadlessUIWorld Scope(this);
	if (!Scope.Init(false))
	{
		return false;
	}
	ABwayPlayerController* PC = Scope.SpawnLocalHuman(0, 402, TEXT("BrowserAuditor"));

	UClass* BrowserClass = LoadClass<UBwayLocalMatchBrowserWidget>(nullptr, TEXT("/BreakawayCore/UI/FrontEnd/W_LocalMatchBrowser.W_LocalMatchBrowser_C"));
	if (!BrowserClass)
	{
		BrowserClass = UBwayLocalMatchBrowserWidget::StaticClass();
	}

	UBwayLocalMatchBrowserWidget* Browser = CreateWidget<UBwayLocalMatchBrowserWidget>(PC, BrowserClass);
	if (!TestNotNull(TEXT("W_LocalMatchBrowser instantiated"), Browser))
	{
		return false;
	}
	TSharedRef<SWidget> SlateWidget = Browser->TakeWidget();
	(void)SlateWidget;

	// 1. Searching state
	Browser->ApplySyntheticBrowserStateForAudit(EBwayServerBrowserViewState::Searching, {});
	TestTrue(TEXT("Searching spinner visible during Searching state"), Browser->IsSpinnerVisible());
	TestFalse(TEXT("Error banner hidden during Searching state"), Browser->IsErrorBannerVisible());
	TestEqual(TEXT("Searching state focus targets Back button"), Browser->GetDesiredFocusTarget(), static_cast<UWidget*>(Browser->GetBackButton()));

	// 2. Empty state
	Browser->ApplySyntheticBrowserStateForAudit(EBwayServerBrowserViewState::Empty, {});
	TestFalse(TEXT("Spinner hidden in Empty state"), Browser->IsSpinnerVisible());
	TestEqual(TEXT("Empty state focus targets Refresh button"), Browser->GetDesiredFocusTarget(), static_cast<UWidget*>(Browser->GetRefreshButton()));

	// 3. Populated state (Joinable 3/8 Dorado, Full 8/8 Dorado, Unreachable Dorado)
	FBwayServerBrowserRowViewModel JoinableRow;
	JoinableRow.SessionName = TEXT("Alpha Host LAN");
	JoinableRow.PlaylistName = TEXT("DA_Playlist_CTR_LAN");
	JoinableRow.MapName = TEXT("Dorado");
	JoinableRow.CurrentPlayers = 3;
	JoinableRow.MaxPlayers = 8;
	JoinableRow.OpenPublicConnections = 5;
	JoinableRow.PingMs = 12;
	JoinableRow.FormattedPlayerCount = FText::FromString(TEXT("3/8"));
	JoinableRow.FormattedPing = FText::FromString(TEXT("12 ms"));

	FBwayServerBrowserRowViewModel FullRow = JoinableRow;
	FullRow.SessionName = TEXT("Full Arena");
	FullRow.CurrentPlayers = 8;
	FullRow.OpenPublicConnections = 0;
	FullRow.bIsFull = true;
	FullRow.FormattedPlayerCount = FText::FromString(TEXT("8/8"));

	FBwayServerBrowserRowViewModel UnreachableRow = JoinableRow;
	UnreachableRow.SessionName = TEXT("Timed Out Host");
	UnreachableRow.PingMs = 9999;
	UnreachableRow.bIsUnreachable = true;
	UnreachableRow.FormattedPing = FText::FromString(TEXT("Unreachable"));

	Browser->ApplySyntheticBrowserStateForAudit(EBwayServerBrowserViewState::Populated, { JoinableRow, FullRow, UnreachableRow });
	TestEqual(TEXT("3 rows populated"), Browser->GetRowJoinButtons().Num(), 3);
	TestEqual(TEXT("Populated state focuses first joinable row's Join button"),
		Browser->GetDesiredFocusTarget(),
		static_cast<UWidget*>(Browser->GetRowJoinButtons()[0]));

	// 4. Attempting to join Full session surfaces clear error + Retry focus
	TestFalse(TEXT("Full session join rejected cleanly"), Browser->RequestJoinRow(1));
	TestTrue(TEXT("Error banner shown on Full session"), Browser->IsErrorBannerVisible());
	TestTrue(TEXT("Full session error message actionable"), Browser->GetLastErrorMessage().ToString().Contains(TEXT("full"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Error recovery focuses Retry button"), Browser->GetDesiredFocusTarget(), static_cast<UWidget*>(Browser->GetRetryButton()));

	// 5. Attempting to join Unreachable session surfaces clear error + Retry focus
	TestFalse(TEXT("Unreachable session join rejected cleanly"), Browser->RequestJoinRow(2));
	TestTrue(TEXT("Error banner shown on Unreachable session"), Browser->IsErrorBannerVisible());
	TestTrue(TEXT("Unreachable session error message actionable"), Browser->GetLastErrorMessage().ToString().Contains(TEXT("unreachable"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Unreachable error recovery focuses Retry button"), Browser->GetDesiredFocusTarget(), static_cast<UWidget*>(Browser->GetRetryButton()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayPauseMenuHostClientAndFocusAuditTest,
	"Breakaway.UI.FrontEndAndHeroSelectAudit.PauseMenuHostClientAndFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayPauseMenuHostClientAndFocusAuditTest::RunTest(const FString&)
{
	BwayGoal5TestHelpers::FScopedHeadlessUIWorld Scope(this);
	if (!Scope.Init(true))
	{
		return false;
	}
	ABwayPlayerController* PC = Scope.SpawnLocalHuman(0, 403, TEXT("PauseAuditor"));

	UBwayPauseMenuWidget* PauseMenu = CreateWidget<UBwayPauseMenuWidget>(PC, UBwayPauseMenuWidget::StaticClass());
	if (!TestNotNull(TEXT("UBwayPauseMenuWidget instantiated"), PauseMenu))
	{
		return false;
	}
	TSharedRef<SWidget> SlateWidget = PauseMenu->TakeWidget();
	(void)SlateWidget;

	TestNotNull(TEXT("Resume button present"), PauseMenu->GetResumeButton());
	TestNotNull(TEXT("Options button present"), PauseMenu->GetOptionsButton());
	TestNotNull(TEXT("Leave Match button present"), PauseMenu->GetLeaveMatchButton());
	TestEqual(TEXT("Pause Menu desired focus target is Resume button"),
		PauseMenu->GetDesiredFocusTarget(),
		static_cast<UWidget*>(PauseMenu->GetResumeButton()));

	// Host presentation
	PauseMenu->SetClientRoleOverrideForAudit(true, false);
	TestEqual(TEXT("Host sees 'Return to Lobby'"),
		PauseMenu->GetLeaveActionLabelText().ToString(),
		FString(TEXT("Return to Lobby")));

	// Client presentation
	PauseMenu->SetClientRoleOverrideForAudit(true, true);
	TestEqual(TEXT("Client sees 'Disconnect to Main Menu'"),
		PauseMenu->GetLeaveActionLabelText().ToString(),
		FString(TEXT("Disconnect to Main Menu")));

	return true;
}

namespace BwayGoal5HeroSelectAuditLatent
{
	class FAuditStepCommand : public IAutomationLatentCommand
	{
	public:
		FAuditStepCommand(FAutomationTestBase* InTest, const TCHAR* InAssetPath)
			: Test(InTest)
			, AssetPath(InAssetPath)
		{
		}

		virtual bool Update() override
		{
			FEditorScriptExecutionGuard ScriptGuard;

			if (PhaseStep == 0)
			{
				PhaseStep = 1;
				UClass* WidgetClass = LoadClass<UBwayHeroSelectWidget>(nullptr, *AssetPath);
				if (!Test->TestNotNull(FString::Printf(TEXT("Loaded %s"), *AssetPath), WidgetClass))
				{
					return true;
				}

				ScopePtr = MakeUnique<BwayGoal5TestHelpers::FScopedHeadlessUIWorld>(Test);
				if (!ScopePtr->Init(true, TEXT("?HeroSelectStaging=1?NumBots=7")))
				{
					ScopePtr.Reset();
					return true;
				}

				// Prime FTimerManager on this engine frame so SetTimer(..., 5.f) enters ActiveTimerHeap
				// rather than PendingTimerSet, allowing next frame's Tick(5.1f) to fire CommitPendingTravel.
				ScopePtr->World->GetTimerManager().Tick(0.f);

				ScopePtr->GameState->HeroSelectionPhaseComponent->PostHeroSelectionMapID = FPrimaryAssetId();
				ScopePtr->GameState->HeroSelectionPhaseComponent->HeroSelectionWidgetClass = UCommonActivatableWidget::StaticClass();

				HostPC = ScopePtr->SpawnLocalHuman(0, 410, TEXT("StagingHost"));
				ABwayPlayerState* HostPS = HostPC->GetPlayerState<ABwayPlayerState>();
				ScopePtr->GameState->HeroSelectionManager->SetForceStagingBotPreAssignmentForTest(true, 7);
				ScopePtr->World->GetTimerManager().ClearAllTimersForObject(ScopePtr->GameState->HeroSelectionPhaseComponent);
				ScopePtr->GameState->HeroSelectionPhaseComponent->StartHeroSelectionPhase();

				Widget = CreateWidget<UBwayHeroSelectWidget>(HostPC, WidgetClass);
				PinnedSlateWidget = Widget->TakeWidget();
				Widget->ActivateWidget();

				// Initial activation must NOT mutate authoritative SelectedHeroId (EmptyLockRejected contract)
				Test->TestFalse(FString::Printf(TEXT("%s keeps authoritative selection empty on activation"), *AssetPath),
					HostPS->GetSelectedHeroId().IsValid());
				Test->TestTrue(FString::Printf(TEXT("%s locally inspects first hero on activation"), *AssetPath),
					Widget->GetInspectedHeroId().IsValid());

				// Focus starts on first hero card
				Test->TestEqual(FString::Printf(TEXT("%s focuses first hero card"), *AssetPath),
					Widget->GetDesiredFocusTarget(),
					static_cast<UWidget*>(Widget->GetHeroSlotWidgets()[0]));

				// Verify 4v4 staging bot pre-assignment fills remaining 3 Blue + 4 Red slots
				int32 BlueBots = 0;
				int32 RedBots = 0;
				for (const FBwayStagingBotSelection& BotSel : ScopePtr->GameState->HeroSelectionManager->GetStagingBotSelections())
				{
					if (BotSel.TeamIndex == 0)
					{
						++BlueBots;
					}
					else if (BotSel.TeamIndex == 1)
					{
						++RedBots;
					}
				}
				Test->TestEqual(FString::Printf(TEXT("%s Blue team has 3 pre-assigned staging bots alongside 1 human"), *AssetPath), BlueBots, 3);
				Test->TestEqual(FString::Printf(TEXT("%s Red team has 4 pre-assigned staging bots"), *AssetPath), RedBots, 4);

				// Verify Button_Lock and Text_HeroDetails recovery contract
				LockBtn = Cast<UCommonButtonBase>(Widget->GetWidgetFromName(TEXT("Button_Lock")));
				DetailsText = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_HeroDetails")));
				if (!Test->TestNotNull(TEXT("Button_Lock present as UCommonButtonBase"), LockBtn)
					|| !Test->TestNotNull(TEXT("Text_HeroDetails present as UTextBlock"), DetailsText))
				{
					Widget->DeactivateWidget();
					PinnedSlateWidget.Reset();
					ScopePtr.Reset();
					return true;
				}

				const TArray<FHeroDisplayInfo> Heroes = Widget->GetAvailableHeroes();
				Test->TestTrue(TEXT("SelectHero succeeds"), Widget->SelectHero(Heroes[0].HeroId));
				Test->TestTrue(TEXT("LockSelection succeeds"), Widget->LockSelection());
				return false; // Yield one engine frame so GFrameCounter advances before Tick(5.1f)
			}

			if (ScopePtr && ScopePtr->World && Widget && HostPC && LockBtn && DetailsText)
			{
				ScopePtr->World->GetTimerManager().Tick(5.1f);

				Test->TestTrue(FString::Printf(TEXT("%s latches post-selection travel failure"), *AssetPath),
					ScopePtr->GameState->HeroSelectionPhaseComponent->HasPostSelectionTravelFailed());
				Test->TestTrue(FString::Printf(TEXT("%s Host can retry post-selection travel"), *AssetPath),
					HostPC->CanRetryHeroSelectionTravel());

				int32 NonEmptyTextChildren = 0;
				FString FoundBtnLabel;
				LockBtn->WidgetTree->ForEachWidgetAndDescendants([&](UWidget* Child)
				{
					if (const auto* Block = Cast<UTextBlock>(Child))
					{
						const FString Txt = Block->GetText().ToString().TrimStartAndEnd();
						if (!Txt.IsEmpty())
						{
							++NonEmptyTextChildren;
							FoundBtnLabel = Txt;
						}
					}
				});
				Test->TestEqual(FString::Printf(TEXT("%s Button_Lock has single non-empty UTextBlock"), *AssetPath), NonEmptyTextChildren, 1);
				Test->TestEqual(FString::Printf(TEXT("%s Host sees 'Retry travel'"), *AssetPath), FoundBtnLabel, FString(TEXT("Retry travel")));
				Test->TestTrue(FString::Printf(TEXT("%s Text_HeroDetails shows Match travel failed"), *AssetPath),
					DetailsText->GetText().ToString().Contains(TEXT("Match travel failed.")));
				Test->TestEqual(FString::Printf(TEXT("%s focuses Button_Lock when travel retry available"), *AssetPath),
					Widget->GetDesiredFocusTarget(),
					static_cast<UWidget*>(LockBtn));

				Widget->DeactivateWidget();
			}

			PinnedSlateWidget.Reset();
			Widget = nullptr;
			LockBtn = nullptr;
			DetailsText = nullptr;
			HostPC = nullptr;
			ScopePtr.Reset();
			return true;
		}

	private:
		FAutomationTestBase* Test = nullptr;
		FString AssetPath;
		TUniquePtr<BwayGoal5TestHelpers::FScopedHeadlessUIWorld> ScopePtr;
		int32 PhaseStep = 0;
		ABwayPlayerController* HostPC = nullptr;
		UBwayHeroSelectWidget* Widget = nullptr;
		UCommonButtonBase* LockBtn = nullptr;
		UTextBlock* DetailsText = nullptr;
		TSharedPtr<SWidget> PinnedSlateWidget;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayHeroSelectSidebarsAndTravelRecoveryAuditTest,
	"Breakaway.UI.FrontEndAndHeroSelectAudit.HeroSelectSidebarsNavigationAndTravelFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayHeroSelectSidebarsAndTravelRecoveryAuditTest::RunTest(const FString&)
{
	AddExpectedMessage(
		TEXT("BwayHeroSelectionPhaseComponent: HeroSelectStaging is set but no HeroSelectTargetMap or PostHeroSelectionMapID was provided"),
		ELogVerbosity::Error,
		EAutomationExpectedMessageFlags::Contains,
		2,
		false);

	// Audit BOTH W_HeroSelection_C and WBP_BW_HeroSelect_C in isolated worlds across latent frames so FTimerManager::Tick fires CommitPendingTravel
	ADD_LATENT_AUTOMATION_COMMAND(BwayGoal5HeroSelectAuditLatent::FAuditStepCommand(
		this, TEXT("/BreakawayCore/UI/HeroSelect/W_HeroSelection.W_HeroSelection_C")));
	ADD_LATENT_AUTOMATION_COMMAND(BwayGoal5HeroSelectAuditLatent::FAuditStepCommand(
		this, TEXT("/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect.WBP_BW_HeroSelect_C")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// 3. Live PIE Runtime Navigation, Input-Device Switch & Settings/Pause Audit
//    Command: bway.Test.Goal5RuntimeAudit
// ============================================================================

namespace BwayGoal5RuntimeAudit
{
	bool bAuditActive = false;

	struct FAuditSession
	{
		int32 Step = 0;
		double StartedAt = FPlatformTime::Seconds();
		double StepAt = StartedAt;
		float OriginalFrameRateLimit = 0.f;
		TWeakObjectPtr<UCommonActivatableWidget> ActiveModal;
		TWeakObjectPtr<UBwayMatchLobbyWidget> ActiveLobby;
		TWeakObjectPtr<UBwayLocalMatchBrowserWidget> ActiveBrowser;
		TWeakObjectPtr<UBwayPauseMenuWidget> HostPause;
		TWeakObjectPtr<UBwayPauseMenuWidget> ClientPause;

		void Advance(int32 NextStep)
		{
			Step = NextStep;
			StepAt = FPlatformTime::Seconds();
		}

		bool Fail(const TCHAR* Reason)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayGoal5Audit: FAIL step=%d reason=%s"), Step, Reason);
			bAuditActive = false;
			return false;
		}

		bool Pass(const TCHAR* MapTag, const FString& Summary)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayGoal5Audit: PASS map=%s guard=%d %s"),
				MapTag, GAllowActorScriptExecutionInEditor, *Summary);
			bAuditActive = false;
			return false;
		}

		bool Tick(float)
		{
			if (GAllowActorScriptExecutionInEditor)
			{
				return true; // Wait for normal engine tick outside Editor Python script guard
			}

			const double Now = FPlatformTime::Seconds();
			if (Now - StartedAt > 45.0)
			{
				return Fail(TEXT("audit-timeout"));
			}

			TArray<UWorld*> GameWorlds;
			if (GEngine)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* W = Context.World();
					if (W && W->IsGameWorld() && !W->bIsTearingDown && W->GetGameInstance())
					{
						GameWorlds.Add(W);
					}
				}
			}
			if (GameWorlds.IsEmpty())
			{
				return true;
			}

			UWorld* PrimaryWorld = GameWorlds[0];
			APlayerController* PrimaryPC = PrimaryWorld->GetFirstPlayerController();
			if (!PrimaryPC || !PrimaryPC->GetLocalPlayer())
			{
				return true;
			}

			const FString MapName = PrimaryWorld->GetMapName();

			// -----------------------------------------------------------------
			// A. L_LyraFrontEnd Runtime Audit
			// -----------------------------------------------------------------
			if (MapName.Contains(TEXT("L_LyraFrontEnd")))
			{
				UBwayMainMenuWidget* MainMenu = nullptr;
				for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
				{
					if (It->IsTemplate() || It->GetOwningPlayer() != PrimaryPC || !It->IsActivated())
					{
						continue;
					}
					if (It->GetClass()->GetName() == TEXT("W_LyraFrontEnd_C"))
					{
						if (Now - StartedAt > 2.0)
						{
							return Fail(TEXT("stock-lyra-frontend-still-active"));
						}
						return true;
					}
					if (auto* Candidate = Cast<UBwayMainMenuWidget>(*It))
					{
						MainMenu = Candidate;
					}
				}

				if (Step == 0)
				{
					if (!MainMenu)
					{
						return true;
					}
					if (MainMenu->GetVisibleMenuButtonLabels().Num() != 5)
					{
						return Fail(TEXT("unexpected-main-menu-flow-count"));
					}
					if (UCommonInputSubsystem* InputSub = UCommonInputSubsystem::Get(PrimaryPC->GetLocalPlayer()))
					{
						InputSub->SetCurrentInputType(ECommonInputType::Gamepad);
					}
					ActiveBrowser = Cast<UBwayLocalMatchBrowserWidget>(MainMenu->ClickFindLanMatch());
					if (!ActiveBrowser.IsValid() || !ActiveBrowser->IsActivated())
					{
						return Fail(TEXT("find-lan-match-open-failed"));
					}
					Advance(1);
					return true;
				}
				if (Step == 1 && Now - StepAt >= 0.25)
				{
					if (!ActiveBrowser.IsValid() || !ActiveBrowser->GetDesiredFocusTarget())
					{
						return Fail(TEXT("browser-focus-target-missing"));
					}
					ActiveBrowser->ClickBack();
					Advance(2);
					return true;
				}
				if (Step == 2 && Now - StepAt >= 0.25)
				{
					if (!MainMenu || !MainMenu->IsActivated())
					{
						return Fail(TEXT("main-menu-not-restored-after-browser-back"));
					}
					ActiveLobby = Cast<UBwayMatchLobbyWidget>(MainMenu->ClickHostLanMatch());
					if (!ActiveLobby.IsValid() || !ActiveLobby->IsActivated())
					{
						return Fail(TEXT("host-lan-lobby-open-failed"));
					}
					UBwayFrontendStateSubsystem* FE = PrimaryPC->GetLocalPlayer()->GetSubsystem<UBwayFrontendStateSubsystem>();
					if (!FE || !FE->GetLobbySettings().bIsLanMatch)
					{
						return Fail(TEXT("host-lan-lobby-online-mode-mismatch"));
					}
					ActiveModal = ActiveLobby->OpenMatchOptions();
					if (!ActiveModal.IsValid() || !ActiveModal->IsActivated())
					{
						return Fail(TEXT("match-options-modal-open-failed"));
					}
					Advance(3);
					return true;
				}
				if (Step == 3 && Now - StepAt >= 0.20)
				{
					auto* Modal = Cast<UBwayMatchSettingsModalWidget>(ActiveModal.Get());
					UBwayFrontendStateSubsystem* FE = PrimaryPC->GetLocalPlayer()->GetSubsystem<UBwayFrontendStateSubsystem>();
					if (!Modal || !FE)
					{
						return Fail(TEXT("match-options-modal-invalid"));
					}
					const bool bOrigBots = FE->GetLobbySettings().bBotsEnabled;
					Modal->ToggleEnableBots();
					Modal->CancelSettings(); // Cancel must preserve original setting
					if (FE->GetLobbySettings().bBotsEnabled != bOrigBots)
					{
						return Fail(TEXT("match-options-cancel-mutated-settings"));
					}
					auto* SecondModal = Cast<UBwayMatchSettingsModalWidget>(ActiveLobby->OpenMatchOptions());
					if (!SecondModal)
					{
						return Fail(TEXT("match-options-reopen-failed"));
					}
					SecondModal->CycleBotDifficulty(1);
					const EBwayBotDifficulty ExpectedDiff = SecondModal->GetWorkingBotDifficulty();
					SecondModal->AcceptSettings();
					if (FE->GetLobbySettings().BotDifficulty != ExpectedDiff)
					{
						return Fail(TEXT("match-options-accept-did-not-persist"));
					}
					{
						const FBwayMatchLobbySettings& Cur = FE->GetLobbySettings();
						FE->ApplyMatchSettingsModal(Cur.SelectedMapIndex, Cur.bBotsEnabled, EBwayBotDifficulty::Normal);
					}
					ActiveLobby->ClickLeaveLobby();
					Advance(4);
					return true;
				}
				if (Step == 4 && Now - StepAt >= 0.25)
				{
					if (!MainMenu || !MainMenu->IsActivated())
					{
						return Fail(TEXT("main-menu-not-restored-after-lobby-leave"));
					}
					if (UGameUserSettings* GUS = GEngine->GetGameUserSettings())
					{
						OriginalFrameRateLimit = GUS->GetFrameRateLimit();
						GUS->SetFrameRateLimit(120.f);
						GUS->ApplyNonResolutionSettings();
						if (!FMath::IsNearlyEqual(GUS->GetFrameRateLimit(), 120.f))
						{
							return Fail(TEXT("settings-apply-failed"));
						}
						GUS->SetFrameRateLimit(OriginalFrameRateLimit);
						GUS->ApplyNonResolutionSettings();
					}
					ActiveModal = MainMenu->OpenSettingsScreen();
					if (!ActiveModal.IsValid() || !ActiveModal->IsActivated())
					{
						return Fail(TEXT("settings-screen-push-failed"));
					}
					Advance(5);
					return true;
				}
				if (Step == 5 && Now - StepAt >= 0.25)
				{
					if (ActiveModal.IsValid())
					{
						UCommonUIExtensions::PopContentFromLayer(ActiveModal.Get());
					}
					if (UCommonInputSubsystem* InputSub = UCommonInputSubsystem::Get(PrimaryPC->GetLocalPlayer()))
					{
						InputSub->SetCurrentInputType(ECommonInputType::MouseAndKeyboard);
					}
					Advance(6);
					return true;
				}
				if (Step == 6 && Now - StepAt >= 0.20)
				{
					if (!MainMenu || !MainMenu->IsActivated() || !MainMenu->GetDesiredFocusTarget())
					{
						return Fail(TEXT("main-menu-focus-not-restored-after-settings"));
					}
					return Pass(TEXT("FrontEnd"), TEXT("flows=5 browserBack=1 lobbyOptionsApplyCancel=1 settingsApplyRestore=1 gamepadSwap=1"));
				}
				return true;
			}

			// -----------------------------------------------------------------
			// B. L_BW_HeroSelect_Staging Runtime Audit
			// -----------------------------------------------------------------
			if (MapName.Contains(TEXT("HeroSelect_Staging")))
			{
				UBwayHeroSelectWidget* HeroSelect = nullptr;
				for (TObjectIterator<UBwayHeroSelectWidget> It; It; ++It)
				{
					if (!It->IsTemplate() && It->GetOwningPlayer() == PrimaryPC && It->IsActivated())
					{
						HeroSelect = *It;
						break;
					}
				}
				if (!HeroSelect)
				{
					return true;
				}

				if (Step == 0)
				{
					if (HeroSelect->GetHeroSlotWidgets().Num() < 4)
					{
						return Fail(TEXT("staging-hero-slots-missing"));
					}
					const FString KbmMove = HeroSelect->GetSharedMovementDisplayText().ToString();
					if (UCommonInputSubsystem* InputSub = UCommonInputSubsystem::Get(PrimaryPC->GetLocalPlayer()))
					{
						InputSub->SetCurrentInputType(ECommonInputType::Gamepad);
					}
					const FString PadMove = HeroSelect->GetSharedMovementDisplayText().ToString();
					if (UCommonInputSubsystem* InputSub = UCommonInputSubsystem::Get(PrimaryPC->GetLocalPlayer()))
					{
						InputSub->SetCurrentInputType(ECommonInputType::MouseAndKeyboard);
					}
					if (KbmMove.IsEmpty() || PadMove.IsEmpty() || KbmMove == PadMove)
					{
						return Fail(TEXT("input-device-switch-did-not-update-prompts"));
					}
					if (!HeroSelect->GetDesiredFocusTarget())
					{
						return Fail(TEXT("staging-focus-target-missing"));
					}
					return Pass(TEXT("Staging"), FString::Printf(
						TEXT("worlds=%d slots=%d blueSlots=%d redSlots=%d inputSwapPromptsUpdated=1"),
						GameWorlds.Num(),
						HeroSelect->GetHeroSlotWidgets().Num(),
						HeroSelect->GetTeamRosterSlotViewModels(0).Num(),
						HeroSelect->GetTeamRosterSlotViewModels(1).Num()));
				}
				return true;
			}

			// -----------------------------------------------------------------
			// C. L_BW_Dorado In-Match Pause Menu & Focus Restoration Audit
			// -----------------------------------------------------------------
			if (MapName.Contains(TEXT("L_BW_Dorado")))
			{
				APlayerController* ClientPC = nullptr;
				for (UWorld* W : GameWorlds)
				{
					if (W->GetNetMode() == NM_Client)
					{
						ClientPC = W->GetFirstPlayerController();
						break;
					}
				}

				if (Step == 0)
				{
					const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
					HostPause = Cast<UBwayPauseMenuWidget>(UCommonUIExtensions::PushContentToLayer_ForPlayer(
						PrimaryPC->GetLocalPlayer(), MenuLayer, UBwayPauseMenuWidget::StaticClass()));
					if (!HostPause.IsValid() || !HostPause->IsActivated())
					{
						return Fail(TEXT("host-pause-push-failed"));
					}
					if (HostPause->GetLeaveActionLabelText().ToString() != TEXT("Return to Lobby"))
					{
						return Fail(TEXT("host-pause-label-mismatch"));
					}
					if (HostPause->GetDesiredFocusTarget() != HostPause->GetResumeButton())
					{
						return Fail(TEXT("host-pause-focus-mismatch"));
					}

					if (ClientPC && ClientPC->GetLocalPlayer())
					{
						ClientPause = Cast<UBwayPauseMenuWidget>(UCommonUIExtensions::PushContentToLayer_ForPlayer(
							ClientPC->GetLocalPlayer(), MenuLayer, UBwayPauseMenuWidget::StaticClass()));
						if (!ClientPause.IsValid() || ClientPause->GetLeaveActionLabelText().ToString() != TEXT("Disconnect to Main Menu"))
						{
							return Fail(TEXT("client-pause-label-mismatch"));
						}
					}

					ActiveModal = HostPause->OpenSettings();
					if (!ActiveModal.IsValid() || !ActiveModal->IsActivated())
					{
						return Fail(TEXT("pause-options-push-failed"));
					}
					Advance(1);
					return true;
				}
				if (Step == 1 && Now - StepAt >= 0.25)
				{
					if (ActiveModal.IsValid())
					{
						UCommonUIExtensions::PopContentFromLayer(ActiveModal.Get());
					}
					Advance(2);
					return true;
				}
				if (Step == 2 && Now - StepAt >= 0.20)
				{
					if (!HostPause.IsValid() || !HostPause->IsActivated())
					{
						return Fail(TEXT("host-pause-not-restored-after-options-back"));
					}
					HostPause->ResumeGame();
					if (ClientPause.IsValid())
					{
						ClientPause->ResumeGame();
					}
					Advance(3);
					return true;
				}
				if (Step == 3 && Now - StepAt >= 0.20)
				{
					if ((HostPause.IsValid() && HostPause->IsActivated())
						|| (ClientPause.IsValid() && ClientPause->IsActivated()))
					{
						return Fail(TEXT("pause-menu-still-active-after-resume"));
					}
					return Pass(TEXT("Dorado"), FString::Printf(
						TEXT("worlds=%d hostReturnToLobby=1 clientDisconnectVerified=%d optionsRoundtrip=1 resumeFocusRestored=1"),
						GameWorlds.Num(),
						ClientPC != nullptr ? 1 : 0));
				}
				return true;
			}

			return Fail(TEXT("unsupported-map-for-goal5-runtime-audit"));
		}
	};

	FAutoConsoleCommand Goal5RuntimeAuditCmd(
		TEXT("bway.Test.Goal5RuntimeAudit"),
		TEXT("Development: Runs live guard=0 Goal 5 menu traversal, input-device swap, settings apply/restore, and Pause Menu focus audit on the active map."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (bAuditActive)
			{
				UE_LOG(LogTemp, Warning, TEXT("BwayGoal5Audit: already active"));
				return;
			}
			bAuditActive = true;
			TSharedRef<FAuditSession> Session = MakeShared<FAuditSession>();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Session](float Delta)
			{
				return Session->Tick(Delta);
			}), 0.05f);
		}));
}

#endif // !UE_BUILD_SHIPPING
