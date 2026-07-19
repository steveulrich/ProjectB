#include "Abilities/BwayGameplayAbility_RawlinsBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RawlinsBase)

DEFINE_LOG_CATEGORY_STATIC(LogBwayRawlinsKit, Log, All);

UBwayGameplayAbility_RawlinsBase::UBwayGameplayAbility_RawlinsBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KitConfig = TSoftObjectPtr<UBwayRawlinsKitConfig>(
		FSoftObjectPath(TEXT("/Hero_Rawlins/Kit/DA_BW_RawlinsKitConfig.DA_BW_RawlinsKitConfig")));
}

const UBwayRawlinsKitConfig* UBwayGameplayAbility_RawlinsBase::ResolveKitConfig() const
{
	if (KitConfig.IsNull())
	{
		return nullptr;
	}

	const UBwayRawlinsKitConfig* Config = KitConfig.LoadSynchronous();
	if (Config)
	{
		UE_LOG(LogBwayRawlinsKit, Log, TEXT("[%s] Resolved kit config: %s"),
			*GetClass()->GetName(), *Config->GetPathName());
	}
	return Config;
}

void UBwayGameplayAbility_RawlinsBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	float DurationSeconds = -1.f;
	if (const UBwayRawlinsKitConfig* Config = ResolveKitConfig())
	{
		DurationSeconds = GetKitCooldownSeconds(*Config);
	}

	ApplyCooldownWithOptionalDuration(Handle, ActorInfo, ActivationInfo, DurationSeconds);
}
