#include "Relic/RelicStateMachine.h"
#include "BwayCharacterWithAbilities.h"
#include "Net/UnrealNetwork.h"

FRelicStateMachine::FRelicStateMachine()
{
    CurrentState = ERelicState::Inactive;
    PreviousState = ERelicState::Inactive;
    TimeInCurrentState = 0.0f;
    bTransitionInProgress = false;
    CurrentCarrier = nullptr;
    LastPossessingTeam = -1;
}

void FRelicStateMachine::UpdateState(float DeltaTime)
{
    // Update time in current state
    TimeInCurrentState += DeltaTime;
}

bool FRelicStateMachine::RequestStateChange(ERelicState NewState, ABwayCharacterWithAbilities* NewCarrier)
{
    // Validate the transition
    if (!CanTransitionTo(NewState, NewCarrier))
    {
        return false;
    }
    
    // If a transition is already in progress, deny the request
    if (bTransitionInProgress)
    {
        return false;
    }
    
    // Set transition flag
    bTransitionInProgress = true;
    
    // Store previous state
    PreviousState = CurrentState;
    
    // Update state
    CurrentState = NewState;
    
    // Update carrier if provided
    if (NewState == ERelicState::Carried)
    {
        CurrentCarrier = NewCarrier;
        
        // Update last possessing team if carrier is valid
        if (CurrentCarrier)
        {
            LastPossessingTeam = CurrentCarrier->GetTeamId();
        }
    }
    else if (NewState != ERelicState::Carried)
    {
        // Clear carrier if not in carried state
        CurrentCarrier = nullptr;
    }
    
    // Reset time in state
    TimeInCurrentState = 0.0f;
    
    // Clear transition flag
    bTransitionInProgress = false;
    
    // Broadcast state changed event
    OnStateChanged.Broadcast(CurrentState, PreviousState, CurrentCarrier);
    
    return true;
}

bool FRelicStateMachine::CanTransitionTo(ERelicState NewState, ABwayCharacterWithAbilities* NewCarrier)
{
    // Always allow transition to Inactive (for match end/reset)
    if (NewState == ERelicState::Inactive)
    {
        return true;
    }
    
    // Check valid transitions based on current state
    switch (CurrentState)
    {
        case ERelicState::Inactive:
            // Can only go to Neutral from Inactive
            return NewState == ERelicState::Neutral;
            
        case ERelicState::Neutral:
            // From Neutral, can go to Carried (pickup)
            return NewState == ERelicState::Carried && NewCarrier != nullptr;
            
        case ERelicState::Carried:
            // From Carried, can go to Dropped or Scoring
            if (NewState == ERelicState::Dropped)
            {
                return true;
            }
            if (NewState == ERelicState::Scoring && CurrentCarrier != nullptr)
            {
                return true;
            }
            return false;
            
        case ERelicState::Dropped:
            // From Dropped, can go to Carried (pickup) or Neutral (timeout)
            if (NewState == ERelicState::Carried)
            {
                return NewCarrier != nullptr;
            }
            return NewState == ERelicState::Neutral;
            
        case ERelicState::Scoring:
            // From Scoring, can only go to Resetting
            return NewState == ERelicState::Resetting;
            
        case ERelicState::Resetting:
            // From Resetting, can only go to Neutral
            return NewState == ERelicState::Neutral;
            
        default:
            return false;
    }
}

FString FRelicStateMachine::GetStateAsString()
{
    switch (CurrentState)
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