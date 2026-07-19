#include "BwayRawlinsKitConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRawlinsKitConfig)

#if WITH_EDITOR
EDataValidationResult UBwayRawlinsKitConfig::IsDataValid(FDataValidationContext& Context) const
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
	FailIfNonPositive(TEXT("PrimaryShotInterval"), PrimaryShotInterval);
	FailIfNonPositive(TEXT("DoubleDownDashDuration"), DoubleDownDashDuration);
	FailIfNonPositive(TEXT("DoubleDownCooldown"), DoubleDownCooldown);
	FailIfNonPositive(TEXT("PowerShotCooldown"), PowerShotCooldown);
	FailIfNonPositive(TEXT("SlideShotSlideDuration"), SlideShotSlideDuration);
	FailIfNonPositive(TEXT("SlideShotCooldown"), SlideShotCooldown);
	FailIfNonPositive(TEXT("BarrageShotInterval"), BarrageShotInterval);
	FailIfNonPositive(TEXT("BarrageCooldown"), BarrageCooldown);
	FailIfNonPositive(TEXT("JailMaxHealth"), JailMaxHealth);
	FailIfNonPositive(TEXT("JailTriggerRadius"), JailTriggerRadius);
	FailIfNonPositive(TEXT("JailCaptureRadius"), JailCaptureRadius);
	FailIfNonPositive(TEXT("JailCageRadius"), JailCageRadius);

	if (PrimaryShotCount < 1)
	{
		Context.AddError(FText::FromString(TEXT("PrimaryShotCount must be >= 1.")));
		Result = EDataValidationResult::Invalid;
	}
	if (BarrageShotCount < 1)
	{
		Context.AddError(FText::FromString(TEXT("BarrageShotCount must be >= 1.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
