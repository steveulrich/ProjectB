// Copyright Epic Games, Inc. All Rights Reserved.

#include "Development/BwayCheatManager.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "BreakawayGameMode.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "Player/LyraPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCheatManager)

DEFINE_LOG_CATEGORY_STATIC(LogBwayCheat, Log, All);

UBwayCheatManager::UBwayCheatManager()
{
	// Default widget class - can be overridden in Blueprint
	// Will be loaded from soft reference when needed
}

void UBwayCheatManager::ShowHeroSelect()
{
#if USING_CHEAT_MANAGER
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		UE_LOG(LogBwayCheat, Warning, TEXT("ShowHeroSelect: No valid player controller"));
		return;
	}

	// If widget already exists and is valid, just make sure it's visible
	if (IsValid(HeroSelectWidgetInstance))
	{
		if (!HeroSelectWidgetInstance->IsInViewport())
		{
			HeroSelectWidgetInstance->AddToViewport(100);
		}
		
		// Set input mode to UI
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(HeroSelectWidgetInstance->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
		
		UE_LOG(LogBwayCheat, Display, TEXT("ShowHeroSelect: Widget already exists, showing it"));
		CheatOutputText(TEXT("Hero selection UI opened"));
		return;
	}

	// Try to load and create the widget
	UClass* WidgetClass = nullptr;
	
	// First try our configured class
	if (!HeroSelectionWidgetClass.IsNull())
	{
		WidgetClass = HeroSelectionWidgetClass.LoadSynchronous();
	}
	
	// Fallback: Try to find the widget in the content
	if (!WidgetClass)
	{
		// Try loading the default hero select widget
		FSoftClassPath WidgetPath(TEXT("/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect.WBP_BW_HeroSelect_C"));
		WidgetClass = WidgetPath.TryLoadClass<UUserWidget>();
	}
	
	if (!WidgetClass)
	{
		UE_LOG(LogBwayCheat, Error, TEXT("ShowHeroSelect: Could not find hero selection widget class"));
		CheatOutputText(TEXT("Error: Hero selection widget not found. Create WBP_BW_HeroSelect in BreakawayCore/UI/HeroSelect/"));
		return;
	}

	// Create the widget
	HeroSelectWidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!HeroSelectWidgetInstance)
	{
		UE_LOG(LogBwayCheat, Error, TEXT("ShowHeroSelect: Failed to create widget instance"));
		CheatOutputText(TEXT("Error: Failed to create hero selection widget"));
		return;
	}

	// If it's our custom hero select widget, bind to its events
	if (UBwayHeroSelectWidget* HeroWidget = Cast<UBwayHeroSelectWidget>(HeroSelectWidgetInstance))
	{
		HeroWidget->OnHeroConfirmed.AddDynamic(this, &UBwayCheatManager::OnHeroSelectionConfirmed);
	}

	// Add to viewport
	HeroSelectWidgetInstance->AddToViewport(100);

	// Set input mode to UI
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(HeroSelectWidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);

	UE_LOG(LogBwayCheat, Display, TEXT("ShowHeroSelect: Widget created and displayed"));
	CheatOutputText(TEXT("Hero selection UI opened"));
#endif
}

void UBwayCheatManager::HideHeroSelect()
{
#if USING_CHEAT_MANAGER
	APlayerController* PC = GetOwningPlayerController();
	
	if (IsValid(HeroSelectWidgetInstance))
	{
		HeroSelectWidgetInstance->RemoveFromParent();
		HeroSelectWidgetInstance = nullptr;
		
		// Restore game input mode
		if (PC)
		{
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->SetShowMouseCursor(false);
		}
		
		UE_LOG(LogBwayCheat, Display, TEXT("HideHeroSelect: Widget removed"));
		CheatOutputText(TEXT("Hero selection UI closed"));
	}
#endif
}

void UBwayCheatManager::SelectHero(FString HeroName)
{
#if USING_CHEAT_MANAGER
	if (HeroName.IsEmpty())
	{
		CheatOutputText(TEXT("Usage: SelectHero <HeroName>"));
		CheatOutputText(TEXT("Use 'ListHeroes' to see available heroes"));
		return;
	}

	UBwayHeroDataAsset* HeroData = FindHeroByName(HeroName);
	if (!HeroData)
	{
		CheatOutputText(FString::Printf(TEXT("Hero '%s' not found. Use 'ListHeroes' to see available heroes."), *HeroName));
		return;
	}

	// Get player state and set the hero
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		UE_LOG(LogBwayCheat, Warning, TEXT("SelectHero: No valid player controller"));
		return;
	}

	ABwayPlayerState* BwayPS = PC->GetPlayerState<ABwayPlayerState>();
	if (!BwayPS)
	{
		UE_LOG(LogBwayCheat, Warning, TEXT("SelectHero: Player state is not ABwayPlayerState"));
		CheatOutputText(TEXT("Error: Invalid player state"));
		return;
	}

	// Get the hero's primary asset ID
	FPrimaryAssetId HeroId = HeroData->GetPrimaryAssetId();
	if (!HeroId.IsValid())
	{
		// Construct the asset ID manually if not set
		HeroId = FPrimaryAssetId(FPrimaryAssetType("BwayHeroDataAsset"), HeroData->GetFName());
	}

	// Call the server RPC to set the hero
	BwayPS->ServerSetSelectedHeroId(HeroId);
	
	CheatOutputText(FString::Printf(TEXT("Selected hero: %s"), *HeroData->DisplayName.ToString()));
	UE_LOG(LogBwayCheat, Display, TEXT("SelectHero: Set hero to %s (ID: %s)"), 
		*HeroData->DisplayName.ToString(), *HeroId.ToString());
