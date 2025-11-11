// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "BwayGoalVolume.generated.h"

class ARelicActor;

/**
 * Goal Trigger Volume for Breakaway
 * Detects when the relic enters the goal area and triggers scoring
 * Works for both thrown and carried relics
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABwayGoalVolume : public AActor
{
	GENERATED_BODY()

public:
	ABwayGoalVolume();

	//~AActor interface
	virtual void BeginPlay() override;
	//~End of AActor interface

	/** Which team this goal belongs to (0 = Team 1, 1 = Team 2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal")
	int32 OwningTeam = 0;

	/** Visual feedback when goal is scored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal|VFX")
	bool bShowDebugSphere = true;

protected:
	/** Trigger volume component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> GoalTrigger;

	/** Visual mesh for the goal (optional) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GoalMesh;

	/** Handle overlap events */
	UFUNCTION()
	void OnGoalOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Visual feedback when scoring */
	UFUNCTION(BlueprintNativeEvent, Category = "Goal")
	void PlayScoringEffects();
	virtual void PlayScoringEffects_Implementation();

private:
	/** Track if a score is currently being processed to avoid double-scoring */
	bool bIsProcessingScore = false;

	/** Reset score processing flag after a delay */
	void ResetScoreProcessing();
};