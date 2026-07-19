#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "BwayGameplayEffect_RawlinsCooldowns.generated.h"

/**
 * Native cooldown GEs for Rawlins functional kit (Step 21a).
 * TargetTags component is added in PostInitProperties — FindOrAddComponent must NOT
 * run inside the UGameplayEffect constructor.
 */
UCLASS(Abstract)
class HERORAWLINSRUNTIME_API UGE_Bway_Cooldown_RawlinsBase : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Bway_Cooldown_RawlinsBase();

	virtual void PostInitProperties() override;

protected:
	void ConfigureDuration(float DurationSeconds);
	void ConfigureGrantedCooldownTag(const FGameplayTag& CooldownTag);

	UPROPERTY()
	FGameplayTag PendingCooldownTag;
};

UCLASS()
class HERORAWLINSRUNTIME_API UGE_Bway_Cooldown_RawlinsDoubleDown : public UGE_Bway_Cooldown_RawlinsBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_RawlinsDoubleDown();
};

UCLASS()
class HERORAWLINSRUNTIME_API UGE_Bway_Cooldown_RawlinsPowerShot : public UGE_Bway_Cooldown_RawlinsBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_RawlinsPowerShot();
};

UCLASS()
class HERORAWLINSRUNTIME_API UGE_Bway_Cooldown_RawlinsSlideShot : public UGE_Bway_Cooldown_RawlinsBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_RawlinsSlideShot();
};

UCLASS()
class HERORAWLINSRUNTIME_API UGE_Bway_Cooldown_RawlinsBlazingBarrage : public UGE_Bway_Cooldown_RawlinsBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_RawlinsBlazingBarrage();
};
