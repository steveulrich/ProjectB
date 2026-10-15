#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "LyraGameplayTags.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "Relic/RelicSettings.h"
#include "Relic/RelicMovementReplicationComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h" // Make sure this is included
#include "AbilitySystem/LyraAbilitySet.h"         // Make sure this is included
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/BwayGameplayAbility_RelicPass.h"
#include "BwayGameState.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h" // Include necessary header for APlayerState
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarBwayRelicDebugFeedback(
    TEXT("bway.Relic.DebugFeedback"), 0,
    TEXT("Log accepted relic audio events and their actual per-peer delivery."));
#endif

namespace
{
bool IsRelicPickupState(ERelicState State)
{
    return State == ERelicState::Neutral || State == ERelicState::Dropped ||
        State == ERelicState::Thrown || State == ERelicState::BeingPassed ||
        State == ERelicState::Passed || State == ERelicState::PendingRequest;
}

bool IsRelicPassFlightState(ERelicState State)
{
    return State == ERelicState::BeingPassed || State == ERelicState::Passed;
}

FVector ComputeSpawnDirectedFumbleVelocity(const FVector& CarrierLocation, const FVector& SpawnLocation, const URelicSettings* RelicSettings)
{
	const float ImpulseScale = RelicSettings ? RelicSettings->DropImpulseMultiplier : 1.0f;
	const float MinHorizontal = RelicSettings ? RelicSettings->FumbleMinHorizontalTowardSpawn : 0.0f;
	const float MaxHorizontal = RelicSettings ? RelicSettings->FumbleDropSpeed : 450.0f;
	const float MaxUp = RelicSettings ? RelicSettings->FumbleDropUpSpeed : 250.0f;
	const float MinUp = RelicSettings ? RelicSettings->FumbleMinUpTowardSpawn : 80.0f;
	const float RefDistance = RelicSettings ? FMath::Max(RelicSettings->FumbleSpawnReferenceDistance, 1.0f) : 4000.0f;
	const float MinFumbleStrength = RelicSettings ? RelicSettings->FumbleImpulseStrength : 600.0f;

	FVector ToSpawnHorizontal = SpawnLocation - CarrierLocation;
	ToSpawnHorizontal.Z = 0.0f;

	const float HorizontalDistance = ToSpawnHorizontal.Size();
	FVector HorizontalDir = FVector::ZeroVector;
	if (HorizontalDistance > KINDA_SMALL_NUMBER)
	{
		HorizontalDir = ToSpawnHorizontal / HorizontalDistance;
	}

	const float DistanceAlpha = FMath::Clamp(HorizontalDistance / RefDistance, 0.0f, 1.0f);
	const float HorizontalSpeed = FMath::Lerp(MinHorizontal, MaxHorizontal, DistanceAlpha);
	const float UpSpeed = FMath::Lerp(MaxUp, MinUp, DistanceAlpha);

	FVector Result = (HorizontalDir * HorizontalSpeed + FVector(0.0f, 0.0f, UpSpeed)) * ImpulseScale;
	const float TargetStrength = MinFumbleStrength * ImpulseScale;
	if (TargetStrength > 0.0f && Result.Size() < TargetStrength)
	{
		const FVector Dir = Result.IsNearlyZero() ? FVector(0.0f, 0.0f, 1.0f) : Result.GetSafeNormal();
		Result = Dir * TargetStrength;
	}
	return Result;
}

FGameplayTag ResolvePickupEventTag(const URelicSettings* RelicSettings)
{
    if (RelicSettings && RelicSettings->PickupEventTag.IsValid())
    {
        return RelicSettings->PickupEventTag;
    }

    return FGameplayTag::RequestGameplayTag(FName("Event.Interaction.PickupRelic"), /*ErrorIfNotFound*/ false);
}

bool HasTagOnCharacterOrPlayerState(const ABwayCharacterWithAbilities* Character, const FGameplayTag& Tag)
{
    if (!Character || !Tag.IsValid())
    {
        return false;
    }
    if (Character->HasMatchingGameplayTag(Tag))
    {
        return true;
    }
    if (const APlayerState* PS = Character->GetPlayerState())
    {
        if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
        {
            return ASC->HasMatchingGameplayTag(Tag);
        }
    }
    return false;
}

bool IsCharacterRequestingRelic(const ABwayCharacterWithAbilities* Character, const URelicSettings* RelicSettings)
{
    if (!Character)
    {
        return false;
    }

    if (HasTagOnCharacterOrPlayerState(Character, BwayGameplayTags::State_Relic_PreparingCatch))
    {
        return true;
    }

    if (RelicSettings && RelicSettings->RequestingTag.IsValid() &&
        HasTagOnCharacterOrPlayerState(Character, RelicSettings->RequestingTag))
    {
        return true;
    }

    const FGameplayTag GameplayRequestingTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.Relic.Requesting"), /*ErrorIfNotFound*/ false);
    if (HasTagOnCharacterOrPlayerState(Character, GameplayRequestingTag))
    {
        return true;
    }

    const FGameplayTag RequestingTag = FGameplayTag::RequestGameplayTag(FName("State.RequestingRelic"), /*ErrorIfNotFound*/ false);
    if (HasTagOnCharacterOrPlayerState(Character, RequestingTag))
    {
        return true;
    }

    const FGameplayTag RequestingMessageTag = FGameplayTag::RequestGameplayTag(FName("State.RequestingRelic.Message"), /*ErrorIfNotFound*/ false);
    if (HasTagOnCharacterOrPlayerState(Character, RequestingMessageTag))
    {
        return true;
    }

    if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Character->GetPlayerState()))
    {
        if (BwayPS->bHasCalledForRelic)
        {
            return true;
        }
    }

    return false;
}

ABwayPlayerState* ResolvePlayerStateFromActor(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }
    if (ABwayPlayerState* DirectPS = Cast<ABwayPlayerState>(Actor))
    {
        return DirectPS;
    }
    if (const APawn* Pawn = Cast<APawn>(Actor))
    {
        if (ABwayPlayerState* PawnPS = Pawn->GetPlayerState<ABwayPlayerState>())
        {
            return PawnPS;
        }
    }
    if (const AController* Controller = Cast<AController>(Actor))
    {
        if (ABwayPlayerState* CtrlPS = Controller->GetPlayerState<ABwayPlayerState>())
        {
            return CtrlPS;
        }
    }
    return Cast<ABwayPlayerState>(ULyraVerbMessageHelpers::GetPlayerStateFromObject(Actor));
}
}

