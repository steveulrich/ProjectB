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
	}
	BoundRosterGameState.Reset();
	ClearPlayerEventBindings();
	SlotWidgets.Reset();
	Super::NativeDestruct();
}

void UBwayTeamPortraitsHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (PortraitRefreshInterval > 0.0f)
	{
		TimeSinceLastPortraitRefresh += InDeltaTime;
		if (TimeSinceLastPortraitRefresh >= PortraitRefreshInterval)
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
		ClampedDisplaySlot == 0 ? FriendlyFrameColor : EnemyFrameColor;

	for (int32 SlotIndex = 0; SlotIndex < SlotWidgets.Num(); ++SlotIndex)
	{
		UBwayTeamPortraitSlotWidget* SlotWidget = SlotWidgets[SlotIndex];
		if (!SlotWidget)
		{
			continue;
		}

		FBwayTeamPortraitSlotViewModel ViewModel;
		ViewModel.SlotIndex = SlotIndex;

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
			ViewModel.FrameColor = OccupiedFrameColor;
		}
		else
		{
			ViewModel.bIsOccupied = false;
			ViewModel.PortraitTexture = EmptyPortraitTexture;
			ViewModel.bIsAlive = true;
			ViewModel.bHasRelic = false;
			ViewModel.FrameColor = EmptyFrameColor;
		}

		SlotWidget->SetSlotViewModel(ViewModel);
	}

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
	}

	BoundRosterGameState = GameState;
	if (GameState)
	{
		GameState->OnTeamsUpdated.AddUniqueDynamic(this, &ThisClass::HandleTeamsUpdated);
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
