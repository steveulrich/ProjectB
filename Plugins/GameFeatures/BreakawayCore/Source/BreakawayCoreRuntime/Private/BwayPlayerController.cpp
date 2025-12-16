// Copyright Epic Games, Inc. All Rights Reserved.

#include "BwayPlayerController.h"
#include "Development/BwayCheatManager.h"

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

