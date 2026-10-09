#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Relic/RelicActor.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicOwnershipTest,
    "Breakaway.Relic.OwnershipLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicOwnershipTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Settings = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("Fixture world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->SetGameState(World->SpawnActor<ABwayGameState>());
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABwayPlayerState* Players[3] = {};
    ULyraAbilitySystemComponent* ASCs[3] = {};
    ABwayCharacterWithAbilities* Pawns[2] = {};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Players[Index] = World->SpawnActor<ABwayPlayerState>();
        if (!TestNotNull(TEXT("PlayerState"), Players[Index])) return false;
        if (!Players[Index]->IsActorInitialized()) static_cast<AActor*>(Players[Index])->PostInitializeComponents();
        ASCs[Index] = Players[Index]->GetLyraAbilitySystemComponent();
        if (Index < 2)
        {
            Pawns[Index] = World->SpawnActor<ABwayCharacterWithAbilities>(ABwayCharacterWithAbilities::StaticClass(),
                FTransform(FVector(Index * 5000.0f, 0.0f, 500.0f)), Spawn);
            if (!TestNotNull(TEXT("Carrier pawn"), Pawns[Index])) return false;
            Pawns[Index]->SetPlayerState(Players[Index]);
        }
        ASCs[Index]->InitAbilityActorInfo(Players[Index], Index < 2 ? static_cast<AActor*>(Pawns[Index]) : Players[Index]);
    }
    auto* Relic = World->SpawnActor<ARelicActor>(ARelicActor::StaticClass(), FTransform(FVector(0, 0, 3000)), Spawn);
    auto* CarrierSet = LoadObject<ULyraAbilitySet>(nullptr,
        TEXT("/BreakawayCore/GameModes/BallMode/Abilities/BallHandlingAbilities/AS_BW_BallAbilities.AS_BW_BallAbilities"));
    FObjectPropertyBase* LoadedSet = FindFProperty<FObjectPropertyBase>(ARelicActor::StaticClass(), TEXT("LoadedRelicAbilitySet"));
    if (!TestNotNull(TEXT("Relic"), Relic) || !TestNotNull(TEXT("Authored carrier set"), CarrierSet) ||
        !TestNotNull(TEXT("Loaded-set property"), LoadedSet)) return false;
    LoadedSet->SetObjectPropertyValue_InContainer(Relic, CarrierSet);
    auto Count = [Relic](const ULyraAbilitySystemComponent* ASC)
    {
        int32 Result = 0;
        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
            if (Spec.SourceObject.Get() == Relic) ++Result;
        return Result;
    };
    // Derive the expected grant from the loaded catalog, independently of pickup.
    FLyraAbilitySet_GrantedHandles ReferenceHandles;
    CarrierSet->GiveToAbilitySystem(ASCs[2], &ReferenceHandles, Relic);
    const int32 AuthoredAbilities = Count(ASCs[2]);
    TestTrue(TEXT("Authored set contains abilities"), AuthoredAbilities > 0);
    ReferenceHandles.TakeFromAbilitySystem(ASCs[2]);
    const int32 Expected = AuthoredAbilities + 1; // Existing native pass ability.

    Relic->OnPickedUp(Pawns[0]);
    TestEqual(TEXT("Initial carrier owns relic"), Relic->CurrentCarrier.Get(), Pawns[0]);
    TestEqual(TEXT("One authored set plus native pass"), Count(ASCs[0]), Expected);
    TestTrue(TEXT("Owner possession flag"), Players[0]->bHasRelic);
    const FGameplayTag CarrierTag = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.RelicCarrier"));
    TestEqual(TEXT("Exactly one owner carrier tag"), ASCs[0]->GetTagCount(CarrierTag), 1);
    Relic->OnRep_CurrentCarrier();
    TestEqual(TEXT("Authority RepNotify cannot grant again"), Count(ASCs[0]), Expected);
    Relic->OnPickedUp(Pawns[0]);
    TestEqual(TEXT("Repeated pickup cannot duplicate grants"), Count(ASCs[0]), Expected);
    Relic->OnPickedUp(Pawns[1]);
    TestEqual(TEXT("Competing pickup cannot replace carrier"), Relic->CurrentCarrier.Get(), Pawns[0]);
    TestEqual(TEXT("Rejected competitor gets no abilities"), Count(ASCs[1]), 0);
    TestFalse(TEXT("Rejected competitor gets no possession flag"), Players[1]->bHasRelic);
    TestEqual(TEXT("Rejected competitor gets no carrier tag"), ASCs[1]->GetTagCount(CarrierTag), 0);
    Relic->OnDropped();
    TestNull(TEXT("Drop clears carrier"), Relic->CurrentCarrier.Get());
    TestEqual(TEXT("Drop removes former owner abilities"), Count(ASCs[0]), 0);
    TestEqual(TEXT("Drop leaves competitor clean"), Count(ASCs[1]), 0);
    TestFalse(TEXT("Drop clears former owner possession"), Players[0]->bHasRelic);
    TestEqual(TEXT("Drop clears owner carrier tag"), ASCs[0]->GetTagCount(CarrierTag), 0);
    TestFalse(TEXT("Drop leaves competitor without possession"), Players[1]->bHasRelic);
    Relic->OnPickedUp(Pawns[1]);
    TestEqual(TEXT("A released relic accepts the next carrier"), Relic->CurrentCarrier.Get(), Pawns[1]);
    TestEqual(TEXT("Next carrier receives exactly one grant"), Count(ASCs[1]), Expected);
    Relic->OnDropped();
    TestEqual(TEXT("Second release removes all fixture grants"), Count(ASCs[1]), 0);
    for (bool Pass : {false, true})
    {
        Relic->OnPickedUp(Pawns[0]);
        // This transient world does not initialize actors for RPC dispatch.
        // Exercise authority cleanup bodies here; transport is a separate gate.
        if (Pass) Relic->Server_PassRelic_Implementation(FVector(100.0f, 0.0f, 0.0f));
        else Relic->Server_ThrowRelic_Implementation(FVector(100.0f, 0.0f, 0.0f));
        TestEqual(TEXT("Throw/pass enters its loose state"), Relic->CurrentState, Pass ? ERelicState::BeingPassed : ERelicState::Thrown);
        TestNull(TEXT("Throw/pass releases carrier"), Relic->CurrentCarrier.Get());
        TestEqual(TEXT("Throw/pass removes carrier abilities"), Count(ASCs[0]), 0);
        TestFalse(TEXT("Throw/pass clears possession"), Players[0]->bHasRelic);
        TestEqual(TEXT("Throw/pass clears carrier tag"), ASCs[0]->GetTagCount(CarrierTag), 0);
        Relic->OnPickedUp(Pawns[1]);
        TestEqual(TEXT("Loose throw/pass can be received"), Relic->CurrentCarrier.Get(), Pawns[1]);
        Relic->BeginResettingState();
        Relic->ClearScoringFlag();
        Relic->OnDropped();
        TestEqual(TEXT("Round reset removes receiver abilities"), Count(ASCs[1]), 0);
        TestFalse(TEXT("Round reset clears receiver possession"), Players[1]->bHasRelic);
        TestEqual(TEXT("Round reset clears receiver tag"), ASCs[1]->GetTagCount(CarrierTag), 0);
    }
    for (ERelicState State : {ERelicState::Scoring, ERelicState::Resetting})
    {
        Relic->CurrentState = State;
        Relic->OnPickedUp(Pawns[0]);
        TestNull(TEXT("Scoring/resetting rejects pickup"), Relic->CurrentCarrier.Get());
        TestEqual(TEXT("Scoring/resetting cannot grant abilities"), Count(ASCs[0]), 0);
    }
    Relic->CurrentState = ERelicState::Dropped;
    Relic->bHasScoredThisRound = true;
    Relic->OnPickedUp(Pawns[0]);
    TestNull(TEXT("Scored relic rejects stale pickup"), Relic->CurrentCarrier.Get());
    Relic->ClearScoringFlag();
    Relic->OnPickedUp(Pawns[0]);
    TestEqual(TEXT("New round permits pickup again"), Count(ASCs[0]), Expected);
    Relic->OnDropped();
    return true;
}
#endif