#endif
}

void UBwayCheatManager::RespawnAsHero()
{
#if USING_CHEAT_MANAGER
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		UE_LOG(LogBwayCheat, Warning, TEXT("RespawnAsHero: No valid player controller"));
		return;
	}

	// Check if we're on the server or need to send an RPC
	if (PC->GetNetMode() == NM_Client)
	{
		// Send cheat to server
		if (ALyraPlayerController* LyraPC = Cast<ALyraPlayerController>(PC))
		{
			LyraPC->ServerCheat(TEXT("RespawnAsHero"));
		}
		CheatOutputText(TEXT("Requesting respawn from server..."));
		return;
	}

	ABwayPlayerState* BwayPS = PC->GetPlayerState<ABwayPlayerState>();
	if (!BwayPS)
	{
		CheatOutputText(TEXT("Error: Invalid player state"));
		return;
	}

	// Check if player has a hero selected
	FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		CheatOutputText(TEXT("Error: No hero selected. Use 'SelectHero <name>' first."));
		return;
	}

	// Get the game mode to trigger respawn
	AGameModeBase* GameMode = UGameplayStatics::GetGameMode(this);
	if (!GameMode)
	{
		CheatOutputText(TEXT("Error: No game mode found"));
		return;
	}

	// Destroy current pawn
	if (APawn* CurrentPawn = PC->GetPawn())
	{
		CurrentPawn->Destroy();
	}

	// Restart player - this will spawn them with their selected hero
	GameMode->RestartPlayer(PC);
	
	// Get hero name for feedback
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
	FString HeroName = HeroData ? HeroData->DisplayName.ToString() : HeroId.ToString();
	
	CheatOutputText(FString::Printf(TEXT("Respawning as %s..."), *HeroName));
	UE_LOG(LogBwayCheat, Display, TEXT("RespawnAsHero: Respawning player as %s"), *HeroName);
#endif
}

void UBwayCheatManager::ListHeroes()
{
#if USING_CHEAT_MANAGER
	CheatOutputText(TEXT("=== Available Heroes ==="));
	
	// Get hero registry
	UBwayHeroRegistry* Registry = UBwayHeroRegistry::Get(this);
	if (!Registry)
	{
		CheatOutputText(TEXT("Error: Hero registry not available"));
		return;
	}

	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> Heroes = Registry->GetAllHeroSoftObjects();
	
	if (Heroes.Num() == 0)
	{
		CheatOutputText(TEXT("No heroes found. Create BwayHeroDataAsset assets in your content."));
		return;
	}

	int32 Index = 1;
	for (const TSoftObjectPtr<UBwayHeroDataAsset>& SoftHero : Heroes)
	{
		// Try to load the hero to get its display name
		if (UBwayHeroDataAsset* Hero = Registry->LoadHeroSync(SoftHero))
		{
			CheatOutputText(FString::Printf(TEXT("  %d. %s"), Index, *Hero->DisplayName.ToString()));
		}
		else
		{
			// Show asset name if we can't load it
			CheatOutputText(FString::Printf(TEXT("  %d. %s (not loaded)"), Index, *SoftHero.GetAssetName()));
		}
		Index++;
	}
	
	CheatOutputText(TEXT("========================"));
	CheatOutputText(TEXT("Use 'SelectHero <name>' to select a hero"));
	CheatOutputText(TEXT("Use 'ChangeHero <name>' to select and respawn"));
#endif
}

void UBwayCheatManager::ChangeHero(FString HeroName)
{
#if USING_CHEAT_MANAGER
	// First select the hero
	SelectHero(HeroName);
	
	// Then respawn
	RespawnAsHero();
#endif
}

void UBwayCheatManager::ToggleHeroSelect()
{
#if USING_CHEAT_MANAGER
	if (IsHeroSelectVisible())
	{
		HideHeroSelect();
	}
	else
	{
		ShowHeroSelect();
	}
#endif
}

bool UBwayCheatManager::IsHeroSelectVisible() const
{
	return IsValid(HeroSelectWidgetInstance) && HeroSelectWidgetInstance->IsInViewport();
}

void UBwayCheatManager::OnHeroSelectionConfirmed()
{
	// Close the widget
	HideHeroSelect();
	
	// Respawn as the selected hero
	RespawnAsHero();
}

UBwayHeroDataAsset* UBwayCheatManager::FindHeroByName(const FString& HeroName) const
{
	UBwayHeroRegistry* Registry = UBwayHeroRegistry::Get(this);
	if (!Registry)
	{
		return nullptr;
	}

	FString SearchName = HeroName.ToLower();
	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> Heroes = Registry->GetAllHeroSoftObjects();
	
	for (const TSoftObjectPtr<UBwayHeroDataAsset>& SoftHero : Heroes)
	{
		if (UBwayHeroDataAsset* Hero = Registry->LoadHeroSync(SoftHero))
		{
			// Check display name (case-insensitive partial match)
			if (Hero->DisplayName.ToString().ToLower().Contains(SearchName))
			{
				return Hero;
			}
			
			// Also check asset name
			if (SoftHero.GetAssetName().ToLower().Contains(SearchName))
			{
				return Hero;
			}
		}
	}
	
	return nullptr;
}

APlayerController* UBwayCheatManager::GetOwningPlayerController() const
{
	return GetOuterAPlayerController();
}

