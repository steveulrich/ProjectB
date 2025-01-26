// InteractionTags.h
#pragma once

#include "GameplayTagContainer.h"

struct FInteractionTags
{
	FInteractionTags();
    
	FGameplayTag Ability_Interaction;
	FGameplayTag Ability_Throw;
	FGameplayTag Ability_Pass;
	FGameplayTag Status_Holding;
	FGameplayTag Status_Charging;
	FGameplayTag Event_InteractionComplete;
	FGameplayTag Event_InteractionFailed;
	FGameplayTag Interactable;
    
	static const FInteractionTags& Get() { return InteractionTags; }
    
private:
	static FInteractionTags InteractionTags;
};