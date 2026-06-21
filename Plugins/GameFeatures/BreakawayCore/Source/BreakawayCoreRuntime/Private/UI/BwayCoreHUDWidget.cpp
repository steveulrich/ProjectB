// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCoreHUDWidget.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "UI/BwayHUDHelpers.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "GameState/BwayScoringComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCoreHUDWidget)

UBwayCoreHUDWidget::UBwayCoreHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayCoreHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	LastRelicPossessingTeam = GetRelicPossessingTeam();
	bLastRelicCarried = IsRelicCarried();

	BindToGameState();
	BindToHealthComponent();
	RefreshAllDisplay();
}

void UBwayCoreHUDWidget::NativeDestruct()
{
	UnbindFromGameState();
	
	if (CachedHealthComponent.IsValid() && bBoundToHealth)
	{
		CachedHealthComponent->OnHealthChanged.RemoveDynamic(this, &UBwayCoreHUDWidget::HandleHealthChanged);
		bBoundToHealth = false;
	}

	Super::NativeDestruct();
}

void UBwayCoreHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Re-bind to health component if needed (e.g., after respawn)
	if (!bBoundToHealth || !CachedHealthComponent.IsValid())
	{
		BindToHealthComponent();
	}

	// Poll relic possession (replicated state; no client-safe delegate today)
	if (RelicPollInterval > 0.0f)
	{
		TimeSinceLastRelicPoll += InDeltaTime;
		if (TimeSinceLastRelicPoll >= RelicPollInterval)
		{
			TimeSinceLastRelicPoll = 0.0f;

			const int32 TeamIndex = GetRelicPossessingTeam();
			const bool bIsCarried = IsRelicCarried();
			if (TeamIndex != LastRelicPossessingTeam || bIsCarried != bLastRelicCarried)
			{
				LastRelicPossessingTeam = TeamIndex;
				bLastRelicCarried = bIsCarried;
				RefreshRelicDisplay(TeamIndex, bIsCarried);
				OnRelicPossessionChanged(TeamIndex, bIsCarried);
			}
		}
	}

	// Client timer fallback (OnRoundTimeChanged is authority-only)
	if (TimerPollInterval > 0.0f)
	{
		TimeSinceLastTimerPoll += InDeltaTime;
		if (TimeSinceLastTimerPoll >= TimerPollInterval)
		{
			TimeSinceLastTimerPoll = 0.0f;
			RefreshTimerDisplay();
		}
	}

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

// ========== DATA ACCESS ==========

float UBwayCoreHUDWidget::GetHealthPercent() const
{
	if (ULyraHealthComponent* HealthComp = GetLocalPlayerHealthComponent())
	{
		return HealthComp->GetHealthNormalized();
	}
	return 0.0f;
}

float UBwayCoreHUDWidget::GetCurrentHealth() const
{
	if (ULyraHealthComponent* HealthComp = GetLocalPlayerHealthComponent())
	{
		return HealthComp->GetHealth();
	}
	return 0.0f;
}

float UBwayCoreHUDWidget::GetMaxHealth() const
{
	if (ULyraHealthComponent* HealthComp = GetLocalPlayerHealthComponent())
	{
		return HealthComp->GetMaxHealth();
	}
	return 0.0f;
}

int32 UBwayCoreHUDWidget::GetTeam1Score() const
{
	const int32 LocalTeam = GetLocalPlayerTeam();
	return UBwayMatchHUDWidgetBase::GetDisplayTeamScore(GetWorld(), 0, LocalTeam);
}

int32 UBwayCoreHUDWidget::GetTeam2Score() const
{
	const int32 LocalTeam = GetLocalPlayerTeam();
	return UBwayMatchHUDWidgetBase::GetDisplayTeamScore(GetWorld(), 1, LocalTeam);
}

int32 UBwayCoreHUDWidget::GetLocalPlayerTeam() const
{
	return UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(this);
}

int32 UBwayCoreHUDWidget::GetRoundTimeRemaining() const
{
	return UBwayMatchHUDWidgetBase::GetRoundTimeRemainingFromWorld(GetWorld());
}

FText UBwayCoreHUDWidget::GetRoundTimeFormatted() const
{
	return UBwayMatchHUDWidgetBase::FormatRoundTime(GetRoundTimeRemaining());
}

int32 UBwayCoreHUDWidget::GetCurrentRoundNumber() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		return GameState->GetCurrentRoundNumber();
	}
	return 0;
}

int32 UBwayCoreHUDWidget::GetRelicPossessingTeam() const
{
	return UBwayMatchHUDWidgetBase::GetDisplayRelicPossessingTeam(GetWorld(), GetLocalPlayerTeam());
}

bool UBwayCoreHUDWidget::IsRelicCarried() const
{
	return UBwayMatchHUDWidgetBase::IsRelicCarriedInWorld(GetWorld());
}

// ========== INTERNAL ==========

