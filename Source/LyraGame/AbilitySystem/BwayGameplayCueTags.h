#pragma once

#include "NativeGameplayTags.h"

// Project-wide cue assets are discovered before game-feature activation. Their
// dictionary entries must therefore belong to the primary game module.
namespace BwayGameplayCueTags
{
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CombatHit);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CombatHitConfirm);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MovementSlide);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ScoringGoal);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AlonaPrimaryFire);
}
