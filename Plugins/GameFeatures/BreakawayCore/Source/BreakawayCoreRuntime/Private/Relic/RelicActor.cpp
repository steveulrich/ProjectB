#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "Relic/RelicSettings.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h" // For FinishSpawningActor if needed later

ARelicActor::ARelicActor()
{
    // Enable ticking and replication
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;


    // Set the root component to replicate movement for physics [1, 2]
    // Note: RelicMesh MUST be the root component for bReplicateMovement to work correctly with physics simulation.
    RelicMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicMesh"));
    SetRootComponent(RelicMesh);
    RelicMesh->SetIsReplicated(true); // Replicate the component itself if needed

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(150.0f); // Example radius, configure via URelicSettings
    InteractionSphere->SetCollisionProfileName(FName("OverlapAllDynamic")); // Adjust profile as needed

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    //SetReplicatingMovement(true); // Important for physics objects
    SetNetUpdateFrequency(66.0f);
    SetMinNetUpdateFrequency(33.0f);
}

void ARelicActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate state machine and its components
    DOREPLIFETIME(ARelicActor, CurrentCarrier);
    DOREPLIFETIME(ARelicActor, CurrentState);
}

void ARelicActor::BeginPlay()
{
    Super::BeginPlay();
    
    // Set interaction sphere radius from settings
    if (RelicSettings)
    {
        InteractionSphere->SetSphereRadius(RelicSettings->PickupRadius);
        RelicMesh->SetMassOverrideInKg(NAME_None, RelicSettings->RelicMass);
        RelicMesh->SetLinearDamping(RelicSettings->LinearDamping);
        RelicMesh->SetAngularDamping(RelicSettings->AngularDamping);
        
        if(!RelicSettings->RelicMesh.IsNull())
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
    }
   
    // Attach other components to the main relic mesh
    InteractionSphere->AttachToComponent(RelicMesh, FAttachmentTransformRules::KeepRelativeTransform);

    // Ensure physics is only simulated on the server initially
    if (HasAuthority())
    {
        // Example: Maybe start simulating if state is Neutral/Dropped
        RelicMesh->SetSimulatePhysics(CurrentState == ERelicState::Neutral || CurrentState == ERelicState::Dropped);
    }
    // Bind overlap event (can also be done in Blueprint)
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ARelicActor::OnInteractionSphereOverlap);
}

void ARelicActor::OnInteractionSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Server-side check only for initiating pickup logic
    if (!HasAuthority())
    {
        return;
    }

    ABwayCharacterWithAbilities* OverlappingCharacter = Cast<ABwayCharacterWithAbilities>(OtherActor);
    if (OverlappingCharacter && CanBePickedUpBy(OverlappingCharacter))
    {
        // Option 2: Try to activate a pickup ability on the character (Recommended GAS approach)
        UAbilitySystemComponent* CharASC = OverlappingCharacter->GetAbilitySystemComponent();
        if (CharASC && RelicSettings && RelicSettings->PickupEventTag.IsValid()) // Assume PickupEventTag is defined in URelicSettings
        {
            FGameplayEventData Payload;
            Payload.EventTag = RelicSettings->PickupEventTag; // e.g., "Event.Interaction.PickupRelic"
            Payload.Instigator = OverlappingCharacter; // Who triggered the pickup
            Payload.Target = this; // The Relic itself is the target of the event

            // Send the event to the character's ASC.
            // The GA_PickupRelic_BP should have an "Event Received" trigger matching this tag.
            CharASC->HandleGameplayEvent(Payload.EventTag, &Payload);
            UE_LOG(LogTemp, Log, TEXT("Server: Sent PickupRelic event to %s"), *GetNameSafe(OverlappingCharacter));
        }
    }
}

bool ARelicActor::CanBePickedUpBy(ABwayCharacterWithAbilities* Character) const
{
    // Server-side check: Is the relic in a pickup-able state AND is the character requesting it?
    return (CurrentState == ERelicState::Neutral || CurrentState == ERelicState::Dropped) &&
           Character && Character->GetBwayPlayerState() && Character->GetBwayPlayerState()->IsRequestingRelic();
}

UAbilitySystemComponent* ARelicActor::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ARelicActor::OnRep_CurrentState()
{
    // Client-side reaction to state changes
    // Example: Play VFX/SFX, update material, etc.
    switch (CurrentState)
    {
    case ERelicState::Carried:
    case ERelicState::Neutral:
    case ERelicState::Dropped:
    case ERelicState::Thrown:
    case ERelicState::BeingPassed:
    case ERelicState::Resetting:
    case ERelicState::Scoring:
    case ERelicState::PendingRequest:
    default:
        RelicMesh->SetSimulatePhysics(false); // Ensure physics is off on clients when carried
        break;
    }
    UE_LOG(LogTemp, Log, TEXT("Relic %s changed state to %d on client"), *GetNameSafe(this), CurrentState);
}

void ARelicActor::OnRep_CurrentCarrier()
{
    // Client-side reaction to carrier changes
    if (CurrentCarrier)
    {
        // If the carrier is now valid, attach visually on the client
        // Note: Actual attachment logic might be complex depending on prediction needs,
        // but a simple visual attach here based on replicated state is common.
        AttachToCarrier(CurrentCarrier);
    }
    else
    {
        // If carrier is null, detach visually
        DetachFromCarrier();
    }
}


