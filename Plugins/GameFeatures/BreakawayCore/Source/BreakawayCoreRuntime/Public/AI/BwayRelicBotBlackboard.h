// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Blackboard key names for BT_BW_RelicBot — must match BB_BW_RelicBot asset keys exactly. */
namespace BwayRelicBotBlackboard
{
	inline const FName RelicLocation(TEXT("RelicLocation"));
	inline const FName EnemyGoalLocation(TEXT("EnemyGoalLocation"));
	inline const FName RelicCarrier(TEXT("RelicCarrier"));
	inline const FName bSuddenDeathActive(TEXT("bSuddenDeathActive"));
	inline const FName bIsRelicCarrier(TEXT("bIsRelicCarrier"));
	inline const FName bEnemyHasRelic(TEXT("bEnemyHasRelic"));
	inline const FName bRelicAvailable(TEXT("bRelicAvailable"));
	inline const FName MyTeamIndex(TEXT("MyTeamIndex"));
}
