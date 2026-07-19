#include "Abilities/BwayGameplayEffect_RawlinsJail.h"

#include "BwayGameplayTags.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayEffect_RawlinsJail)

UGE_Bway_RawlinsJailed::UGE_Bway_RawlinsJailed()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	PendingGrantedTags.Add(TAG_Gameplay_MovementStopped);
	PendingGrantedTags.Add(BwayGameplayTags::State_Status_Jailed);
}

void UGE_Bway_RawlinsJailed::PostInitProperties()
{
	Super::PostInitProperties();

	if (PendingGrantedTags.Num() == 0)
	{
		return;
	}

	FInheritedTagContainer TagChanges;
	for (const FGameplayTag& Tag : PendingGrantedTags)
	{
		TagChanges.AddTag(Tag);
	}

	UTargetTagsGameplayEffectComponent& TargetTags = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();
	TargetTags.SetAndApplyTargetTagChanges(TagChanges);
}