ARelicActor::ARelicActor()
{
    // Enable ticking and replication
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // Movement is replicated by URelicMovementReplicationComponent. Do not also
    // use AActor::bReplicateMovement, or the two systems will fight each other.
    SetReplicatingMovement(false);

    // Set the root component to replicate movement for physics [1, 2]
    // Note: RelicMesh MUST be the root component for bReplicateMovement to work correctly with physics simulation.
    RelicMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicMesh"));
    SetRootComponent(RelicMesh);
    RelicMesh->SetIsReplicated(true); // Replicate the component itself if needed
    RelicMesh->SetUseCCD(true);
    RelicMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // SceneComponent replicates AttachChildren. Only this stable default subobject
    // belongs in the mesh's networked list; each peer owns its Niagara children.
    VisualEffectsRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualEffectsRoot"));
    VisualEffectsRoot->SetupAttachment(RelicMesh);
    VisualEffectsRoot->SetIsReplicated(false);

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(150.0f); // Example radius, configure via URelicSettings
    InteractionSphere->SetCollisionProfileName(FName("OverlapAllDynamic")); // Adjust profile as needed

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // Create the smooth movement replication component
    MovementReplicationComponent = CreateDefaultSubobject<URelicMovementReplicationComponent>(TEXT("MovementReplicationComponent"));

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
    DOREPLIFETIME(ARelicActor, bIsPassInFlight);
    DOREPLIFETIME(ARelicActor, PassingTeamIndex);
    DOREPLIFETIME(ARelicActor, bHasScoredThisRound);
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
            if (UStaticMesh* LoadedMesh = RelicSettings->RelicMesh.LoadSynchronous())
            {
                RelicMesh->SetStaticMesh(LoadedMesh);
            }

            if (UMaterialInterface* LoadedMaterial = RelicSettings->RelicMaterial.LoadSynchronous())
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

    // Initialize dynamic material for team color tinting
    if (RelicMesh && RelicSettings && !RelicSettings->RelicMaterial.IsNull())
    {
        if (UMaterialInterface* BaseMaterial = RelicSettings->RelicMaterial.LoadSynchronous())
        {
            TeamColorMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            if (TeamColorMaterialInstance)
            {
                RelicMesh->SetMaterial(0, TeamColorMaterialInstance);
            }
        }
    }
    // Neutral can equal the class default and therefore receive no state RepNotify.
    UpdateStateVFX(CurrentState);
    UpdateTeamColorTinting();
}

void ARelicActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindCarrierCCTagListener();
    for (ERelicState State : {ERelicState::Neutral, ERelicState::Carried, ERelicState::Dropped, ERelicState::Scoring})
    {
        CleanupStateVFX(State);
    }
    DestroyTrailEffect();
    Super::EndPlay(EndPlayReason);
}

void ARelicActor::InitializeRelicData(const URelicSettings* InRelicSettings)
{
    if (!InRelicSettings)
    {
        UE_LOG(LogTemp, Error, TEXT("ARelicActor::InitializeRelicData - InRelicData is null!"));
        return;
    }

    // Store the settings from the data asset (now properly const)
    RelicSettings = InRelicSettings;

    // Apply configuration immediately (before BeginPlay)
    ApplyRelicConfiguration();

    if (HasActorBegunPlay())
    {
        UpdateStateVFX(CurrentState);
        UpdateTeamColorTinting();
    }

    UE_LOG(LogTemp, Log, TEXT("Relic initialized with data asset: %s"), *InRelicSettings->GetName());
}

void ARelicActor::ApplyRelicConfiguration()
{
    if (!RelicSettings)
    {
        UE_LOG(LogTemp, Warning, TEXT("ApplyRelicConfiguration called but RelicSettings is null"));
        return;
    }

    // Apply mesh if specified
    if (RelicMesh && !RelicSettings->RelicMesh.IsNull())
    {
        if (UStaticMesh* LoadedMesh = RelicSettings->RelicMesh.LoadSynchronous())
        {
            RelicMesh->SetStaticMesh(LoadedMesh);
        }
    }

    // Apply material if specified
    if (RelicMesh && !RelicSettings->RelicMaterial.IsNull())
    {
        if (UMaterialInterface* LoadedMaterial = RelicSettings->RelicMaterial.LoadSynchronous())
        {
            RelicMesh->SetMaterial(0, LoadedMaterial);
        }
    }

    // Apply physics settings
    if (RelicMesh)
    {
        RelicMesh->SetMassOverrideInKg(NAME_None, RelicSettings->RelicMass, true);
        RelicMesh->SetLinearDamping(RelicSettings->LinearDamping);
        RelicMesh->SetAngularDamping(RelicSettings->AngularDamping);
        RelicMesh->SetUseCCD(true);
        RelicMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    }

    // Apply interaction radius
    if (InteractionSphere)
    {
        InteractionSphere->SetSphereRadius(RelicSettings->PickupRadius);
    }

    // Load ability set asynchronously if specified
    if (!RelicSettings->RelicAbilitySet.IsNull())
    {
        FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
        StreamableManager.RequestAsyncLoad(
            RelicSettings->RelicAbilitySet.ToSoftObjectPath(),
            FStreamableDelegate::CreateLambda([this]()
            {
                if (RelicSettings && !RelicSettings->RelicAbilitySet.IsNull())
                {
                    LoadedRelicAbilitySet = RelicSettings->RelicAbilitySet.Get();
                    UE_LOG(LogTemp, Log, TEXT("Relic ability set loaded: %s"),
                        *GetNameSafe(LoadedRelicAbilitySet));
                }
            })
        );
    }

    UE_LOG(LogTemp, Log, TEXT("Relic configuration applied successfully"));
}

int32 ARelicActor::ResolveCharacterTeamIndex(const ABwayCharacterWithAbilities* Character) const
{
    if (!Character)
    {
        return INDEX_NONE;
    }

    if (const APlayerState* PS = Character->GetPlayerState())
    {
        if (const ABwayGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
        {
            const int32 TeamIdx = GameState->GetPlayerTeam(PS);
            if (TeamIdx != INDEX_NONE)
            {
                return TeamIdx;
            }
        }
        if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(PS))
        {
            const uint8 RawId = TeamAgent->GetGenericTeamId().GetId();
            if (RawId == 1 || RawId == 2)
            {
                return static_cast<int32>(RawId) - 1;
            }
        }
    }

    const uint8 PawnTeamId = Character->GetGenericTeamId().GetId();
    if (PawnTeamId == 1 || PawnTeamId == 2)
    {
        return static_cast<int32>(PawnTeamId) - 1;
    }

    return INDEX_NONE;
}

