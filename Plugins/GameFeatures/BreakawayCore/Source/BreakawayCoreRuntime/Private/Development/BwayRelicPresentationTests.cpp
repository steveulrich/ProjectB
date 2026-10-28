#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwayRelicPresentationTestTypes.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "LyraGameplayTags.h"
#include "Sound/SoundWave.h"

namespace BwayRelicPresentationTest
{
struct FFixture
{
    explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
    ~FFixture()
    {
        if (World)
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    }
    FAutomationTestBase& Test;
    UWorld* World = nullptr;
    URelicSettings* Settings = nullptr;
    ABwayRelicPresentationTestActor* Relic = nullptr;
    ABwayCharacterWithAbilities* Pawns[2] = {};
    ABwayPlayerState* Players[2] = {};
    ULyraAbilitySystemComponent* ASCs[2] = {};
    USoundBase* PickupSound = nullptr;
    USoundBase* DropSound = nullptr;
    USoundBase* ResetSound = nullptr;
    UNiagaraSystem* PickupEffect = nullptr;
    UNiagaraSystem* DropEffect = nullptr;

    bool Setup()
    {
        const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!Test.TestNotNull(TEXT("Relic presentation world"), World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        World->SetGameState(World->SpawnActor<ABwayGameState>());
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            Players[Index] = World->SpawnActor<ABwayPlayerState>();
            Pawns[Index] = World->SpawnActor<ABwayCharacterWithAbilities>(FVector(Index * 5000.f, 0.f, 500.f), FRotator::ZeroRotator, Spawn);
            if (!Players[Index] || !Pawns[Index]) return false;
            Players[Index]->SetGenericTeamId(FGenericTeamId(Index + 1));
            Pawns[Index]->SetPlayerState(Players[Index]);
            Pawns[Index]->SetGenericTeamId(FGenericTeamId(Index + 1));
            ASCs[Index] = Players[Index]->GetLyraAbilitySystemComponent();
            ASCs[Index]->InitAbilityActorInfo(Players[Index], Pawns[Index]);
        }
        Settings = NewObject<URelicSettings>(World);
        Settings->RelicMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        Settings->RelicSocket = TEXT("");
        Settings->PickupCooldownAfterDrop = 0.f;
        PickupSound = NewObject<USoundWave>(World);
        DropSound = NewObject<USoundWave>(World);
        ResetSound = NewObject<USoundWave>(World);
        PickupEffect = NewObject<UNiagaraSystem>(World);
        DropEffect = NewObject<UNiagaraSystem>(World);
        Settings->PickupSound = PickupSound;
        Settings->DropSound = DropSound;
        Settings->ResetSound = ResetSound;
        Settings->PickupBurstEffect = PickupEffect;
        Settings->DropBurstEffect = DropEffect;
        Relic = World->SpawnActor<ABwayRelicPresentationTestActor>(FVector(0.f, 0.f, 3000.f), FRotator::ZeroRotator, Spawn);
        if (!Test.TestNotNull(TEXT("Relic presentation actor"), Relic)) return false;
        Relic->InitializeRelicData(Settings);
        Relic->PrimeCarrierSet();
        return true;
    }

