// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayMidfieldDividerComponent.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FVector SpawnLocation = MidfieldSpawnLocation;
	FRotator SpawnRotation = MidfieldSpawnRotation;
	if (bAlignToTeamGoals)
	{
		FVector Center, TeamOneDirection;
		if (!UBwayMidfieldRulesLibrary::TryGetMidfieldFrame(this, Center, TeamOneDirection))
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayMidfieldDivider: Cannot align without one distinct goal per team"));
			return;
		}
		SpawnLocation += Center;
		// Goal actors sit above the floor. Keep the authored ground-stripe height.
		SpawnLocation.Z = MidfieldSpawnLocation.Z;
		SpawnRotation = (TeamOneDirection.Rotation().Quaternion() * MidfieldSpawnRotation.Quaternion()).Rotator();
	}
	if (IsValid(SpawnedDivider))
	{
		SpawnedDivider->SetActorLocationAndRotation(SpawnLocation, SpawnRotation);
		SpawnedDivider->SetSuddenDeathVisualActive(false);
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
		SpawnLocation,
		SpawnRotation,
		SpawnParams);

	if (!SpawnedDivider)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayMidfieldDivider: Failed to spawn divider at %s"), *SpawnLocation.ToString());
		return;
	}

	SpawnedDivider->SetSuddenDeathVisualActive(false);
	UE_LOG(LogTemp, Log, TEXT("BwayMidfieldDivider: Spawned %s at %s rotation=%s"),
		*GetNameSafe(SpawnedDivider), *SpawnLocation.ToString(), *SpawnRotation.ToString());
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
