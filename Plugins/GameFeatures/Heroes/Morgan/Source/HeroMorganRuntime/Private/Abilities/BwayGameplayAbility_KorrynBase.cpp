#include "Abilities/BwayGameplayAbility_KorrynBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_KorrynBase)

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
	return KitConfig.LoadSynchronous();
}
