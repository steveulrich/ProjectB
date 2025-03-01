#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GoalZone.generated.h"

class UBoxComponent;
class ABwayCharacterWithAbilities;
class ARelicActor;

/**
 * Goal zone for Relic scoring
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API AGoalZone : public AActor
{
	GENERATED_BODY()
    
public:    
	AGoalZone();
    
protected:
	virtual void BeginPlay() override;
    
public:    
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerVolume;
    
	// Team properties
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal")
	int32 TeamId;
    
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal")
	FGameplayTag TeamTag;
    
	// Events
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    
	// Helper methods
	UFUNCTION(BlueprintPure, Category = "Goal")
	bool IsOpposingTeam(ABwayCharacterWithAbilities* Character) const;
    
	UFUNCTION(BlueprintCallable, Category = "Goal")
	void ProcessRelicScoringAttempt(ABwayCharacterWithAbilities* Character, ARelicActor* Relic);
};