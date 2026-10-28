#include "AbilitySystem/BwayGameplayCueTags.h"

namespace BwayGameplayCueTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CombatHit, "GameplayCue.Breakaway.Combat.Hit", "Project hit confirmation feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CombatHitConfirm, "GameplayCue.Breakaway.Combat.HitConfirm", "Authored production hit-confirmation feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MovementSlide, "GameplayCue.Breakaway.Movement.Slide", "Project slide feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ScoringGoal, "GameplayCue.Breakaway.Scoring.Goal", "Project goal celebration feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ArgusPrimaryAttack, "GameplayCue.Breakaway.Weapon.ArgusPrimary.Attack", "Argus primary melee swing feedback, including missed attacks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AlonaPrimaryFire, "GameplayCue.Breakaway.Weapon.AlonaPrimary.Fire", "Alona primary attack launch feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(KorrynPrimaryFire, "GameplayCue.Breakaway.Weapon.KorrynPrimary.Fire", "Korryn primary attack launch feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(RawlinsPrimaryFire, "GameplayCue.Breakaway.Weapon.RawlinsPrimary.Fire", "Rawlins primary per-shot firing feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(RawlinsBarrageFire, "GameplayCue.Breakaway.Weapon.RawlinsBarrage.Fire", "Rawlins Blazing Barrage per-shot firing feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityCastMelee, "GameplayCue.Breakaway.Ability.Cast.Melee", "Committed melee ability cast, including misses.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityCastHeal, "GameplayCue.Breakaway.Ability.Cast.Heal", "Committed healing ability cast, independent of effective healing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityCastStatus, "GameplayCue.Breakaway.Ability.Cast.Status", "Committed status ability cast; not a damage confirmation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityCastRanged, "GameplayCue.Breakaway.Ability.Cast.Ranged", "Committed ranged or area burst ability cast.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityCastMovement, "GameplayCue.Breakaway.Ability.Cast.Movement", "Committed movement or escape ability cast.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityActiveDefense, "GameplayCue.Breakaway.Ability.Active.Defense", "Defensive presentation owned by the active ability lifetime.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityActiveHealing, "GameplayCue.Breakaway.Ability.Active.Healing", "Healing channel presentation owned by the active ability lifetime.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityActiveEthereal, "GameplayCue.Breakaway.Ability.Active.Ethereal", "Ethereal presentation owned by the active ability lifetime.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityEnd, "GameplayCue.Breakaway.Ability.End", "End or cancellation of a previously committed ability presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BuildableStart, "GameplayCue.Breakaway.Buildable.Start", "One-shot activation of a completed buildable.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BuildableActive, "GameplayCue.Breakaway.Buildable.Active", "Current buildable active state; late observers receive only this loop.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BuildableEnd, "GameplayCue.Breakaway.Buildable.End", "Actual buildable destruction or trap consumption, excluding reset and travel.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ZoneHealing, "GameplayCue.Breakaway.Zone.Healing", "Healing ground zone presentation owned by the zone actor lifetime.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ZoneHarmful, "GameplayCue.Breakaway.Zone.Harmful", "Harmful ground zone presentation owned by the zone actor lifetime.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatusSlowed, "GameplayCue.Breakaway.Status.Slowed", "Slow presentation owned by active Gameplay Effects on the affected target.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatusSilenced, "GameplayCue.Breakaway.Status.Silenced", "Silence presentation owned by active Gameplay Effects on the affected target.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatusRooted, "GameplayCue.Breakaway.Status.Rooted", "Movement restriction presentation owned by the active jail Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatusArmorShred, "GameplayCue.Breakaway.Status.ArmorShred", "Armor reduction presentation owned by active Gameplay Effects on the affected target.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatusDamageAmplified, "GameplayCue.Breakaway.Status.DamageAmplified", "Incoming damage amplification presentation owned by active Gameplay Effects on the affected target.");
}
