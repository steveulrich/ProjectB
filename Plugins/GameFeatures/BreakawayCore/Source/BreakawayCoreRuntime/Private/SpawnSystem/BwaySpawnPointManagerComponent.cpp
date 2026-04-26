// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "SpawnSystem/BwaySpawnPoint.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
bool IsRelicSpawnTag(const FGameplayTag& Tag)
{
	const FGameplayTag RelicSpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"), /*ErrorIfNotFound*/ false);
	return RelicSpawnTag.IsValid() && Tag.MatchesTagExact(RelicSpawnTag);
}
}

UBwaySpawnPointManagerComponent::UBwaySpawnPointManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UBwaySpawnPointManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only run on server
	if (GetOwner()->HasAuthority())
	{
		if (bAutoDiscoverSpawnPoints)
		{
			DiscoverSpawnPoints();
		}
	}
}

void UBwaySpawnPointManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear all references
	RegisteredSpawnPoints.Empty();
	SpawnPointsByTag.Empty();
	SpawnPointsByTeam.Empty();

	Super::EndPlay(EndPlayReason);
}

// ========================================
// Spawn Point Discovery
// ========================================

void UBwaySpawnPointManagerComponent::DiscoverSpawnPoints()
{
	if (!GetWorld())
	{
		return;
	}

	// Clear existing registrations
	RegisteredSpawnPoints.Empty();

	// Find all spawn point actors in the level
	int32 DiscoveredCount = 0;
	for (TActorIterator<ABwaySpawnPoint> It(GetWorld()); It; ++It)
	{
		ABwaySpawnPoint* SpawnPoint = *It;
		if (SpawnPoint)
		{
			RegisterSpawnPoint(SpawnPoint);
			DiscoveredCount++;
		}
	}

	// Rebuild lookup tables for efficient queries
	RebuildLookupTables();

	UE_LOG(LogTemp, Log, TEXT("SpawnPointManager: Discovered %d spawn points"), DiscoveredCount);

	// Auto-spawn if configured
	if (bAutoSpawnOnDiscovery)
	{
		if (AutoSpawnTags.IsEmpty())
		{
			// Spawn at all points
			for (ABwaySpawnPoint* SpawnPoint : RegisteredSpawnPoints)
			{
				if (SpawnPoint && SpawnPoint->bAutoSpawnOnBeginPlay && !IsRelicSpawnTag(SpawnPoint->SpawnPointTag))
				{
					SpawnPoint->SpawnObject();
				}
			}
		}
		else
		{
			// Spawn only at points matching the tags
			for (const FGameplayTag& Tag : AutoSpawnTags)
			{
				if (!IsRelicSpawnTag(Tag))
				{
					SpawnObjectsAtPoints(Tag);
				}
			}
		}
	}
}

void UBwaySpawnPointManagerComponent::RegisterSpawnPoint(ABwaySpawnPoint* SpawnPoint)
{
	if (!SpawnPoint || RegisteredSpawnPoints.Contains(SpawnPoint))
	{
		return;
	}

	RegisteredSpawnPoints.Add(SpawnPoint);

	UE_LOG(LogTemp, Log, TEXT("SpawnPointManager: Registered spawn point %s with tag %s"), 
		*SpawnPoint->GetName(), *SpawnPoint->SpawnPointTag.ToString());
}

void UBwaySpawnPointManagerComponent::UnregisterSpawnPoint(ABwaySpawnPoint* SpawnPoint)
{
	if (!SpawnPoint)
	{
		return;
	}

	RegisteredSpawnPoints.Remove(SpawnPoint);
	RebuildLookupTables();

	UE_LOG(LogTemp, Log, TEXT("SpawnPointManager: Unregistered spawn point %s"), *SpawnPoint->GetName());
}

// ========================================
// Query Functions
// ========================================

TArray<ABwaySpawnPoint*> UBwaySpawnPointManagerComponent::GetSpawnPointsByTag(FGameplayTag Tag) const
{
	TArray<ABwaySpawnPoint*> Result;

	if (!Tag.IsValid())
	{
		return Result;
	}

	// Use cached lookup table if available
	if (const TArray<ABwaySpawnPoint*>* Found = SpawnPointsByTag.Find(Tag))
	{
		Result = *Found;
		return Result;
	}

	// Fallback: Linear search (less efficient)
	for (ABwaySpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (SpawnPoint && SpawnPoint->MatchesTag(Tag))
		{
			Result.Add(SpawnPoint);
		}
	}

	return Result;
}

TArray<ABwaySpawnPoint*> UBwaySpawnPointManagerComponent::GetSpawnPointsByTags(const FGameplayTagContainer& GameplayTags) const
{
	TArray<ABwaySpawnPoint*> Result;

	for (ABwaySpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (SpawnPoint && SpawnPoint->MatchesAnyTags(GameplayTags))
		{
			Result.Add(SpawnPoint);
		}
	}

	return Result;
}

TArray<ABwaySpawnPoint*> UBwaySpawnPointManagerComponent::GetSpawnPointsByTeam(int32 TeamIndex) const
{
	TArray<ABwaySpawnPoint*> Result;

	// Use cached lookup table if available
	if (const TArray<ABwaySpawnPoint*>* Found = SpawnPointsByTeam.Find(TeamIndex))
	{
		Result = *Found;
		return Result;
	}

	// Fallback: Linear search
	for (ABwaySpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (SpawnPoint && SpawnPoint->TeamIndex == TeamIndex)
		{
			Result.Add(SpawnPoint);
		}
	}

	return Result;
}

