// Public/Components/InteractionHandlerComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InteractionHandlerComponent.generated.h"

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class INTERACTIVEOBJECT_API UInteractionHandlerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionHandlerComponent();

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    float InteractionRange;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    float PassTargetRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    FName HoldSocketName;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    float InteractRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    FGameplayTag InteractableTagFilter;
    
    // Core interaction functions
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestPickup(class UInteractionComponent* Target);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestDrop(class UInteractionComponent* Target);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestThrow(class UInteractionComponent* Target, const FVector& Direction, float Strength);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestPass(class UInteractionComponent* Target, AActor* TargetActor);

    // Helper functions
    UFUNCTION(BlueprintCallable, Category = "Interaction", meta=(DisplayName="Find Best Interactable"))
    UInteractionComponent* FindBestInteractable() const;

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    TArray<AActor*> FindValidPassTargets() const;

protected:
    virtual void OnRegister() override;
    virtual void BeginPlay() override;

private:
    bool ValidateInteraction(const UInteractionComponent* Target) const;
    TArray<UInteractionComponent*> FindInteractablesInRadius() const;

    // DEBUG
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
    bool bDebugDrawInteraction;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
    float DebugDrawDuration;
};