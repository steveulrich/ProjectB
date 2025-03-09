#include "Relic/RelicSettings.h"

URelicSettings::URelicSettings()
{
	// Initialize default gameplay tags
	//RelicTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.Object.Relic"));
	//CarrierTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.RelicCarrier"));
	//DroppedTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.RelicDropped"));
	//ScoringTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.RelicScoring"));
    
	// Initialize team tags
	//Team1Tag = FGameplayTag::RequestGameplayTag(FName("Gameplay.Team.1"));
	//Team2Tag = FGameplayTag::RequestGameplayTag(FName("Gameplay.Team.2"));
    
	// Initialize restricted ability tags with common movement abilities
	//RestrictedAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement.Dash")));
	//RestrictedAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Special")));
}