bool ARelicActor::IsDropPickupCooldownActiveFor(const ABwayCharacterWithAbilities* Character) const
{
    if (!Character || !LastDroppedCarrier.IsValid() || Character != LastDroppedCarrier.Get())
    {
        return false;
    }

    const float Cooldown = RelicSettings ? RelicSettings->PickupCooldownAfterDrop : 0.5f;
    if (Cooldown <= 0.0f || !GetWorld())
    {
        return false;
    }

    const double Elapsed = GetWorld()->GetTimeSeconds() - LastDropWorldTime;
    return Elapsed >= 0.0 && Elapsed < static_cast<double>(Cooldown);
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
        if (APlayerState* PlayerState = OverlappingCharacter->GetPlayerState())
        {
            // Get the Ability System Component from the PlayerState
            UAbilitySystemComponent* PlayerStateASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);

            const FGameplayTag PickupEventTag = ResolvePickupEventTag(RelicSettings);
            if (PlayerStateASC && PickupEventTag.IsValid())
            {
                FGameplayEventData Payload;
                Payload.EventTag = PickupEventTag;
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

        // Direct authoritative catch/interception fallback when no Blueprint pickup listener consumed the overlap.
        // OnPickedUp is idempotent and guarded by CurrentCarrier, preventing duplicate ability grants.
        if (!CurrentCarrier && CanBePickedUpBy(OverlappingCharacter))
        {
            OnPickedUp(OverlappingCharacter);
        }
    }
}

bool ARelicActor::CanBePickedUpBy(ABwayCharacterWithAbilities* Character) const
{
    UE_LOG(LogTemp, Verbose, TEXT("Relic current state is %d"), CurrentState);

    if (!Character || CurrentCarrier || bHasScoredThisRound || !IsRelicPickupState(CurrentState))
    {
        return false;
    }

    if (IsDropPickupCooldownActiveFor(Character))
    {
        return false;
    }

    if (HasTagOnCharacterOrPlayerState(Character, LyraGameplayTags::Status_Death) ||
        HasTagOnCharacterOrPlayerState(Character, LyraGameplayTags::Status_Death_Dying))
    {
        return false;
    }

    if (IsCharacterRequestingRelic(Character, RelicSettings))
    {
        return true;
    }

    // Opposing defenders overlapping an in-flight pass can intercept it mid-air.
    if (bIsPassInFlight || IsRelicPassFlightState(CurrentState))
    {
        const int32 EffectivePassingTeam = (PassingTeamIndex != INDEX_NONE) ? PassingTeamIndex : LastPossessingTeam;
        const int32 CatcherTeam = ResolveCharacterTeamIndex(Character);
        if (EffectivePassingTeam != INDEX_NONE && CatcherTeam != INDEX_NONE && EffectivePassingTeam != CatcherTeam)
        {
            return true;
        }
    }

    return false;
}

