#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "GameplayCueNotify_Static.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayGameplayCueRegistrationTest, "Breakaway.Presentation.GameplayCueRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayGameplayCueRegistrationTest::RunTest(const FString&)
{
    const FGameplayTag Tags[] = {BwayGameplayCueTags::CombatHit, BwayGameplayCueTags::CombatHitConfirm,
        BwayGameplayCueTags::MovementSlide,
        BwayGameplayCueTags::ScoringGoal, BwayGameplayCueTags::AlonaPrimaryFire,
        FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Weapon.Pistol.Fire"), false)};
    const TCHAR* Paths[] = {
        TEXT("/Game/GameplayCueNotifies/GCN_BW_HitConfirmation.GCN_BW_HitConfirmation_C"),
        TEXT("/BreakawayCore/GameplayCues/GCN_BW_CombatFeedback_HitConfirm.GCN_BW_CombatFeedback_HitConfirm_C"),
        TEXT("/Game/GameplayCueNotifies/GCN_BW_SlideTrail.GCN_BW_SlideTrail_C"),
        TEXT("/Game/GameplayCueNotifies/GCN_BW_GoalScored.GCN_BW_GoalScored_C"),
        TEXT("/BreakawayCore/GameplayCues/GCN_BW_Weapon_AlonaPrimary_Fire.GCN_BW_Weapon_AlonaPrimary_Fire_C"),
        TEXT("/ShooterCore/Weapons/Pistol/GCN_Weapon_Pistol_Fire.GCN_Weapon_Pistol_Fire_C")};
    // Check dictionary entries before loading the assets or their game-feature classes.
    for (const FGameplayTag Tag : Tags) TestTrue(TEXT("Cue tag is available before asset load"), Tag.IsValid());
    TSet<FGameplayTag> Seen;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Paths); ++Index)
    {
        UClass* Class = LoadClass<UGameplayCueNotify_Static>(nullptr, Paths[Index]);
        if (!TestNotNull(Paths[Index], Class)) continue;
        const auto* Cue = Class->GetDefaultObject<UGameplayCueNotify_Static>();
        TestEqual(Paths[Index], Cue->GameplayCueTag, Tags[Index]);
        TestFalse(TEXT("Inspected cues have unique tags"), Seen.Contains(Cue->GameplayCueTag));
        Seen.Add(Cue->GameplayCueTag);
    }
    return true;
}
#endif
