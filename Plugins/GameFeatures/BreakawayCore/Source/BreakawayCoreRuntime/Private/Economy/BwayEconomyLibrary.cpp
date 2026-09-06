#include "Economy/BwayEconomyLibrary.h"

#include "AbilitySystemGlobals.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayEconomyLibrary)

const FName UBwayGameplayEffect_SpendGold::DebitMagnitudeName(TEXT("GoldDebit"));

UBwayGameplayEffect_SpendGold::UBwayGameplayEffect_SpendGold()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FSetByCallerFloat Debit;
	Debit.DataName = DebitMagnitudeName;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UBwayGoldAttributeSet::GetCurrentGoldAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Debit);
}

bool UBwayEconomyLibrary::TrySpendGold(APlayerState* PlayerState, int32 Cost)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || Cost < 0)
	{
		return false;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState, true);
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	if (!Gold || !FMath::IsFinite(Gold->GetCurrentGold()) || Gold->GetCurrentGold() < static_cast<double>(Cost))
	{
		return false;
	}
	if (Cost == 0)
	{
		return true;
	}
	FGameplayEffectSpec Spec(GetDefault<UBwayGameplayEffect_SpendGold>(), ASC->MakeEffectContext(), 1.0f);
	Spec.SetSetByCallerMagnitude(UBwayGameplayEffect_SpendGold::DebitMagnitudeName, -static_cast<float>(Cost));
	return ASC->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied();
}