UAbilitySystemComponent* ARelicActor::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ARelicActor::OnRep_CurrentState()
{
    // Persistent presentation follows current state. Audio is a transient event;
    // a late join or relevancy refresh must not replay a previous pickup/goal.
    UpdateStateVFX(CurrentState);
    UpdateTeamColorTinting();

    if (RelicMesh && !HasAuthority())
    {
        const bool bShouldSimulateForSmoothing =
            CurrentState == ERelicState::Neutral ||
            CurrentState == ERelicState::Dropped ||
            CurrentState == ERelicState::Thrown ||
            CurrentState == ERelicState::BeingPassed ||
            CurrentState == ERelicState::Passed;

        // Clients simulate only so the custom movement replication component can
        // smooth loose-ball movement. The server still owns authoritative state.
        RelicMesh->SetSimulatePhysics(bShouldSimulateForSmoothing);
        RelicMesh->SetCollisionEnabled(bShouldSimulateForSmoothing ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        if (bShouldSimulateForSmoothing)
        {
            RelicMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("Relic %s changed state to %d on client"), *GetNameSafe(this), static_cast<int32>(CurrentState));
#if !UE_BUILD_SHIPPING
    if (CVarBwayRelicDebugFeedback.GetValueOnGameThread() != 0)
    {
        UE_LOG(LogTemp, Display, TEXT("BwayRelicFeedback: State state=%d net=%d settings=%s"),
            int32(CurrentState), int32(GetNetMode()), *GetPathNameSafe(RelicSettings));
    }
#endif
}

void ARelicActor::OnRep_CurrentCarrier()
{
    // Authority performs attachment and ability grants once in the accepted
    // transition. A RepNotify is presentation-only and must not grant twice.
    if (HasAuthority()) return;

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

void ARelicActor::OnRep_LastPossessingTeam()
{
    // Team and state RepNotifies can arrive in either order.
    UpdateTeamColorTinting();
}

void ARelicActor::SetRelicState(ERelicState NewState)
{
    if (HasAuthority()) // Only server changes the state authoritatively
    {
        bIsPassInFlight = IsRelicPassFlightState(NewState);
        if (bIsPassInFlight && PassingTeamIndex == INDEX_NONE)
        {
            PassingTeamIndex = LastPossessingTeam;
        }
        else if (!bIsPassInFlight && NewState != ERelicState::Carried)
        {
            PassingTeamIndex = INDEX_NONE;
        }

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
                // Disable smooth replication when carried (no physics)
                if (MovementReplicationComponent)
                {
                    MovementReplicationComponent->EnableSmoothReplication(false);
                }
                break;
            case ERelicState::Neutral:
            case ERelicState::Dropped:
                RelicMesh->SetSimulatePhysics(true); // Enable physics simulation on server
                RelicMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                RelicMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
                RelicMesh->SetUseCCD(true);
                // Enable smooth replication for physics-based movement
                if (MovementReplicationComponent)
                {
                    MovementReplicationComponent->EnableSmoothReplication(true);
                }
                break;
            case ERelicState::Thrown:
            case ERelicState::BeingPassed:
            case ERelicState::Passed:
                RelicMesh->SetSimulatePhysics(true); // Enable physics simulation on server
                RelicMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                RelicMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
                RelicMesh->SetUseCCD(true);
                // Enable smooth replication for physics-based movement
                if (MovementReplicationComponent)
                {
                    MovementReplicationComponent->EnableSmoothReplication(true);
                }
                break;
            case ERelicState::Resetting:
            case ERelicState::Scoring:
                RelicMesh->SetSimulatePhysics(false);
                RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                // Disable smooth replication during reset/scoring
                if (MovementReplicationComponent)
                {
                    MovementReplicationComponent->EnableSmoothReplication(false);
                }
                break;
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

ABwayPlayerState* ARelicActor::GetLastPossessingPlayerState() const
{
    return LastPossessingPlayerState.Get();
}

void ARelicActor::OnPickedUp(ABwayCharacterWithAbilities* NewCarrier)
{
    if (!HasAuthority() || !IsValid(NewCarrier) || CurrentCarrier ||
        bHasScoredThisRound || !IsRelicPickupState(CurrentState) ||
        IsDropPickupCooldownActiveFor(NewCarrier))
    {
        return;
    }

    const bool bWasPassInFlight = bIsPassInFlight || IsRelicPassFlightState(CurrentState);
    const int32 PreviousPassingTeam = (PassingTeamIndex != INDEX_NONE) ? PassingTeamIndex : LastPossessingTeam;
    const int32 CatcherTeamIndex = ResolveCharacterTeamIndex(NewCarrier);

    // Clear drop cooldown and damage accumulation once a valid pickup occurs.
    LastDroppedCarrier.Reset();
    LastDropWorldTime = -1000.0;
    AccumulatedCarrierDamage = 0.0f;
    LastCarrierDamageInstigatorPS.Reset();
    bIsPassInFlight = false;
    PassingTeamIndex = INDEX_NONE;

    // Keep player identity after CurrentCarrier is cleared by release or scoring.
    ABwayPlayerState* CatcherPS = Cast<ABwayPlayerState>(NewCarrier->GetPlayerState());
    LastPossessingPlayerState = CatcherPS;
    LastPossessingTeam = CatcherTeamIndex;

    if (bWasPassInFlight && PreviousPassingTeam != INDEX_NONE && CatcherTeamIndex != INDEX_NONE &&
        PreviousPassingTeam != CatcherTeamIndex && CatcherPS)
    {
        CatcherPS->AddInterception();
    }

    // Commit ownership before callbacks or attachment can synchronously produce
    // another pickup overlap. Repeated and competing calls are now no-ops.
    CurrentCarrier = NewCarrier;
    SetRelicState(ERelicState::Carried);

    // Notify relic manager component of carrier change
    if (ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>())
    {
        if (UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
        {
            RelicMgr->OnRelicCarrierChanged(NewCarrier);
        }
    }

    AttachToCarrier(NewCarrier);

    // Play pickup audio
    PlayStateAudio(ERelicState::Carried);
}


void ARelicActor::OnDropped()
{
    DropFromCarrierInternal(/*InitialVelocity=*/ nullptr, /*bRecordForcedFumble=*/ false, /*InstigatorActor=*/ nullptr);
}

void ARelicActor::DropRelic(const FVector& ImpulseVelocity, AActor* InstigatorActor, bool bIsForcedFumble)
{
    if (!HasAuthority() || CurrentState != ERelicState::Carried || !CurrentCarrier)
    {
        return;
    }

    FVector EffectiveVelocity = ImpulseVelocity;
    if (EffectiveVelocity.IsNearlyZero() && bIsForcedFumble)
    {
        const FVector CarrierLocation = CurrentCarrier->GetActorLocation();
        FVector SpawnLocation = CarrierLocation;
        if (ABwayGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
        {
            if (UBwayRelicManagerComponent* RelicManager = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
            {
                RelicManager->GetRelicSpawnLocationForPosition(CarrierLocation, SpawnLocation);
            }
        }
        EffectiveVelocity = ComputeSpawnDirectedFumbleVelocity(CarrierLocation, SpawnLocation, RelicSettings);
    }

    const FVector* VelocityPtr = EffectiveVelocity.IsNearlyZero() ? nullptr : &EffectiveVelocity;
    DropFromCarrierInternal(VelocityPtr, bIsForcedFumble, InstigatorActor);
}

bool ARelicActor::HandleCarrierDamage(float DamageAmount, AActor* InstigatorActor)
{
    if (!HasAuthority() || CurrentState != ERelicState::Carried || !CurrentCarrier || DamageAmount <= 0.0f)
    {
        return false;
    }

    if (RelicSettings && !RelicSettings->bFumbleOnDamage)
    {
        return false;
    }

    if (ABwayPlayerState* InstigatorPS = ResolvePlayerStateFromActor(InstigatorActor))
    {
        LastCarrierDamageInstigatorPS = InstigatorPS;
    }

    AccumulatedCarrierDamage += DamageAmount;
    const float FumbleThreshold = RelicSettings ? RelicSettings->FumbleDamageThreshold : 50.0f;

    const bool bHasHardCCTag =
        HasTagOnCharacterOrPlayerState(CurrentCarrier, BwayGameplayTags::State_Status_Silenced) ||
        HasTagOnCharacterOrPlayerState(CurrentCarrier, BwayGameplayTags::State_Status_Jailed) ||
        HasTagOnCharacterOrPlayerState(CurrentCarrier, FGameplayTag::RequestGameplayTag(FName("State.Stunned"), false));

    if (AccumulatedCarrierDamage + KINDA_SMALL_NUMBER >= FumbleThreshold || bHasHardCCTag)
    {
        ForceFumbleFromDamageWithInstigator(InstigatorActor);
        return true;
    }

    return false;
}

void ARelicActor::ForceFumbleFromDamage()
{
    ForceFumbleFromDamageWithInstigator(LastCarrierDamageInstigatorPS.Get());
}

void ARelicActor::ForceFumbleFromDamageWithInstigator(AActor* InstigatorActor)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentState != ERelicState::Carried || !CurrentCarrier)
    {
        return;
    }

    ABwayCharacterWithAbilities* FumblingCarrier = CurrentCarrier;
    const FVector CarrierLocation = FumblingCarrier->GetActorLocation();
    FVector SpawnLocation = CarrierLocation;

    if (ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>())
    {
        if (UBwayRelicManagerComponent* RelicManager = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
        {
            RelicManager->GetRelicSpawnLocationForPosition(CarrierLocation, SpawnLocation);
        }
    }

    const FVector DropVelocity = ComputeSpawnDirectedFumbleVelocity(CarrierLocation, SpawnLocation, RelicSettings);

    DropFromCarrierInternal(&DropVelocity, /*bRecordForcedFumble=*/ true, InstigatorActor);

    UE_LOG(LogTemp, Log, TEXT("Relic %s force-fumbled off %s"), *GetNameSafe(this), *GetNameSafe(FumblingCarrier));
}

void ARelicActor::DropFromCarrierInternal(const FVector* InitialVelocity, bool bRecordForcedFumble, AActor* InstigatorActor)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bCompletingReset = CurrentState == ERelicState::Resetting;

    if (bRecordForcedFumble && CurrentCarrier)
    {
        ABwayPlayerState* CarrierPS = Cast<ABwayPlayerState>(CurrentCarrier->GetPlayerState());
        ABwayPlayerState* AttackerPS = ResolvePlayerStateFromActor(InstigatorActor);
        if (!AttackerPS)
        {
            AttackerPS = LastCarrierDamageInstigatorPS.Get();
        }

        if (AttackerPS && AttackerPS != CarrierPS)
        {
            AttackerPS->AddForcedFumble();
        }
        if (CarrierPS)
        {
            CarrierPS->AddForcedFumble();
        }
    }

    if (!bCompletingReset && CurrentCarrier)
    {
        LastDroppedCarrier = CurrentCarrier;
        LastDropWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        const int32 TeamIdx = ResolveCharacterTeamIndex(CurrentCarrier);
        if (TeamIdx != INDEX_NONE)
        {
            LastPossessingTeam = TeamIdx;
        }

        if (UWorld* World = GetWorld())
        {
            const float Cooldown = RelicSettings ? RelicSettings->PickupCooldownAfterDrop : 0.5f;
            if (Cooldown > 0.0f)
            {
                TWeakObjectPtr<ARelicActor> WeakThis(this);
                TWeakObjectPtr<ABwayCharacterWithAbilities> DroppedCarrierWeak = LastDroppedCarrier;
                const double RecordedDropTime = LastDropWorldTime;
                FTimerHandle CooldownHandle;
                World->GetTimerManager().SetTimer(CooldownHandle, [WeakThis, DroppedCarrierWeak, RecordedDropTime]()
                {
                    if (ARelicActor* Self = WeakThis.Get())
                    {
                        if (Self->LastDroppedCarrier == DroppedCarrierWeak && FMath::IsNearlyEqual(Self->LastDropWorldTime, RecordedDropTime))
                        {
                            Self->LastDroppedCarrier.Reset();
                            Self->LastDropWorldTime = -1000.0;
                        }
                    }
                }, Cooldown, false);
            }
        }
    }

    AccumulatedCarrierDamage = 0.0f;
    LastCarrierDamageInstigatorPS.Reset();
    bIsPassInFlight = false;
    PassingTeamIndex = INDEX_NONE;

    // Notify relic manager component carrier is gone (do this first while CurrentCarrier is still valid)
    if (ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>())
    {
        if (UBwayRelicManagerComponent* RelicMgr = GameState->FindComponentByClass<UBwayRelicManagerComponent>())
        {
            RelicMgr->OnRelicCarrierChanged(nullptr);
        }
    }

    // Detach from carrier (uses CurrentCarrier internally for ability cleanup)
    DetachFromCarrier(InitialVelocity);

    // Now clear the replicated carrier property
    CurrentCarrier = nullptr;

    const bool bChangedState = CurrentState != ERelicState::Dropped;
    // Set state (LastPossessingTeam is maintained - don't reset it)
    SetRelicState(ERelicState::Dropped);

    // A reset uses this release/physics path after relocation. Announce its
    // destination once instead of playing a drop sound or the old-location sound.
    if (bChangedState)
    {
        PlayStateAudio(bCompletingReset ? ERelicState::Resetting : ERelicState::Dropped);
    }
}

void ARelicActor::OnEnteredGoal(int32 ScoringTeam)
{
    if (!HasAuthority())
    {
        return;
    }

    // Validate that we haven't already scored this round
    if (bHasScoredThisRound)
    {
        UE_LOG(LogTemp, Warning, TEXT("Relic attempted to score again in same round - ignoring"));
        return;
    }

    // Cannot score if relic is resetting or already scoring
    if (CurrentState == ERelicState::Resetting || CurrentState == ERelicState::Scoring)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Relic is in invalid state for scoring: %d"), (int32)CurrentState);
        return;
    }

    // Preserve the scorer for cue ownership before release clears CurrentCarrier.
    ABwayCharacterWithAbilities* ScoringCarrier = CurrentCarrier;

    // Release the carrier before scoring VFX so round-reset pawn destroy/restart is safe.
    if (CurrentCarrier)
    {
        DetachFromCarrier();
        CurrentCarrier = nullptr;
    }

    // Set scoring state internally
    SetRelicState(ERelicState::Scoring);

    // Mark as scored
    bHasScoredThisRound = true;

    if (AbilitySystemComponent)
    {
        FGameplayCueParameters CueParams;
        CueParams.Location = FVector_NetQuantize10(GetActorLocation());
        CueParams.RawMagnitude = static_cast<float>(ScoringTeam);
        CueParams.NormalizedMagnitude = static_cast<float>(ScoringTeam);
        CueParams.Instigator = ScoringCarrier;
        CueParams.EffectCauser = this;
        AbilitySystemComponent->ExecuteGameplayCue(BwayGameplayCueTags::ScoringGoal, CueParams);
    }

    // Play scoring audio (VFX will be handled by OnRep_CurrentState)
    PlayStateAudio(ERelicState::Scoring);

    UE_LOG(LogTemp, Log, TEXT("Relic entered goal for Team %d"), ScoringTeam + 1);
}

void ARelicActor::ClearScoringFlag()
{
    if (!HasAuthority())
    {
        return;
    }

    bHasScoredThisRound = false;
    LastDroppedCarrier.Reset();
    LastDropWorldTime = -1000.0;
    UE_LOG(LogTemp, Verbose, TEXT("Relic scoring flag cleared for new round"));
}

void ARelicActor::BeginResettingState()
{
    if (!HasAuthority())
    {
        return;
    }

    LastPossessingPlayerState.Reset();
    LastPossessingTeam = INDEX_NONE;
    LastDroppedCarrier.Reset();
    LastDropWorldTime = -1000.0;
    AccumulatedCarrierDamage = 0.0f;
    LastCarrierDamageInstigatorPS.Reset();
    bIsPassInFlight = false;
    PassingTeamIndex = INDEX_NONE;
    SetRelicState(ERelicState::Resetting);
    // The completed reset emits audio after relocation through OnDropped.
}

FVector ARelicActor::ComputeChargedThrowVelocity(const FVector& AimDirection, float ChargeTimeSeconds) const
{
    const float MinSpeed = RelicSettings ? RelicSettings->MinThrowSpeed : 800.0f;
    const float MaxSpeed = RelicSettings ? RelicSettings->MaxThrowSpeed : 2000.0f;
    const float MaxCharge = RelicSettings ? FMath::Max(RelicSettings->MaxChargeTime, KINDA_SMALL_NUMBER) : 1.5f;
    const float Alpha = FMath::Clamp(ChargeTimeSeconds / MaxCharge, 0.0f, 1.0f);
    const float Speed = FMath::Lerp(MinSpeed, MaxSpeed, Alpha);

    FVector Direction = AimDirection.GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        if (CurrentCarrier)
        {
            Direction = CurrentCarrier->GetActorForwardVector().GetSafeNormal();
        }
        else
        {
            Direction = GetActorForwardVector().GetSafeNormal();
        }
    }
    if (Direction.IsNearlyZero())
    {
        Direction = FVector::ForwardVector;
    }

    return Direction * Speed;
}

FVector ARelicActor::ComputeDirectedPassVelocity(const FVector& TargetLocation) const
{
    const float Speed = (RelicSettings && RelicSettings->PassSpeed > 0.0f)
        ? RelicSettings->PassSpeed
        : (RelicSettings ? RelicSettings->ThrowVelocity : 1500.0f);
    const FVector Origin = CurrentCarrier ? CurrentCarrier->GetActorLocation() : GetActorLocation();
    FVector Direction = (TargetLocation - Origin).GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        Direction = CurrentCarrier ? CurrentCarrier->GetActorForwardVector().GetSafeNormal() : FVector::ForwardVector;
    }
    return Direction * Speed;
}

void ARelicActor::ThrowRelicCharged(float ChargeTimeSeconds, const FVector& AimDirection)
{
    const FVector Velocity = ComputeChargedThrowVelocity(AimDirection, ChargeTimeSeconds);
    if (HasAuthority())
    {
        Server_ThrowRelic_Implementation(Velocity);
    }
    else
    {
        Server_ThrowRelic(Velocity);
    }
}

// --- Throw/Pass RPCs ---

bool ARelicActor::Server_ThrowRelic_Validate(const FVector& ThrowVelocity)
{
    const float BaseMaxThrow = RelicSettings ? FMath::Max(RelicSettings->MaxThrowSpeed, RelicSettings->ThrowVelocity) : 2000.0f;
    const float MaxAllowedSpeed = FMath::Max(BaseMaxThrow * 1.5f, BaseMaxThrow + 500.0f);
    return CurrentState == ERelicState::Carried &&
        CurrentCarrier != nullptr &&
        !ThrowVelocity.ContainsNaN() &&
        ThrowVelocity.SizeSquared() <= FMath::Square(MaxAllowedSpeed);
}
void ARelicActor::Server_ThrowRelic_Implementation(const FVector& ThrowVelocity)
{
    if (HasAuthority() && CurrentState == ERelicState::Carried && CurrentCarrier!= nullptr)
    {
        const int32 CarrierTeam = ResolveCharacterTeamIndex(CurrentCarrier);
        if (CarrierTeam != INDEX_NONE)
        {
            LastPossessingTeam = CarrierTeam;
        }
        LastDroppedCarrier = CurrentCarrier;
        LastDropWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        bIsPassInFlight = false;
        PassingTeamIndex = INDEX_NONE;

        // Send client prediction RPC to the throwing client for immediate feedback
        if (APlayerController* CarrierPC = CurrentCarrier->GetController<APlayerController>())
        {
            ClientPredictThrow(ThrowVelocity);
        }

        DetachFromCarrier(&ThrowVelocity); // Detaches and applies impulse
        CurrentCarrier = nullptr;

        SetRelicState(ERelicState::Thrown);
    }
}

bool ARelicActor::Server_PassRelic_Validate(const FVector& PassVelocity)
{
    const float BaseMaxPass = RelicSettings ? FMath::Max(RelicSettings->PassSpeed, RelicSettings->ThrowVelocity) : 1500.0f;
    const float MaxAllowedSpeed = FMath::Max(BaseMaxPass * 1.5f, BaseMaxPass + 500.0f);
    return CurrentState == ERelicState::Carried &&
        CurrentCarrier != nullptr &&
        !PassVelocity.ContainsNaN() &&
        PassVelocity.SizeSquared() <= FMath::Square(MaxAllowedSpeed);
}
void ARelicActor::Server_PassRelic_Implementation(const FVector& PassVelocity)
{
    if (HasAuthority() && CurrentState == ERelicState::Carried && CurrentCarrier!= nullptr)
    {
        const int32 CarrierTeam = ResolveCharacterTeamIndex(CurrentCarrier);
        if (CarrierTeam != INDEX_NONE)
        {
            LastPossessingTeam = CarrierTeam;
        }
        LastDroppedCarrier = CurrentCarrier;
        LastDropWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        PassingTeamIndex = LastPossessingTeam;
        bIsPassInFlight = true;

        DetachFromCarrier(&PassVelocity); // Detaches and applies impulse
        CurrentCarrier = nullptr;

        SetRelicState(ERelicState::BeingPassed);
    }
}

void ARelicActor::BindCarrierCCTagListener(ABwayCharacterWithAbilities* Carrier)
{
    UnbindCarrierCCTagListener();
    if (!HasAuthority() || !Carrier)
    {
        return;
    }

    UAbilitySystemComponent* ASC = nullptr;
    if (APlayerState* CarrierPS = Carrier->GetPlayerState())
    {
        ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CarrierPS);
    }
    if (!ASC)
    {
        ASC = Carrier->GetAbilitySystemComponent();
    }
    if (!ASC)
    {
        return;
    }

    BoundCarrierASC = ASC;
    CarrierTagEventHandle = ASC->RegisterGenericGameplayTagEvent().AddUObject(
        this, &ARelicActor::HandleCarrierGameplayTagChanged);
}

void ARelicActor::UnbindCarrierCCTagListener()
{
    if (BoundCarrierASC.IsValid() && CarrierTagEventHandle.IsValid())
    {
        BoundCarrierASC->RegisterGenericGameplayTagEvent().Remove(CarrierTagEventHandle);
    }
    CarrierTagEventHandle.Reset();
    BoundCarrierASC.Reset();
}

void ARelicActor::HandleCarrierGameplayTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (!HasAuthority() || NewCount <= 0 || CurrentState != ERelicState::Carried || !CurrentCarrier)
    {
        return;
    }

    if (RelicSettings && !RelicSettings->bFumbleOnDamage)
    {
        return;
    }

    const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"), /*ErrorIfNotFound*/ false);
    const bool bIsHardCCTag =
        (Tag == BwayGameplayTags::State_Status_Silenced) ||
        (Tag == BwayGameplayTags::State_Status_Jailed) ||
        (StunnedTag.IsValid() && Tag.MatchesTag(StunnedTag));

    if (!bIsHardCCTag)
    {
        return;
    }

    AActor* InstigatorActor = LastCarrierDamageInstigatorPS.Get();
    if (!InstigatorActor && BoundCarrierASC.IsValid())
    {
        FGameplayEffectQuery Query;
        TArray<FActiveGameplayEffectHandle> ActiveHandles = BoundCarrierASC->GetActiveEffects(Query);
        for (const FActiveGameplayEffectHandle& Handle : ActiveHandles)
        {
            if (const FActiveGameplayEffect* ActiveGE = BoundCarrierASC->GetActiveGameplayEffect(Handle))
            {
                FGameplayTagContainer GrantedTags;
                ActiveGE->Spec.GetAllGrantedTags(GrantedTags);
                if (GrantedTags.HasTag(Tag))
                {
                    if (AActor* EffectInstigator = ActiveGE->Spec.GetEffectContext().GetInstigator())
                    {
                        InstigatorActor = EffectInstigator;
                        break;
                    }
                }
            }
        }
    }

    ForceFumbleFromDamageWithInstigator(InstigatorActor);
}

// --- Attachment/Detachment Helpers ---

void ARelicActor::AttachToCarrier(ABwayCharacterWithAbilities* Carrier)
{
    if (!Carrier) return;

    // Use settings for socket name
    FName AttachSocketName = RelicSettings ? FName(RelicSettings->RelicSocket) : NAME_None;

    // Zero velocity and disable physics before attaching
    if (RelicMesh->IsSimulatingPhysics())
    {
        RelicMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        RelicMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
    RelicMesh->SetSimulatePhysics(false);
    RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Attach to the carrier's mesh
    if (USkeletalMeshComponent* CarrierMesh = Carrier->GetMesh())
    {
        AttachToComponent(CarrierMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
    }

    if (HasAuthority())
    {
        BindCarrierCCTagListener(Carrier);

        // --- Grant Ability Set ---
        if (APlayerState* CarrierPlayerState = Carrier->GetPlayerState())
        {
            ULyraAbilitySystemComponent* PlayerStateASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CarrierPlayerState));
            ULyraAbilitySet* AbilitySetToGrant = LoadedRelicAbilitySet;

            if (PlayerStateASC && AbilitySetToGrant)
            {
                AbilitySetToGrant->GiveToAbilitySystem(PlayerStateASC, &GrantedCarrierSetHandle, this);
                UE_LOG(LogTemp, Log, TEXT("Server: Granted RelicAbilitySet '%s' to PlayerState ASC of %s (Carrier: %s)"), *GetNameSafe(AbilitySetToGrant), *GetNameSafe(CarrierPlayerState), *GetNameSafe(Carrier));

                if (!GrantedPassAbilityHandle.IsValid())
                {
                    FGameplayAbilitySpec PassSpec(UBwayGameplayAbility_RelicPass::StaticClass(), 1);
                    PassSpec.SourceObject = this;
                    PassSpec.GetDynamicSpecSourceTags().AddTag(FGameplayTag::RequestGameplayTag(FName("InputTag.Relic.Pass"), false));
                    GrantedPassAbilityHandle = PlayerStateASC->GiveAbility(PassSpec);
                    UE_LOG(LogTemp, Log, TEXT("Server: Granted native RelicPass ability to %s (Handle: %s)"), *GetNameSafe(CarrierPlayerState), *GrantedPassAbilityHandle.ToString());
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Server: Failed to grant RelicAbilitySet to PlayerState. PlayerStateASC Valid: %d, AbilitySetToGrant Valid: %d"),
                    IsValid(PlayerStateASC), IsValid(AbilitySetToGrant));
            }

            if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(CarrierPlayerState))
            {
                BwayPS->ApplyRelicCarrierTag(true, EGameplayTagReplicationState::TagAndCountToAll);
                BwayPS->SetHasRelic(true);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Server: Carrier %s has no PlayerState, cannot grant RelicAbilitySet."), *GetNameSafe(Carrier));
        }
        // --- End Grant Ability Set ---
    }
    else if (Carrier->IsLocallyControlled())
    {
        // Client-side mirror for the local carrier so combat abilities are blocked before
        // replicated loose tags arrive (prevents LocalPredicted rubberbanding).
        if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Carrier->GetPlayerState()))
        {
            BwayPS->ApplyRelicCarrierTag(true, EGameplayTagReplicationState::None);
        }
    }

    // If called on client via OnRep_CurrentCarrier, this visually attaches the relic.
    // If called on server via OnPickedUp, this sets up the authoritative state.
}

