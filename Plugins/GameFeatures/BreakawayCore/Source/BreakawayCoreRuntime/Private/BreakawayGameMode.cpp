// Fill out your copyright notice in the Description page of Project Settings.


#include "BreakawayGameMode.h"

#include "BwayCharacterWithAbilities.h"
#include "BreakawayCoreRuntime/HeroSystems/BwayHeroRegistry.h"
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
	if (PS && PS->GetSelectedHeroId().IsValid())
	{
		if (const UBwayHeroDataAsset* HeroData = GetGameInstance()->GetSubsystem<UBwayHeroRegistry>()->GetHeroDataById(PS->GetSelectedHeroId()))
		{
			UE_LOG(LogBreakawayGame, Log, TEXT("Spawning hero pawn for player %s with hero data %s"), *NewPlayer->GetName(), *HeroData->GetName());

			FVector SpawnLocation = FVector::ZeroVector;
			FRotator SpawnRotation = FRotator::ZeroRotator;

			ABwayCharacterWithAbilities* HeroPawn = GetWorld()->SpawnActorDeferred<ABwayCharacterWithAbilities>(
				ABwayCharacterWithAbilities::StaticClass(), 
				FTransform(SpawnRotation, SpawnLocation), 
				NewPlayer, 
				nullptr, 
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn
			);

			HeroPawn->InitializeHeroData(HeroData);
			NewPlayer->Possess(HeroPawn);
		}
		else
		{
			UE_LOG(LogBreakawayGame, Warning, TEXT("Failed to resolve HeroData for %s"), *PS->GetSelectedHeroId().ToString());
		}
	}
}



