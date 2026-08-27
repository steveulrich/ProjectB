// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayCombatReadabilityConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCombatReadabilityConfig)

float UBwayCombatReadabilityConfig::EvaluateNameplateScale(float Distance) const
{
	const float Start = NameplateScaleDistanceStart;
	const float End = FMath::Max(Start + KINDA_SMALL_NUMBER, NameplateScaleDistanceEnd);
	const float Alpha = FMath::Clamp((Distance - Start) / (End - Start), 0.f, 1.f);

	float Scale = 1.f;
	if (NameplateScaleCurve)
	{
		Scale = FMath::Max(0.01f, NameplateScaleCurve->GetFloatValue(Alpha));
	}
	else
	{
		// Near = MaxScale, far = MinScale
		Scale = FMath::Lerp(MaxNameplateScale, MinNameplateScale, Alpha);
	}

	return FMath::Max(0.01f, Scale * CombatFeedbackScaleMultiplier);
}

FLinearColor UBwayCombatReadabilityConfig::ResolveIncomingDamageColor() const
{
	return bUseColorblindPalette ? ColorblindIncomingDamageColor : IncomingDamageColor;
}

FLinearColor UBwayCombatReadabilityConfig::ResolveOutgoingDamageColor() const
{
	return bUseColorblindPalette ? ColorblindOutgoingDamageColor : OutgoingDamageColor;
}

FLinearColor UBwayCombatReadabilityConfig::ResolveIncomingHealColor() const
{
	return bUseColorblindPalette ? ColorblindIncomingHealColor : IncomingHealColor;
}

#if WITH_EDITOR
EDataValidationResult UBwayCombatReadabilityConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (MaxNameplateRange <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("MaxNameplateRange must be > 0.")));
		Result = EDataValidationResult::Invalid;
	}

	if (NameplateScaleDistanceEnd <= NameplateScaleDistanceStart)
	{
		Context.AddError(FText::FromString(TEXT("NameplateScaleDistanceEnd must be greater than NameplateScaleDistanceStart.")));
		Result = EDataValidationResult::Invalid;
	}

	if (MinNameplateScale <= 0.f || MaxNameplateScale <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("Nameplate scale values must be > 0.")));
		Result = EDataValidationResult::Invalid;
	}

	if (NumberPopLifespan <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("NumberPopLifespan must be > 0.")));
		Result = EDataValidationResult::Invalid;
	}

	if (NumberPopEndScale < 0.f || NumberPopEndScale > 1.f)
	{
		Context.AddError(FText::FromString(TEXT("NumberPopEndScale must be in [0, 1].")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
