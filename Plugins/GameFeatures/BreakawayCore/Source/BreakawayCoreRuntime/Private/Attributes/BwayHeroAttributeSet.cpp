#include "Attributes/BwayHeroAttributeSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroAttributeSet)

UBwayHeroAttributeSet::UBwayHeroAttributeSet()
{
}

void UBwayHeroAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBwayHeroAttributeSet, AttackStrength, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBwayHeroAttributeSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBwayHeroAttributeSet, MoveSpeedRating, COND_None, REPNOTIFY_Always);
}

void UBwayHeroAttributeSet::OnRep_AttackStrength(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBwayHeroAttributeSet, AttackStrength, OldValue);
}

void UBwayHeroAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBwayHeroAttributeSet, Armor, OldValue);
}

void UBwayHeroAttributeSet::OnRep_MoveSpeedRating(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBwayHeroAttributeSet, MoveSpeedRating, OldValue);
}
