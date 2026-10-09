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
}
