#include "Attributes/BwayHeroAttributeSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroAttributeSet)

UBwayHeroAttributeSet::UBwayHeroAttributeSet()
	: MoveSpeedMultiplier(1.f)
	, IncomingDamageMultiplier(1.f)
{
}

void UBwayHeroAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBwayHeroAttributeSet, AttackStrength, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBwayHeroAttributeSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBwayHeroAttributeSet, MoveSpeedRating, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBwayHeroAttributeSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBwayHeroAttributeSet, IncomingDamageMultiplier, COND_None, REPNOTIFY_Always);
}

void UBwayHeroAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UBwayHeroAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UBwayHeroAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.05f, 3.f);
	}
	else if (Attribute == GetIncomingDamageMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.05f, 5.f);
	}
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

void UBwayHeroAttributeSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBwayHeroAttributeSet, MoveSpeedMultiplier, OldValue);
}

void UBwayHeroAttributeSet::OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBwayHeroAttributeSet, IncomingDamageMultiplier, OldValue);
}
