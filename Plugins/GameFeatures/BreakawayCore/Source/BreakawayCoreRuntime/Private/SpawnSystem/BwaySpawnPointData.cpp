// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpawnSystem/BwaySpawnPointData.h"
#include "SpawnSystem/BwaySpawnPoint.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "BwayGoalVolume.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

// ========================================
// UBwaySpawnPointData
// ========================================

TSubclassOf<AActor> UBwaySpawnPointData::GetActorClass() const
{
	if (ActorClass.IsNull())
	{
		return nullptr;
	}

	// Load the class synchronously
	// For better performance, consider async loading in BeginPlay
	return ActorClass.LoadSynchronous();
}

void UBwaySpawnPointData::InitializeSpawnedActor_Implementation(AActor* SpawnedActor, ABwaySpawnPoint* SpawnPoint)
{
	if (!SpawnedActor || !SpawnPoint)
	{
		return;
	}

	// Apply team index if configured
	if (bUseSpawnPointTeam)
	{
		// Check if the spawned actor has a team interface or property
		// This is a placeholder - implement based on your team system
		// Example: If actor implements IGenericTeamAgentInterface
		// IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(SpawnedActor);
		// if (TeamAgent)
		// {
		//     TeamAgent->SetGenericTeamId(FGenericTeamId(SpawnPoint->TeamIndex));
		// }
	}

	UE_LOG(LogTemp, Log, TEXT("Initialized spawned actor %s from spawn point %s"), 
		*SpawnedActor->GetName(), *SpawnPoint->GetName());
}

bool UBwaySpawnPointData::MatchesSpawnType(FGameplayTag Tag) const
{
	return SpawnType.MatchesTagExact(Tag);
}

#if WITH_EDITOR
void UBwaySpawnPointData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Validate configuration
	if (bOverrideRespawnSettings && bEnableRespawning && RespawnDelay < 0.0f)
	{
		RespawnDelay = 0.0f;
	}
}
#endif

// ========================================
// UBwayRelicSpawnData
// ========================================

UBwayRelicSpawnData::UBwayRelicSpawnData()
{
	// Set default spawn type
	SpawnType = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"));
	
	// Relics typically don't respawn (they reset position instead)
	bEnableRespawning = false;
}

void UBwayRelicSpawnData::InitializeSpawnedActor_Implementation(AActor* SpawnedActor, ABwaySpawnPoint* SpawnPoint)
{
	Super::InitializeSpawnedActor_Implementation(SpawnedActor, SpawnPoint);

	// Cast to relic actor
	ARelicActor* RelicActor = Cast<ARelicActor>(SpawnedActor);
	if (!RelicActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("RelicSpawnData: Spawned actor is not a RelicActor"));
		return;
	}

	// Load and apply relic settings if specified
	if (!RelicSettings.IsNull())
	{
		URelicSettings* LoadedSettings = RelicSettings.LoadSynchronous();
		if (LoadedSettings)
		{
			RelicActor->InitializeRelicData(LoadedSettings);
			UE_LOG(LogTemp, Log, TEXT("Initialized relic with settings: %s"), *LoadedSettings->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load relic settings: %s"), *RelicSettings.ToString());
		}
	}
}

// ========================================
// UBwayGoalSpawnData
// ========================================

UBwayGoalSpawnData::UBwayGoalSpawnData()
{
	// Set default spawn type
	SpawnType = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Goal"));
	
	// Goals never respawn
	bEnableRespawning = false;
	
	// Use spawn point team to determine goal ownership
	bUseSpawnPointTeam = true;
}

void UBwayGoalSpawnData::InitializeSpawnedActor_Implementation(AActor* SpawnedActor, ABwaySpawnPoint* SpawnPoint)
{
	Super::InitializeSpawnedActor_Implementation(SpawnedActor, SpawnPoint);

	// Cast to goal volume
	ABwayGoalVolume* GoalVolume = Cast<ABwayGoalVolume>(SpawnedActor);
	if (!GoalVolume)
	{
		UE_LOG(LogTemp, Warning, TEXT("GoalSpawnData: Spawned actor is not a BwayGoalVolume"));
		return;
	}

	// Set goal properties from spawn data
	GoalVolume->OwningTeam = SpawnPoint->TeamIndex;
	GoalVolume->bShowDebugSphere = bShowDebugSphere;

	// Set goal trigger extent if the goal has a box component
	if (UBoxComponent* TriggerBox = GoalVolume->FindComponentByClass<UBoxComponent>())
	{
		TriggerBox->SetBoxExtent(GoalExtent);
	}

	UE_LOG(LogTemp, Log, TEXT("Initialized goal volume for Team %d at %s"), 
		SpawnPoint->TeamIndex + 1, *SpawnPoint->GetActorLocation().ToString());
}
