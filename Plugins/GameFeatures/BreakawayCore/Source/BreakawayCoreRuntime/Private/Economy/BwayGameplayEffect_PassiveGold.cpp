#include "Economy/BwayGameplayEffect_PassiveGold.h"

#include "Economy/BwayGoldAttributeSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayEffect_PassiveGold)

const FName UBwayGameplayEffect_PassiveGold::IncomeMagnitudeName(TEXT("PassiveGoldPerSecond"));

UBwayGameplayEffect_PassiveGold::UBwayGameplayEffect_PassiveGold()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = false;

	FSetByCallerFloat Income;
	Income.DataName = IncomeMagnitudeName;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UBwayGoldAttributeSet::GetCurrentGoldAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Income);
}
