// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayRelicManagerComponent.h"
#include "BwayGameState.h"
#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "SpawnSystem/BwaySpawnPoint.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRelicManagerComponent)

UBwayRelicManagerComponent::UBwayRelicManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UBwayRelicManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache the spawn point manager
	if (AActor* Owner = GetOwner())
	{
		CachedSpawnPointManager = Owner->FindComponentByClass<UBwaySpawnPointManagerComponent>();
	}

	// Initialize relic spawn tag if not set
	if (!RelicSpawnTag.IsValid())
	{
		RelicSpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"));
	}
}

void UBwayRelicManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBwayRelicManagerComponent, ActiveRelic);
	DOREPLIFETIME(UBwayRelicManagerComponent, RelicPossessingTeam);
}

ARelicActor* UBwayRelicManagerComponent::SpawnRelic()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return nullptr;
	}

	if (!RelicClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRelicManager: No RelicClass set!"));
		return nullptr;
	}

	// Use spawn point manager if available
	if (CachedSpawnPointManager)
	{
		TArray<AActor*> SpawnedRelics = CachedSpawnPointManager->SpawnObjectsAtPoints(RelicSpawnTag);
		if (SpawnedRelics.Num() > 0)
		{
			ActiveRelic = Cast<ARelicActor>(SpawnedRelics[0]);
			if (ActiveRelic)
			{
				UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Spawned relic via spawn point manager"));
				return ActiveRelic;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: Could not spawn relic via spawn points"));
	return nullptr;
}

void UBwayRelicManagerComponent::ResetRelic()
{
	if (GetOwnerRole() != ROLE_Authority || !ActiveRelic)
	{
		return;
	}

	ActiveRelic->ClearScoringFlag();

	if (CachedSpawnPointManager)
	{
		ABwaySpawnPoint* RelicSpawn = CachedSpawnPointManager->GetRandomSpawnPoint(RelicSpawnTag);
		if (RelicSpawn)
		{
			ActiveRelic->SetActorLocation(RelicSpawn->GetActorLocation());
			ActiveRelic->SetActorRotation(RelicSpawn->GetActorRotation());
			ActiveRelic->OnDropped();
			
			OnRelicResetEvent.Broadcast(ActiveRelic);
			UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Relic reset to spawn point: %s"), *RelicSpawn->GetName());
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: No relic spawn points found for reset"));
}

void UBwayRelicManagerComponent::OnRelicCarrierChanged(ABwayCharacterWithAbilities* NewCarrier)
{
	ABwayGameState* BwayGS = Cast<ABwayGameState>(GetOwner());

	if (NewCarrier && NewCarrier->GetPlayerState() && BwayGS)
	{
		RelicPossessingTeam = BwayGS->GetPlayerTeam(NewCarrier->GetPlayerState());
	}
	else
	{
		RelicPossessingTeam = -1; // Neutral
	}

	OnRelicCarrierChangedEvent.Broadcast(ActiveRelic, NewCarrier);
}

void UBwayRelicManagerComponent::OnRep_RelicPossessingTeam()
{
	if (RelicPossessingTeam >= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Team %d now possesses the relic"), RelicPossessingTeam + 1);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Relic is now neutral"));
	}
}
