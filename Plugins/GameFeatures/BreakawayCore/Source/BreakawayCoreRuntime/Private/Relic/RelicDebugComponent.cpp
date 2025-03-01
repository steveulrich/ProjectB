#include "Relic/RelicDebugComponent.h"
#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

URelicDebugComponent::URelicDebugComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    bDebugVisualizationEnabled = false;
    DebuggedRelic = nullptr;
}

void URelicDebugComponent::BeginPlay()
{
    Super::BeginPlay();
}

void URelicDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (!DebuggedRelic)
    {
        return;
    }
    
    // Check for error states
    CheckForErrorStates();
    
    // Draw debug visualization if enabled
    if (bDebugVisualizationEnabled)
    {
        FVector RelicLocation = DebuggedRelic->GetActorLocation();
        
        // Draw a sphere around the Relic
        DrawDebugSphere(
            GetWorld(),
            RelicLocation,
            50.0f,
            12,
            FColor::Yellow,
            false,
            -1.0f, // Persistent between frames
            0,
            2.0f
        );
        
        // Draw state text
        FString StateText = GetRelicStateString() + " (" + FString::Printf(TEXT("%.1f"), GetTimeInCurrentState()) + "s)";
        DrawDebugString(
            GetWorld(),
            RelicLocation + FVector(0, 0, 100),
            StateText,
            nullptr,
            FColor::White,
            0.0f,
            true
        );
        
        // Draw carrier name if carried
        if (DebuggedRelic->IsCarried())
        {
            DrawDebugString(
                GetWorld(),
                RelicLocation + FVector(0, 0, 125),
                "Carried by: " + GetCarrierName(),
                nullptr,
                FColor::Green,
                0.0f,
                true
            );
        }
    }
}

void URelicDebugComponent::SetRelicToDebug(ARelicActor* InRelic)
{
    DebuggedRelic = InRelic;
}

FString URelicDebugComponent::GetRelicStateString() const
{
    if (!DebuggedRelic)
    {
        return FString("No Relic");
    }
    
    switch (DebuggedRelic->GetState())
    {
        case ERelicState::Inactive:
            return FString("Inactive");
        case ERelicState::Neutral:
            return FString("Neutral");
        case ERelicState::Carried:
            return FString("Carried");
        case ERelicState::Dropped:
            return FString("Dropped");
        case ERelicState::Scoring:
            return FString("Scoring");
        case ERelicState::Resetting:
            return FString("Resetting");
        default:
            return FString("Unknown");
    }
}

float URelicDebugComponent::GetTimeInCurrentState() const
{
    if (!DebuggedRelic)
    {
        return 0.0f;
    }
    
    return DebuggedRelic->StateMachine.TimeInCurrentState;
}

FString URelicDebugComponent::GetCarrierName() const
{
    if (!DebuggedRelic || !DebuggedRelic->IsCarried() || !DebuggedRelic->GetCarrier())
    {
        return FString("None");
    }
    
    return DebuggedRelic->GetCarrier()->GetName();
}

void URelicDebugComponent::CheckForErrorStates()
{
    if (!DebuggedRelic)
    {
        return;
    }
    
    bool bErrorDetected = false;
    
    // Check for known error conditions
    if (IsRelicStuck())
    {
        UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Relic appears to be stuck in state %s for %.1f seconds"), 
            *GetRelicStateString(), GetTimeInCurrentState());
        bErrorDetected = true;
    }
    
    if (IsRelicOutOfBounds())
    {
        UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Relic is out of bounds at %s"), 
            *DebuggedRelic->GetActorLocation().ToString());
        bErrorDetected = true;
    }
    
    // Attempt recovery if error detected
    if (bErrorDetected && DebuggedRelic->HasAuthority())
    {
        // In a development build, attempt automatic recovery
#if !UE_BUILD_SHIPPING
        AttemptRecoveryFromErrorState();
#endif
    }
}

