// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayMatchHUDWidgetBase.h"
#include "BwayGameState.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Relic/RelicActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchHUDWidgetBase)

ABwayGameState* UBwayMatchHUDWidgetBase::ResolveBwayGameState(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	if (UWorld* World = Widget->GetWorld())
	{
		return World->GetGameState<ABwayGameState>();
	}

	return nullptr;
}

FText UBwayMatchHUDWidgetBase::FormatRoundTime(const int32 TotalSeconds)
{
	const int32 Minutes = TotalSeconds / 60;
	const int32 Seconds = TotalSeconds % 60;

	return FText::Format(
		NSLOCTEXT("BwayMatchHUD", "TimeFormat", "{0}:{1}"),
		FText::AsNumber(Minutes),
		FText::Format(
			NSLOCTEXT("BwayMatchHUD", "SecondsFormat", "{0}"),
			FText::FromString(FString::Printf(TEXT("%02d"), Seconds))));
}

int32 UBwayMatchHUDWidgetBase::GetTeamScoreFromWorld(const UWorld* World, const int32 TeamIndex)
{
	if (!World || TeamIndex < 0)
	{
		return 0;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			return Scoring->GetTeamScore(TeamIndex);
		}
	}

	return 0;
}

int32 UBwayMatchHUDWidgetBase::GetRoundTimeRemainingFromWorld(const UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		return GameState->GetRoundTimeRemaining();
	}

	return 0;
}

int32 UBwayMatchHUDWidgetBase::GetRelicPossessingTeamFromWorld(const UWorld* World)
{
	if (!World)
	{
		return -1;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			return RelicMgr->GetRelicPossessingTeam();
		}
	}

	return -1;
}

bool UBwayMatchHUDWidgetBase::IsRelicCarriedInWorld(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			if (const ARelicActor* Relic = RelicMgr->GetRelicActor())
			{
				return Relic->GetCurrentState() == ERelicState::Carried;
			}
		}
	}

	return false;
}

FText UBwayMatchHUDWidgetBase::GetRelicCarrierNameFromWorld(const UWorld* World)
{
	if (!World)
	{
		return FText::GetEmpty();
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			if (const ARelicActor* Relic = RelicMgr->GetRelicActor())
			{
				if (const ABwayCharacterWithAbilities* Carrier = Relic->CurrentCarrier)
				{
					if (const APlayerState* CarrierPS = Carrier->GetPlayerState())
					{
						return FText::FromString(CarrierPS->GetPlayerName());
					}
				}
			}
		}
	}

	return FText::GetEmpty();
}

int32 UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForPlayerController(const APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return -1;
	}

	const APlayerState* PS = PlayerController->GetPlayerState<APlayerState>();
	if (!PS || PS->IsOnlyASpectator())
	{
		return -1;
	}

	if (const UWorld* World = PlayerController->GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			return GameState->GetPlayerTeam(PS);
		}
	}

	return -1;
}

int32 UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return -1;
	}

	return GetLocalPlayerTeamForPlayerController(Widget->GetOwningPlayer());
}

bool UBwayMatchHUDWidgetBase::ShouldSwapTeamsForDisplay(const int32 LocalPlayerTeamIndex)
{
	return LocalPlayerTeamIndex == 1;
}

int32 UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(const int32 GameTeamIndex, const int32 LocalPlayerTeamIndex)
{
	if (GameTeamIndex < 0)
	{
		return -1;
	}

	if (!ShouldSwapTeamsForDisplay(LocalPlayerTeamIndex))
	{
		return GameTeamIndex;
	}

	return GameTeamIndex == 0 ? 1 : 0;
}

int32 UBwayMatchHUDWidgetBase::MapDisplaySlotToGameTeam(const int32 DisplaySlotIndex, const int32 LocalPlayerTeamIndex)
{
	if (DisplaySlotIndex < 0 || DisplaySlotIndex > 1)
	{
		return -1;
	}

	if (!ShouldSwapTeamsForDisplay(LocalPlayerTeamIndex))
	{
		return DisplaySlotIndex;
	}

	return DisplaySlotIndex == 0 ? 1 : 0;
}

