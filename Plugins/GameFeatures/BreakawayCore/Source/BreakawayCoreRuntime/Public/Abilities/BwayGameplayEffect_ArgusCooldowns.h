#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "BwayGameplayEffect_ArgusCooldowns.generated.h"

/**
 * Native cooldown GEs for Argus sheet parity (Step 18c).
 * Granted tags drive UGameplayAbility::CheckCooldown via GetCooldownTags().
 *
 * TargetTags component is added in PostInitProperties — FindOrAddComponent must NOT
 * run inside the UGameplayEffect constructor (fatal: empty-name NewObject as default subobject).
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UGE_Bway_Cooldown_ArgusBase : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Bway_Cooldown_ArgusBase();

	virtual void PostInitProperties() override;

protected:
	void ConfigureDuration(float DurationSeconds);
	void ConfigureGrantedCooldownTag(const FGameplayTag& CooldownTag);

	UPROPERTY()
	FGameplayTag PendingCooldownTag;
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UGE_Bway_Cooldown_ArgusSlide : public UGE_Bway_Cooldown_ArgusBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_ArgusSlide();
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UGE_Bway_Cooldown_NoRetreat : public UGE_Bway_Cooldown_ArgusBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_NoRetreat();
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UGE_Bway_Cooldown_ForGlory : public UGE_Bway_Cooldown_ArgusBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_ForGlory();
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UGE_Bway_Cooldown_Retribution : public UGE_Bway_Cooldown_ArgusBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_Retribution();
};
