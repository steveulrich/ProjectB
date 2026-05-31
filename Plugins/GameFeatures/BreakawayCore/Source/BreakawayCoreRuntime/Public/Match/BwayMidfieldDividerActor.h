// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayMidfieldDividerActor.generated.h"

class UStaticMeshComponent;

/**
 * Visual midfield divider for sudden-death awareness.
 * Spawned at world origin by the game mode; visibility is driven by UBwayMidfieldDividerComponent.
 */
UCLASS(Blueprintable)
class BREAKAWAYCORERUNTIME_API ABwayMidfieldDividerActor : public AActor
{
	GENERATED_BODY()

public:
	ABwayMidfieldDividerActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority-only: show or hide the divider mesh on server and clients. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Sudden Death")
	void SetSuddenDeathVisualActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Sudden Death")
	bool IsSuddenDeathVisualActive() const { return bSuddenDeathVisualActive; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DividerMesh;

	UPROPERTY(ReplicatedUsing = OnRep_SuddenDeathVisualActive)
	bool bSuddenDeathVisualActive = false;

	UFUNCTION()
	void OnRep_SuddenDeathVisualActive();

	void ApplyVisualState();
};
