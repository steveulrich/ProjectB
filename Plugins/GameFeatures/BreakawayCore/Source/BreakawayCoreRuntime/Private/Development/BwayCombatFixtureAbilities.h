#pragma once

#if !UE_BUILD_SHIPPING
#include "Abilities/GameplayAbility.h"

namespace BwayCombatFixture
{
// Use already loaded native families, so selection works for renamed Blueprint
// abilities and cannot accidentally choose a secondary with a matching label.
inline bool IsPrimaryAbility(const UGameplayAbility* Ability)
{
	if (!Ability) return false;
	const TCHAR* Families[] = {
		TEXT("/Script/BreakawayCoreRuntime.BwayGameplayAbility_MeleePrimary"),
		TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsPrimary"),
		TEXT("/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaPrimary"),
		TEXT("/Script/HeroMorganRuntime.BwayGameplayAbility_KorrynPrimary")};
	for (const TCHAR* Path : Families)
	{
		const UClass* Family = FindObject<UClass>(nullptr, Path);
		if (Family && Ability->IsA(Family)) return true;
	}
	return false;
}
}
#endif
