#include "Abilities/BwayGameplayAbility_AlonaBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_AlonaBase)

DEFINE_LOG_CATEGORY_STATIC(LogBwayAlonaKit, Log, All);

UBwayGameplayAbility_AlonaBase::UBwayGameplayAbility_AlonaBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KitConfig = TSoftObjectPtr<UBwayAlonaKitConfig>(
		FSoftObjectPath(TEXT("/Hero_Alona/Kit/DA_BW_AlonaKitConfig.DA_BW_AlonaKitConfig")));
}

const UBwayAlonaKitConfig* UBwayGameplayAbility_AlonaBase::ResolveKitConfig() const
{
	if (KitConfig.IsNull())
	{
		return nullptr;
	}

	const UBwayAlonaKitConfig* Config = KitConfig.LoadSynchronous();
	if (Config)
	{
		UE_LOG(LogBwayAlonaKit, Log, TEXT("[%s] Resolved kit config: %s"),
			*GetClass()->GetName(), *Config->GetPathName());
	}
	return Config;
}

void UBwayGameplayAbility_AlonaBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	float DurationSeconds = -1.f;
	if (const UBwayAlonaKitConfig* Config = ResolveKitConfig())
	{
		DurationSeconds = GetKitCooldownSeconds(*Config);
	}

	ApplyCooldownWithOptionalDuration(Handle, ActorInfo, ActivationInfo, DurationSeconds);
}
