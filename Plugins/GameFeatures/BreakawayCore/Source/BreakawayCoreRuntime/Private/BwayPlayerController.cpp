// Copyright Epic Games, Inc. All Rights Reserved.

#include "BwayPlayerController.h"
#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Development/BwayCheatManager.h"
#include "GameplayTagContainer.h"
#include "GameState/BwayFrontendStateSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayPlayerController)

ABwayPlayerController::ABwayPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if USING_CHEAT_MANAGER
	// Override the cheat manager with our Breakaway-specific one
	CheatClass = UBwayCheatManager::StaticClass();
#endif
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
		Server_SubmitPreSelectedHero(PreSelectedHeroId);
	}
}

void ABwayPlayerController::Server_SubmitPreSelectedHero_Implementation(FPrimaryAssetId PreSelectedHeroId)
{
	if (!PreSelectedHeroId.IsValid())
	{
		return;
	}

	if (ABwayPlayerState* BwayPS = GetPlayerState<ABwayPlayerState>())
	{
		if (!BwayPS->IsHeroLocked())
		{
			BwayPS->ServerSetSelectedHeroId(PreSelectedHeroId);
			BwayPS->ServerLockHeroSelection();
		}

		if (GetPawn())
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
}

void ABwayPlayerController::Client_ShowResults_Implementation(int32 WinningTeam, const TSoftClassPtr<UUserWidget>& WidgetClass)
{
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
	InputMode.SetWidgetToFocus(ResultsWidget->TakeWidget());
	SetInputMode(InputMode);

	UE_LOG(LogTemp, Log, TEXT("BwayPlayerController: Results screen shown for winning team %d"), WinningTeam + 1);
}

void ABwayPlayerController::Server_RequestReturnToFrontEnd_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (ABwayGameState* GS = World->GetGameState<ABwayGameState>())
		{
			GS->ReturnToFrontEnd();
		}
	}
}