void ARelicActor::DetachFromCarrier(const FVector* InitialVelocity)
{
    if (HasAuthority())
    {
        UnbindCarrierCCTagListener();
    }

    // Detach from parent FIRST visually
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // Re-enable collision while keeping Pawn channel overlap so catches do not bounce off capsules
    RelicMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RelicMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    RelicMesh->SetUseCCD(true);

    if (HasAuthority())
    {
        // --- Clear Ability Set (Server Only) ---
        if (CurrentCarrier) // Check if there was a carrier before clearing CurrentCarrier
        {
            if (APlayerState* CarrierPlayerState = CurrentCarrier->GetPlayerState())
            {
                if (ULyraAbilitySystemComponent* PlayerStateASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CarrierPlayerState))) // Ensure the handle is valid before taking
                {
                    GrantedCarrierSetHandle.TakeFromAbilitySystem(PlayerStateASC);
					if (GrantedPassAbilityHandle.IsValid())
					{
						PlayerStateASC->ClearAbility(GrantedPassAbilityHandle);
						GrantedPassAbilityHandle = FGameplayAbilitySpecHandle();
					}
                    UE_LOG(LogTemp, Log, TEXT("Server: Cleared Ability Set from PlayerState ASC of %s (Carrier: %s)"), *GetNameSafe(CarrierPlayerState), *GetNameSafe(CurrentCarrier));

                }

                if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(CarrierPlayerState))
                {
                    BwayPS->ApplyRelicCarrierTag(false, EGameplayTagReplicationState::TagAndCountToAll);
                    BwayPS->SetHasRelic(false);
                }
            }
        }
        // --- End Clear Ability Set ---


        // Enable server-side physics AFTER clearing abilities/detaching
        RelicMesh->SetSimulatePhysics(true);
        if (InitialVelocity)
        {
            RelicMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            RelicMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            RelicMesh->AddImpulse(*InitialVelocity, NAME_None, true); // true for velocity change
        }
    }
    else
    {
        const bool bShouldSimulateForSmoothing =
            CurrentState == ERelicState::Neutral ||
            CurrentState == ERelicState::Dropped ||
            CurrentState == ERelicState::Thrown ||
            CurrentState == ERelicState::BeingPassed ||
            CurrentState == ERelicState::Passed;
        RelicMesh->SetSimulatePhysics(bShouldSimulateForSmoothing);
    }
}

