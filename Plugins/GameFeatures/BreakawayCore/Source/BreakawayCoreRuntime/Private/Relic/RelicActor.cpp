#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "Relic/RelicDataAsset.h"
#include "Relic/RelicSettings.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h" // Make sure this is included
#include "AbilitySystem/LyraAbilitySet.h"         // Make sure this is included
#include "AbilitySystemGlobals.h"
#include "BreakawayGameMode.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h" // Include necessary header for APlayerState
#include "Kismet/GameplayStatics.h" // For FinishSpawningActor if needed later

ARelicActor::ARelicActor()
{
    // Enable ticking and replication
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // Enable movement replication for smooth physics sync
    SetReplicatingMovement(true); // Important for physics objects

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

    SetNetUpdateFrequency(66.0f);
    SetMinNetUpdateFrequency(33.0f);
}

void ARelicActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate state machine and its components
    DOREPLIFETIME(ARelicActor, CurrentCarrier);
    DOREPLIFETIME(ARelicActor, CurrentState);
    DOREPLIFETIME(ARelicActor, LastPossessingTeam);
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

        // --- Preload Ability Set (Server Only) ---
        if (HasAuthority())
        {
            if (!RelicSettings->RelicAbilitySet.IsNull())
            {
                // Load the ability set specified in settings
                LoadedRelicAbilitySet = RelicSettings->RelicAbilitySet.LoadSynchronous();
                if (!LoadedRelicAbilitySet)
                {
                    UE_LOG(LogTemp, Error, TEXT("ARelicActor %s failed to load RelicAbilitySet: %s"),
                        *GetNameSafe(this), *RelicSettings->RelicAbilitySet.ToString());
                }
                else
                {
                    UE_LOG(LogTemp, Log, TEXT("ARelicActor %s preloaded RelicAbilitySet: %s"),
                       *GetNameSafe(this), *GetNameSafe(LoadedRelicAbilitySet));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ARelicActor %s has no RelicAbilitySet specified in RelicSettings."), *GetNameSafe(this));
            }
        }
        // --- End Preload ---
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


void ARelicActor::InitializeRelicData(const URelicDataAsset* InRelicData)
{
    if (!InRelicData)
    {
        UE_LOG(LogTemp, Error, TEXT("ARelicActor::InitializeRelicData - InRelicData is null!"));
        return;
    }

    // Store the settings from the data asset
    RelicSettings = InRelicData->RelicSettings;
    
    if (!RelicSettings)
    {
        UE_LOG(LogTemp, Error, TEXT("ARelicActor::InitializeRelicData - RelicSettings is null in data asset %s!"), 
            *InRelicData->GetName());
        return;
    }

    // Apply configuration immediately
    ApplyRelicConfiguration();
}

void ARelicActor::ApplyRelicConfiguration()
{
    if (!RelicSettings || !RelicMesh)
    {
        return;
    }

    // Apply mesh if specified
    if (!RelicSettings->RelicMesh.IsNull())
    {
        if (UStaticMesh* LoadedMesh = RelicSettings->RelicMesh.LoadSynchronous())
        {
            RelicMesh->SetStaticMesh(LoadedMesh);
        }
    }

    // Apply material if specified
    if (!RelicSettings->RelicMaterial.IsNull())
    {
        if (UMaterialInterface* LoadedMaterial = RelicSettings->RelicMaterial.LoadSynchronous())
        {
            RelicMesh->SetMaterial(0, LoadedMaterial);
        }
    }

    // Apply physics settings
    RelicMesh->SetMassOverrideInKg(NAME_None, RelicSettings->RelicMass, true);
    RelicMesh->SetLinearDamping(RelicSettings->LinearDamping);
    RelicMesh->SetAngularDamping(RelicSettings->AngularDamping);

    // Apply interaction radius
    if (InteractionSphere)
    {
        InteractionSphere->SetSphereRadius(RelicSettings->PickupRadius);
    }
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
        // Get the PlayerState associated with the character
        APlayerState* PlayerState = OverlappingCharacter->GetPlayerState();
        if (PlayerState)
        {
            // Get the Ability System Component from the PlayerState
            UAbilitySystemComponent* PlayerStateASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);

            if (PlayerStateASC && RelicSettings && RelicSettings->PickupEventTag.IsValid())
            {
                FGameplayEventData Payload;
                Payload.EventTag = RelicSettings->PickupEventTag;
                Payload.Instigator = OverlappingCharacter; // Character still initiated it
                Payload.Target = this; // The Relic itself is the target of the event

                // Send the event to the PLAYER STATE's ASC.
                // The GA_PickupRelic_BP (granted to the PlayerState) should listen for this.
                PlayerStateASC->HandleGameplayEvent(Payload.EventTag, &Payload);
                UE_LOG(LogTemp, Log, TEXT("Server: Sent PickupRelic event to PlayerState ASC of %s (Owner: %s)"), *GetNameSafe(PlayerState), *GetNameSafe(OverlappingCharacter));
            }
            else
            {
                 UE_LOG(LogTemp, Warning, TEXT("Server: Could not send PickupRelic event. PlayerState ASC: %s, RelicSettings: %s, PickupEventTag Valid: %d"),
                    PlayerStateASC ? TEXT("Valid") : TEXT("INVALID"),
                    RelicSettings ? TEXT("Valid") : TEXT("INVALID"),
                    RelicSettings ? RelicSettings->PickupEventTag.IsValid() : 0);
            }
        }
        else
        {
             UE_LOG(LogTemp, Warning, TEXT("Server: Overlapping character %s has no PlayerState."), *GetNameSafe(OverlappingCharacter));
        }
    }
}

