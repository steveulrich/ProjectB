// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildable/BuildableBase.h"
#include "Net/UnrealNetwork.h" // Required for DOREPLIFETIME
#include "Engine/ActorChannel.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h" // For ATrapBase
#include "TimerManager.h" // Required for FTimerManager
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Player/LyraPlayerState.h"


const FName ABuildableActor::MeshComponentName = TEXT("MeshComponent");

// --- ABuildableActor ---

ABuildableActor::ABuildableActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true; // Buildables should replicate

    MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(MeshComponentName);
    RootComponent = MeshComponent;
}

void ABuildableActor::InitializeAbilitySystem()
{
    Super::InitializeAbilitySystem();

    HealthSet->InitMaxHealth(100.f);
    HealthSet->InitHealth(1.f);
}

void ABuildableActor::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (BuildTime > 0.0f)
        {
            bIsActive = false;
            GetWorldTimerManager().SetTimer(BuildTimerHandle, this, &ABuildableActor::FinishBuilding, BuildTime, false);
        }
        else
        {
            FinishBuilding(); // No build time, activate immediately
        }
    }
}

void ABuildableActor::InitializeBuildable(AController* InOwningPlayerController, int32 InTeamId)
{
    if (HasAuthority())
    {
        // If already placed and re-initialized (e.g. round start), re-evaluate build state
        if (BuildTime > 0.0f && !bIsActive && !GetWorldTimerManager().IsTimerActive(BuildTimerHandle))
        {
             GetWorldTimerManager().SetTimer(BuildTimerHandle, this, &ABuildableActor::FinishBuilding, BuildTime, false);
        }
        else if (BuildTime <= 0.0f)
        {
            FinishBuilding();
        }
    }
}

void ABuildableActor::FinishBuilding()
{
    bIsActive = true;
    // UE_LOG(LogTemp, Log, TEXT("Buildable %s finished building and is now active."), *GetName());
    // Additional logic for when building completes (e.g., play a sound, enable functionality)
}

// --- ATurretBase ---

ATurretBase::ATurretBase()
{
    PrimaryActorTick.bCanEverTick = true;

    AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

    if (SightConfig)
    {
        SightConfig->SightRadius = AttackRadius; // Use AttackRadius from your previous setup
        SightConfig->LoseSightRadius = AttackRadius + 500.0f; // Typically larger than SightRadius
        SightConfig->PeripheralVisionAngleDegrees = 90.0f; // Configure this with data
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

        // Configure what senses are part of this perception component
        AIPerceptionComponent->ConfigureSense(*SightConfig);
        AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
    }

    // Set default values from the document
    AttackRadius = 1000.0f; // Default, can be overridden by derived classes or data
    FireRate = 1.0f; // [cite: 155]
}

void ATurretBase::BeginPlay()
{
    Super::BeginPlay();

    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ATurretBase::OnTargetPerceptionUpdated);
        if ( bIsActive ) // bIsActive would be set when building completes
        {
            AIPerceptionComponent->Activate();
        }
        AcquireNewTarget(); // Initial target scan
    }
}

void ATurretBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentTargetActor.IsValid())
    {
        // Optional: Add logic to check if target is still valid (e.g., in range, line of sight if not handled by perception losing sight)
        // For turrets, you might want to rotate towards the target here
    }
    else if (bIsActive)
    {
        // If no current target and active, try to find one
        AcquireNewTarget();
    }
}
void ATurretBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!bIsActive || !Actor) return;

    if (Stimulus.WasSuccessfullySensed()) // Target Detected or Re-detected
    {
        if (IsTargetValid(Actor))
        {
            if (!CurrentTargetActor.IsValid() || Actor == CurrentTargetActor.Get()) // New target or current target confirmed
            {
                CurrentTargetActor = Actor;
                // UE_LOG(LogTemp, Warning, TEXT("Turret %s Acquired Target: %s"), *GetNameSafe(this), *GetNameSafe(Actor));
                AttackTarget();
            }
            else
            {
                // Potentially switch target if the new stimulus is for a "better" target (e.g. closer, higher priority)
                // For now, we'll stick to the first valid target.
            }
        }
    }
    else // Target Lost
    {
        if (Actor == CurrentTargetActor.Get())
        {
            // UE_LOG(LogTemp, Warning, TEXT("Turret %s Lost Target: %s"), *GetNameSafe(this), *GetNameSafe(Actor));
            CurrentTargetActor = nullptr;
            GetWorldTimerManager().ClearTimer(FireTimerHandle);
            AcquireNewTarget(); // Try to find a new target
        }
    }
}

