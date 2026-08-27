#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "BwayGameplayEffect_DamageSetByCaller.generated.h"

/**
 * Instant SetByCaller → Damage meta attribute.
 *
 * Do NOT use Lyra's GE_Damage_Basic_SetByCaller here: that GE runs ULyraDamageExecution,
 * which adds Source ULyraCombatSet::BaseDamage on top of SetByCaller.Damage. Breakaway
 * already folds Attack Strength into CalculateScaledDamage / ApplyDamageFromSource, and
 * also writes Attack into CombatSet::BaseDamage for fallbacks — so Lyra's GE double-counts.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UGE_Bway_Damage_SetByCaller : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Bway_Damage_SetByCaller();

	virtual void PostInitProperties() override;
};
