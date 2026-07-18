#include "BwayAlonaKitConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAlonaKitConfig)

#if WITH_EDITOR
EDataValidationResult UBwayAlonaKitConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto FailIfNonPositive = [&](const TCHAR* Name, float Value)
	{
		if (Value <= 0.f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s must be > 0."), Name)));
			Result = EDataValidationResult::Invalid;
		}
	};

	FailIfNonPositive(TEXT("PrimaryProjectileSpeed"), PrimaryProjectileSpeed);
	FailIfNonPositive(TEXT("PrimaryProjectileLifeSpan"), PrimaryProjectileLifeSpan);
	FailIfNonPositive(TEXT("SunsGraceInvulnerabilityDuration"), SunsGraceInvulnerabilityDuration);
	FailIfNonPositive(TEXT("SunsGraceCooldown"), SunsGraceCooldown);
	FailIfNonPositive(TEXT("RadianceActiveDuration"), RadianceActiveDuration);
	FailIfNonPositive(TEXT("RadianceHealTickInterval"), RadianceHealTickInterval);
	FailIfNonPositive(TEXT("RadianceStrengthDivisor"), RadianceStrengthDivisor);
	FailIfNonPositive(TEXT("RadianceCooldown"), RadianceCooldown);
	FailIfNonPositive(TEXT("SunBurstCooldown"), SunBurstCooldown);
	FailIfNonPositive(TEXT("BlessingZoneRadius"), BlessingZoneRadius);
	FailIfNonPositive(TEXT("BlessingZoneDuration"), BlessingZoneDuration);
	FailIfNonPositive(TEXT("BlessingCooldown"), BlessingCooldown);

	return Result;
}
#endif