// --- Visual/Audio Feedback Implementation ---

void ARelicActor::UpdateStateVFX(ERelicState NewState)
{
    // Persistent effects are reconciled from state, including initial replication
    // and relevancy re-entry. Repeated notifications must not restart the effect.
    const bool bCanRender = RelicSettings && VisualEffectsRoot && GetNetMode() != NM_DedicatedServer;
    UNiagaraSystem* EffectToSpawn = nullptr;
    TObjectPtr<UNiagaraComponent>* ComponentToSet = nullptr;
    if (bCanRender)
    {
        switch (NewState)
        {
        case ERelicState::Neutral:
            EffectToSpawn = RelicSettings->IdleEffect.LoadSynchronous();
            ComponentToSet = &IdleEffectComponent;
            break;
        case ERelicState::Carried:
            EffectToSpawn = RelicSettings->CarriedEffect.LoadSynchronous();
            ComponentToSet = &CarriedEffectComponent;
            break;
        case ERelicState::Dropped:
            EffectToSpawn = RelicSettings->DroppedEffect.LoadSynchronous();
            ComponentToSet = &DroppedEffectComponent;
            break;
        case ERelicState::Scoring:
            EffectToSpawn = RelicSettings->ScoringEffect.LoadSynchronous();
            ComponentToSet = &ScoringEffectComponent;
            break;
        default:
            break;
        }
    }
    for (TObjectPtr<UNiagaraComponent>* Component : {&IdleEffectComponent, &CarriedEffectComponent, &DroppedEffectComponent, &ScoringEffectComponent})
    {
        if (Component != ComponentToSet || !IsValid(*Component) || (*Component)->GetAsset() != EffectToSpawn)
        {
            if (IsValid(*Component)) (*Component)->DestroyComponent();
            *Component = nullptr;
        }
    }
    if (EffectToSpawn && ComponentToSet && !IsValid(*ComponentToSet))
    {
        *ComponentToSet = UNiagaraFunctionLibrary::SpawnSystemAttached(
            EffectToSpawn, VisualEffectsRoot, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset, false, true, ENCPoolMethod::None, false);
    }
    if (bCanRender && (NewState == ERelicState::Thrown || NewState == ERelicState::BeingPassed || NewState == ERelicState::Passed))
    {
        SpawnTrailEffect();
    }
    else
    {
        DestroyTrailEffect();
    }
#if !UE_BUILD_SHIPPING
    if (CVarBwayRelicDebugFeedback.GetValueOnGameThread() != 0)
    {
        const UNiagaraComponent* StateComponent = ComponentToSet ? ComponentToSet->Get() : nullptr;
        UE_LOG(LogTemp, Display, TEXT("BwayRelicFeedback: VFX state=%d net=%d stateComponent=%s trailComponent=%s stateParent=%s trailParent=%s anchorReplicated=%d meshReplicated=%d"),
            int32(NewState), int32(GetNetMode()), *GetNameSafe(StateComponent), *GetNameSafe(TrailEffectComponent),
            *GetNameSafe(StateComponent ? StateComponent->GetAttachParent() : nullptr),
            *GetNameSafe(TrailEffectComponent ? TrailEffectComponent->GetAttachParent() : nullptr),
            VisualEffectsRoot && VisualEffectsRoot->GetIsReplicated(), RelicMesh && RelicMesh->GetIsReplicated());
    }
#endif
}