void UBwayCoreHUDWidget::BindToGameState()
{
	if (UWorld* World = GetWorld())
	{
		ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
		if (GameState)
		{
			CachedGameState = GameState;

			// Bind to scoring component delegate
			if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
			{
				Scoring->OnTeamScoreChanged.AddDynamic(this, &UBwayCoreHUDWidget::HandleScoreChanged);
			}

			// Bind to game state round delegates
			GameState->OnRoundTimeChanged.AddDynamic(this, &UBwayCoreHUDWidget::HandleRoundTimeChanged);
			GameState->OnRoundStateChanged.AddDynamic(this, &UBwayCoreHUDWidget::HandleRoundStateChanged);
		}
	}
}

void UBwayCoreHUDWidget::UnbindFromGameState()
{
	if (CachedGameState.IsValid())
	{
		if (UBwayScoringComponent* Scoring = CachedGameState->FindComponentByClass<UBwayScoringComponent>())
		{
			Scoring->OnTeamScoreChanged.RemoveDynamic(this, &UBwayCoreHUDWidget::HandleScoreChanged);
		}

		CachedGameState->OnRoundTimeChanged.RemoveDynamic(this, &UBwayCoreHUDWidget::HandleRoundTimeChanged);
		CachedGameState->OnRoundStateChanged.RemoveDynamic(this, &UBwayCoreHUDWidget::HandleRoundStateChanged);
	}
}

void UBwayCoreHUDWidget::BindToHealthComponent()
{
	if (bBoundToHealth && CachedHealthComponent.IsValid())
	{
		return; // Already bound
	}

	// Get the local player's pawn
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (ULyraHealthComponent* HealthComp = ULyraHealthComponent::FindHealthComponent(Pawn))
			{
				// Unbind from old if exists
				if (CachedHealthComponent.IsValid() && bBoundToHealth)
				{
					CachedHealthComponent->OnHealthChanged.RemoveDynamic(this, &UBwayCoreHUDWidget::HandleHealthChanged);
				}

				CachedHealthComponent = HealthComp;
				HealthComp->OnHealthChanged.AddDynamic(this, &UBwayCoreHUDWidget::HandleHealthChanged);
				bBoundToHealth = true;

				// Initial update
				const float Health = HealthComp->GetHealth();
				const float MaxHealth = HealthComp->GetMaxHealth();
				const float HealthPercent = HealthComp->GetHealthNormalized();
				RefreshHealthDisplay(Health, MaxHealth, HealthPercent);
				OnHealthChanged(Health, MaxHealth, HealthPercent);
				LastKnownHealth = Health;
			}
		}
	}
}

void UBwayCoreHUDWidget::HandleHealthChanged(ULyraHealthComponent* HealthComp, float OldValue, float NewValue, AActor* Instigator)
{
	if (HealthComp)
	{
		const float MaxHealth = HealthComp->GetMaxHealth();
		const float HealthPercent = HealthComp->GetHealthNormalized();
		RefreshHealthDisplay(NewValue, MaxHealth, HealthPercent);
		OnHealthChanged(NewValue, MaxHealth, HealthPercent);

		// Check for death
		if (NewValue <= 0.0f && LastKnownHealth > 0.0f)
		{
			OnLocalPlayerDied();
		}
		// Check for respawn (health went from 0 to positive)
		else if (NewValue > 0.0f && LastKnownHealth <= 0.0f)
		{
			OnLocalPlayerRespawned();
		}

		LastKnownHealth = NewValue;
	}
}

void UBwayCoreHUDWidget::HandleScoreChanged(int32 TeamIndex, int32 NewScore)
{
	RefreshScoreDisplay();
}

void UBwayCoreHUDWidget::HandleRoundTimeChanged(int32 SecondsRemaining)
{
	RefreshTimerDisplay();
}

void UBwayCoreHUDWidget::HandleRoundStateChanged(FName NewState)
{
	RefreshRoundLabel();
	RefreshTeamPortraits();
	OnRoundStateChanged(NewState);
}

ABwayGameState* UBwayCoreHUDWidget::GetBwayGameState() const
{
	if (CachedGameState.IsValid())
	{
		return CachedGameState.Get();
	}

	if (UWorld* World = GetWorld())
	{
		return World->GetGameState<ABwayGameState>();
	}

	return nullptr;
}

ULyraHealthComponent* UBwayCoreHUDWidget::GetLocalPlayerHealthComponent() const
{
	if (CachedHealthComponent.IsValid())
	{
		return CachedHealthComponent.Get();
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			return ULyraHealthComponent::FindHealthComponent(Pawn);
		}
	}

	return nullptr;
}

void UBwayCoreHUDWidget::RefreshAllDisplay()
{
	RefreshScoreDisplay();
	RefreshTimerDisplay();
	RefreshRoundLabel();
	RefreshRelicDisplay(GetRelicPossessingTeam(), IsRelicCarried());
	RefreshTeamPortraits();

	if (ULyraHealthComponent* HealthComp = GetLocalPlayerHealthComponent())
	{
		RefreshHealthDisplay(HealthComp->GetHealth(), HealthComp->GetMaxHealth(), HealthComp->GetHealthNormalized());
	}
}