ABwaySpawnPoint* UBwaySpawnPointManagerComponent::GetRandomSpawnPoint(FGameplayTag Tag) const
{
	TArray<ABwaySpawnPoint*> MatchingPoints;

	if (Tag.IsValid())
	{
		MatchingPoints = GetSpawnPointsByTag(Tag);
	}
	else
	{
		MatchingPoints = RegisteredSpawnPoints;
	}

	if (MatchingPoints.Num() == 0)
	{
		return nullptr;
	}

	// Return random spawn point
	const int32 RandomIndex = FMath::RandRange(0, MatchingPoints.Num() - 1);
	return MatchingPoints[RandomIndex];
}

ABwaySpawnPoint* UBwaySpawnPointManagerComponent::GetClosestSpawnPoint(FVector Location, FGameplayTag OptionalTag) const
{
	TArray<ABwaySpawnPoint*> PointsToSearch;

	if (OptionalTag.IsValid())
	{
		PointsToSearch = GetSpawnPointsByTag(OptionalTag);
	}
	else
	{
		PointsToSearch = RegisteredSpawnPoints;
	}

	if (PointsToSearch.Num() == 0)
	{
		return nullptr;
	}

	// Find closest spawn point
	ABwaySpawnPoint* ClosestPoint = nullptr;
	float ClosestDistanceSq = FLT_MAX;

	for (ABwaySpawnPoint* SpawnPoint : PointsToSearch)
	{
		if (!SpawnPoint)
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Location, SpawnPoint->GetActorLocation());
		if (DistanceSq < ClosestDistanceSq)
		{
			ClosestDistanceSq = DistanceSq;
			ClosestPoint = SpawnPoint;
		}
	}

	return ClosestPoint;
}

TArray<ABwaySpawnPoint*> UBwaySpawnPointManagerComponent::GetAllSpawnPoints() const
{
	return RegisteredSpawnPoints;
}

int32 UBwaySpawnPointManagerComponent::GetSpawnPointCount(FGameplayTag Tag) const
{
	if (Tag.IsValid())
	{
		return GetSpawnPointsByTag(Tag).Num();
	}
	return RegisteredSpawnPoints.Num();
}

// ========================================
// Spawn Management
// ========================================

TArray<AActor*> UBwaySpawnPointManagerComponent::SpawnObjectsAtPoints(FGameplayTag Tag)
{
	TArray<AActor*> SpawnedActors;

	if (IsRelicSpawnTag(Tag))
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnPointManager: Relics must be spawned by BwayRelicManager, not SpawnObjectsAtPoints"));
		return SpawnedActors;
	}

	TArray<ABwaySpawnPoint*> MatchingPoints = GetSpawnPointsByTag(Tag);

	for (ABwaySpawnPoint* SpawnPoint : MatchingPoints)
	{
		if (SpawnPoint)
		{
			AActor* SpawnedActor = SpawnPoint->SpawnObject();
			if (SpawnedActor)
			{
				SpawnedActors.Add(SpawnedActor);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SpawnPointManager: Spawned %d objects at spawn points with tag %s"), 
		SpawnedActors.Num(), *Tag.ToString());

	return SpawnedActors;
}

void UBwaySpawnPointManagerComponent::DespawnObjectsAtPoints(FGameplayTag Tag)
{
	TArray<ABwaySpawnPoint*> MatchingPoints = GetSpawnPointsByTag(Tag);

	int32 DespawnCount = 0;
	for (ABwaySpawnPoint* SpawnPoint : MatchingPoints)
	{
		if (SpawnPoint && SpawnPoint->HasSpawnedObject())
		{
			SpawnPoint->DespawnObject();
			DespawnCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SpawnPointManager: Despawned %d objects at spawn points with tag %s"), 
		DespawnCount, *Tag.ToString());
}

void UBwaySpawnPointManagerComponent::ResetAllSpawnPoints(FGameplayTag OptionalTag)
{
	TArray<ABwaySpawnPoint*> PointsToReset;

	if (OptionalTag.IsValid())
	{
		PointsToReset = GetSpawnPointsByTag(OptionalTag);
	}
	else
	{
		PointsToReset = RegisteredSpawnPoints;
	}

	for (ABwaySpawnPoint* SpawnPoint : PointsToReset)
	{
		if (SpawnPoint)
		{
			// Despawn existing object
			if (SpawnPoint->HasSpawnedObject())
			{
				SpawnPoint->DespawnObject();
			}

			// Respawn new object
			SpawnPoint->SpawnObject();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SpawnPointManager: Reset %d spawn points"), PointsToReset.Num());
}

// ========================================
// Protected/Private Functions
// ========================================

void UBwaySpawnPointManagerComponent::RebuildLookupTables()
{
	SpawnPointsByTag.Empty();
	SpawnPointsByTeam.Empty();

	for (ABwaySpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (!SpawnPoint)
		{
			continue;
		}

		// Build tag lookup
		if (SpawnPoint->SpawnPointTag.IsValid())
		{
			TArray<ABwaySpawnPoint*>& TagArray = SpawnPointsByTag.FindOrAdd(SpawnPoint->SpawnPointTag);
			TagArray.Add(SpawnPoint);
		}

		// Build team lookup
		if (SpawnPoint->TeamIndex >= 0)
		{
			TArray<ABwaySpawnPoint*>& TeamArray = SpawnPointsByTeam.FindOrAdd(SpawnPoint->TeamIndex);
			TeamArray.Add(SpawnPoint);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("SpawnPointManager: Rebuilt lookup tables - %d tags, %d teams"), 
		SpawnPointsByTag.Num(), SpawnPointsByTeam.Num());
}