void ARelicActor::CleanupStateVFX(ERelicState OldState)
{
    TObjectPtr<UNiagaraComponent>* ComponentToCleanup = nullptr;

    switch (OldState)
    {
    case ERelicState::Neutral:
        ComponentToCleanup = &IdleEffectComponent;
        break;
    case ERelicState::Carried:
        ComponentToCleanup = &CarriedEffectComponent;
        break;
    case ERelicState::Dropped:
        ComponentToCleanup = &DroppedEffectComponent;
        break;
    case ERelicState::Scoring:
        ComponentToCleanup = &ScoringEffectComponent;
        break;
    default:
        break;
    }

    if (ComponentToCleanup)
    {
        if (IsValid(*ComponentToCleanup)) (*ComponentToCleanup)->DestroyComponent();
        *ComponentToCleanup = nullptr;
    }
}

void ARelicActor::SpawnTrailEffect()
{
    if (!RelicSettings || !VisualEffectsRoot || GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    UNiagaraSystem* Effect = RelicSettings->TrailEffect.LoadSynchronous();
    if (IsValid(TrailEffectComponent) && TrailEffectComponent->GetAsset() == Effect) return;
    DestroyTrailEffect();
    if (!Effect) return;
    TrailEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        Effect, VisualEffectsRoot, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset, false, false, ENCPoolMethod::None, false);
    if (TrailEffectComponent)
    {
        TrailEffectComponent->SetVariableFloat(TEXT("User.Trail_Length_Second"), FMath::Clamp(RelicSettings->TrailLifetimeSeconds, 0.05f, 2.0f));
        UpdateTeamColorTinting();
        TrailEffectComponent->Activate(true);
    }
}

