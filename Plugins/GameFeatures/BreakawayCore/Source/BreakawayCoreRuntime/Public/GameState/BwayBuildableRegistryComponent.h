#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "BwayBuildableRegistryComponent.generated.h"

class ABuildableActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildableRegistered, ABuildableActor*, Buildable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildableUnregistered, ABuildableActor*, Buildable);

/**
 * Server-authoritative registry of placed buildables for match-wide persistence and late-joiner queries.
 * Buildables with bPersistsBetweenRounds survive round resets; this component tracks them for the full match.
 */
UCLASS(ClassGroup=(Breakaway), meta=(BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayBuildableRegistryComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayBuildableRegistryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Buildables")
	void RegisterBuildable(ABuildableActor* Buildable);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Buildables")
	void UnregisterBuildable(ABuildableActor* Buildable);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Buildables")
	TArray<ABuildableActor*> GetAllBuildables() const;

	UFUNCTION(BlueprintPure, Category = "Breakaway|Buildables")
	TArray<ABuildableActor*> GetBuildablesForTeam(int32 TeamIndex) const;

	UFUNCTION(BlueprintPure, Category = "Breakaway|Buildables")
	int32 GetBuildableCountForPlayer(APlayerState* PlayerState) const;

	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Buildables")
	FOnBuildableRegistered OnBuildableRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Buildables")
	FOnBuildableUnregistered OnBuildableUnregistered;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_RegisteredBuildables)
	TArray<TObjectPtr<ABuildableActor>> RegisteredBuildables;

	UFUNCTION()
	void OnRep_RegisteredBuildables();

	void PruneInvalidEntries();
};
