// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayMidfieldDividerComponent.h"
#include "Match/BwayMidfieldDividerActor.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMidfieldDividerComponent)

UBwayMidfieldDividerComponent::UBwayMidfieldDividerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DividerActorClass = ABwayMidfieldDividerActor::StaticClass();
}

void UBwayMidfieldDividerComponent::SpawnMidfieldDivider()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (IsValid(SpawnedDivider))
	{
		SpawnedDivider->SetSuddenDeathVisualActive(false);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const TSubclassOf<ABwayMidfieldDividerActor> ClassToSpawn = DividerActorClass
		? DividerActorClass
		: TSubclassOf<ABwayMidfieldDividerActor>(ABwayMidfieldDividerActor::StaticClass());

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	SpawnedDivider = World->SpawnActor<ABwayMidfieldDividerActor>(
		ClassToSpawn,
		MidfieldSpawnLocation,
		MidfieldSpawnRotation,
		SpawnParams);

	if (!SpawnedDivider)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayMidfieldDivider: Failed to spawn divider at %s"), *MidfieldSpawnLocation.ToString());
		return;
	}

	SpawnedDivider->SetSuddenDeathVisualActive(false);
	UE_LOG(LogTemp, Log, TEXT("BwayMidfieldDivider: Spawned %s at %s"),
		*GetNameSafe(SpawnedDivider), *MidfieldSpawnLocation.ToString());
}

void UBwayMidfieldDividerComponent::SetSuddenDeathDividerVisible(const bool bVisible)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!IsValid(SpawnedDivider))
	{
		SpawnMidfieldDivider();
	}

	if (IsValid(SpawnedDivider))
	{
		SpawnedDivider->SetSuddenDeathVisualActive(bVisible);
	}
}
