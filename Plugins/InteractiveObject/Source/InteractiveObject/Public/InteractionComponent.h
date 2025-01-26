// Public/Components/InteractionComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "InteractionComponent.generated.h"

UENUM(BlueprintType)
enum class EInteractionState : uint8
{
    Free,
    Held,
    Thrown,
    Passing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionStateChanged, EInteractionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOwnerChanged, AActor*, NewOwner, AActor*, PreviousOwner);

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class INTERACTIVEOBJECT_API UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionComponent();

    // Core interaction states
    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteractionStateChanged OnInteractionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnOwnerChanged OnOwnerChanged;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    bool bCanBePickedUp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    bool bCanBeThrown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    bool bCanBePassed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Physics")
    float ThrowStrengthMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Physics")
    float PassStrengthMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Configuration")
    FGameplayTag InteractableTag; // Tag to identify interactable objects
    
    // State management
    UFUNCTION(BlueprintPure, Category = "Interaction")
    EInteractionState GetCurrentState() const { return CurrentState; }

    UFUNCTION(BlueprintPure, Category = "Interaction")
    AActor* GetCurrentOwner() const { return CurrentOwner; }

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetInteractionState(EInteractionState NewState);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetOwner(AActor* NewOwner);

    // Interaction interface
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    virtual bool CanBePickedUp(AActor* InteractingActor) const;

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    virtual bool CanBeThrown(AActor* InteractingActor) const;

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    virtual bool CanBePassed(AActor* InteractingActor, AActor* TargetActor) const;

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    AActor* GetCurrentlyHeldObject() const;

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void ThrowHeldObject(const FVector& Velocity);

private:
    UPROPERTY()
    TWeakObjectPtr<AActor> HoldingActor;

protected:
    virtual void OnRegister() override;
    virtual void BeginPlay() override;
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // State management
    UPROPERTY(ReplicatedUsing = OnRep_CurrentState)
    EInteractionState CurrentState;

    UPROPERTY(Replicated)
    AActor* CurrentOwner;

    UFUNCTION()
    virtual void OnRep_CurrentState();

    // Internal state change handlers
    UFUNCTION()
    virtual void HandleStateChange(EInteractionState NewState);
    UFUNCTION()
    virtual void HandleOwnerChange(AActor* NewOwner);

};
