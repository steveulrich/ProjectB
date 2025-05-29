#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayActorWithAbilitiesAndHealth.h"
#include "Components/SphereComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "BuildableBase.generated.h"

/**
 * Base class for all buildable structures in the game.
 * Each Warrior will have specific buildables derived from this or its subclasses.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABuildableActor : public ABwayActorWithAbilitiesAndHealth
{
    GENERATED_BODY()

public:
    ABuildableActor();

    // Public Static Const Component Names
    const static FName MeshComponentName;

    /** Get the actor's root skeletal mesh component */
    UFUNCTION(BlueprintPure)
    FORCEINLINE USkeletalMeshComponent* GetMesh() const { return MeshComponent; }
    
    //~UBwayActorWithAbilities implementation
    virtual void InitializeAbilitySystem() override;
    //~End of UBwayActorWithAbilities implementation

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
    
protected:
    /** Root Component is Skeletal Mesh */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<USkeletalMeshComponent> MeshComponent;

    // --- Configuration Properties (can be set in Blueprints or Data Assets) ---

    /** Time it takes for the buildable to become active after placement. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buildable|Config")
    float BuildTime = 0.0f;  // 

    /** Is the buildable invulnerable during its build time? */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buildable|Config")
    bool bInvulnerableDuringBuild = true;  // 

    /** Should this buildable persist across rounds if not destroyed? */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buildable|Config")
    bool bPersistsBetweenRounds = true;  // 


    bool bIsDestroyed = false;
    bool bIsActive = false;
    FTimerHandle BuildTimerHandle;

    virtual void FinishBuilding();

private:
    
    void InitializeBuildable(AController* InOwningPlayerController, int32 InTeamId);

};

UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwayBuildableDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /* -------- Buildables -------- */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buildables")
    TSubclassOf<ABuildableActor> BuildableActorClass;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mesh")
    TObjectPtr<USkeletalMesh> PrimaryBuildableMesh;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mesh")
    TArray<TObjectPtr<USkeletalMesh>> CosmeticBuildableMeshes;

};
/**
 * Base class for Turret-type buildables.
 * Shares common logic for turrets like target acquisition and firing.
 */
UCLASS(Blueprintable, BlueprintType)
class BREAKAWAYCORERUNTIME_API ATurretBase : public ABuildableActor
{
    GENERATED_BODY()

public:
    ATurretBase();

protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;  // Or your custom detection component

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurretAI|Perception")
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
    float AttackRadius = 1000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
    float FireRate = 1.0f; // Shots per second

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
    float MinAttackRange = 0.0f;  // For buildables like Ballista that have a deadzone 

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
    bool bCanTargetEnemyBuildables = true;  // 

    FTimerHandle FireTimerHandle;
    TWeakObjectPtr<AActor> CurrentTargetActor;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    virtual void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    virtual void AcquireNewTarget();
    virtual void AttackTarget();
    virtual bool IsTargetValid(AActor* Target) const;
    virtual bool IsHostile(AActor* ActorToTest) const; // Helper to determine if an actor is an enemy

    // Override to implement specific firing logic
    UFUNCTION(BlueprintNativeEvent, Category = "TurretAI")
    void FireAtTargetVisuals(AActor* Target);
    virtual void FireAtTargetVisuals_Implementation(AActor* Target);

public:
    // Called from the timer to actually fire
    virtual void PerformFireAction();
};


/**
 * Base class for Trap-type buildables.
 * Shares common logic for traps like triggering effects on proximity or contact.
 */
UCLASS(Blueprintable, BlueprintType)
class BREAKAWAYCORERUNTIME_API ATrapBase : public ABuildableActor
{
    GENERATED_BODY()

public:
    ATrapBase();

protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> TriggerVolume; // Or BoxComponent, etc.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
    bool bDestroyOnTrigger = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
    float EffectRadius = 300.0f;

    virtual void BeginPlay() override;

    UFUNCTION()
    virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // Override to implement specific trap effect
    UFUNCTION(BlueprintNativeEvent, Category = "Trap")
    void ApplyTrapEffect(AActor* TargetActor);
    virtual void ApplyTrapEffect_Implementation(AActor* TargetActor);
};
