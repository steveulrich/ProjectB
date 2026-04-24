// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCoreHUDWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "GameState/BwayScoringComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "Character/LyraHealthComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Relic/RelicActor.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCoreHUDWidget)

UBwayCoreHUDWidget::UBwayCoreHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayCoreHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToGameState();
	BindToHealthComponent();
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
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			return Scoring->GetTeamScore(0);
		}
	}
	return 0;
}

int32 UBwayCoreHUDWidget::GetTeam2Score() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			return Scoring->GetTeamScore(1);
		}
	}
	return 0;
}

int32 UBwayCoreHUDWidget::GetLocalPlayerTeam() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
			{
				return GameState->GetPlayerTeam(PS);
			}
		}
	}
	return -1;
}

int32 UBwayCoreHUDWidget::GetRoundTimeRemaining() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		return GameState->GetRoundTimeRemaining();
	}
	return 0;
}

FText UBwayCoreHUDWidget::GetRoundTimeFormatted() const
{
	const int32 TotalSeconds = GetRoundTimeRemaining();
	const int32 Minutes = TotalSeconds / 60;
	const int32 Seconds = TotalSeconds % 60;
	
	return FText::Format(
		NSLOCTEXT("HUD", "TimeFormat", "{0}:{1}"),
		FText::AsNumber(Minutes),
		FText::Format(NSLOCTEXT("HUD", "SecondsFormat", "{0}"), 
			FText::FromString(FString::Printf(TEXT("%02d"), Seconds)))
	);
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
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			return RelicMgr->GetRelicPossessingTeam();
		}
	}
	return -1;
}

bool UBwayCoreHUDWidget::IsRelicCarried() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			if (ARelicActor* Relic = RelicMgr->GetRelicActor())
			{
				return Relic->GetCurrentState() == ERelicState::Carried;
			}
		}
	}
	return false;
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
				OnHealthChanged(Health, MaxHealth, HealthComp->GetHealthNormalized());
				LastKnownHealth = Health;
			}
		}
	}
}

void UBwayCoreHUDWidget::HandleHealthChanged(ULyraHealthComponent* HealthComp, float OldValue, float NewValue, AActor* Instigator)
{
	if (HealthComp)
	{
		OnHealthChanged(NewValue, HealthComp->GetMaxHealth(), HealthComp->GetHealthNormalized());

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
	// Fetch both team scores from the ScoringComponent to maintain the OnScoreChanged(T1, T2) BP event
	int32 Team1Score = 0;
	int32 Team2Score = 0;
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			Team1Score = Scoring->GetTeamScore(0);
			Team2Score = Scoring->GetTeamScore(1);
		}
	}
	OnScoreChanged(Team1Score, Team2Score);
}

void UBwayCoreHUDWidget::HandleRoundTimeChanged(int32 SecondsRemaining)
{
	OnRoundTimeUpdated(SecondsRemaining, GetRoundTimeFormatted());
}

void UBwayCoreHUDWidget::HandleRoundStateChanged(FName NewState)
{
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

