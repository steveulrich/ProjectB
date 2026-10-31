// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayTeamPortraitsHUDWidget.h"

#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "LyraGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayTeamPortraitsHUDWidget)

#define LOCTEXT_NAMESPACE "BwayTeamPortraitsHUDWidget"

UBwayTeamPortraitsHUDWidget::UBwayTeamPortraitsHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FriendlyTeamLabel = LOCTEXT("FriendlyTeam", "FRIENDLY TEAM");
	EnemyTeamLabel = LOCTEXT("EnemyTeam", "ENEMY TEAM");
}

void UBwayTeamPortraitsHUDWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyTeamLabel();
}

void UBwayTeamPortraitsHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildSlotWidgets();
	BindToRosterEvents();
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::NativeDestruct()
{
	if (BoundRosterGameState.IsValid())
	{
		BoundRosterGameState->OnTeamsUpdated.RemoveDynamic(this, &ThisClass::HandleTeamsUpdated);
		if (UBwayRoundManagementComponent* RoundMgmt = BoundRosterGameState->GetRoundManagement())
		{
			RoundMgmt->OnMatchPhaseChanged.RemoveDynamic(this, &ThisClass::HandleMatchPhaseChanged);
		}
	}
	BoundRosterGameState.Reset();
	ClearPlayerEventBindings();
	SlotWidgets.Reset();
	Super::NativeDestruct();
}

void UBwayTeamPortraitsHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float ClampedPortraitInterval = (PortraitRefreshInterval > 0.0f) ? FMath::Min(PortraitRefreshInterval, 0.15f) : 0.0f;
	const float ActiveInterval = bAnySlotWaitingToRespawn ? RespawnCountdownRefreshInterval : ClampedPortraitInterval;
	if (ActiveInterval > 0.0f)
	{
		TimeSinceLastPortraitRefresh += InDeltaTime;
		if (TimeSinceLastPortraitRefresh >= ActiveInterval)
		{
			TimeSinceLastPortraitRefresh = 0.0f;
			RefreshTeamPortraits();
		}
	}
}

