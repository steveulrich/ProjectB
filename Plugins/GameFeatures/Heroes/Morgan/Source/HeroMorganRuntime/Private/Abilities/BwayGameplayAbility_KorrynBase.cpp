#include "Abilities/BwayGameplayAbility_KorrynBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_KorrynBase)

DEFINE_LOG_CATEGORY_STATIC(LogBwayKorrynKit, Log, All);

UBwayGameplayAbility_KorrynBase::UBwayGameplayAbility_KorrynBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KitConfig = TSoftObjectPtr<UBwayKorrynKitConfig>(
		FSoftObjectPath(TEXT("/Hero_Morgan/Kit/DA_BW_KorrynKitConfig.DA_BW_KorrynKitConfig")));
}

const UBwayKorrynKitConfig* UBwayGameplayAbility_KorrynBase::ResolveKitConfig() const
{
	if (KitConfig.IsNull())
	{
		return nullptr;
	}

	const UBwayKorrynKitConfig* Config = KitConfig.LoadSynchronous();
	if (Config)
	{
		UE_LOG(LogBwayKorrynKit, Log, TEXT("[%s] Resolved kit config: %s"),
			*GetClass()->GetName(), *Config->GetPathName());
	}
	return Config;
}

void UBwayGameplayAbility_KorrynBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	float DurationSeconds = -1.f;
	if (const UBwayKorrynKitConfig* Config = ResolveKitConfig())
	{
		DurationSeconds = GetKitCooldownSeconds(*Config);
	}

	ApplyCooldownWithOptionalDuration(Handle, ActorInfo, ActivationInfo, DurationSeconds);
}
