// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayHUDHelpers.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "AbilitySystemComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LyraGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHUDHelpers)

bool UBwayHUDHelpers::IsPlayerAliveForHUD(const ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return false;
	}

	if (const UAbilitySystemComponent* AbilitySystemComponent = PlayerState->GetAbilitySystemComponent())
	{
		if (AbilitySystemComponent->HasMatchingGameplayTag(LyraGameplayTags::Status_Death))
		{
			return false;
		}
	}

	// Lyra clears death tags when the old pawn uninitializes, before the replacement pawn exists.
	// Treat that pawnless respawn window as dead so portraits cannot briefly flash alive.
	const APawn* Pawn = PlayerState->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	if (const ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(Pawn))
	{
		return !HealthComponent->IsDeadOrDying();
	}

	return true;
}

bool UBwayHUDHelpers::GetTeamPlayerHUDData(
	const UObject* WorldContextObject,
	int32 TeamIndex,
	TArray<FTeamPlayerHUDData>& OutPlayerData)
{
	OutPlayerData.Empty();

	if (!WorldContextObject)
	{
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return false;
	}

	ABwayGameState* GameState = Cast<ABwayGameState>(World->GetGameState());
	if (!GameState)
	{
		return false;
	}

	// Validate team index
	if (TeamIndex < 0 || TeamIndex > 1)
	{
		return false;
	}

	// Get team info
	const FTeamInfo& TeamInfo = GameState->GetTeamInfo(TeamIndex);

	TArray<ABwayPlayerState*> SortedPlayers;
	SortedPlayers.Reserve(TeamInfo.TeamMembers.Num());
	for (APlayerState* PlayerState : TeamInfo.TeamMembers)
	{
		if (ABwayPlayerState* BwayPlayerState = Cast<ABwayPlayerState>(PlayerState))
		{
			SortedPlayers.Add(BwayPlayerState);
		}
	}

	// TeamInfo insertion order can differ across peers. PlayerNum is preferred when assigned;
	// replicated PlayerId and name provide deterministic tie-breakers for bots/default PlayerNum.
	SortedPlayers.Sort(
		[](const ABwayPlayerState& A, const ABwayPlayerState& B)
		{
			if (A.GetPlayerNum() != B.GetPlayerNum())
			{
				return A.GetPlayerNum() < B.GetPlayerNum();
			}
			if (A.GetPlayerId() != B.GetPlayerId())
			{
				return A.GetPlayerId() < B.GetPlayerId();
			}
			return A.GetPlayerName().Compare(B.GetPlayerName(), ESearchCase::IgnoreCase) < 0;
		});

	// Process the four stable team positions.
	int32 SlotIndex = 0;
	for (ABwayPlayerState* BwayPS : SortedPlayers)
	{
		if (!BwayPS || SlotIndex >= 4)
		{
			continue;
		}

		FTeamPlayerHUDData PlayerData;
		PlayerData.PlayerName = BwayPS->GetPlayerName();
		PlayerData.PlayerState = BwayPS;
		PlayerData.SlotIndex = SlotIndex;
		PlayerData.bHasRelic = BwayPS->bHasRelic;
		PlayerData.bIsAlive = IsPlayerAliveForHUD(BwayPS);

		// Get hero data from the selected hero ID
		FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
		if (HeroId.IsValid())
		{
			// Use the static registry function to get hero data
			PlayerData.HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
			
			// Fallback: load directly from asset manager
			if (!PlayerData.HeroData)
			{
				UAssetManager& AssetManager = UAssetManager::Get();
				FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(HeroId);
				if (AssetPath.IsValid())
				{
					PlayerData.HeroData = Cast<UBwayHeroDataAsset>(AssetPath.TryLoad());
				}
			}

			// Extract portrait from hero data
			if (PlayerData.HeroData)
			{
				PlayerData.PortraitTexture = PlayerData.HeroData->Portrait;
			}
		}

		OutPlayerData.Add(PlayerData);
		SlotIndex++;
	}

	return true;
}

