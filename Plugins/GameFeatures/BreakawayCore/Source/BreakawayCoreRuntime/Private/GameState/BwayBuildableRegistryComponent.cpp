#include "GameState/BwayBuildableRegistryComponent.h"
#include "Buildable/BuildableBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBuildableRegistryComponent)

UBwayBuildableRegistryComponent::UBwayBuildableRegistryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UBwayBuildableRegistryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBwayBuildableRegistryComponent, RegisteredBuildables);
}

void UBwayBuildableRegistryComponent::RegisterBuildable(ABuildableActor* Buildable)
{
	if (!GetOwner()->HasAuthority() || !Buildable)
	{
		return;
	}

	PruneInvalidEntries();

	if (RegisteredBuildables.Contains(Buildable))
	{
		return;
	}

	RegisteredBuildables.Add(Buildable);
	OnBuildableRegistered.Broadcast(Buildable);
}

void UBwayBuildableRegistryComponent::UnregisterBuildable(ABuildableActor* Buildable)
{
	if (!GetOwner()->HasAuthority() || !Buildable)
	{
		return;
	}

	const int32 Removed = RegisteredBuildables.Remove(Buildable);
	if (Removed > 0)
	{
		OnBuildableUnregistered.Broadcast(Buildable);
	}
}

TArray<ABuildableActor*> UBwayBuildableRegistryComponent::GetAllBuildables() const
{
	TArray<ABuildableActor*> Result;
	for (const TObjectPtr<ABuildableActor>& Buildable : RegisteredBuildables)
	{
		if (IsValid(Buildable))
		{
			Result.Add(Buildable.Get());
		}
	}
	return Result;
}

TArray<ABuildableActor*> UBwayBuildableRegistryComponent::GetBuildablesForTeam(int32 TeamIndex) const
{
	TArray<ABuildableActor*> Result;
	const uint8 LyraTeamId = static_cast<uint8>(FMath::Max(0, TeamIndex) + 1);

	for (const TObjectPtr<ABuildableActor>& Buildable : RegisteredBuildables)
	{
		if (IsValid(Buildable) && Buildable->GetTeamId().GetId() == LyraTeamId)
		{
			Result.Add(Buildable.Get());
		}
	}
	return Result;
}

int32 UBwayBuildableRegistryComponent::GetBuildableCountForPlayer(APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return 0;
	}

	int32 Count = 0;
	for (const TObjectPtr<ABuildableActor>& Buildable : RegisteredBuildables)
	{
		if (IsValid(Buildable) && Buildable->GetOwner() == PlayerState->GetOwner())
		{
			++Count;
		}
	}
	return Count;
}

void UBwayBuildableRegistryComponent::OnRep_RegisteredBuildables()
{
	PruneInvalidEntries();
}

void UBwayBuildableRegistryComponent::PruneInvalidEntries()
{
	RegisteredBuildables.RemoveAll([](const TObjectPtr<ABuildableActor>& Buildable)
	{
		return !IsValid(Buildable);
	});
}
