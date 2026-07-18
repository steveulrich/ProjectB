#include "Abilities/BwayGameplayAbility_ArgusBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_ArgusBase)

DEFINE_LOG_CATEGORY_STATIC(LogBwayArgusKit, Log, All);

UBwayGameplayAbility_ArgusBase::UBwayGameplayAbility_ArgusBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KitConfig = TSoftObjectPtr<UBwayArgusKitConfig>(
		FSoftObjectPath(TEXT("/Hero_Spartacus/Kit/DA_BW_ArgusKitConfig.DA_BW_ArgusKitConfig")));
}

const UBwayArgusKitConfig* UBwayGameplayAbility_ArgusBase::ResolveKitConfig() const
{
	if (KitConfig.IsNull())
	{
		return nullptr;
	}

	const UBwayArgusKitConfig* Config = KitConfig.LoadSynchronous();
	if (Config)
	{
		UE_LOG(LogBwayArgusKit, Log, TEXT("[%s] Resolved kit config: %s"),
			*GetClass()->GetName(), *Config->GetPathName());
	}
	return Config;
}

void UBwayGameplayAbility_ArgusBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	float DurationSeconds = -1.f;
	if (const UBwayArgusKitConfig* Config = ResolveKitConfig())
	{
		DurationSeconds = GetKitCooldownSeconds(*Config);
	}

	ApplyCooldownWithOptionalDuration(Handle, ActorInfo, ActivationInfo, DurationSeconds);
}