void UBwayTeamPortraitsHUDWidget::NotifyRoundStateChanged(FName NewState)
{
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::RebuildSlotWidgets()
{
	SlotWidgets.Reset();

	if (!TeamPortraitsRow || !PortraitSlotWidgetClass)
	{
		return;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer)
	{
		return;
	}

	const int32 SlotCount = FMath::Clamp(PortraitSlotCount, 1, 4);
	TeamPortraitsRow->ClearChildren();
	SlotWidgets.Reserve(SlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UBwayTeamPortraitSlotWidget* SlotWidget =
			CreateWidget<UBwayTeamPortraitSlotWidget>(OwningPlayer, PortraitSlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		if (UHorizontalBoxSlot* HorizontalSlot = TeamPortraitsRow->AddChildToHorizontalBox(SlotWidget))
		{
			HorizontalSlot->SetPadding(PortraitSlotPadding);
		}

		SlotWidgets.Add(SlotWidget);
	}
}

void UBwayTeamPortraitsHUDWidget::ApplyTeamLabel()
{
	if (!Text_TeamLabel)
	{
		return;
	}

	Text_TeamLabel->SetText(DisplaySlotIndex == 0 ? FriendlyTeamLabel : EnemyTeamLabel);
	Text_TeamLabel->SetVisibility(ESlateVisibility::Collapsed);
}

void UBwayTeamPortraitsHUDWidget::RefreshTeamPortraits()
{
	BindToRosterEvents();
	ApplyTeamLabel();

	if (SlotWidgets.Num() == 0)
	{
		RebuildSlotWidgets();
	}

	const int32 LocalTeam = GetLocalPlayerTeamForWidget(this);
	const int32 ClampedDisplaySlot = FMath::Clamp(DisplaySlotIndex, 0, 1);

	TArray<FTeamPlayerHUDData> TeamData;
	UBwayHUDHelpers::GetTeamPlayerHUDDataForDisplaySlot(this, ClampedDisplaySlot, LocalTeam, TeamData);

	const FLinearColor OccupiedFrameColor =
		ClampedDisplaySlot == 0
			? FLinearColor(0.09f, 0.84f, 0.92f, 1.0f)
			: FLinearColor(0.89f, 0.22f, 0.20f, 1.0f);
	const bool bIsFriendlySlot = (ClampedDisplaySlot == 0);

	ABwayGameState* GameState = GetBwayGameState();
	const UBwayRoundManagementComponent* RoundMgmt = GameState ? GameState->GetRoundManagement() : nullptr;
	const double ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	const bool bRoundRespawnActive = !RoundMgmt || (
		(!RoundMgmt->IsOrchestratingMatchFlow() || RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::Playing)
		&& RoundMgmt->GetCurrentRoundState() == ERoundState::RoundActive);
	const int32 ActiveRoundNumber = RoundMgmt ? RoundMgmt->GetCurrentRoundNumber() : 0;

	bool bFoundWaitingSlot = false;

	for (int32 SlotIndex = 0; SlotIndex < SlotWidgets.Num(); ++SlotIndex)
	{
		UBwayTeamPortraitSlotWidget* SlotWidget = SlotWidgets[SlotIndex];
		if (!SlotWidget)
		{
			continue;
		}

		FBwayTeamPortraitSlotViewModel ViewModel;
		ViewModel.SlotIndex = SlotIndex;
		ViewModel.bIsFriendly = bIsFriendlySlot;

		if (TeamData.IsValidIndex(SlotIndex))
		{
			const FTeamPlayerHUDData& Data = TeamData[SlotIndex];
			ViewModel.bIsOccupied = true;
			ViewModel.PlayerName = Data.PlayerName;
			ViewModel.PortraitTexture = Data.PortraitTexture
				? Data.PortraitTexture.Get()
				: EmptyPortraitTexture.Get();
			ViewModel.bIsAlive = Data.bIsAlive;
			ViewModel.bHasRelic = Data.bHasRelic;
			ViewModel.HealthPercent = Data.bIsAlive ? FMath::Clamp(Data.HealthPercent, 0.0f, 1.0f) : 0.0f;
			ViewModel.FrameColor = OccupiedFrameColor;

			if (!Data.bIsAlive && Data.PlayerState && bRoundRespawnActive)
			{
				const FBwayRespawnSchedule Schedule = Data.PlayerState->GetRespawnSchedule();
				const bool bMatchesCurrentRound = (ActiveRoundNumber <= 0 || Schedule.RoundNumber == ActiveRoundNumber);
				const int32 RemainingSeconds = bMatchesCurrentRound ? Schedule.GetRemainingSeconds(ServerTime) : 0;
				if (Schedule.bIsWaitingToRespawn && bMatchesCurrentRound && RemainingSeconds > 0)
				{
					ViewModel.bIsWaitingToRespawn = true;
					ViewModel.RespawnRemainingSeconds = RemainingSeconds;
					bFoundWaitingSlot = true;
				}
			}
		}
		else
		{
			ViewModel.bIsOccupied = false;
			ViewModel.PortraitTexture = EmptyPortraitTexture;
			ViewModel.bIsAlive = true;
			ViewModel.bHasRelic = false;
			ViewModel.HealthPercent = 0.0f;
			ViewModel.bIsWaitingToRespawn = false;
			ViewModel.RespawnRemainingSeconds = 0;
			ViewModel.FrameColor = EmptyFrameColor;
		}

		SlotWidget->SetSlotViewModel(ViewModel);
	}

	bAnySlotWaitingToRespawn = bFoundWaitingSlot;
	RefreshPlayerEventBindings(TeamData);
}

void UBwayTeamPortraitsHUDWidget::BindToRosterEvents()
{
	ABwayGameState* GameState = GetBwayGameState();
	if (BoundRosterGameState.Get() == GameState)
	{
		return;
	}

	if (BoundRosterGameState.IsValid())
	{
		BoundRosterGameState->OnTeamsUpdated.RemoveDynamic(this, &ThisClass::HandleTeamsUpdated);
		if (UBwayRoundManagementComponent* OldRoundMgmt = BoundRosterGameState->GetRoundManagement())
		{
			OldRoundMgmt->OnMatchPhaseChanged.RemoveDynamic(this, &ThisClass::HandleMatchPhaseChanged);
		}
	}

	BoundRosterGameState = GameState;
	if (GameState)
	{
		GameState->OnTeamsUpdated.AddUniqueDynamic(this, &ThisClass::HandleTeamsUpdated);
		if (UBwayRoundManagementComponent* RoundMgmt = GameState->GetRoundManagement())
		{
			RoundMgmt->OnMatchPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchPhaseChanged);
		}
	}
}

void UBwayTeamPortraitsHUDWidget::RefreshPlayerEventBindings(const TArray<FTeamPlayerHUDData>& TeamData)
{
	TArray<ABwayPlayerState*> DesiredPlayerStates;
	DesiredPlayerStates.Reserve(TeamData.Num());
	for (const FTeamPlayerHUDData& Data : TeamData)
	{
		if (Data.PlayerState)
		{
			DesiredPlayerStates.AddUnique(Data.PlayerState.Get());
		}
	}

	bool bBindingsMatch = DesiredPlayerStates.Num() == BoundPortraitPlayers.Num();
	if (bBindingsMatch)
	{
		for (int32 Index = 0; Index < DesiredPlayerStates.Num(); ++Index)
		{
			if (BoundPortraitPlayers[Index].PlayerState.Get() != DesiredPlayerStates[Index])
			{
				bBindingsMatch = false;
				break;
			}
		}
	}

	if (bBindingsMatch)
	{
		return;
	}

	ClearPlayerEventBindings();
	BoundPortraitPlayers.Reserve(DesiredPlayerStates.Num());

	for (ABwayPlayerState* PlayerState : DesiredPlayerStates)
	{
		if (!PlayerState)
		{
			continue;
		}

		PlayerState->OnSelectedHeroChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandlePortraitHeroChanged);
		PlayerState->OnRelicPossessionChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandlePortraitRelicChanged);
		PlayerState->OnPlayerNumAssigned.AddUniqueDynamic(
			this,
			&ThisClass::HandlePortraitPlayerNumChanged);
		PlayerState->OnRespawnScheduleChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandlePortraitRespawnScheduleChanged);

		FBoundPortraitPlayer& Binding = BoundPortraitPlayers.AddDefaulted_GetRef();
		Binding.PlayerState = PlayerState;
		Binding.AbilitySystemComponent = PlayerState->GetAbilitySystemComponent();
		if (Binding.AbilitySystemComponent.IsValid())
		{
			Binding.DeathTagChangedHandle =
				Binding.AbilitySystemComponent->RegisterGameplayTagEvent(
					LyraGameplayTags::Status_Death,
					EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &ThisClass::HandleDeathTagChanged);
		}
	}
}