bool ARelicActor::CanBePickedUpBy(ABwayCharacterWithAbilities* Character) const
{
    UE_LOG(LogTemp, Log, TEXT("Relic current state is %d"), CurrentState);
    
    // Server-side check: Is the relic in a pickup-able state AND is the character requesting it?
    return CurrentState != ERelicState::Carried && (Character && Character->HasMatchingGameplayTag(RelicSettings->RequestingTag));
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
    if (!HasAuthority() || !NewCarrier)
    {
        return;
    }

    CurrentCarrier = NewCarrier;
    CurrentState = ERelicState::Carried;
    
    // NEW: Track which team picked up the relic
    if (NewCarrier->GetPlayerState())
    {
        ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>();
        if (GameState)
        {
            LastPossessingTeam = GameState->GetPlayerTeam(NewCarrier->GetPlayerState());
        }
    }
    
    // Notify game mode of carrier change
    if (ABreakawayGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakawayGameMode>())
    {
        GameMode->OnRelicCarrierChanged(NewCarrier);
    }
    
    LastPossessingTeam = NewCarrier->GetGenericTeamId(); // Store the team ID of the new carrier
    CurrentCarrier = NewCarrier; // Set replicated property
    OnRep_CurrentCarrier(); // Call RepNotify manually on server

    AttachToCarrier(NewCarrier);
    SetRelicState(ERelicState::Carried);
}


void ARelicActor::OnDropped()
{
    if (!HasAuthority())
    {
        return;
    }
    
    // LastPossessingTeam is maintained - don't reset it
    CurrentCarrier = nullptr;
    CurrentState = ERelicState::Dropped;
    
    // Notify game mode carrier is gone
    if (ABreakawayGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakawayGameMode>())
    {
        GameMode->OnRelicCarrierChanged(nullptr);
    }
    
    DetachFromCarrier(); // Detaches and sets physics state
    CurrentCarrier = nullptr; // Clear replicated property AFTER potentially using it above
    OnRep_CurrentCarrier(); // Call RepNotify manually on server

    SetRelicState(ERelicState::Dropped);
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
    FName AttachSocketName = RelicSettings ? FName(RelicSettings->RelicSocket) : NAME_None;

    // Ensure physics is off before attaching
    RelicMesh->SetSimulatePhysics(false);
    RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Attach to the carrier's mesh
    USkeletalMeshComponent* CarrierMesh = Carrier->GetMesh();
    if (CarrierMesh)
    {
        AttachToComponent(CarrierMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
    }

    if (HasAuthority())
    {
        // --- Grant Ability Set ---
        APlayerState* CarrierPlayerState = Carrier->GetPlayerState();
        if (CarrierPlayerState)
        {
            ULyraAbilitySystemComponent* PlayerStateASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CarrierPlayerState));
            ULyraAbilitySet* AbilitySetToGrant = LoadedRelicAbilitySet;

            if (PlayerStateASC && AbilitySetToGrant)
            {
                AbilitySetToGrant->GiveToAbilitySystem(PlayerStateASC, &GrantedCarrierSetHandle, this);
                UE_LOG(LogTemp, Log, TEXT("Server: Granted RelicAbilitySet '%s' to PlayerState ASC of %s (Carrier: %s)"), *GetNameSafe(AbilitySetToGrant), *GetNameSafe(CarrierPlayerState), *GetNameSafe(Carrier));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Server: Failed to grant RelicAbilitySet to PlayerState. PlayerStateASC Valid: %d, AbilitySetToGrant Valid: %d"),
                    IsValid(PlayerStateASC), IsValid(AbilitySetToGrant));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Server: Carrier %s has no PlayerState, cannot grant RelicAbilitySet."), *GetNameSafe(Carrier));
        }
        // --- End Grant Ability Set ---
    }

    // If called on client via OnRep_CurrentCarrier, this visually attaches the relic.
    // If called on server via OnPickedUp, this sets up the authoritative state.
}

void ARelicActor::DetachFromCarrier(const FVector* InitialVelocity)
{
    // Detach from parent FIRST visually
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // Re-enable collision
    RelicMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    if (HasAuthority())
    {
        // --- Clear Ability Set (Server Only) ---
        if (CurrentCarrier) // Check if there was a carrier before clearing CurrentCarrier
        {
            APlayerState* CarrierPlayerState = CurrentCarrier->GetPlayerState();
            if (CarrierPlayerState)
            {
                ULyraAbilitySystemComponent* PlayerStateASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CarrierPlayerState));

                if (PlayerStateASC) // Ensure the handle is valid before taking
                {
                    GrantedCarrierSetHandle.TakeFromAbilitySystem(PlayerStateASC);
                    UE_LOG(LogTemp, Log, TEXT("Server: Cleared Ability Set from PlayerState ASC of %s (Carrier: %s)"), *GetNameSafe(CarrierPlayerState), *GetNameSafe(CurrentCarrier));
                }
            }
        }
        // --- End Clear Ability Set ---

        
        // Enable server-side physics AFTER clearing abilities/detaching
        RelicMesh->SetSimulatePhysics(true);
        if (InitialVelocity)
        {
            RelicMesh->AddImpulse(*InitialVelocity, NAME_None, true); // true for velocity change
        }
    }
    else
    {
        // Clients DO NOT simulate physics for thrown/passed objects
        RelicMesh->SetSimulatePhysics(false);
    }
}

