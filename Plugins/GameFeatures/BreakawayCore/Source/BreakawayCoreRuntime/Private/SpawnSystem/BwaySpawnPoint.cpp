// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpawnSystem/BwaySpawnPoint.h"
#include "SpawnSystem/BwaySpawnPointData.h"
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"

ABwaySpawnPoint::ABwaySpawnPoint()
{
	// Disable ticking by default (only need it for debug visualization)
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Spawn points themselves don't replicate, only what they spawn

	// Create root scene component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootSceneComponent);

#if WITH_EDITORONLY_DATA
	// Create billboard for editor visualization
	EditorBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (EditorBillboard)
	{
		EditorBillboard->SetupAttachment(RootComponent);
		EditorBillboard->bIsScreenSizeScaled = true;
		EditorBillboard->bHiddenInGame = true;
	}

	// Create arrow showing spawn direction
	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	if (DirectionArrow)
	{
		DirectionArrow->SetupAttachment(RootComponent);
		DirectionArrow->ArrowSize = 2.0f;
		DirectionArrow->bIsScreenSizeScaled = true;
		DirectionArrow->bHiddenInGame = true;
	}
#endif
}

void ABwaySpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	// Only spawn on server
	if (!HasAuthority())
	{
		return;
	}

	const FGameplayTag RelicSpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"), /*ErrorIfNotFound*/ false);
	if (RelicSpawnTag.IsValid() && SpawnPointTag.MatchesTagExact(RelicSpawnTag))
	{
		// Relics are spawned exclusively by UBwayRelicManagerComponent so it can
		// own ActiveRelic, reset behavior, and round lifecycle.
		bAutoSpawnOnBeginPlay = false;
	}

	// Auto-spawn if configured
	if (bAutoSpawnOnBeginPlay)
	{
		SpawnObject();
	}

	// Update debug visualization
	UpdateDebugVisualization();
}

void ABwaySpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	// Update visualization colors in editor
	UpdateDebugVisualization();
#endif
}

#if WITH_EDITOR
void ABwaySpawnPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Update visualization when properties change
	UpdateDebugVisualization();
}
#endif

// ========================================
// Spawning
// ========================================

AActor* ABwaySpawnPoint::SpawnObject()
{
	// Only server can spawn
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnPoint %s: Attempted to spawn on client"), *GetName());
		return nullptr;
	}

	// Don't spawn if already have an object
	if (SpawnedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnPoint %s: Already has a spawned object"), *GetName());
		return nullptr;
	}

	// Get the class to spawn
	TSubclassOf<AActor> ClassToSpawn = GetClassToSpawn();
	if (!ClassToSpawn)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnPoint %s: No valid class to spawn"), *GetName());
		return nullptr;
	}

	// Setup spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Spawn the actor
	AActor* NewActor = GetWorld()->SpawnActor<AActor>(ClassToSpawn, GetSpawnTransform(), SpawnParams);
	
	if (NewActor)
	{
		SpawnedObject = NewActor;
		
		// Bind to destruction event for respawning
		if (bEnableRespawning)
		{
			NewActor->OnDestroyed.AddDynamic(this, &ABwaySpawnPoint::OnSpawnedObjectDestroyed);
		}

		// Call post-spawn event
		OnObjectSpawned(NewActor);

		UE_LOG(LogTemp, Log, TEXT("SpawnPoint %s: Successfully spawned %s"), 
			*GetName(), *NewActor->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnPoint %s: Failed to spawn actor of class %s"), 
			*GetName(), *ClassToSpawn->GetName());
	}

	return NewActor;
}

void ABwaySpawnPoint::DespawnObject()
{
	if (SpawnedObject)
	{
		// Unbind destruction event
		SpawnedObject->OnDestroyed.RemoveDynamic(this, &ABwaySpawnPoint::OnSpawnedObjectDestroyed);
		
		// Destroy the object
		SpawnedObject->Destroy();
		SpawnedObject = nullptr;

		UE_LOG(LogTemp, Log, TEXT("SpawnPoint %s: Despawned object"), *GetName());
	}
}

void ABwaySpawnPoint::OnSpawnedObjectDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == SpawnedObject)
	{
		SpawnedObject = nullptr;

		UE_LOG(LogTemp, Log, TEXT("SpawnPoint %s: Spawned object was destroyed"), *GetName());

		// Schedule respawn if enabled
		if (bEnableRespawning && RespawnDelay > 0.0f)
		{
			GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &ABwaySpawnPoint::HandleRespawnTimer, 
				RespawnDelay, false);

			UE_LOG(LogTemp, Log, TEXT("SpawnPoint %s: Scheduled respawn in %.1f seconds"), 
				*GetName(), RespawnDelay);
		}
	}
}

void ABwaySpawnPoint::HandleRespawnTimer()
{
	// This is the timer callback - it must return void
	SpawnObject();
}

// ========================================
// Query Functions
// ========================================

bool ABwaySpawnPoint::MatchesTag(FGameplayTag Tag) const
{
	return SpawnPointTag.MatchesTagExact(Tag);
}

bool ABwaySpawnPoint::MatchesAnyTags(const FGameplayTagContainer& GameplayTags) const
{
	return GameplayTags.HasTagExact(SpawnPointTag);
}

FTransform ABwaySpawnPoint::GetSpawnTransform() const
{
	return GetActorTransform();
}

// ========================================
// Protected/Private Functions
// ========================================

TSubclassOf<AActor> ABwaySpawnPoint::GetClassToSpawn_Implementation() const
{
	// If we have spawn data, use it
	if (SpawnData)
	{
		return SpawnData->GetActorClass();
	}

	// Otherwise, return null (subclasses or blueprints should override)
	return nullptr;
}

void ABwaySpawnPoint::OnObjectSpawned_Implementation(AActor* NewObject)
{
	// Default implementation does nothing
	// Subclasses or blueprints can override for custom logic
	
	// Example: Initialize spawned object with spawn point data
	if (SpawnData && NewObject)
	{
		SpawnData->InitializeSpawnedActor(NewObject, this);
	}
}

void ABwaySpawnPoint::UpdateDebugVisualization()
{
#if WITH_EDITORONLY_DATA
	if (!EditorBillboard || !DirectionArrow)
	{
		return;
	}

	// Get color based on spawn type
	FLinearColor ColorToUse = (DebugColor != FLinearColor::White) ? DebugColor : GetDebugColorForType();

	// Update arrow color
	DirectionArrow->SetArrowColor(ColorToUse);

	// Make arrow visible/invisible based on debug setting
	DirectionArrow->SetVisibility(bShowDebugVisualization);
	EditorBillboard->SetVisibility(bShowDebugVisualization);
#endif
}

FLinearColor ABwaySpawnPoint::GetDebugColorForType() const
{
	// Return color based on spawn point tag
	if (SpawnPointTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"))))
	{
		return FLinearColor::Yellow;
	}
	else if (SpawnPointTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Goal"))))
	{
		// Color based on team
		if (TeamIndex == 0)
		{
			return FLinearColor::Blue;
		}
		else if (TeamIndex == 1)
		{
			return FLinearColor::Red;
		}
		return FLinearColor::Green;
	}
	else if (SpawnPointTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Powerup"))))
	{
		return FLinearColor(1.0f, 0.5f, 0.0f); // Orange
	}

	// Default
	return FLinearColor::White;
}
