#include "BwayGameplayTags.h"

namespace BwayGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Buildable_PlacementSession, "Ability.Buildable.PlacementSession",
		"Active buildable placement preview session. Other abilities cancel via CancelAbilitiesWithTag on their CDO.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Buildable_PlacementExempt, "Ability.Buildable.PlacementExempt",
		"Confirm/Cancel/start abilities that must not cancel an in-progress placement session on activate.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_BuildablePlacement, "State.BuildablePlacement",
		"ASC owns this while placement preview is active. Primary/RelicRequest use ActivationBlockedTags.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Status_Silenced, "State.Status.Silenced",
		"Target cannot activate Breakaway combat abilities (UBwayGameplayAbility_Base). Slide / RelicRequest remain allowed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Status_Ethereal, "State.Status.Ethereal",
		"Ethereal state (Flock): ignore pawn collision and float; pairs with Gameplay.DamageImmunity.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Status_ArmorShred, "State.Status.ArmorShred",
		"Armor reduced by stacking shred effects (Korryn primary).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Status_Slowed, "State.Status.Slowed",
		"MoveSpeedMultiplier reduced by a slow effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Status_DamageAmplified, "State.Status.DamageAmplified",
		"IncomingDamageMultiplier increased (Circle of Spite).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Status_Jailed, "State.Status.Jailed",
		"Encaged by Rawlins Jail: Gameplay.MovementStopped while abilities remain usable.");
}
