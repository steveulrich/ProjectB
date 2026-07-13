#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Player/LyraPlayerBotController.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

UBwayHeroSelectionPhaseComponent::UBwayHeroSelectionPhaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);

	// BreakawayCore tags may not be registered while the CDO is constructed.
	PhaseTag = FGameplayTag::RequestGameplayTag(FName("Breakaway.GamePhase.HeroSelection"), false);
}

void UBwayHeroSelectionPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only server manages the phase
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Listen for when Lyra's phase system starts the HeroSelection phase
	if (UWorld* World = GetWorld())
	{
		if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
		{
			if (!PhaseTag.IsValid())
			{
				PhaseTag = FGameplayTag::RequestGameplayTag(FName("Breakaway.GamePhase.HeroSelection"), false);
			}

			if (!PhaseTag.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: Breakaway.GamePhase.HeroSelection is not registered; cannot listen for hero selection phase"));
				return;
			}

			PhaseSubsystem->WhenPhaseStartsOrIsActive(
				PhaseTag,
				EPhaseTagMatchType::ExactMatch,
				FLyraGamePhaseTagDelegate::CreateUObject(this, &UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated)
			);
			
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Listening for Lyra phase: %s"), *PhaseTag.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Could not find LyraGamePhaseSubsystem - phase integration disabled"));
		}
	}



	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Initialized"));

	if (ShouldSkipHeroSelectionPhase())
	{
		bHeroSelectionCompleted = true;
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: SkipHeroSelection active — marking hero selection complete; phase progression deferred to RoundManagement"));
	}
	else if (ShouldTravelToPostHeroSelectionMap())
	{
		if (ULyraExperienceManagerComponent* ExperienceManager = GetOwner()->FindComponentByClass<ULyraExperienceManagerComponent>())
		{
			ExperienceManager->CallOrRegister_OnExperienceLoaded_LowPriority(
				FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HeroSelectStaging URL option found; waiting for experience load before starting hero selection"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: No LyraExperienceManagerComponent found; starting staging hero selection on next tick"));
			GetWorld()->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &ThisClass::StartHeroSelectionPhase));
		}
	}
	else if (UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayWithoutHeroSelectFlow(this))
	{
		if (ULyraExperienceManagerComponent* ExperienceManager = GetOwner()->FindComponentByClass<ULyraExperienceManagerComponent>())
		{
			ExperienceManager->CallOrRegister_OnExperienceLoaded_LowPriority(
				FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleDevDirectPlayExperienceLoaded));
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Direct editor play detected; waiting for experience load before dev hero selection"));
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &ThisClass::StartDevDirectPlayHeroSelection));
		}
	}
}

void UBwayHeroSelectionPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bPhaseActive)
	{
		EndHeroSelectionPhase();
	}



	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Hero Selection Phase
// ============================================================

void UBwayHeroSelectionPhaseComponent::StartHeroSelectionPhase()
{
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: StartHeroSelectionPhase called on client"));
		return;
	}

	if (ShouldSkipHeroSelectionPhase())
	{
		bHeroSelectionCompleted = true;
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: SkipHeroSelection active — ignoring hero selection phase start; not calling EndPhaseAndProgressToNext (RoundManagement owns phase flow)"));
		return;
	}

	if (bPhaseActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Phase already active"));
		return;
	}

	bPhaseActive = true;
	bHeroSelectionCompleted = false;

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Starting hero selection phase"));

	// Get selection manager
	UBwayHeroSelectionManager* Manager = GetSelectionManager();
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No selection manager found!"));
		EndHeroSelectionPhase(); // Fail gracefully
		return;
	}

	// IMPORTANT: Bind to the all-ready delegate FIRST, before any locking calls.
	// This ensures we receive the broadcast even if all players are pre-selected from Frontend.
	Manager->OnAllPlayersReady.AddDynamic(this, &UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady);

	// Start selection phase FIRST — this resets all player selections and starts the timer.
	// Must happen before registering players and applying pre-selections.
	Manager->StartHeroSelection();

	// Register all current players
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			Manager->RegisterPlayer(PS);
		}
	}

	// Now apply any pre-selections from the Frontend subsystem (after registration)
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
			{
				if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(BwayPS->GetPlayerController()))
				{
					PC->Client_RequestPreSelectedHero();
				}
			}
		}
	}

	// Bots do not have a local hero-selection UI, so assign and lock them immediately.
	AssignRandomHeroes(/*bOnlyBots=*/true, /*bLockImmediately=*/true);

	// Let Blueprint run cosmetic/audio/camera hooks, but always perform the C++
	// CommonUI push as well so overrides cannot swallow the client RPC.
	ShowHeroSelectionUI();
	if (!ShouldSuppressAutoHeroSelectUI())
	{
		PushHeroSelectionUIToPlayers();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Auto hero select UI suppressed for direct editor play"));
	}

	// Broadcast event
	OnHeroSelectionPhaseStarted.Broadcast();

	// Explicitly check if all players are already ready (e.g. all pre-selected from Frontend).
	// The delegate may have already fired during the lock calls above, but if not, check now.
	if (Manager->AreAllPlayersReady())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: All players pre-selected from Frontend — ending phase immediately"));
		HandleAllPlayersReady();
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Hero selection phase started"));
}