void UBwayCoreHUDWidget::RefreshScoreDisplay()
{
	const int32 Team1Score = GetTeam1Score();
	const int32 Team2Score = GetTeam2Score();
	UpdateBoundScoreTexts(Team1Score, Team2Score);
	OnScoreChanged(Team1Score, Team2Score);
}

void UBwayCoreHUDWidget::RefreshTimerDisplay()
{
	const int32 SecondsRemaining = GetRoundTimeRemaining();
	const FText FormattedTime = GetRoundTimeFormatted();
	UpdateBoundTimerText(SecondsRemaining);
	OnRoundTimeUpdated(SecondsRemaining, FormattedTime);
}

void UBwayCoreHUDWidget::RefreshRoundLabel()
{
	if (Text_RoundLabel)
	{
		const int32 RoundNumber = FMath::Max(GetCurrentRoundNumber(), 1);
		Text_RoundLabel->SetText(FText::Format(
			NSLOCTEXT("BwayCoreHUD", "RoundLabel", "ROUND {0}"),
			FText::AsNumber(RoundNumber)));
	}
}

void UBwayCoreHUDWidget::RefreshRelicDisplay(int32 DisplayPossessingTeam, bool bIsCarried)
{
	const FText StatusText = BuildRelicStatusText(DisplayPossessingTeam);
	const FText CarrierName = bIsCarried ? UBwayMatchHUDWidgetBase::GetRelicCarrierNameFromWorld(GetWorld()) : FText::GetEmpty();

	if (Text_RelicStatus)
	{
		Text_RelicStatus->SetText(StatusText);
	}

	if (Text_CarrierName)
	{
		if (bIsCarried && !CarrierName.IsEmpty())
		{
			Text_CarrierName->SetText(CarrierName);
			Text_CarrierName->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_CarrierName->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UBwayCoreHUDWidget::RefreshHealthDisplay(float NewHealth, float MaxHealth, float HealthPercent)
{
	if (Progress_Health)
	{
		Progress_Health->SetPercent(HealthPercent);
	}

	if (Text_HealthValues)
	{
		Text_HealthValues->SetText(FText::Format(
			NSLOCTEXT("BwayCoreHUD", "HealthValues", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(NewHealth)),
			FText::AsNumber(FMath::RoundToInt(MaxHealth))));
	}
}

void UBwayCoreHUDWidget::RefreshTeamPortraits()
{
	const int32 LocalTeam = GetLocalPlayerTeam();

	if (const TArray<UImage*> Team1Images = GetTeamPortraitImages(0); Team1Images.Num() > 0)
	{
		UBwayHUDHelpers::UpdateTeamPortraitsForDisplaySlot(this, 0, LocalTeam, Team1Images, EmptyPortraitTexture);
	}

	if (const TArray<UImage*> Team2Images = GetTeamPortraitImages(1); Team2Images.Num() > 0)
	{
		UBwayHUDHelpers::UpdateTeamPortraitsForDisplaySlot(this, 1, LocalTeam, Team2Images, EmptyPortraitTexture);
	}
}

FText UBwayCoreHUDWidget::BuildRelicStatusText(const int32 DisplayPossessingTeam) const
{
	switch (DisplayPossessingTeam)
	{
	case 0:
		return Team1StatusText;
	case 1:
		return Team2StatusText;
	default:
		return NeutralStatusText;
	}
}

void UBwayCoreHUDWidget::UpdateBoundScoreTexts(int32 Team1Score, int32 Team2Score)
{
	if (Text_Team1Score)
	{
		Text_Team1Score->SetText(FText::AsNumber(Team1Score));
	}

	if (Text_Team2Score)
	{
		Text_Team2Score->SetText(FText::AsNumber(Team2Score));
	}
}

void UBwayCoreHUDWidget::UpdateBoundTimerText(int32 SecondsRemaining)
{
	if (Text_Timer)
	{
		Text_Timer->SetText(UBwayMatchHUDWidgetBase::FormatRoundTime(SecondsRemaining));

		const FLinearColor TimerColor = (SecondsRemaining <= 60)
			? FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)
			: FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		Text_Timer->SetColorAndOpacity(TimerColor);
	}
}

TArray<UImage*> UBwayCoreHUDWidget::GetTeamPortraitImages(int32 DisplaySlotIndex) const
{
	TArray<UImage*> Images;
	if (DisplaySlotIndex == 0)
	{
		if (Team1_Portrait_1) { Images.Add(Team1_Portrait_1); }
		if (Team1_Portrait_2) { Images.Add(Team1_Portrait_2); }
		if (Team1_Portrait_3) { Images.Add(Team1_Portrait_3); }
		if (Team1_Portrait_4) { Images.Add(Team1_Portrait_4); }
	}
	else if (DisplaySlotIndex == 1)
	{
		if (Team2_Portrait_1) { Images.Add(Team2_Portrait_1); }
		if (Team2_Portrait_2) { Images.Add(Team2_Portrait_2); }
		if (Team2_Portrait_3) { Images.Add(Team2_Portrait_3); }
		if (Team2_Portrait_4) { Images.Add(Team2_Portrait_4); }
	}

	return Images;
}

