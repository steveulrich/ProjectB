#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "BwayGameplayEffect_PassiveGold.generated.h"

/** Shared definition for server-driven, removable round income. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayEffect_PassiveGold : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBwayGameplayEffect_PassiveGold();
	static const FName IncomeMagnitudeName;
};
