#include "BwayArgusKitConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayArgusKitConfig)

#if WITH_EDITOR
EDataValidationResult UBwayArgusKitConfig::IsDataValid(FDataValidationContext& Context) const
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

	FailIfNonPositive(TEXT("SlideDashDuration"), SlideDashDuration);
	FailIfNonPositive(TEXT("SlideCooldown"), SlideCooldown);
	FailIfNonPositive(TEXT("NoRetreatChargeDuration"), NoRetreatChargeDuration);
	FailIfNonPositive(TEXT("NoRetreatCooldown"), NoRetreatCooldown);
	FailIfNonPositive(TEXT("ForGloryCooldown"), ForGloryCooldown);
	FailIfNonPositive(TEXT("RetributionCooldown"), RetributionCooldown);

	return Result;
}
#endif
