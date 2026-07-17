#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayKorrynKitConfig.h"
#include "BwayGameplayAbility_KorrynBase.generated.h"

/**
 * Shared Korryn ability helpers — optional kit config soft ref overlays sheet defaults.
 */
UCLASS(Abstract)
class HEROMORGANRUNTIME_API UBwayGameplayAbility_KorrynBase : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_KorrynBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	const UBwayKorrynKitConfig* ResolveKitConfig() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Config")
	TSoftObjectPtr<UBwayKorrynKitConfig> KitConfig;
};