void ATurretBase::AcquireNewTarget()
{
    if (!bIsActive || !AIPerceptionComponent) return;

    TArray<AActor*> PerceivedActors;
    AIPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

    AActor* BestTarget = nullptr;
    float BestTargetDistSq = MAX_FLT;

    for (AActor* PerceivedActor : PerceivedActors)
    {
        if (IsTargetValid(PerceivedActor))
        {
            float DistSq = FVector::DistSquared(GetActorLocation(), PerceivedActor->GetActorLocation());
            if (DistSq < BestTargetDistSq)
            {
                // Check MinAttackRange
                if (MinAttackRange > 0.f && DistSq < FMath::Square(MinAttackRange))
                {
                    continue; // Target is too close
                }
                BestTarget = PerceivedActor;
                BestTargetDistSq = DistSq;
            }
        }
    }

    if (BestTarget)
    {
        CurrentTargetActor = BestTarget;
        // UE_LOG(LogTemp, Warning, TEXT("Turret %s Acquired New Target (from scan): %s"), *GetNameSafe(this), *GetNameSafe(BestTarget));
        AttackTarget();
    }
    else
    {
        CurrentTargetActor = nullptr;
        GetWorldTimerManager().ClearTimer(FireTimerHandle);
    }
}

bool ATurretBase::IsHostile(AActor* ActorToTest) const
{
    if (!ActorToTest)
    {
        return false;
    }

    // Hostility check using Lyra's team system if available, otherwise fallback to TeamID comparison.
    // Try to use Lyra's team system via IGenericTeamAgentInterface
    const IGenericTeamAgentInterface* TargetTeamAgent = Cast<IGenericTeamAgentInterface>(ActorToTest);
    const IGenericTeamAgentInterface* OwnerTeamAgent = nullptr;

    if (TargetTeamAgent && OwnerTeamAgent)
    {
        return OwnerTeamAgent->GetTeamAttitudeTowards(*ActorToTest) == ETeamAttitude::Hostile;
    }

    // Fallback: Use TeamID if available
    const ALyraPlayerState* TargetPS = Cast<ALyraPlayerState>(Cast<APawn>(ActorToTest)->GetPlayerState());
    if (TargetPS)
    {
        return FGenericTeamId::GetAttitude( TeamId , TargetPS->GetTeamId()) == ETeamAttitude::Hostile;
    }

    // Fallback: If ActorToTest is a buildable, compare TeamIDs
    const ABuildableActor* OtherBuildable = Cast<ABuildableActor>(ActorToTest);
    if (OtherBuildable)
    {
        return OtherBuildable->GetTeamId() != this->GetTeamId() && OtherBuildable->GetTeamId() != 0;
    }

    // Simpler custom check if not using Lyra's team system fully:
    // Assuming Your Characters/Pawns have a GetTeamId() method or PlayerStates do.
    // ABuildableActor* OtherBuildable = Cast<ABuildableActor>(ActorToTest);
    // if (OtherBuildable)
    // {
    //     return OtherBuildable->GetTeamId() != this->GetTeamId() && OtherBuildable->GetTeamId() != 0; // Team 0 might be neutral
    // }
    //
    // AYourGameCharacter* OtherCharacter = Cast<AYourGameCharacter>(ActorToTest);
    // if (OtherCharacter && OtherCharacter->GetPlayerState())
    // {
    //      AYourGamePlayerState* TargetPS = Cast<AYourGamePlayerState>(OtherCharacter->GetPlayerState());
    //      if(TargetPS) return TargetPS->GetTeamId() != this->GetTeamId() && TargetPS->GetTeamId() != 0;
    // }

    return false; // Default to not hostile if unsure
}