void ARelicActor::DestroyTrailEffect()
{
    if (IsValid(TrailEffectComponent)) TrailEffectComponent->DestroyComponent();
    TrailEffectComponent = nullptr;
}

void ARelicActor::UpdateTeamColorTinting()
{
    // Get team color based on LastPossessingTeam
    FLinearColor TeamColor = FLinearColor::White; // Default neutral color

    if (LastPossessingTeam >= 0)
    {
        // Use Blue for Team 0, Red for Team 1 (matching goal volume logic)
        TeamColor = (LastPossessingTeam == 0) ? FLinearColor::Blue : FLinearColor::Red;
    }

    // Update material parameter (assuming material has "TeamColor" parameter)
    if (TeamColorMaterialInstance)
    {
        TeamColorMaterialInstance->SetVectorParameterValue(FName("TeamColor"), TeamColor);
    }
    if (IsValid(TrailEffectComponent))
    {
        TrailEffectComponent->SetVariableLinearColor(TEXT("User.Team_Color"), TeamColor);
    }
}

void ARelicActor::PlayStateAudio(ERelicState State)
{
    if (!HasAuthority() || !RelicSettings)
    {
        return;
    }

    USoundBase* SoundToPlay = nullptr;

    switch (State)
    {
    case ERelicState::Carried:
        SoundToPlay = RelicSettings->PickupSound.LoadSynchronous();
        break;
    case ERelicState::Dropped:
        SoundToPlay = RelicSettings->DropSound.LoadSynchronous();
        break;
    case ERelicState::Scoring:
        SoundToPlay = RelicSettings->ScoringSound.LoadSynchronous();
        break;
    case ERelicState::Resetting:
        SoundToPlay = RelicSettings->ResetSound.LoadSynchronous();
        break;
    default:
        break;
    }

    if (SoundToPlay && RelicMesh)
    {
        Multicast_PlayStateAudio(State, SoundToPlay, RelicMesh->GetComponentLocation(), ++NextAudioEventId);
    }
}

void ARelicActor::Multicast_PlayStateAudio_Implementation(ERelicState State, USoundBase* Sound,
    FVector_NetQuantize Location, uint32 EventId)
{
    const bool bSubmitAudio = Sound && GetNetMode() != NM_DedicatedServer;
    if (bSubmitAudio)
    {
        UGameplayStatics::PlaySoundAtLocation(
            GetWorld(),
            Sound,
            Location,
            FRotator::ZeroRotator,
            1.0f,
            1.0f,
            0.0f,
            nullptr,
            nullptr,
            nullptr
        );
    }
#if !UE_BUILD_SHIPPING
    if (CVarBwayRelicDebugFeedback.GetValueOnGameThread() != 0)
    {
        UE_LOG(LogTemp, Display, TEXT("BwayRelicFeedback: Audio event=%u state=%d net=%d sound=%s submitted=%d settings=%s location=%s"),
            EventId, int32(State), int32(GetNetMode()), *GetPathNameSafe(Sound), bSubmitAudio,
            *GetPathNameSafe(RelicSettings), *Location.ToCompactString());
    }
#endif
}

void ARelicActor::ClientPredictThrow_Implementation(const FVector& ThrowVelocity)
{
    // Client-side prediction: immediately apply throw physics on client
    // This will be corrected by server replication, but provides instant feedback
    if (!HasAuthority() && RelicMesh)
    {
        // Detach visually if still attached
        if (GetAttachParentActor())
        {
            DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        }

        // Enable physics temporarily for prediction
        RelicMesh->SetSimulatePhysics(true);
        RelicMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        // Apply the throw velocity
        RelicMesh->SetPhysicsLinearVelocity(ThrowVelocity);

        // The server's authoritative state will correct this shortly
        UE_LOG(LogTemp, VeryVerbose, TEXT("Client predicted throw with velocity: %s"), *ThrowVelocity.ToString());
    }
}
