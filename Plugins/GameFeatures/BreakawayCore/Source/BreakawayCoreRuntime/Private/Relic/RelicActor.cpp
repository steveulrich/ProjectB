#include "Relic/RelicActor.h"
#include "Relic/RelicGameplayEffects.h"
#include "BwayCharacterWithAbilities.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BreakawayGameMode.h"

ARelicActor::ARelicActor()
{
    // Enable ticking and replication
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    // Create components
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    SetRootComponent(InteractionSphere);
    InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
    InteractionSphere->SetSphereRadius(150.0f);
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ARelicActor::OnInteractionSphereBeginOverlap);

    RelicMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicMesh"));
    RelicMesh->SetupAttachment(RootComponent);
    RelicMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    RelicMesh->SetSimulatePhysics(false);
    RelicMesh->SetGenerateOverlapEvents(true);

    ActiveEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ActiveEffectComponent"));
    ActiveEffectComponent->SetupAttachment(RelicMesh);
    ActiveEffectComponent->SetAutoActivate(false);

    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);
    AudioComponent->SetAutoActivate(false);

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // Initialize variables
    CurrentSpawnLocationIndex = 0;
    bIsPredictingPickup = false;
    PredictedCarrier = nullptr;
}

void ARelicActor::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize state machine
    StateMachine.OnStateChanged.AddUObject(this, &ARelicActor::HandleStateChanged);
    
    // Set initial state to Inactive
    if (HasAuthority())
    {
        StateMachine.RequestStateChange(ERelicState::Inactive);
    }
    
    // Set interaction sphere radius from settings
    if (RelicSettings)
    {
        InteractionSphere->SetSphereRadius(RelicSettings->PickupRadius);
    }
    
    // Load and set up mesh and materials
    if (RelicSettings && !RelicSettings->RelicMesh.IsNull())
    {
        UStaticMesh* LoadedMesh = RelicSettings->RelicMesh.LoadSynchronous();
        if (LoadedMesh)
        {
            RelicMesh->SetStaticMesh(LoadedMesh);
        }
        
        UMaterialInterface* LoadedMaterial = RelicSettings->RelicMaterial.LoadSynchronous();
        if (LoadedMaterial)
        {
            RelicMesh->SetMaterial(0, LoadedMaterial);
        }
    }
    
    // Initialize physics properties
    if (RelicSettings)
    {
        RelicMesh->SetMassOverrideInKg(NAME_None, RelicSettings->RelicMass);
        RelicMesh->SetLinearDamping(RelicSettings->LinearDamping);
        RelicMesh->SetAngularDamping(RelicSettings->AngularDamping);
    }
    
    // If we're the authority, start the game with the Relic in Neutral state
    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimerForNextTick([this]()
        {
            StateMachine.RequestStateChange(ERelicState::Neutral);
            
            // Move to spawn location
            SetActorLocation(GetNextSpawnLocation());
        });
    }
}

void ARelicActor::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    
    // Additional initialization after components are set up
}

void ARelicActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update state machine
    StateMachine.UpdateState(DeltaTime);
    
    // Client prediction reconciliation
    if (bIsPredictingPickup && StateMachine.CurrentState == ERelicState::Carried && 
        StateMachine.CurrentCarrier == PredictedCarrier)
    {
        // Successful prediction
        ReconcileAfterPrediction(true);
    }
        // Client prediction reconciliation continued
    else if (bIsPredictingPickup && 
             (StateMachine.CurrentState != ERelicState::Carried || 
              StateMachine.CurrentCarrier != PredictedCarrier))
    {
        // Failed prediction
        ReconcileAfterPrediction(false);
    }
    
    // Networking smoothing for remote clients when the Relic is being carried
    if (!HasAuthority() && StateMachine.CurrentState == ERelicState::Carried && 
        StateMachine.CurrentCarrier && !StateMachine.CurrentCarrier->IsLocallyControlled())
    {
        // Implement smooth following of the carrier for remote clients
        // This would be more complex in a real implementation
    }
}


void ARelicActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate state machine and its components
    DOREPLIFETIME(ARelicActor, StateMachine);
    DOREPLIFETIME(ARelicActor, CurrentCarrier);
    DOREPLIFETIME(ARelicActor, LastPossessingTeam);
}

