// Fill out your copyright notice in the Description page of Project Settings.


#include "BreakawayGameMode.h"

#include "BwayCharacterWithAbilities.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Player/LyraPlayerState.h"

DEFINE_LOG_CATEGORY(LogBreakawayGame);

ABreakawayGameMode::ABreakawayGameMode()
{
	
}

void ABreakawayGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogBreakawayGame, Warning, TEXT("Breakaway GameMode Loaded"));
}

void ABreakawayGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	ALyraPlayerState* PS = Cast<ALyraPlayerState>(NewPlayer->PlayerState);
	if (PS && PS->SelectedHeroData)
	{
		// Spawn the hero pawn with the selected hero data
		UE_LOG(LogBreakawayGame, Log, TEXT("Spawning hero pawn for player %s with hero data %s"), *NewPlayer->GetName(), *PS->SelectedHeroData->GetName());

		// Temporarily just get a spawn location at origin
		FVector SpawnLocation = FVector::ZeroVector; // Replace with your desired spawn location logic
		FRotator SpawnRotation = FRotator::ZeroRotator; // Replace with your desired spawn rotation logic
		// Spawn the hero pawn
		ABwayCharacterWithAbilities* HeroPawn = GetWorld()->SpawnActorDeferred<ABwayCharacterWithAbilities>(ABwayCharacterWithAbilities::StaticClass(), FTransform(SpawnRotation, SpawnLocation), NewPlayer, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		HeroPawn->InitializeHeroData(PS->SelectedHeroData);
		NewPlayer->Possess(HeroPawn);
	}
}



