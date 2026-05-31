// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayMidfieldRulesLibrary.h"
#include "BwayGameState.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "Relic/RelicActor.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMidfieldRulesLibrary)

int32 UBwayMidfieldRulesLibrary::GetRelicHalfFromLocation(const FVector RelicWorldLocation, const float MidfieldTolerance)
{
	const float SafeTolerance = FMath::Max(0.0f, MidfieldTolerance);

	if (RelicWorldLocation.X < -SafeTolerance)
	{
		// Team 1 (index 0) half — negative X from midfield origin
		return 0;
	}

	if (RelicWorldLocation.X > SafeTolerance)
	{
		// Team 2 (index 1) half — positive X
		return 1;
	}

	return -1;
}

int32 UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromLocation(const FVector RelicWorldLocation, const float MidfieldTolerance)
{
	return GetRelicHalfFromLocation(RelicWorldLocation, MidfieldTolerance);
}

int32 UBwayMidfieldRulesLibrary::GetRelicHalfFromRelic(const ARelicActor* Relic, const float MidfieldTolerance)
{
	if (!Relic)
	{
		return -1;
	}

	const int32 HalfFromPosition = GetRelicHalfFromLocation(Relic->GetActorLocation(), MidfieldTolerance);
	if (HalfFromPosition >= 0)
	{
		return HalfFromPosition;
	}

	// Exactly on the midfield plane — fall back to last team that possessed the relic.
	if (Relic->LastPossessingTeam >= 0)
	{
		return Relic->LastPossessingTeam;
	}

	return -1;
}

int32 UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(const ARelicActor* Relic, const float MidfieldTolerance)
{
	return GetRelicHalfFromRelic(Relic, MidfieldTolerance);
}

static ARelicActor* GetActiveRelicFromWorld(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	const ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	if (!GameState)
	{
		return nullptr;
	}

	const UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>();
	return RelicMgr ? RelicMgr->GetRelicActor() : nullptr;
}

int32 UBwayMidfieldRulesLibrary::GetRelicHalfFromWorld(const UObject* WorldContextObject, const float MidfieldTolerance)
{
	return GetRelicHalfFromRelic(GetActiveRelicFromWorld(WorldContextObject), MidfieldTolerance);
}

int32 UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromWorld(const UObject* WorldContextObject, const float MidfieldTolerance)
{
	return GetLosingTeamAtMidfieldFromRelic(GetActiveRelicFromWorld(WorldContextObject), MidfieldTolerance);
}
