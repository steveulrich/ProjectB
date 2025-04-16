#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "BreakawayGameMode.h"
#include "Player/LyraPlayerState.h"

ARelicActor::ARelicActor()
{
    // Enable ticking and replication
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicatingMovement(true); // Important for physics objects
    SetNetUpdateFrequency(66.0f);
    SetMinNetUpdateFrequency(33.0f);

    // Create components
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
    InteractionSphere->SetSphereRadius(150.0f);
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ARelicActor::OnInteractionSphereBeginOverlap);

    RelicMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicMesh"));
    SetRootComponent(RelicMesh);
    RelicMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    RelicMesh->SetSimulatePhysics(false);

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
    StateMachine = FRelicStateMachine();
    StateMachine.OnStateChanged.AddUObject(this, &ARelicActor::HandleStateChanged);
    
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
        StateMachine.RequestStateChange(ERelicState::Neutral);
    }

    // Attach other components to the main relic mesh
    InteractionSphere->AttachToComponent(RelicMesh, FAttachmentTransformRules::KeepRelativeTransform);
    ActiveEffectComponent->AttachToComponent(RelicMesh, FAttachmentTransformRules::KeepRelativeTransform);
    AudioComponent->AttachToComponent(RelicMesh, FAttachmentTransformRules::KeepRelativeTransform);

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
    //StateMachine.UpdateState(DeltaTime);
    
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
        Character->IsRequestingRelic = false;

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

void ARelicActor::ThrowRelic(const FVector& ThrowVelocity)
{
    if (HasAuthority() && IsCarried())
    {
        // Detach the relic from the current carrier
        DetachFromCharacter();

        // Enable physics simulation
        ConfigurePhysics(true);

        // Apply the throw velocity as an impulse
        if (RelicMesh && RelicMesh->IsSimulatingPhysics())
        {
            RelicMesh->AddImpulse(ThrowVelocity, NAME_None, true); // true for velocity change
        }
        
        StateMachine.RequestStateChange(ERelicState::Dropped);

    }
}

