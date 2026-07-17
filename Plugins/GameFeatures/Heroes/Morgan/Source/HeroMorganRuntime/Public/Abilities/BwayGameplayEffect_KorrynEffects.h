#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "BwayGameplayEffect_KorrynEffects.generated.h"

/**
 * Native status / cooldown GEs for Korryn (Step 20a).
 * TargetTags component is added in PostInitProperties — not in the constructor.
 */
UCLASS(Abstract)
class HEROMORGANRUNTIME_API UGE_Bway_KorrynEffectBase : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Bway_KorrynEffectBase();
	virtual void PostInitProperties() override;

protected:
	void ConfigureDuration(float DurationSeconds);
	void ConfigureGrantedTag(const FGameplayTag& Tag);
	void AddGrantedTag(const FGameplayTag& Tag);

	UPROPERTY()
	TArray<FGameplayTag> PendingGrantedTags;
};

UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_Cooldown_KorrynFlock : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_KorrynFlock();
};

UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_Cooldown_KorrynBurdenOfSin : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_KorrynBurdenOfSin();
};

UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_Cooldown_KorrynCircleOfSpite : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_KorrynCircleOfSpite();
};

UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_Cooldown_KorrynAuraOfSilence : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_Cooldown_KorrynAuraOfSilence();
};

/** Primary hit: -2 Armor, stacks to 5 (-10), duration refresh 2s. */
UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_KorrynArmorShred : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_KorrynArmorShred();
};

/** Burden of Sin: MoveSpeedMultiplier *= 0.5 for 2s. */
UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_KorrynBurdenSlow : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_KorrynBurdenSlow();
};

/** Circle of Spite: MoveSpeedMultiplier *= 0.85 while in zone (infinite until removed). */
UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_KorrynCircleSlow : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_KorrynCircleSlow();
};

/** Circle of Spite: IncomingDamageMultiplier = 1.35 while in zone. */
UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_KorrynCircleDamageAmp : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_KorrynCircleDamageAmp();
};

/** Aura of Silence: grants State.Status.Silenced for 5s. */
UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_KorrynSilence : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_KorrynSilence();
};

/** Flock ethereal + damage immunity for ability duration (applied as GE backup / tag source). */
UCLASS()
class HEROMORGANRUNTIME_API UGE_Bway_KorrynFlockEthereal : public UGE_Bway_KorrynEffectBase
{
	GENERATED_BODY()
public:
	UGE_Bway_KorrynFlockEthereal();
};
