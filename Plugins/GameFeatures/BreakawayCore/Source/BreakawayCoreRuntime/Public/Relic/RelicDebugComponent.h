#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Relic/RelicStateMachine.h"
#include "RelicDebugComponent.generated.h"

class ARelicActor;

/**
 * Debug component for Relic system
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API URelicDebugComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    URelicDebugComponent();

protected:
    virtual void BeginPlay() override;

public:    
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    // Set the Relic to debug
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void SetRelicToDebug(ARelicActor* InRelic);
    
    // Debug UI Information
    UFUNCTION(BlueprintPure, Category = "Debug")
    FString GetRelicStateString() const;
    
    UFUNCTION(BlueprintPure, Category = "Debug")
    float GetTimeInCurrentState() const;
    
    UFUNCTION(BlueprintPure, Category = "Debug")
    FString GetCarrierName() const;
    
    // Error handling methods
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void CheckForErrorStates();
    
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void AttemptRecoveryFromErrorState();
    
    // Console commands
    UFUNCTION(Exec, Category = "Debug")
    void DebugForceRelicState(FString StateName);
    
    UFUNCTION(Exec, Category = "Debug")
    void DebugRelicInfo();
    
    // Visualization
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void ToggleDebugVisualization(bool bEnable);
    
private:
    // Reference to the Relic being debugged
    UPROPERTY()
    TObjectPtr<ARelicActor> DebuggedRelic;
    
    // Is visualization enabled?
    bool bDebugVisualizationEnabled;
    
    // Error detection
    bool IsRelicStuck() const;
    bool IsRelicOutOfBounds() const;
    bool HasRelicBeenInStateForTooLong() const;
    
    // State timeouts (seconds)
    const float MaxTimeInDroppedState = 10.0f;
    const float MaxTimeInScoringState = 5.0f;
    const float MaxTimeInResettingState = 5.0f;
};