bool ATurretBase::IsTargetValid(AActor* Target) const
{
    if (!Target || !bIsActive)
    {
        return false;
    }

    // Check if target is alive (e.g., has a health component and health > 0)
    // UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>();
    // if (TargetHealth && TargetHealth->GetCurrentHealth() <= 0)
    // {
    //     return false;
    // }

    // Check distance against AttackRadius (SightConfig->SightRadius already handles initial detection)
    // but good for re-validation if target moves.
    if (FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) > FMath::Square(SightConfig->SightRadius))
    {
        return false;
    }

    // Check MinAttackRange if applicable
    if (MinAttackRange > 0.f && FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) < FMath::Square(MinAttackRange))
    {
        return false; // Target is too close
    }

    // Check hostility
    if (!IsHostile(Target))
    {
         // Also check if it's an enemy buildable if bCanTargetEnemyBuildables is true
        if (bCanTargetEnemyBuildables)
        {
            ABuildableActor* EnemyBuildable = Cast<ABuildableActor>(Target);
            if (EnemyBuildable && EnemyBuildable->GetTeamId() != 0 && EnemyBuildable->GetTeamId() != GetTeamId())
            {
                return true; // It's a valid enemy buildable
            }
        }
        return false; // Not hostile or not a valid buildable target
    }


    return true;
}

void ATurretBase::AttackTarget()
{
    if (CurrentTargetActor.IsValid() && bIsActive)
    {
        // Start the firing timer
        float EffectiveFireRate = FMath::Max(0.01f, FireRate); // Prevent division by zero or too rapid fire
        GetWorldTimerManager().SetTimer(FireTimerHandle, this, &ATurretBase::PerformFireAction, 1.0f / EffectiveFireRate, true);
    }
    else
    {
        GetWorldTimerManager().ClearTimer(FireTimerHandle);
    }
}

void ATurretBase::PerformFireAction()
{
    if (CurrentTargetActor.IsValid() && bIsActive && IsTargetValid(CurrentTargetActor.Get()))
    {
        // UE_LOG(LogTemp, Warning, TEXT("Turret %s Firing at %s"), *GetNameSafe(this), *GetNameSafe(CurrentTargetActor.Get()));
        FireAtTargetVisuals(CurrentTargetActor.Get()); // Call the BlueprintNativeEvent for visuals/projectile spawning
    }
    else
    {
        // Target became invalid or turret deactivated, stop firing and find new target
        CurrentTargetActor = nullptr;
        GetWorldTimerManager().ClearTimer(FireTimerHandle);
        AcquireNewTarget();
    }
}

void ATurretBase::FireAtTargetVisuals_Implementation(AActor* Target)
{
    // Base implementation - to be overridden in specific turrets like FireCatapult
    // This is where you would spawn projectiles, play sounds, VFX etc.
    // UE_LOG(LogTemp, Warning, TEXT("ATurretBase::FireAtTarget_Implementation - Override in derived classes to spawn projectiles/effects."));
}

// Ensure to add "AIModule" to your project's Build.cs file if it's not already there:
// PrivateDependencyModuleNames.AddRange(new string[] { ..., "AIModule" });

// --- ATrapBase ---

ATrapBase::ATrapBase()
{
    TriggerVolume = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerVolume"));
    TriggerVolume->SetupAttachment(RootComponent);
    TriggerVolume->SetSphereRadius(300.0f); // Default, will be overridden by EffectRadius
    TriggerVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic")); // Or a custom profile
}

void ATrapBase::BeginPlay()
{
    Super::BeginPlay();

    if (TriggerVolume)
    {
        TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ATrapBase::OnOverlapBegin);
        TriggerVolume->SetSphereRadius(EffectRadius); // Apply configured radius
    }
}

void ATrapBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bIsActive || bIsDestroyed || OtherActor == this) return;

    // Add team checks and other validation as needed
    // For example, ensure the trap doesn't trigger on friendly units or other traps.
    // AController* MyController = GetOwningPlayerController();
    // if (MyController)
    // {
    //     // Example: Check if OtherActor is hostile
    //     // This requires a robust team/allegiance system
    // }

    ApplyTrapEffect(OtherActor); // Call BlueprintNativeEvent

    if (bDestroyOnTrigger && HasAuthority())
    {
        OnDestroyed.Broadcast(this); // Destroy the trap after it triggers
    }
}

void ATrapBase::ApplyTrapEffect_Implementation(AActor* TargetActor)
{
    // Native C++ implementation for the trap's effect.
    // This could be applying damage, a status effect, playing sounds/VFX.
    // UE_LOG(LogTemp, Log, TEXT("Trap %s triggered by %s"), *GetName(), *TargetActor->GetName());
}