void ARelicActor::OnRep_StateMachine()
{
    // Handle state machine replication
    StateMachine.OnStateChanged.Broadcast(
        StateMachine.CurrentState,
        StateMachine.PreviousState,
        StateMachine.CurrentCarrier
    );
    
    // Update visuals and gameplay based on new state
    HandleStateChanged(
        StateMachine.CurrentState,
        StateMachine.PreviousState,
        StateMachine.CurrentCarrier
    );
}
bool ARelicActor::TryPickup(ABwayCharacterWithAbilities* Character)
{
    // Local validation
    if (!Character || !IsAvailableForPickup())
    {
        return false;
    }
    
    // Check if we're on server or client
    if (HasAuthority())
    {
        // Server-side pickup
        return StateMachine.RequestStateChange(ERelicState::Carried, Character);
    }
    else
    {
        // Client-side prediction
        if (Character->IsLocallyControlled())
        {
            bIsPredictingPickup = true;
            PredictedCarrier = Character;
            
            // Send request to server
            ServerTryPickup(Character);
            
            // Apply visual effects immediately for responsive feedback
            AttachToCharacter(Character);
            UpdateVisualEffects(ERelicState::Carried);
            PlayStateChangeSound(ERelicState::Carried, StateMachine.CurrentState);
            
            return true;
        }
        else
        {
            // Request server pickup for remote characters
            ServerTryPickup(Character);
            return false;
        }
    }
}

bool ARelicActor::DropRelic(bool bIsIntentional)
{
    // Check if we're in a state where dropping is allowed
    if (StateMachine.CurrentState != ERelicState::Carried || !StateMachine.CurrentCarrier)
    {
        return false;
    }
    
    // Cache the carrier for impulse calculation
    ABwayCharacterWithAbilities* PreviousCarrier = StateMachine.CurrentCarrier;
    
    // Check if we're on server or client
    if (HasAuthority())
    {
        // Server-side drop
        if (StateMachine.RequestStateChange(ERelicState::Dropped))
        {
            // Apply physics impulse
            ApplyDropImpulse(PreviousCarrier);
            
            // Start dropped timeout timer
            if (RelicSettings)
            {
                GetWorldTimerManager().SetTimer(
                    DroppedTimeoutHandle,
                    this,
                    &ARelicActor::HandleDroppedTimeout,
                    RelicSettings->DroppedStateDuration
                );
            }
            
            return true;
        }
        return false;
    }
    else
    {
        // Client-side prediction for local player
        if (PreviousCarrier && PreviousCarrier->IsLocallyControlled())
        {
            // Send request to server
            ServerDropRelic(bIsIntentional);
            
            // Apply visual changes immediately for responsive feedback
            DetachFromCharacter();
            ConfigurePhysics(true);
            UpdateVisualEffects(ERelicState::Dropped);
            PlayStateChangeSound(ERelicState::Dropped, ERelicState::Carried);
            
            // Apply impulse locally
            ApplyDropImpulse(PreviousCarrier);
            
            return true;
        }
        else
        {
            // Request server for remote characters
            ServerDropRelic(bIsIntentional);
            return false;
        }
    }
}

bool ARelicActor::TryScore(ABwayCharacterWithAbilities* Character, int32 ScoringTeam)
{
    // Validate the scoring attempt
    if (StateMachine.CurrentState != ERelicState::Carried || 
        StateMachine.CurrentCarrier != Character || 
        StateMachine.LastPossessingTeam == ScoringTeam)
    {
        return false;
    }
    
    // Check if we're on server or client
    if (HasAuthority())
    {
        // Server-side scoring
        if (StateMachine.RequestStateChange(ERelicState::Scoring))
        {
            // Notify game mode of score
            ABreakawayGameMode* GameMode = Cast<ABreakawayGameMode>(GetWorld()->GetAuthGameMode());
            if (GameMode)
            {
                //TODO: GameMode->HandleRelicScore(Character, ScoringTeam);
            }
            
            // Start scoring confirmation timer
            if (RelicSettings)
            {
                GetWorldTimerManager().SetTimer(
                    ScoringCompleteHandle,
                    this,
                    &ARelicActor::HandleScoringComplete,
                    RelicSettings->ScoringConfirmationDuration
                );
            }
            
            return true;
        }
        return false;
    }
    else
    {
        // Client-side request
        ServerTryScore(Character, ScoringTeam);
        
        // No client-side prediction for scoring as it's a critical game state change
        return false;
    }
}

void ARelicActor::ResetRelic()
{
    if (!HasAuthority())
    {
        return;
    }
    
    // Transition to resetting state
    if (StateMachine.RequestStateChange(ERelicState::Resetting))
    {
        // Detach from any carrier
        DetachFromCharacter();
        
        // Move to spawn location
        SetActorLocation(GetNextSpawnLocation());
        
        // Configure physics
        ConfigurePhysics(false);
        
        // Start reset timer
        if (RelicSettings)
        {
            GetWorldTimerManager().SetTimer(
                ResetCompleteHandle,
                this,
                &ARelicActor::HandleResetComplete,
                RelicSettings->ResetDuration
            );
        }
    }
}