void ARelicActor::SetRelicState(ERelicState NewState)
{
    if (HasAuthority()) // Only server changes the state authoritatively
    {
        if (CurrentState!= NewState)
        {
            ERelicState OldState = CurrentState;
            CurrentState = NewState;

            // Call RepNotify manually on the server to ensure server logic runs too
            OnRep_CurrentState();

            // Handle state transitions server-side
            switch (NewState)
            {
            case ERelicState::Carried:
                RelicMesh->SetSimulatePhysics(false);
                RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Prevent physics collision while carried
                break;
            case ERelicState::Neutral:
            case ERelicState::Dropped:
                RelicMesh->SetSimulatePhysics(true); // Enable physics simulation on server
                RelicMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                break;
            case ERelicState::Thrown:
            case ERelicState::BeingPassed:
                RelicMesh->SetSimulatePhysics(true); // Enable physics simulation on server
                RelicMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                break;
            case ERelicState::Resetting:
            case ERelicState::Scoring:
            case ERelicState::PendingRequest:
            default:
                break;
            }
            UE_LOG(LogTemp, Log, TEXT("Relic %s changed state from %d to %d on server"), *GetNameSafe(this), OldState, NewState);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Client attempted to change Relic state!"));
    }
}

// --- Interaction Logic ---

void ARelicActor::OnPickedUp(ABwayCharacterWithAbilities* NewCarrier)
{
    if (HasAuthority() && NewCarrier)
    {
        LastPossessingTeam = NewCarrier->GetGenericTeamId(); // Store the team ID of the new carrier
        CurrentCarrier = NewCarrier; // Set replicated property
        OnRep_CurrentCarrier(); // Call RepNotify manually on server

        AttachToCarrier(NewCarrier);
        SetRelicState(ERelicState::Carried);
    }
}


void ARelicActor::OnDropped()
{
    if (HasAuthority())
    {
        DetachFromCarrier(); // Detaches and sets physics state
        CurrentCarrier = nullptr; // Clear replicated property
        OnRep_CurrentCarrier(); // Call RepNotify manually on server

        // Transition to Dropped state (or Neutral depending on rules)
        SetRelicState(ERelicState::Dropped);
        
        //TODO: Potentially start a timer before changing to Neutral
    }
}

// --- Throw/Pass RPCs ---

bool ARelicActor::Server_ThrowRelic_Validate(const FVector& ThrowVelocity) { return true; } // Basic validation
void ARelicActor::Server_ThrowRelic_Implementation(const FVector& ThrowVelocity)
{
    if (HasAuthority() && CurrentState == ERelicState::Carried && CurrentCarrier!= nullptr)
    {
        DetachFromCarrier(&ThrowVelocity); // Detaches and applies impulse
        CurrentCarrier = nullptr;
        OnRep_CurrentCarrier(); // Call RepNotify manually on server

        SetRelicState(ERelicState::Thrown);
        Multicast_PlayThrowPassFX(); // Trigger cosmetic effects on all clients
    }
}

bool ARelicActor::Server_PassRelic_Validate(const FVector& PassVelocity) { return true; } // Basic validation
void ARelicActor::Server_PassRelic_Implementation(const FVector& PassVelocity)
{
    if (HasAuthority() && CurrentState == ERelicState::Carried && CurrentCarrier!= nullptr)
    {
        DetachFromCarrier(&PassVelocity); // Detaches and applies impulse
        CurrentCarrier = nullptr;
        OnRep_CurrentCarrier(); // Call RepNotify manually on server

        SetRelicState(ERelicState::BeingPassed);
        Multicast_PlayThrowPassFX(); // Trigger cosmetic effects on all clients
    }
}

void ARelicActor::Multicast_PlayThrowPassFX_Implementation()
{
    // Play sound/VFX associated with throwing/passing
    // This runs on the server and all clients
    // Example: UGameplayStatics::PlaySoundAtLocation(...);
    // Example: UNiagaraFunctionLibrary::SpawnSystemAtLocation(...);
}

// --- Attachment/Detachment Helpers ---

void ARelicActor::AttachToCarrier(ABwayCharacterWithAbilities* Carrier)
{
    if (!Carrier) return;

    // Use settings for socket name
    FName AttachSocketName = FName( RelicSettings->RelicSocket ); // Default socket name if not set

    // Ensure physics is off before attaching
    RelicMesh->SetSimulatePhysics(false);
    RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Attach to the carrier's mesh
    USkeletalMeshComponent* CarrierMesh = Carrier->GetMesh(); // Assuming standard character mesh setup
    if (CarrierMesh)
    {
        AttachToComponent(CarrierMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
    }

    // If called on client via OnRep_CurrentCarrier, this visually attaches the relic.
    // If called on server via OnPickedUp, this sets up the authoritative state.
}

void ARelicActor::DetachFromCarrier(const FVector* InitialVelocity)
{
    // Detach from parent
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // Re-enable collision and server-side physics
    RelicMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (HasAuthority())
    {
        RelicMesh->SetSimulatePhysics(true);
        if (InitialVelocity)
        {
            RelicMesh->AddImpulse(*InitialVelocity, NAME_None, true); // true for velocity change [4]
        }
    }
    else
    {
        // Clients DO NOT simulate physics for thrown/passed objects [4]
        RelicMesh->SetSimulatePhysics(false);
    }
}