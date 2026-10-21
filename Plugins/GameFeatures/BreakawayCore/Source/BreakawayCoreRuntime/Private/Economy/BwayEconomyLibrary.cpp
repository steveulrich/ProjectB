#include "Economy/BwayEconomyLibrary.h"

#include "AbilitySystemGlobals.h"
#include "BwayPlayerState.h"
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

bool UBwayEconomyLibrary::TryRefundGold(APlayerState* PlayerState, int32 RefundAmount)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || RefundAmount < 0)
	{
		return false;
	}
	if (RefundAmount == 0)
	{
		return true;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState, true);
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	if (!Gold || !FMath::IsFinite(Gold->GetCurrentGold()))
	{
		return false;
	}

	ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState);
	const bool bPrevSuppress = BwayPS ? BwayPS->IsSuppressingMatchGoldEarningsTracking() : false;
	if (BwayPS)
	{
		BwayPS->SetSuppressMatchGoldEarningsTracking(true);
	}

	const float TargetBalance = Gold->GetCurrentGold() + static_cast<float>(RefundAmount);
	if (TargetBalance > Gold->GetMaxGold())
	{
		ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetMaxGoldAttribute(), FMath::Max(TargetBalance, 10000.0f));
	}

	FGameplayEffectSpec Spec(GetDefault<UBwayGameplayEffect_SpendGold>(), ASC->MakeEffectContext(), 1.0f);
	Spec.SetSetByCallerMagnitude(UBwayGameplayEffect_SpendGold::DebitMagnitudeName, static_cast<float>(RefundAmount));
	const bool bApplied = ASC->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied();

	if (BwayPS)
	{
		BwayPS->SetSuppressMatchGoldEarningsTracking(bPrevSuppress);
	}
	return bApplied;
}

bool UBwayEconomyLibrary::GrantStartingMatchGold(APlayerState* PlayerState, int32 StartingGold)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || StartingGold < 0)
	{
		return false;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState, true);
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	if (!Gold)
	{
		return false;
	}

	ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState);
	const bool bPrevSuppress = BwayPS ? BwayPS->IsSuppressingMatchGoldEarningsTracking() : false;
	if (BwayPS)
	{
		BwayPS->SetSuppressMatchGoldEarningsTracking(true);
	}

	const float DesiredMaxGold = FMath::Max(Gold->GetMaxGold(), FMath::Max(static_cast<float>(StartingGold), 10000.0f));
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetMaxGoldAttribute(), DesiredMaxGold);
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), static_cast<float>(StartingGold));

	if (BwayPS)
	{
		BwayPS->SetSuppressMatchGoldEarningsTracking(bPrevSuppress);
	}
	PlayerState->ForceNetUpdate();
	return true;
}