void ARelicActor::AttachToCharacter(ABwayCharacterWithAbilities* Character)
{
    if (!Character)
    {
        return;
    }
    
    // Disable physics
    ConfigurePhysics(false);
    
    // Attach to socket on character
    RelicMesh->AttachToComponent(
        Character->GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        FName("RelicSocket")  // This socket would need to be created on the character mesh
    );
    
    // Apply gameplay effects to carrier
    ApplyCarrierEffects(Character);
}

void ARelicActor::DetachFromCharacter()
{
    // Remove gameplay effects from carrier
    if (StateMachine.CurrentCarrier)
    {
        RemoveCarrierEffects(StateMachine.CurrentCarrier);
    }
    
    // Detach from character
    RelicMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    
    // Make sure root component location is updated
    SetActorLocation(RelicMesh->GetComponentLocation());
}

void ARelicActor::ApplyCarrierEffects(ABwayCharacterWithAbilities* Character)
{
    if (!Character || !RelicSettings)
    {
        return;
    }
    
    // Get the ability system component from the character
    UAbilitySystemComponent* CharacterASC = Character->GetAbilitySystemComponent();
    if (!CharacterASC)
    {
        return;
    }
    
    // Clear any previous effect handles
    ActiveEffectHandles.Empty();
    
    // Apply gameplay effect for movement speed modification
    if (RelicSettings->CarrierEffect)
    {
        FGameplayEffectContextHandle EffectContext = CharacterASC->MakeEffectContext();
        EffectContext.AddSourceObject(this);
        
        FGameplayEffectSpecHandle EffectSpec = CharacterASC->MakeOutgoingSpec(
            RelicSettings->CarrierEffect,
            1.0f,
            EffectContext
        );
        
        if (EffectSpec.IsValid())
        {
            // Set the carrier speed modifier as a SetByCallerMagnitude
            FName SpeedMagnitudeParam = FName("Data.MovementSpeed");
            EffectSpec.Data->SetSetByCallerMagnitude(SpeedMagnitudeParam, RelicSettings->CarrierSpeedModifier);
            
            // Apply the effect
            FActiveGameplayEffectHandle EffectHandle = CharacterASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data);
            if (EffectHandle.IsValid())
            {
                ActiveEffectHandles.Add(EffectHandle);
            }
        }
    }
    
    // Apply carrier tag
    if (RelicSettings->CarrierTag.IsValid())
    {
        CharacterASC->AddLooseGameplayTag(RelicSettings->CarrierTag);
    }
    
    // Apply ability restriction tags
    if (RelicSettings->bRestrictAbilitiesWhileCarrying && !RelicSettings->RestrictedAbilityTags.IsEmpty())
    {
        for (const FGameplayTag& Tag : RelicSettings->RestrictedAbilityTags)
        {
            CharacterASC->AddLooseGameplayTag(Tag);
        }
    }
}

void ARelicActor::RemoveCarrierEffects(ABwayCharacterWithAbilities* Character)
{
    if (!Character)
    {
        return;
    }
    
    // Get the ability system component from the character
    UAbilitySystemComponent* CharacterASC = Character->GetAbilitySystemComponent();
    if (!CharacterASC)
    {
        return;
    }
    
    // Remove all applied gameplay effects
    for (const FActiveGameplayEffectHandle& EffectHandle : ActiveEffectHandles)
    {
        if (EffectHandle.IsValid())
        {
            CharacterASC->RemoveActiveGameplayEffect(EffectHandle);
        }
    }
    
    // Clear effect handles
    ActiveEffectHandles.Empty();
    
    // Remove carrier tag
    if (RelicSettings && RelicSettings->CarrierTag.IsValid())
    {
        CharacterASC->RemoveLooseGameplayTag(RelicSettings->CarrierTag);
    }
    
    // Remove restricted ability tags
    if (RelicSettings && RelicSettings->bRestrictAbilitiesWhileCarrying && !RelicSettings->RestrictedAbilityTags.IsEmpty())
    {
        for (const FGameplayTag& Tag : RelicSettings->RestrictedAbilityTags)
        {
            CharacterASC->RemoveLooseGameplayTag(Tag);
        }
    }
}

void ARelicActor::ServerTryPickup_Implementation(ABwayCharacterWithAbilities* Character)
{
    TryPickup(Character);
}

bool ARelicActor::ServerTryPickup_Validate(ABwayCharacterWithAbilities* Character)
{
    // Basic validation
    return Character != nullptr;
}