void URelicDebugComponent::AttemptRecoveryFromErrorState()
{
    if (!DebuggedRelic || !DebuggedRelic->HasAuthority())
    {
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Attempting recovery for Relic"));
    
    // Based on the current state, take appropriate recovery action
    switch (DebuggedRelic->GetState())
    {
        case ERelicState::Dropped:
            // If stuck in dropped state, force reset
            if (GetTimeInCurrentState() > MaxTimeInDroppedState)
            {
                UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Forcing Relic reset from stuck Dropped state"));
                DebuggedRelic->ResetRelic();
            }
            break;
            
        case ERelicState::Scoring:
            // If stuck in scoring state, force reset
            if (GetTimeInCurrentState() > MaxTimeInScoringState)
            {
                UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Forcing Relic reset from stuck Scoring state"));
                DebuggedRelic->ResetRelic();
            }
            break;
            
        case ERelicState::Resetting:
            // If stuck in resetting state, force to neutral
            if (GetTimeInCurrentState() > MaxTimeInResettingState)
            {
                UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Forcing Relic to Neutral from stuck Resetting state"));
                DebuggedRelic->StateMachine.RequestStateChange(ERelicState::Neutral);
            }
            break;
            
        case ERelicState::Carried:
            // If carried but carrier is invalid, force drop
            if (!DebuggedRelic->GetCarrier())
            {
                UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Forcing Relic drop due to invalid carrier"));
                DebuggedRelic->DropRelic(false);
            }
            break;
            
        default:
            break;
    }
    
    // If out of bounds, move back to spawn
    if (IsRelicOutOfBounds())
    {
        UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Moving out-of-bounds Relic back to spawn"));
        DebuggedRelic->ResetRelic();
    }
}

void URelicDebugComponent::DebugForceRelicState(FString StateName)
{
    if (!DebuggedRelic || !DebuggedRelic->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Cannot force state - no valid Relic or no authority"));
        return;
    }
    
    ERelicState NewState;
    
    // Convert string to enum
    if (StateName.Equals("Inactive", ESearchCase::IgnoreCase))
    {
        NewState = ERelicState::Inactive;
    }
    else if (StateName.Equals("Neutral", ESearchCase::IgnoreCase))
    {
        NewState = ERelicState::Neutral;
    }
    else if (StateName.Equals("Dropped", ESearchCase::IgnoreCase))
    {
        NewState = ERelicState::Dropped;
    }
    else if (StateName.Equals("Reset", ESearchCase::IgnoreCase) || StateName.Equals("Resetting", ESearchCase::IgnoreCase))
    {
        NewState = ERelicState::Resetting;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("RELIC DEBUG: Invalid state name '%s'"), *StateName);
        return;
    }
    
    // Force state change
    DebuggedRelic->StateMachine.RequestStateChange(NewState);
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: Forced Relic to state %s"), *StateName);
}

void URelicDebugComponent::DebugRelicInfo()
{
    if (!DebuggedRelic)
    {
        UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: No Relic being debugged"));
        return;
    }
    
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: === Relic Information ==="));
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: State: %s"), *GetRelicStateString());
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: Time in state: %.1f seconds"), GetTimeInCurrentState());
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: Location: %s"), *DebuggedRelic->GetActorLocation().ToString());
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: Carrier: %s"), *GetCarrierName());
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: Last possessing team: %d"), DebuggedRelic->GetLastPossessingTeam());
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: Has authority: %s"), DebuggedRelic->HasAuthority() ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: ==========================="));
}

void URelicDebugComponent::ToggleDebugVisualization(bool bEnable)
{
    bDebugVisualizationEnabled = bEnable;
    
    if (bEnable)
    {
        UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: Debug visualization enabled"));
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("RELIC DEBUG: Debug visualization disabled"));
    }
}

bool URelicDebugComponent::IsRelicStuck() const
{
    if (!DebuggedRelic)
    {
        return false;
    }
    
    // Check for state-specific timeout conditions
    switch (DebuggedRelic->GetState())
    {
        case ERelicState::Dropped:
            return GetTimeInCurrentState() > MaxTimeInDroppedState;
            
        case ERelicState::Scoring:
            return GetTimeInCurrentState() > MaxTimeInScoringState;
            
        case ERelicState::Resetting:
            return GetTimeInCurrentState() > MaxTimeInResettingState;
            
        case ERelicState::Carried:
            // If carried but no valid carrier, consider it stuck
            return DebuggedRelic->GetCarrier() == nullptr;
            
        default:
            return false;
    }
}

bool URelicDebugComponent::IsRelicOutOfBounds() const
{
    if (!DebuggedRelic)
    {
        return false;
    }
    
    // Simple check if Relic is below the kill plane
    float KillZ = -10000.0f; // This would typically be referenced from the world settings
    return DebuggedRelic->GetActorLocation().Z < KillZ;
}

bool URelicDebugComponent::HasRelicBeenInStateForTooLong() const
{
    // Already covered by IsRelicStuck
    return false;
}