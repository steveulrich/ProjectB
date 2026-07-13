#include "BwayGameplayTags.h"

namespace BwayGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Buildable_PlacementSession, "Ability.Buildable.PlacementSession",
		"Active buildable placement preview session. Other abilities cancel via CancelAbilitiesWithTag on their CDO.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Buildable_PlacementExempt, "Ability.Buildable.PlacementExempt",
		"Confirm/Cancel/start abilities that must not cancel an in-progress placement session on activate.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_BuildablePlacement, "State.BuildablePlacement",
		"ASC owns this while placement preview is active. Primary/RelicRequest use ActivationBlockedTags.");
}