    void PrepareCatch(int32 Index)
    {
        ASCs[Index]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1, EGameplayTagReplicationState::None);
    }
    void Expect(ERelicFeedbackEvent Event, USoundBase* Sound, UNiagaraSystem* Effect, int32 Count)
    {
        Test.TestEqual(TEXT("Accepted interaction kind"), Relic->LastEvent, Event);
        Test.TestEqual(TEXT("Resolved interaction sound"), Relic->LastSound, Sound);
        Test.TestEqual(TEXT("Resolved finite interaction burst"), Relic->LastEffect, Effect);
        Test.TestEqual(TEXT("One sound per accepted interaction"), Relic->AudioCalls, Count);
        Test.TestEqual(TEXT("One burst route per accepted interaction"), Relic->InteractionCalls, Count);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicPresentationFallbackTest,
    "Breakaway.CombatPolish.Relic.AcceptedInteractions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayRelicPresentationFallbackTest::RunTest(const FString&)
{
    using namespace BwayRelicPresentationTest;
    FFixture F(*this);
    if (!F.Setup()) return false;
    TestEqual(TEXT("Settings initialization is silent"), F.Relic->AudioCalls, 0);
    F.Relic->OnPickedUp(F.Pawns[0]);
    F.Expect(ERelicFeedbackEvent::Pickup, F.PickupSound, F.PickupEffect, 1);
    F.Relic->OnPickedUp(F.Pawns[0]);
    F.Relic->OnPickedUp(F.Pawns[1]);
    F.Relic->OnRep_CurrentState();
    F.Relic->OnRep_CurrentCarrier();
    TestEqual(TEXT("Repeated/rejected pickup and RepNotifies stay silent"), F.Relic->AudioCalls, 1);
    F.Relic->Server_PassRelic_Implementation(FVector(1000.f, 0.f, 200.f));
    F.Expect(ERelicFeedbackEvent::PassLaunch, F.DropSound, F.DropEffect, 2);
    TestEqual(TEXT("Pass retains production flight state"), F.Relic->CurrentState, ERelicState::BeingPassed);
    F.Relic->Server_PassRelic_Implementation(FVector(1000.f, 0.f, 200.f));
    TestEqual(TEXT("Rejected repeated pass stays silent"), F.Relic->AudioCalls, 2);
    F.PrepareCatch(1);
    F.Relic->OnPickedUp(F.Pawns[1]);
    F.Expect(ERelicFeedbackEvent::PreparedCatch, F.PickupSound, F.PickupEffect, 3);
    TestFalse(TEXT("Reception still clears held catch state"), F.ASCs[1]->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
    TestFalse(TEXT("Reception still clears flight metadata"), F.Relic->bIsPassInFlight);
    F.Relic->ForceFumbleFromDamageWithInstigator(F.Pawns[0]);
    F.Expect(ERelicFeedbackEvent::Fumble, F.DropSound, F.DropEffect, 4);
    TestNull(TEXT("Fumble still releases the carrier"), F.Relic->CurrentCarrier.Get());
    F.Relic->ForceFumbleFromDamageWithInstigator(F.Pawns[0]);
    F.Relic->OnDropped();
    TestEqual(TEXT("Repeated fumble/drop cannot repeat feedback"), F.Relic->AudioCalls, 4);
    F.Relic->OnPickedUp(F.Pawns[0]);
    F.Relic->Server_ThrowRelic_Implementation(FVector(1000.f, 0.f, 200.f));
    F.Expect(ERelicFeedbackEvent::ThrowLaunch, F.DropSound, F.DropEffect, 6);
    F.Players[1]->bHasCalledForRelic = true;
    F.Relic->OnPickedUp(F.Pawns[1]);
    F.Expect(ERelicFeedbackEvent::Pickup, F.PickupSound, F.PickupEffect, 7);
    TestFalse(TEXT("Timed request indicator does not manufacture prepared-catch feedback"), F.Relic->LastEvent == ERelicFeedbackEvent::PreparedCatch);
    F.Relic->OnDropped();
    F.Expect(ERelicFeedbackEvent::Drop, F.DropSound, F.DropEffect, 8);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicPresentationOverridesTest,
    "Breakaway.CombatPolish.Relic.AuthoredOverridesAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayRelicPresentationOverridesTest::RunTest(const FString&)
{
    using namespace BwayRelicPresentationTest;
    FFixture F(*this);
    if (!F.Setup()) return false;
    USoundBase* Sounds[4];
    UNiagaraSystem* Effects[4];
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Sounds[Index] = NewObject<USoundWave>(F.World);
        Effects[Index] = NewObject<UNiagaraSystem>(F.World);
    }
    F.Settings->PassLaunchSound = Sounds[0]; F.Settings->PassLaunchEffect = Effects[0];
    F.Settings->PreparedCatchSound = Sounds[1]; F.Settings->PreparedCatchEffect = Effects[1];
    F.Settings->FumbleSound = Sounds[2]; F.Settings->FumbleEffect = Effects[2];
    F.Settings->ThrowLaunchSound = Sounds[3]; F.Settings->ThrowLaunchEffect = Effects[3];
    F.Relic->OnPickedUp(F.Pawns[0]);
    F.Relic->Server_PassRelic_Implementation(FVector(1000.f, 0.f, 200.f));
    F.Expect(ERelicFeedbackEvent::PassLaunch, Sounds[0], Effects[0], 2);
    F.PrepareCatch(1);
    F.Relic->OnPickedUp(F.Pawns[1]);
    F.Expect(ERelicFeedbackEvent::PreparedCatch, Sounds[1], Effects[1], 3);
    F.Relic->DropRelic(FVector(500.f, 0.f, 200.f), F.Pawns[0], true);
    F.Expect(ERelicFeedbackEvent::Fumble, Sounds[2], Effects[2], 4);
    F.Relic->OnPickedUp(F.Pawns[0]);
    F.Relic->Server_ThrowRelic_Implementation(FVector(1000.f, 0.f, 200.f));
    F.Expect(ERelicFeedbackEvent::ThrowLaunch, Sounds[3], Effects[3], 6);
    F.Relic->BeginResettingState();
    F.Relic->ClearScoringFlag();
    F.Relic->OnDropped();
    TestEqual(TEXT("Reset preserves the existing reset audio route"), F.Relic->AudioCalls, 7);
    TestEqual(TEXT("Reset sound stays reset-specific"), F.Relic->LastSound, F.ResetSound);
    TestEqual(TEXT("Reset audio marker keeps Resetting state"), F.Relic->LastAudioState, ERelicState::Resetting);
    TestEqual(TEXT("Reset does not emit a drop/fumble/launch burst"), F.Relic->InteractionCalls, 6);
    F.Relic->OnRep_CurrentState();
    F.Relic->OnRep_CurrentCarrier();
    TestEqual(TEXT("Reset RepNotifies do not replay audio"), F.Relic->AudioCalls, 7);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicPresentationReceiptTest,
    "Breakaway.CombatPolish.Relic.EventReceipt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayRelicPresentationReceiptTest::RunTest(const FString&)
{
    using namespace BwayRelicPresentationTest;
    FFixture F(*this);
    if (!F.Setup()) return false;
    F.Relic->UseSimulatedRole();
    F.Relic->CurrentState = ERelicState::BeingPassed;
    F.Relic->OnRep_CurrentState();
    F.Relic->InitializeRelicData(F.Settings);
    F.Relic->OnPickedUp(F.Pawns[0]);
    TestEqual(TEXT("Late state/settings receipt and client pickup emit no historical one-shot"), F.Relic->AudioCalls, 0);
    F.Relic->Receive(ERelicFeedbackEvent::PassLaunch, F.DropSound, F.DropEffect, 5);
    F.Relic->Receive(ERelicFeedbackEvent::PassLaunch, F.DropSound, F.DropEffect, 5);
    F.Relic->Receive(ERelicFeedbackEvent::Pickup, F.PickupSound, F.PickupEffect, 4);
    F.Expect(ERelicFeedbackEvent::PassLaunch, F.DropSound, F.DropEffect, 1);
    F.Relic->Receive(ERelicFeedbackEvent::PreparedCatch, F.PickupSound, F.PickupEffect, 6);
    F.Expect(ERelicFeedbackEvent::PreparedCatch, F.PickupSound, F.PickupEffect, 2);
    F.Relic->Receive(ERelicFeedbackEvent::PassLaunch, nullptr, F.DropEffect, 7);
    TestEqual(TEXT("VFX-only event does not manufacture sound"), F.Relic->AudioCalls, 2);
    TestEqual(TEXT("Later event of same kind is allowed"), F.Relic->InteractionCalls, 3);
    return true;
}
#endif