void UBwayHUDHelpers::UpdateTeamPortraits(
	const UObject* WorldContextObject,
	int32 TeamIndex,
	const TArray<UImage*>& PortraitImages,
	UTexture2D* EmptySlotTexture,
	FLinearColor DeadPlayerTint)
{
	TArray<FTeamPlayerHUDData> PlayerData;
	if (!GetTeamPlayerHUDData(WorldContextObject, TeamIndex, PlayerData))
	{
		return;
	}

	// Update each portrait image
	for (int32 i = 0; i < PortraitImages.Num(); i++)
	{
		UImage* PortraitImage = PortraitImages[i];
		if (!PortraitImage)
		{
			continue;
		}

		// Check if we have a player for this slot
		if (i < PlayerData.Num())
		{
			const FTeamPlayerHUDData& Data = PlayerData[i];

			// Set the portrait texture
			if (Data.PortraitTexture)
			{
				PortraitImage->SetBrushFromTexture(Data.PortraitTexture);
				PortraitImage->SetVisibility(ESlateVisibility::Visible);

				// Apply tint based on alive status
				if (Data.bIsAlive)
				{
					PortraitImage->SetColorAndOpacity(FLinearColor::White);
				}
				else
				{
					PortraitImage->SetColorAndOpacity(DeadPlayerTint);
				}
			}
			else if (EmptySlotTexture)
			{
				// Player exists but no hero selected yet
				PortraitImage->SetBrushFromTexture(EmptySlotTexture);
				PortraitImage->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
				PortraitImage->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				// No texture available, hide the slot
				PortraitImage->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		else
		{
			// No player in this slot
			if (EmptySlotTexture)
			{
				PortraitImage->SetBrushFromTexture(EmptySlotTexture);
				PortraitImage->SetColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f));
				PortraitImage->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				PortraitImage->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}
}

int32 UBwayHUDHelpers::GetLocalPlayerTeamForHUD(const UObject* WorldContextObject)
{
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		return UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForPlayerController(PC);
	}

	return -1;
}

int32 UBwayHUDHelpers::MapDisplaySlotToGameTeam(const int32 DisplaySlotIndex, const int32 LocalPlayerTeamIndex)
{
	return UBwayMatchHUDWidgetBase::MapDisplaySlotToGameTeam(DisplaySlotIndex, LocalPlayerTeamIndex);
}

bool UBwayHUDHelpers::GetTeamPlayerHUDDataForDisplaySlot(
	const UObject* WorldContextObject,
	const int32 DisplaySlotIndex,
	const int32 LocalPlayerTeamIndex,
	TArray<FTeamPlayerHUDData>& OutPlayerData)
{
	const int32 GameTeamIndex = MapDisplaySlotToGameTeam(DisplaySlotIndex, LocalPlayerTeamIndex);
	return GetTeamPlayerHUDData(WorldContextObject, GameTeamIndex, OutPlayerData);
}

void UBwayHUDHelpers::UpdateTeamPortraitsForDisplaySlot(
	const UObject* WorldContextObject,
	const int32 DisplaySlotIndex,
	const int32 LocalPlayerTeamIndex,
	const TArray<UImage*>& PortraitImages,
	UTexture2D* EmptySlotTexture,
	FLinearColor DeadPlayerTint)
{
	const int32 GameTeamIndex = MapDisplaySlotToGameTeam(DisplaySlotIndex, LocalPlayerTeamIndex);
	UpdateTeamPortraits(WorldContextObject, GameTeamIndex, PortraitImages, EmptySlotTexture, DeadPlayerTint);
}

const UBwayHeroDataAsset* UBwayHUDHelpers::GetPlayerHeroData(const ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return nullptr;
	}

	FPrimaryAssetId HeroId = PlayerState->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		return nullptr;
	}

	// Try registry first (static function)
	if (const UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
	{
		return HeroData;
	}

	// Fallback to direct load
	UAssetManager& AssetManager = UAssetManager::Get();
	FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(HeroId);
	if (AssetPath.IsValid())
	{
		return Cast<UBwayHeroDataAsset>(AssetPath.TryLoad());
	}

	return nullptr;
}

UTexture2D* UBwayHUDHelpers::GetPlayerHeroPortrait(const ABwayPlayerState* PlayerState)
{
	const UBwayHeroDataAsset* HeroData = GetPlayerHeroData(PlayerState);
	if (HeroData)
	{
		return HeroData->Portrait;
	}
	return nullptr;
}

