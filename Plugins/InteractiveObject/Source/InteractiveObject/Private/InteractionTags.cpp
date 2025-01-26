// InteractionTags.cpp
#include "InteractionTags.h"
#include "GameplayTagsManager.h"

FInteractionTags FInteractionTags::InteractionTags;

FInteractionTags::FInteractionTags()
{
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
    
	Ability_Interaction = TagManager.AddNativeGameplayTag(TEXT("Ability.Interaction"));
	Ability_Throw = TagManager.AddNativeGameplayTag(TEXT("Ability.Throw"));
	Ability_Pass = TagManager.AddNativeGameplayTag(TEXT("Ability.Pass"));
	Status_Holding = TagManager.AddNativeGameplayTag(TEXT("Status.Holding"));
	Status_Charging = TagManager.AddNativeGameplayTag(TEXT("Status.Charging"));
	Event_InteractionComplete = TagManager.AddNativeGameplayTag(TEXT("Event.Interaction.Complete"));
	Event_InteractionFailed = TagManager.AddNativeGameplayTag(TEXT("Event.Interaction.Failed"));
	Interactable = TagManager.AddNativeGameplayTag(TEXT("Interactable"));
}