void UBwayHeroSelectionPhaseComponent::EndHeroSelectionPhase()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!bPhaseActive)
	{
		return;
	}

	bPhaseActive = false;
	bHeroSelectionCompleted = true;

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Ending hero selection phase"));

	// Get selection manager
	UBwayHeroSelectionManager* Manager = GetSelectionManager();
	if (Manager)
	{
		// Unbind from events
		Manager->OnAllPlayersReady.RemoveDynamic(this, &UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady);

		// Assign random/fallback heroes to anyone who didn't select
		AssignRandomHeroes(/*bOnlyBots=*/false, /*bLockImmediately=*/true);

		// End selection phase
		Manager->EndHeroSelection();
	}

	// Let Blueprint run cosmetic cleanup, then always remove the C++ CommonUI widget.
	HideHeroSelectionUI();
	PopHeroSelectionUIFromPlayers();

	if (!ShouldTravelToPostHeroSelectionMap())
	{
		if (const ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
		{
			if (UBwayRoundManagementComponent* RoundManagement = GameState->GetRoundManagement())
			{
				if (RoundManagement->IsOrchestratingMatchFlow())
				{
					UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: RM orchestrator active — deferring spawn to Warmup"));
				}
				else
				{
					SpawnHeroesForAllPlayers();
				}
			}
			else
			{
				SpawnHeroesForAllPlayers();
			}
		}
	}
	else
	{
		TryTravelToPostHeroSelectionMap();
	}

	// Broadcast event
	OnHeroSelectionPhaseEnded.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Hero selection phase ended"));
}

void UBwayHeroSelectionPhaseComponent::SkipHeroSelection()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Skipping hero selection"));

	bPhaseActive = false;
	bHeroSelectionCompleted = true;

	// Assign random/fallback heroes to everyone
	AssignRandomHeroes(/*bOnlyBots=*/false, /*bLockImmediately=*/true);

	// Spawn heroes immediately
	if (!ShouldTravelToPostHeroSelectionMap())
	{
		if (const ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
		{
			if (UBwayRoundManagementComponent* RoundManagement = GameState->GetRoundManagement())
			{
				if (RoundManagement->IsOrchestratingMatchFlow())
				{
					UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: RM orchestrator active — deferring spawn to Warmup"));
				}
				else
				{
					SpawnHeroesForAllPlayers();
				}
			}
			else
			{
				SpawnHeroesForAllPlayers();
			}
		}
	}
	else
	{
		TryTravelToPostHeroSelectionMap();
	}
}

void UBwayHeroSelectionPhaseComponent::ShowHeroSelectionUI_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: ShowHeroSelectionUI base impl (override in Blueprint)"));
}

void UBwayHeroSelectionPhaseComponent::PushHeroSelectionUIToPlayers()
{
	if (ShouldSuppressAutoHeroSelectUI())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Pushing hero selection UI to players"));

	// Base C++ implementation: ask each owning client to create its local widget.
	if (HeroSelectionWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: No HeroSelectionWidgetClass configured"));
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(It->Get()))
		{
			// Skip showing UI if this player already locked their selection (e.g. from Frontend)
			if (ABwayPlayerState* PS = PC->GetPlayerState<ABwayPlayerState>())
			{
				if (PS->IsHeroLocked())
				{
					continue;
				}
			}

			PC->Client_ShowHeroSelection(HeroSelectionWidgetClass);
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Sent hero select widget RPC to %s"), *PC->GetName());
		}
	}
}

void UBwayHeroSelectionPhaseComponent::HideHeroSelectionUI_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HideHeroSelectionUI base impl (override in Blueprint)"));
}

void UBwayHeroSelectionPhaseComponent::PopHeroSelectionUIFromPlayers()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Popping hero selection UI from players"));

	// Base C++ implementation: remove hero selection widgets and restore game input
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(It->Get()))
		{
			PC->Client_HideHeroSelection();
		}
	}
}

void UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated(const FGameplayTag& InPhaseTag)
{
	if (ShouldTravelToPostHeroSelectionMap())
	{
		UE_LOG(LogTemp, Verbose, TEXT("BwayHeroSelectionPhaseComponent: Ignoring Lyra hero-selection phase on staging map; experience load handles staging start"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HeroSelection phase activated by Lyra phase system! (Tag: %s)"), *InPhaseTag.ToString());
	
	// Now start our hero selection logic
	StartHeroSelectionPhase();
}

void UBwayHeroSelectionPhaseComponent::HandleDevDirectPlayExperienceLoaded(const ULyraExperienceDefinition* Experience)
{
	StartDevDirectPlayHeroSelection();
}

void UBwayHeroSelectionPhaseComponent::StartDevDirectPlayHeroSelection()
{
	if (!GetOwner()->HasAuthority() || ShouldSkipHeroSelectionPhase() || ShouldTravelToPostHeroSelectionMap() || bPhaseActive || bHeroSelectionCompleted)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Starting direct editor-play hero selection (auto UI)"));
	StartHeroSelectionPhase();
}

void UBwayHeroSelectionPhaseComponent::HandleExperienceLoaded(const ULyraExperienceDefinition* Experience)
{
	if (!GetOwner()->HasAuthority() || bPhaseActive || bHeroSelectionCompleted || !ShouldTravelToPostHeroSelectionMap())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Experience loaded; starting staging hero selection"));
	StartHeroSelectionPhase();
}

void UBwayHeroSelectionPhaseComponent::EndPhaseAndProgressToNext()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// In Lyra's phase system, starting a new sibling phase automatically ends the
	// current one (phases are nested by tag — sibling phases cancel each other).
	// So we start the next phase, which auto-cancels HeroSelection.
	TSubclassOf<ULyraGamePhaseAbility> PhaseAbilityClass = NextPhaseAbilityClass;
	if (!PhaseAbilityClass)
	{
		const TSoftClassPtr<ULyraGamePhaseAbility> FallbackPhaseAbilityClass(
			FSoftObjectPath(TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Warmup.BW_Phase_Warmup_C")));
		PhaseAbilityClass = FallbackPhaseAbilityClass.LoadSynchronous();
		if (PhaseAbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: NextPhaseAbilityClass is not set; using BW_Phase_Warmup fallback"));
		}
	}

	if (PhaseAbilityClass)
	{
		if (UWorld* World = GetWorld())
		{
			if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
			{
				PhaseSubsystem->StartPhase(PhaseAbilityClass);
				UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Started next phase via %s"),
					*PhaseAbilityClass->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: NextPhaseAbilityClass is not set and BW_Phase_Warmup fallback failed to load! Cannot progress beyond hero selection."));
	}
}

bool UBwayHeroSelectionPhaseComponent::ShouldTravelToPostHeroSelectionMap() const
{
	return UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this);
}

bool UBwayHeroSelectionPhaseComponent::TryTravelToPostHeroSelectionMap() const
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!World || !GameMode)
	{
		return false;
	}

	const FString& OptionsString = GameMode->OptionsString;
	if (!UGameplayStatics::HasOption(OptionsString, TEXT("HeroSelectStaging")))
	{
		return false;
	}

	FString TargetMap = UGameplayStatics::ParseOption(OptionsString, TEXT("HeroSelectTargetMap"));
	if (TargetMap.IsEmpty() && PostHeroSelectionMapID.IsValid())
	{
		if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
		{
			TargetMap = AssetManager->GetPrimaryAssetPath(PostHeroSelectionMapID).GetLongPackageName();
		}
	}

	if (TargetMap.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: HeroSelectStaging is set but no HeroSelectTargetMap or PostHeroSelectionMapID was provided"));
		return false;
	}

	FString TargetExperience = UGameplayStatics::ParseOption(OptionsString, TEXT("HeroSelectTargetExperience"));
	if (TargetExperience.IsEmpty() && PostHeroSelectionExperienceID.IsValid())
	{
		TargetExperience = PostHeroSelectionExperienceID.PrimaryAssetName.ToString();
	}

	FString TravelURL = TargetMap;
	if (!TargetExperience.IsEmpty())
	{
		TravelURL += FString::Printf(TEXT("?Experience=%s"), *TargetExperience);
	}

	if (UGameplayStatics::HasOption(OptionsString, TEXT("listen")))
	{
		TravelURL += TEXT("?listen");
	}

	TravelURL += TEXT("?SkipHeroSelection=1");
	TravelURL += TEXT("?SeamlessTravel");

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Hero selection complete; travelling to match URL %s"), *TravelURL);
	return World->ServerTravel(TravelURL, /*bAbsolute=*/true, /*bShouldSkipGameNotify=*/false);
}

bool UBwayHeroSelectionPhaseComponent::ShouldSkipHeroSelectionPhase() const
{
	return UBwayHeroSelectionFlowLibrary::ShouldSkipHeroSelectionWorld(this);
}

void UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: All players ready - ending phase"));

	const bool bWillTravelToMatchMap = ShouldTravelToPostHeroSelectionMap();

	// End our phase logic (assigns defaults, hides UI, spawns heroes)
	EndHeroSelectionPhase();
	if (bWillTravelToMatchMap)
	{
		return;
	}

	if (const ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		if (UBwayRoundManagementComponent* RoundManagement = GameState->GetRoundManagement())
		{
			if (RoundManagement->IsOrchestratingMatchFlow())
			{
				RoundManagement->TryCompletePrematchAfterHeroLocks();
				return;
			}
		}
	}

	// Progress Lyra's phase to the next one (e.g. Warmup / Playing)
	EndPhaseAndProgressToNext();
}

// ============================================================
// Internal Helpers
// ============================================================

void UBwayHeroSelectionPhaseComponent::SpawnHeroesForAllPlayers()
{
	ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	if (!GameState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No game state"));
		return;
	}

	// Spawn hero for each player
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
		{
			SpawnHeroForPlayer(BwayPS);
		}
	}
}

void UBwayHeroSelectionPhaseComponent::SpawnHeroForPlayer_Implementation(ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	// Get selected hero
	FPrimaryAssetId HeroId = PlayerState->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Player %s has no hero selected"), 
			*PlayerState->GetPlayerName());
		return;
	}

	// Load hero data
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
	if (!HeroData)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: Could not load hero data for %s"), 
			*HeroId.ToString());
		return;
	}

	// Get player controller
	APlayerController* PC = Cast<APlayerController>(PlayerState->GetOwner());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No controller for player %s"), 
			*PlayerState->GetPlayerName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Spawning hero %s for player %s"), 
		*HeroData->DisplayName.ToString(), *PlayerState->GetPlayerName());

	// Trigger a respawn through game mode (which calls ApplyHeroDataToNewPawn)
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		GameMode->RestartPlayer(PC);
	}
}

