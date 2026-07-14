#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "BwayGameplayEffect_AlonaCooldowns.generated.h"

/**
 * Native cooldown GEs for Alona functional kit (Step 19a).
 * TargetTags component is added in PostInitProperties — FindOrAddComponent must NOT
 * run inside the UGameplayEffect constructor.
 */
UCLASS(Abstract)
class HEROALONARUNTIME_API UGE_Bway_Cooldown_AlonaBase : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Bway_Cooldown_AlonaBase();

	virtual void PostInitProperties() override;

protected:
	void ConfigureDuration(float DurationSeconds);
	void ConfigureGrantedCooldownTag(const FGameplayTag& CooldownTag);

	UPROPERTY()
	FGameplayTag PendingCooldownTag;
};

UCLASS()
class HEROALONARUNTIME_API UGE_Bway_Cooldown_AlonaSunsGrace : public UGE_Bway_Cooldown_AlonaBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_AlonaSunsGrace();
};

UCLASS()
class HEROALONARUNTIME_API UGE_Bway_Cooldown_AlonaRadiance : public UGE_Bway_Cooldown_AlonaBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_AlonaRadiance();
};

UCLASS()
class HEROALONARUNTIME_API UGE_Bway_Cooldown_AlonaSunBurst : public UGE_Bway_Cooldown_AlonaBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_AlonaSunBurst();
};

UCLASS()
class HEROALONARUNTIME_API UGE_Bway_Cooldown_AlonaBlessing : public UGE_Bway_Cooldown_AlonaBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_AlonaBlessing();
};