void ARelicActor::ServerDropRelic_Implementation(bool bIsIntentional)
{
    DropRelic(bIsIntentional);
}

bool ARelicActor::ServerDropRelic_Validate(bool bIsIntentional)
{
    // All inputs are valid
    return true;
}

void ARelicActor::ServerTryScore_Implementation(ABwayCharacterWithAbilities* Character, int32 ScoringTeam)
{
    TryScore(Character, ScoringTeam);
}

bool ARelicActor::ServerTryScore_Validate(ABwayCharacterWithAbilities* Character, int32 ScoringTeam)
{
    // Basic validation
    return Character != nullptr && ScoringTeam >= 0;
}

void ARelicActor::MulticastOnStateChanged_Implementation(ERelicState NewState, ERelicState PreviousState)
{
    // Skip if we're the authority (already handled)
    if (HasAuthority())
    {
        return;
    }
    
    // Update visual effects
    UpdateVisualEffects(NewState);
    
    // Play sound effects
    PlayStateChangeSound(NewState, PreviousState);
    
    // Handle physics and attachments
    if (NewState == ERelicState::Carried)
    {
        if (StateMachine.CurrentCarrier)
        {
            AttachToCharacter(StateMachine.CurrentCarrier);
        }
        ConfigurePhysics(false);
    }
    else if (NewState == ERelicState::Dropped)
    {
        DetachFromCharacter();
        ConfigurePhysics(true);
    }
    else if (NewState == ERelicState::Neutral || NewState == ERelicState::Resetting)
    {
        DetachFromCharacter();
        ConfigurePhysics(false);
    }
}

void ARelicActor::OnInteractionSphereBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    // Check if the Relic is available for pickup
    if (!IsAvailableForPickup())
    {
        return;
    }
    
    // Check if the overlapping actor is a character
    ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(OtherActor);
    if (!Character)
    {
        return;
    }
    
    // Auto-pickup for simplicity (in a real game, you might require a button press)
    TryPickup(Character);
}

void ARelicActor::HandleStateChanged(ERelicState NewState, ERelicState PreviousState, ABwayCharacterWithAbilities* Carrier)
{
    // Handle state-specific logic
    switch (NewState)
    {
        case ERelicState::Neutral:
            ConfigurePhysics(false);
            break;
            
        case ERelicState::Carried:
            if (Carrier)
            {
                AttachToCharacter(Carrier);
            }
            ConfigurePhysics(false);
            break;
            
        case ERelicState::Dropped:
            DetachFromCharacter();
            ConfigurePhysics(true);
            
            // Start dropped timeout timer
            if (HasAuthority() && RelicSettings)
            {
                GetWorldTimerManager().SetTimer(
                    DroppedTimeoutHandle,
                    this,
                    &ARelicActor::HandleDroppedTimeout,
                    RelicSettings->DroppedStateDuration
                );
            }
            break;
            
        case ERelicState::Scoring:
            // Logic handled in TryScore
            break;
            
        case ERelicState::Resetting:
            DetachFromCharacter();
            ConfigurePhysics(false);
            break;
            
        case ERelicState::Inactive:
            DetachFromCharacter();
            ConfigurePhysics(false);
            break;
    }
    
    // Update visual effects
    UpdateVisualEffects(NewState);
    
    // Play state change sound
    PlayStateChangeSound(NewState, PreviousState);
    
    // Notify clients about state change
    if (HasAuthority())
    {
        MulticastOnStateChanged(NewState, PreviousState);
    }
}

void ARelicActor::HandleDroppedTimeout()
{
    if (HasAuthority() && StateMachine.CurrentState == ERelicState::Dropped)
    {
        // Reset to neutral state
        StateMachine.RequestStateChange(ERelicState::Neutral);
        
        // Move to spawn location
        SetActorLocation(GetNextSpawnLocation());
    }
}

void ARelicActor::HandleScoringComplete()
{
    if (HasAuthority() && StateMachine.CurrentState == ERelicState::Scoring)
    {
        // Begin the reset process
        ResetRelic();
    }
}

void ARelicActor::HandleResetComplete()
{
    if (HasAuthority() && StateMachine.CurrentState == ERelicState::Resetting)
    {
        // Return to neutral state
        StateMachine.RequestStateChange(ERelicState::Neutral);
    }
}