void UBwayHeroSelectionPhaseComponent::AssignDefaultHeroes(bool bOnlyBots)
{
	AssignRandomHeroes(bOnlyBots, /*bLockImmediately=*/true);
}

void UBwayHeroSelectionPhaseComponent::AssignRandomHeroes(bool bOnlyBots, bool bLockImmediately)
{
	if (UBwayHeroSelectionManager* Manager = GetSelectionManager())
	{
		const FPrimaryAssetId DirectPieBotHero = UBwayHeroSelectionFlowLibrary::ResolveDirectPieBotHeroId(this);
		if (bOnlyBots && DirectPieBotHero.IsValid())
		{
			if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
			{
				for (APlayerState* PS : GameState->PlayerArray)
				{
					ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
					if (!BwayPS)
					{
						continue;
					}

					AController* Controller = Cast<AController>(BwayPS->GetOwner());
					if (!Controller || !Controller->IsA<AAIController>())
					{
						continue;
					}

					if (!BwayPS->GetSelectedHeroId().IsValid() || !BwayPS->IsHeroLocked())
					{
						Manager->AssignHeroToPlayer(BwayPS, DirectPieBotHero, bLockImmediately);
					}
				}
			}
			return;
		}

		Manager->AssignRandomHeroToPlayers(bOnlyBots, ResolveFallbackHeroId(), bLockImmediately);
	}
}

FPrimaryAssetId UBwayHeroSelectionPhaseComponent::ResolveFallbackHeroId() const
{
	if (!DefaultHeroData.IsNull())
	{
		if (UBwayHeroDataAsset* DefaultHero = DefaultHeroData.LoadSynchronous())
		{
			return DefaultHero->GetPrimaryAssetId();
		}
	}

	if (UBwayHeroRegistry* HeroRegistry = UBwayHeroRegistry::Get(this))
	{
		for (const TSoftObjectPtr<UBwayHeroDataAsset>& HeroSoftObject : HeroRegistry->GetAllHeroSoftObjects())
		{
			if (UBwayHeroDataAsset* CandidateHero = HeroRegistry->LoadHeroSync(HeroSoftObject))
			{
				const FPrimaryAssetId CandidateId = CandidateHero->GetPrimaryAssetId();
				if (CandidateId.IsValid())
				{
					return CandidateId;
				}
			}
		}
	}

	return FPrimaryAssetId();
}

bool UBwayHeroSelectionPhaseComponent::ShouldSuppressAutoHeroSelectUI() const
{
	return false;
}

UBwayHeroSelectionManager* UBwayHeroSelectionPhaseComponent::GetSelectionManager() const
{
	ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	if (!GameState)
	{
		return nullptr;
	}

	return GameState->FindComponentByClass<UBwayHeroSelectionManager>();
}