#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace BwayWardAuraTest
{
class FRun : public IAutomationLatentCommand
{
public:
    FRun(FAutomationTestBase* InTest, int32 InOwnerMode) : Test(InTest), OwnerMode(InOwnerMode) {}
    ~FRun() override
    {
        if (World) World->DestroyWorld(false);
        if (Instance) Instance->Shutdown();
        if (World) GEngine->DestroyWorldContext(World);
    }
    ABwayCharacterWithAbilities* SpawnPawn(int32 Team, FVector Location,
        APlayerController* PC = nullptr, ABwayPlayerState* ExistingPlayer = nullptr)
    {
        auto* PS = ExistingPlayer ? ExistingPlayer : World->SpawnActor<ABwayPlayerState>();
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(ABwayCharacterWithAbilities::StaticClass(), Location, FRotator::ZeroRotator, Params);
        if (!PS || !Pawn) return nullptr;
        PS->SetGenericTeamId(FGenericTeamId(Team)); Pawn->SetGenericTeamId(FGenericTeamId(Team)); GS->AddPlayerToTeam(PS, Team - 1);
        if (PC) { PS->SetOwner(PC); PC->SetPlayerState(PS); PC->Possess(Pawn); }
        Pawn->SetPlayerState(PS);
        auto* ASC = PS->GetLyraAbilitySystemComponent();
        UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
        if (!ASC->GetSet<ULyraHealthSet>()) ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(PS));
        auto* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
        Extension->SetPawnData(NewObject<ULyraPawnData>(World)); Extension->InitializeAbilitySystem(ASC, PS);
        // Test-only baseline; preserve current modifiers on a reused PlayerState ASC.
        ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 1000);
        ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 1000);
        ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1);
        return Pawn;
    }
    ABuildableActor* SpawnWard(float X)
    {
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr,
            TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_BuildableData_Hexweaver.DA_BW_BuildableData_Hexweaver"));
        if (!Data || !Data->BuildableActorClass) return nullptr;
        FActorSpawnParameters Params; Params.Owner = Owner; Params.Instigator = Owner->GetPawn();
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Ward = World->SpawnActor<ABuildableActor>(Data->BuildableActorClass, FVector(X,0,10000), FRotator::ZeroRotator, Params);
        if (!Ward) return nullptr;
        Ward->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
        UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(Ward, Data, 0);
        Ward->InitializeBuildable(Owner, 0);
        return Ward;
    }
    void Advance(float Seconds = .35f) { World->GetTimerManager().Tick(Seconds); }
    void Move(ABwayCharacterWithAbilities* Pawn, float X, float Y = 0)
    { Pawn->SetActorLocation(FVector(X,Y,10000), false, nullptr, ETeleportType::TeleportPhysics); }
    void Observe(const TCHAR* Phase, ABwayPlayerState* Player, float Expected)
    {
        auto* ASC = Player->GetLyraAbilitySystemComponent();
        const float Speed = ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute());
        const bool Slowed = ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed);
        Test->TestTrue(FString::Printf(TEXT("%s speed %.2f"), Phase, Expected), FMath::IsNearlyEqual(Speed, Expected, .001f));
        Test->TestEqual(FString::Printf(TEXT("%s slow tag"), Phase), Slowed, Expected < 1.f);
        UE_LOG(LogTemp, Display, TEXT("BwayWardAura: Verify ownerMode=%d phase=%s speed=%.3f expected=%.3f slowed=%d effects=%d avatar=%s"),
            OwnerMode, Phase, Speed, Expected, Slowed, ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), *GetNameSafe(ASC->GetAvatarActor()));
    }
    bool Update() override
    {
        if (Step == 0)
        {
            Instance.Reset(NewObject<UGameInstance>(GEngine)); Instance->InitializeStandalone(); World = Instance->GetWorld();
            if (!Test->TestNotNull(TEXT("Initialized Ward test world"), World)) return true;
            World->InitializeActorsForPlay(FURL()); GS = World->SpawnActor<ABwayGameState>(); World->SetGameState(GS); World->SetBegunPlay(true);
            Owner = World->SpawnActor<APlayerController>();
            if (!Test->TestNotNull(TEXT("Ward owner"), Owner)) return true;
            OwnerPawn = SpawnPawn(1, FVector(0,-3000,10000), Owner);
            Ally = SpawnPawn(1,FVector(2000,500,10000)); Enemy = SpawnPawn(2,FVector(200,0,10000));
            if (!OwnerPawn || !Ally || !Enemy) { Test->AddError(TEXT("Ward pawn setup failed")); return true; }
            OwnerPlayer = OwnerPawn->GetPlayerState<ABwayPlayerState>();
            VictimPlayer = Enemy->GetPlayerState<ABwayPlayerState>(); AllyPlayer = Ally->GetPlayerState<ABwayPlayerState>();
            Observe(TEXT("baseline"), VictimPlayer, 1);
            First = SpawnWard(0); if (!Test->TestNotNull(TEXT("Actual Ward Blueprint"), First)) return true;
            Advance(1.1f);
        }
        else if (Step == 1)
        {
            Test->TestEqual(TEXT("Authored Ward health"), First->GetAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()),600.f);
            TArray<USphereComponent*> Spheres; First->GetComponents(Spheres);
            const auto* Aura = Spheres.FindByPredicate([](const auto* Sphere) { return Sphere->GetFName() == TEXT("SlowSphere"); });
            if (!Test->TestTrue(TEXT("Authored 600cm aura"), Aura && FMath::IsNearlyEqual((*Aura)->GetScaledSphereRadius(),600.f))) return true;
            Observe(TEXT("enemy-already-inside"), VictimPlayer,.5f);
            if (OwnerMode > 0) Owner->UnPossess();
            if (OwnerMode == 2) { OwnerPawn->Destroy(); OwnerPawn = nullptr; }
            Move(Enemy,2000); Move(Ally,100); Advance();
            Observe(TEXT("friendly-after-owner-change"), AllyPlayer,1);
            Observe(TEXT("enemy-outside"), VictimPlayer,1);
            Move(Enemy,200); Observe(TEXT("enemy-reentered"), VictimPlayer,.5f);
        }
        else if (Step == 2)
        {
            Owner->UnPossess();
            if (OwnerPawn) { ULyraPawnExtensionComponent::FindPawnExtensionComponent(OwnerPawn)->UninitializeAbilitySystem(); OwnerPawn->Destroy(); }
            OwnerPawn = SpawnPawn(1,FVector(0,-3000,10000),Owner,OwnerPlayer);
            if (!Test->TestNotNull(TEXT("Replacement placer pawn with same PlayerState"), OwnerPawn)) return true;
            Advance(); Observe(TEXT("owner-replaced-enemy"),VictimPlayer,.5f); Observe(TEXT("owner-replaced-ally"),AllyPlayer,1);
            VictimPlayer->SetGenericTeamId(FGenericTeamId(1)); Enemy->SetGenericTeamId(FGenericTeamId(1)); GS->AddPlayerToTeam(VictimPlayer,0);
        }
        else if (Step == 3)
        {
            // TimerManager ticks only once per engine frame. Each timer-dependent
            // transition gets its own latent step rather than a duplicate Tick call.
            Advance(); Observe(TEXT("target-changed-to-friendly-without-moving"),VictimPlayer,1);
            VictimPlayer->SetGenericTeamId(FGenericTeamId(2)); Enemy->SetGenericTeamId(FGenericTeamId(2)); GS->AddPlayerToTeam(VictimPlayer,1);
        }
        else if (Step == 4)
        {
            Advance(); Observe(TEXT("target-changed-back-without-moving"),VictimPlayer,.5f);
            Second = SpawnWard(150); if (!Test->TestNotNull(TEXT("Second actual Ward"), Second)) return true;
        }
        else if (Step == 5)
        {
            Advance(1.1f);
            // Current authored MultiplyCompound policy. Final-alpha stacking parity is a separate reference check.
            Observe(TEXT("two-independent-authored-sources"),VictimPlayer,.25f);
            Test->TestTrue(TEXT("Enemy destroys first Ward through gameplay damage"), UBwayDamageLibrary::ApplyDamageToEnemyBuildable(Enemy,First,2000,Enemy));
            Observe(TEXT("one-source-destroyed-other-remains"),VictimPlayer,.5f);
            Test->TestEqual(TEXT("One registered Ward remains"),GS->BuildableRegistryComponent->GetAllBuildables().Num(),1);
        }
        else if (Step == 6)
        {
            // Real uninitialization clears the pawn's ASC pointer before end-overlap can run.
            auto* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Enemy);
            Extension->UninitializeAbilitySystem(); Enemy->Destroy(); Enemy = nullptr;
            Advance(); Observe(TEXT("old-avatar-detached-effect-removed"),VictimPlayer,1);
            Enemy = SpawnPawn(2,FVector(2000,0,10000),nullptr,VictimPlayer);
            if (!Test->TestNotNull(TEXT("Replacement victim with persistent ASC"),Enemy)) return true;
            Observe(TEXT("respawn-outside-has-no-stale-slow"),VictimPlayer,1);
            Move(Enemy,200); Observe(TEXT("replacement-reenters"),VictimPlayer,.5f);
        }
        else if (Step == 7)
        {
            ULyraHealthComponent::FindHealthComponent(Enemy)->StartDeath(); Advance();
            Observe(TEXT("dead-target-released-in-place"),VictimPlayer,1);
            ULyraPawnExtensionComponent::FindPawnExtensionComponent(Enemy)->UninitializeAbilitySystem(); Enemy->Destroy();
            Enemy = SpawnPawn(2,FVector(200,0,10000),nullptr,VictimPlayer);
            if (!Test->TestNotNull(TEXT("Replacement initialized while already overlapping"),Enemy)) return true;
        }
        else if (Step == 8)
        {
            Advance(); Observe(TEXT("new-avatar-in-place-slowed"),VictimPlayer,.5f);
            GS->GetRoundManagement()->ResetRoundState();
        }
        else if (Step == 9)
        {
            Advance();
            Test->TestTrue(TEXT("Ward survives production buildable round cleanup"), IsValid(Second) && !Second->IsActorBeingDestroyed());
            Observe(TEXT("round-cleanup-survivor"),VictimPlayer,.5f);
            // Destroy the source while the target pawn can no longer resolve the persistent ASC.
            ULyraPawnExtensionComponent::FindPawnExtensionComponent(Enemy)->UninitializeAbilitySystem();
            Second->Destroy(); Observe(TEXT("source-destroyed-after-avatar-detach"),VictimPlayer,1);
            Observe(TEXT("friendly-final-cleanup"),AllyPlayer,1);
            Test->TestEqual(TEXT("No Ward registry residue"),GS->BuildableRegistryComponent->GetAllBuildables().Num(),0);
            UE_LOG(LogTemp, Display, TEXT("BwayWardAura: Complete ownerMode=%d"),OwnerMode);
            return true;
        }
        ++Step; return false;
    }
private:
    FAutomationTestBase* Test;
    int32 OwnerMode = 0, Step = 0;
    TStrongObjectPtr<UGameInstance> Instance;
    UWorld* World = nullptr;
    ABwayGameState* GS = nullptr;
    APlayerController* Owner = nullptr;
    ABwayPlayerState *OwnerPlayer = nullptr, *VictimPlayer = nullptr, *AllyPlayer = nullptr;
    ABwayCharacterWithAbilities *OwnerPawn = nullptr, *Ally = nullptr, *Enemy = nullptr;
    ABuildableActor *First = nullptr, *Second = nullptr;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayWardLiveOwnerTest,"Breakaway.Buildables.WardAura.LiveOwner",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayWardLiveOwnerTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(BwayWardAuraTest::FRun(this,0)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayWardUnpossessedOwnerTest,"Breakaway.Buildables.WardAura.UnpossessedOwner",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayWardUnpossessedOwnerTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(BwayWardAuraTest::FRun(this,1)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayWardDestroyedOwnerTest,"Breakaway.Buildables.WardAura.DestroyedOwner",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayWardDestroyedOwnerTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(BwayWardAuraTest::FRun(this,2)); return true; }
#endif