int32 UBwayMatchHUDWidgetBase::GetDisplayTeamScore(
	const UWorld* World,
	const int32 DisplaySlotIndex,
	const int32 LocalPlayerTeamIndex)
{
	const int32 GameTeamIndex = MapDisplaySlotToGameTeam(DisplaySlotIndex, LocalPlayerTeamIndex);
	return GetTeamScoreFromWorld(World, GameTeamIndex);
}

int32 UBwayMatchHUDWidgetBase::GetDisplayRelicPossessingTeam(const UWorld* World, const int32 LocalPlayerTeamIndex)
{
	const int32 GameTeamIndex = GetRelicPossessingTeamFromWorld(World);
	return MapGameTeamToDisplaySlot(GameTeamIndex, LocalPlayerTeamIndex);
}

int32 UBwayMatchHUDWidgetBase::GetCurrentRoundNumberFromWorld(const UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		return GameState->GetCurrentRoundNumber();
	}

	return 0;
}

void UBwayMatchHUDWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	BindToMatchData();
}

void UBwayMatchHUDWidgetBase::NativeDestruct()
{
	UnbindFromMatchData();
	Super::NativeDestruct();
}

ABwayGameState* UBwayMatchHUDWidgetBase::GetBwayGameState() const
{
	if (CachedGameState.IsValid())
	{
		return CachedGameState.Get();
	}

	return ResolveBwayGameState(this);
}

UBwayScoringComponent* UBwayMatchHUDWidgetBase::GetScoringComponent() const
{
	if (CachedScoringComponent.IsValid())
	{
		return CachedScoringComponent.Get();
	}

	if (const ABwayGameState* GameState = GetBwayGameState())
	{
		return GameState->FindComponentByClass<UBwayScoringComponent>();
	}

	return nullptr;
}

UBwayRelicManagerComponent* UBwayMatchHUDWidgetBase::GetRelicManagerComponent() const
{
	if (const ABwayGameState* GameState = GetBwayGameState())
	{
		return GameState->FindComponentByClass<UBwayRelicManagerComponent>();
	}

	return nullptr;
}

void UBwayMatchHUDWidgetBase::BindToMatchData()
{
	if (bBoundToMatchData)
	{
		return;
	}

	if (ABwayGameState* GameState = ResolveBwayGameState(this))
	{
		CachedGameState = GameState;

		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			CachedScoringComponent = Scoring;
			Scoring->OnTeamScoreChanged.AddDynamic(this, &UBwayMatchHUDWidgetBase::HandleTeamScoreChanged);
		}

		GameState->OnRoundTimeChanged.AddDynamic(this, &UBwayMatchHUDWidgetBase::HandleRoundTimeChanged);
		GameState->OnRoundStateChanged.AddDynamic(this, &UBwayMatchHUDWidgetBase::HandleRoundStateChanged);
		bBoundToMatchData = true;
	}
}

void UBwayMatchHUDWidgetBase::UnbindFromMatchData()
{
	if (!bBoundToMatchData)
	{
		return;
	}

	if (CachedScoringComponent.IsValid())
	{
		CachedScoringComponent->OnTeamScoreChanged.RemoveDynamic(this, &UBwayMatchHUDWidgetBase::HandleTeamScoreChanged);
	}

	if (CachedGameState.IsValid())
	{
		CachedGameState->OnRoundTimeChanged.RemoveDynamic(this, &UBwayMatchHUDWidgetBase::HandleRoundTimeChanged);
		CachedGameState->OnRoundStateChanged.RemoveDynamic(this, &UBwayMatchHUDWidgetBase::HandleRoundStateChanged);
	}

	CachedScoringComponent.Reset();
	CachedGameState.Reset();
	bBoundToMatchData = false;
}

void UBwayMatchHUDWidgetBase::HandleTeamScoreChanged(int32 TeamIndex, int32 NewScore)
{
	NotifyTeamScoreChanged(TeamIndex, NewScore);
}

void UBwayMatchHUDWidgetBase::HandleRoundTimeChanged(int32 SecondsRemaining)
{
	NotifyRoundTimeChanged(SecondsRemaining);
}

void UBwayMatchHUDWidgetBase::HandleRoundStateChanged(FName NewState)
{
	NotifyRoundStateChanged(NewState);
}
