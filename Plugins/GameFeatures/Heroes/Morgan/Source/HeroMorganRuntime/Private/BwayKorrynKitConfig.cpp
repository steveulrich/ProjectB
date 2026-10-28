#include "BwayKorrynKitConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayKorrynKitConfig)

#if WITH_EDITOR
EDataValidationResult UBwayKorrynKitConfig::IsDataValid(FDataValidationContext& Context) const
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
	FailIfNonPositive(TEXT("PrimaryCooldown"), PrimaryCooldown);
	FailIfNonPositive(TEXT("ArmorShredDuration"), ArmorShredDuration);
	FailIfNonPositive(TEXT("FlockDuration"), FlockDuration);
	FailIfNonPositive(TEXT("FlockCooldown"), FlockCooldown);
	FailIfNonPositive(TEXT("BurdenCooldown"), BurdenCooldown);
	FailIfNonPositive(TEXT("BurdenSlowDuration"), BurdenSlowDuration);
	FailIfNonPositive(TEXT("CircleCooldown"), CircleCooldown);
	FailIfNonPositive(TEXT("CircleDuration"), CircleDuration);
	FailIfNonPositive(TEXT("AuraCooldown"), AuraCooldown);
	FailIfNonPositive(TEXT("AuraSilenceDuration"), AuraSilenceDuration);

	return Result;
}
#endif