void UBwayTeamPortraitsHUDWidget::ClearPlayerEventBindings()
{
	for (FBoundPortraitPlayer& Binding : BoundPortraitPlayers)
	{
		if (Binding.PlayerState.IsValid())
		{
			Binding.PlayerState->OnSelectedHeroChanged.RemoveDynamic(
				this,
				&ThisClass::HandlePortraitHeroChanged);
			Binding.PlayerState->OnRelicPossessionChanged.RemoveDynamic(
				this,
				&ThisClass::HandlePortraitRelicChanged);
			Binding.PlayerState->OnPlayerNumAssigned.RemoveDynamic(
				this,
				&ThisClass::HandlePortraitPlayerNumChanged);
			Binding.PlayerState->OnRespawnScheduleChanged.RemoveDynamic(
				this,
				&ThisClass::HandlePortraitRespawnScheduleChanged);
		}

		if (Binding.AbilitySystemComponent.IsValid() && Binding.DeathTagChangedHandle.IsValid())
		{
			Binding.AbilitySystemComponent->UnregisterGameplayTagEvent(
				Binding.DeathTagChangedHandle,
				LyraGameplayTags::Status_Death,
				EGameplayTagEventType::NewOrRemoved);
		}
	}

	BoundPortraitPlayers.Reset();
}

void UBwayTeamPortraitsHUDWidget::HandlePortraitHeroChanged(FPrimaryAssetId NewHeroId)
{
	(void)NewHeroId;
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::HandlePortraitRelicChanged(bool bHasRelic)
{
	(void)bHasRelic;
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::HandlePortraitPlayerNumChanged(int32 PlayerNum)
{
	(void)PlayerNum;
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::HandlePortraitRespawnScheduleChanged()
{
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::HandleMatchPhaseChanged(EBwayMatchPhase NewPhase)
{
	(void)NewPhase;
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::HandleTeamsUpdated()
{
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::HandleDeathTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
	(void)ChangedTag;
	(void)NewCount;
	RefreshTeamPortraits();
}

#undef LOCTEXT_NAMESPACE