FVector ARelicActor::GetNextSpawnLocation()
{
    if (!RelicSettings)
    {
        return FVector::ZeroVector;
    }
    
    // Check if we have any spawn locations defined
    if (RelicSettings->SpawnLocations.Num() > 0)
    {
        if (RelicSettings->bUseRandomSpawnLocation)
        {
            // Random spawn location
            int32 RandomIndex = FMath::RandRange(0, RelicSettings->SpawnLocations.Num() - 1);
            return RelicSettings->SpawnLocations[RandomIndex];
        }
        else
        {
            // Round-robin spawn location
            if (CurrentSpawnLocationIndex >= RelicSettings->SpawnLocations.Num())
            {
                CurrentSpawnLocationIndex = 0;
            }
            
            FVector Location = RelicSettings->SpawnLocations[CurrentSpawnLocationIndex];
            CurrentSpawnLocationIndex++;
            return Location;
        }
    }
    
    // Fallback to default spawn location
    return RelicSettings->DefaultSpawnLocation;
}

void ARelicActor::ConfigurePhysics(bool bEnablePhysics)
{
    if (RelicMesh)
    {
        RelicMesh->SetSimulatePhysics(bEnablePhysics);
        RelicMesh->SetCollisionEnabled(bEnablePhysics ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly);
    }
}

void ARelicActor::UpdateVisualEffects(ERelicState NewState)
{
    if (!RelicSettings || !ActiveEffectComponent)
    {
        return;
    }
    
    // Stop current effect
    ActiveEffectComponent->Deactivate();
    
    // Start appropriate effect based on state
    UNiagaraSystem* EffectToPlay = nullptr;
    
    switch (NewState)
    {
        case ERelicState::Neutral:
            EffectToPlay = RelicSettings->IdleEffect.LoadSynchronous();
            break;
            
        case ERelicState::Carried:
            EffectToPlay = RelicSettings->CarriedEffect.LoadSynchronous();
            break;
            
        case ERelicState::Dropped:
            EffectToPlay = RelicSettings->DroppedEffect.LoadSynchronous();
            break;
            
        case ERelicState::Scoring:
            EffectToPlay = RelicSettings->ScoringEffect.LoadSynchronous();
            break;
            
        default:
            break;
    }
    
    if (EffectToPlay)
    {
        ActiveEffectComponent->SetAsset(EffectToPlay);
        ActiveEffectComponent->Activate(true);
    }
}

void ARelicActor::PlayStateChangeSound(ERelicState NewState, ERelicState PreviousState)
{
    if (!RelicSettings || !AudioComponent)
    {
        return;
    }
    
    // Play appropriate sound based on state transition
    USoundBase* SoundToPlay = nullptr;
    
    if (PreviousState == ERelicState::Neutral && NewState == ERelicState::Carried ||
        PreviousState == ERelicState::Dropped && NewState == ERelicState::Carried)
    {
        // Pickup sound
        SoundToPlay = RelicSettings->PickupSound.LoadSynchronous();
    }
    else if (PreviousState == ERelicState::Carried && NewState == ERelicState::Dropped)
    {
        // Drop sound
        SoundToPlay = RelicSettings->DropSound.LoadSynchronous();
    }
    else if (NewState == ERelicState::Scoring)
    {
        // Scoring sound
        SoundToPlay = RelicSettings->ScoringSound.LoadSynchronous();
    }
    
    if (SoundToPlay)
    {
        AudioComponent->SetSound(SoundToPlay);
        AudioComponent->Play();
    }
}

void ARelicActor::ApplyDropImpulse(ABwayCharacterWithAbilities* PreviousCarrier)
{
    if (!RelicMesh || !PreviousCarrier || !RelicSettings)
    {
        return;
    }
    
    // Get carrier movement velocity
    FVector CarrierVelocity = PreviousCarrier->GetVelocity();
    
    // Apply impulse in the direction the carrier was moving
    if (!CarrierVelocity.IsNearlyZero())
    {
        FVector ImpulseDirection = CarrierVelocity.GetSafeNormal();
        float ImpulseMagnitude = CarrierVelocity.Size() * RelicSettings->DropImpulseMultiplier;
        
        RelicMesh->AddImpulse(ImpulseDirection * ImpulseMagnitude, NAME_None, true);
    }
}

void ARelicActor::ReconcileAfterPrediction(bool bSuccess)
{
    bIsPredictingPickup = false;
    PredictedCarrier = nullptr;
    
    if (!bSuccess)
    {
        // Prediction failed, revert to server state
        if (StateMachine.CurrentState == ERelicState::Carried && StateMachine.CurrentCarrier)
        {
            AttachToCharacter(StateMachine.CurrentCarrier);
        }
        else
        {
            DetachFromCharacter();
            
            if (StateMachine.CurrentState == ERelicState::Dropped)
            {
                ConfigurePhysics(true);
            }
            else
            {
                ConfigurePhysics(false);
            }
        }
        
        UpdateVisualEffects(StateMachine.CurrentState);
    }
}
