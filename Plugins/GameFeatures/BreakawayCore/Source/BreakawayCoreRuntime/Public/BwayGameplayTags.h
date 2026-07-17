#pragma once

#include "NativeGameplayTags.h"

namespace BwayGameplayTags
{
	BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Buildable_PlacementSession);
	BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Buildable_PlacementExempt);
	BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_BuildablePlacement);

	/** Silenced — combat abilities (UBwayGameplayAbility_Base) cannot activate. */
	BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Silenced);

	/** Ethereal / Flock — damage immunity companion + pawn pass-through state. */
	BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Ethereal);

	/** Armor shred stacks from Korryn primary (and similar). */
	BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_ArmorShred);

	/** Movement slow from Burden / Circle / Cursed Ward. */
	BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Slowed);

	/** Incoming damage amplification (Circle of Spite). */
	BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_DamageAmplified);
}