bool ARelicActor::TryScore(int32 ScoringTeam)
{
    // Validate the scoring attempt
    if (StateMachine.CurrentState != ERelicState::Carried || 
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
        ServerTryScore(ScoringTeam);
        
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
    
    // If there's still a carrier, remove effects
    if (StateMachine.CurrentCarrier)
    {
        RemoveCarrierEffects(StateMachine.CurrentCarrier);
    }
    
    // Transition to resetting state
    if (StateMachine.RequestStateChange(ERelicState::Resetting))
    {
        // Detach from any carrier
        DetachFromCharacter();
        
        // Configure physics
        ConfigurePhysics(true);
    }
}

void ARelicActor::AttachToCharacter(ABwayCharacterWithAbilities* Character)
{
    if (!Character)
    {
        return;
    }
    RelicMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    RelicMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    
    // Disable physics
    ConfigurePhysics(false);

    // Attach to socket on character
    RelicMesh->AttachToComponent(
        Character->GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        FName(RelicSettings->RelicSocket)  // This socket would need to be created on the character mesh, lets put this on RelicSettings
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

    // Get the Pawn and Controller
    APawn* Pawn = Cast<APawn>(Character);
    if (!Pawn)
        return;
        
    AController* Controller = Pawn->GetController();
    if (!Controller)
        return;
    
    // Get the PlayerState
    ALyraPlayerState* PlayerState = Cast<ALyraPlayerState>(Controller->PlayerState);
    if (!PlayerState)
        return;

    ULyraAbilitySystemComponent* LyraASC = PlayerState->GetLyraAbilitySystemComponent();
    if (!LyraASC)
        return;
    
    
    // Clear any previous effect handles
    ActiveEffectHandles.Empty();
    
    // Apply gameplay effect for movement speed modification
    if (RelicSettings->CarrierEffect)
    {
        FGameplayEffectContextHandle EffectContext = LyraASC->MakeEffectContext();
        EffectContext.AddSourceObject(this);
        
        FGameplayEffectSpecHandle EffectSpec = LyraASC->MakeOutgoingSpec(
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
            FActiveGameplayEffectHandle EffectHandle = LyraASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data);
            if (EffectHandle.IsValid())
            {
                ActiveEffectHandles.Add(EffectHandle);
            }
        }
    }

    // Grant ability set if configured
    if (RelicSettings->RelicAbilitySet != nullptr)
    {
        RelicSettings->RelicAbilitySet->GiveToAbilitySystem(LyraASC, &RelicAbilityHandles);
    }
    
    // Apply carrier tag
    if (RelicSettings->CarrierTag.IsValid())
    {
        LyraASC->AddLooseGameplayTag(RelicSettings->CarrierTag);
    }
    
    // Apply ability restriction tags
    if (RelicSettings->bRestrictAbilitiesWhileCarrying && !RelicSettings->RestrictedAbilityTags.IsEmpty())
    {
        for (const FGameplayTag& Tag : RelicSettings->RestrictedAbilityTags)
        {
            LyraASC->AddLooseGameplayTag(Tag);
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
    ULyraAbilitySystemComponent* LyraASC = Character->GetLyraAbilitySystemComponent();
    if (!LyraASC)
    {
        return;
    }
    
    // Remove all applied gameplay effects
    for (const FActiveGameplayEffectHandle& EffectHandle : ActiveEffectHandles)
    {
        if (EffectHandle.IsValid())
        {
            LyraASC->RemoveActiveGameplayEffect(EffectHandle);
        }
    }
    
    // Clear effect handles
    ActiveEffectHandles.Empty();
    
    // Remove carrier tag
    if (RelicSettings && RelicSettings->CarrierTag.IsValid())
    {
        LyraASC->RemoveLooseGameplayTag(RelicSettings->CarrierTag);
    }
    
    // Remove restricted ability tags
    if (RelicSettings && RelicSettings->bRestrictAbilitiesWhileCarrying && !RelicSettings->RestrictedAbilityTags.IsEmpty())
    {
        for (const FGameplayTag& Tag : RelicSettings->RestrictedAbilityTags)
        {
            LyraASC->RemoveLooseGameplayTag(Tag);
        }
    }
    
    // Remove granted ability set
    RelicAbilityHandles.TakeFromAbilitySystem(LyraASC);
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

void ARelicActor::ServerTryScore_Implementation(int32 ScoringTeam)
{
    TryScore(ScoringTeam);
}

bool ARelicActor::ServerTryScore_Validate(int32 ScoringTeam)
{
    // Basic validation
    return ScoringTeam >= 0;
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
        ConfigurePhysics(true);
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

    if (Character->IsRequestingRelic)
    {
        TryPickup(Character);
    }
}

void ARelicActor::HandleStateChanged(ERelicState NewState, ERelicState PreviousState, ABwayCharacterWithAbilities* Carrier)
{
    // Handle state-specific logic
    switch (NewState)
    {
        case ERelicState::Neutral:
            ConfigurePhysics(true);
            break;
            
        case ERelicState::Carried:
            // ConfigurePhysics() is called within AttachToCharacter()
            if (Carrier)
            {
                AttachToCharacter(Carrier);
            }
            break;
            
        case ERelicState::Dropped:
            DetachFromCharacter();
            ConfigurePhysics(true);
            
            break;
            
        case ERelicState::Scoring:
            // Logic handled in TryScore
            break;
            
        case ERelicState::Resetting:
            DetachFromCharacter();
            ConfigurePhysics(false); // TODO: Check if this needs to be true?
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
        ForceNetUpdate();
        MulticastOnStateChanged(NewState, PreviousState);
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

void ARelicActor::ConfigurePhysics(bool bEnablePhysics)
{
    if (!RelicMesh || !RelicSettings)
    {
        return;
    }
    
    RelicMesh->SetSimulatePhysics(bEnablePhysics);
    RelicMesh->SetCollisionEnabled(bEnablePhysics ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly);

    // Apply collision profile based on current state
    FName CollisionProfileName;
    
    switch (StateMachine.CurrentState)
    {
        case ERelicState::Neutral:
            CollisionProfileName = RelicSettings->NeutralCollisionProfileName;
            break;
            
        case ERelicState::Carried:
            CollisionProfileName = RelicSettings->CarriedCollisionProfileName;
            break;
            
        case ERelicState::Dropped:
            CollisionProfileName = RelicSettings->DroppedCollisionProfileName;
            break;
            
        case ERelicState::Scoring:
            CollisionProfileName = RelicSettings->ScoringCollisionProfileName;
            break;
            
        case ERelicState::Resetting:
            CollisionProfileName = RelicSettings->ResettingCollisionProfileName;
            break;
            
        case ERelicState::Inactive:
        default:
            CollisionProfileName = RelicSettings->InactiveCollisionProfileName;
            break;
    }
    
    RelicMesh->SetCollisionProfileName(CollisionProfileName);

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
    // TODO: Apply Knockout/ForcedDrop impulse in the direction of the relic spawn, with increased magnitude based on distance
    
    // Toss straight up!
    FVector ImpulseDirection = FVector::UpVector;
    float ImpulseMagnitude = RelicSettings->DropImpulseMultiplier;
    
    RelicMesh->AddImpulse(ImpulseDirection * ImpulseMagnitude, NAME_None, true);
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
            
            if (StateMachine.CurrentState != ERelicState::Carried)
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