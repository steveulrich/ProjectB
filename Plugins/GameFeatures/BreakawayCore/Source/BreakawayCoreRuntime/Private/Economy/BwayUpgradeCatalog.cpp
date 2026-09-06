#include "Economy/BwayUpgradeCatalog.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayUpgradeCatalog)

const FName UBwayUpgradeEffect::MagnitudeName(TEXT("UpgradeMagnitude"));

UBwayUpgradeEffect::UBwayUpgradeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

bool UBwayUpgradeEffect::IsSupportedUpgradeEffect() const
{
	// Rollback must remove only reversible attribute modifiers, without executing
	// damage, granting abilities, changing tags, or removing another effect.
	const bool bSupportedLifetime = DurationPolicy == EGameplayEffectDurationType::Infinite
		&& Period.IsStatic() && Period.GetValue() == 0.0f
		&& Executions.IsEmpty() && GEComponents.IsEmpty()
		&& GetStackingType() == EGameplayEffectStackingType::None
		&& !Modifiers.IsEmpty();
	if (!bSupportedLifetime) return false;
	for (const FGameplayModifierInfo& Modifier : Modifiers)
	{
		// Explicitly exclude health and gold: changing/clamping those values is not reversible.
		const bool bSupportedAttribute = Modifier.Attribute == UBwayHeroAttributeSet::GetAttackStrengthAttribute()
			|| Modifier.Attribute == UBwayHeroAttributeSet::GetArmorAttribute();
		const FSetByCallerFloat& Value = Modifier.ModifierMagnitude.GetSetByCallerFloat();
		if (!bSupportedAttribute || Modifier.ModifierOp != EGameplayModOp::Additive
			|| Modifier.ModifierMagnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::SetByCaller
			|| Value.DataName != MagnitudeName || Value.DataTag.IsValid()) return false;
	}
	return true;
}

UBwayUpgradeEffect_AttackStrength::UBwayUpgradeEffect_AttackStrength()
{
	FSetByCallerFloat Value;
	Value.DataName = MagnitudeName;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UBwayHeroAttributeSet::GetAttackStrengthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Value);
}

const FBwayUpgradeDefinition* UBwayUpgradeCatalog::FindUpgrade(FName Id) const
{
	return Upgrades.FindByPredicate([Id](const FBwayUpgradeDefinition& Entry) { return Entry.Id == Id; });
}

bool UBwayUpgradeCatalog::ValidateCatalog(FString& Error) const
{
	Error.Reset();
	if (MaxOwnedUpgrades <= 0 || Upgrades.IsEmpty())
	{
		Error = TEXT("Catalog needs entries and a positive slot limit.");
		return false;
	}
	TSet<FName> Ids;
	for (const FBwayUpgradeDefinition& Entry : Upgrades)
	{
		if (Entry.Id.IsNone() || Ids.Contains(Entry.Id) || Entry.Ranks.IsEmpty()
			|| !Entry.EffectClass || !Entry.EffectClass.GetDefaultObject()->IsSupportedUpgradeEffect())
		{
			Error = FString::Printf(TEXT("Invalid or duplicate upgrade definition: %s"), *Entry.Id.ToString());
			return false;
		}
		Ids.Add(Entry.Id);
		for (const FBwayUpgradeRank& Rank : Entry.Ranks)
		{
			if (Rank.Cost < 0 || !FMath::IsFinite(Rank.Magnitude))
			{
				Error = FString::Printf(TEXT("Invalid cost or magnitude: %s"), *Entry.Id.ToString());
				return false;
			}
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UBwayUpgradeCatalog::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateCatalog(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
