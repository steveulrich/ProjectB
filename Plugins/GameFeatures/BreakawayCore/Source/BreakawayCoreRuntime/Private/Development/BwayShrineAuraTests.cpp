#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "TimerManager.h"
#include "Teams/LyraTeamSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace BwayShrineAuraTest
{
class FRun : public IAutomationLatentCommand
{
public:
    explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FRun()
    {
        if (World) World->DestroyWorld(false);
        if (Instance) Instance->Shutdown();
        if (World) GEngine->DestroyWorldContext(World);
    }
    float Health(ABwayCharacterWithAbilities* Pawn) const
    {
        auto* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
        return ASC ? ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1;
    }
    ABwayCharacterWithAbilities* SpawnPawn(int32 Team, FVector Location, APlayerController* Controller = nullptr)
    {
        auto* Player = World->SpawnActor<ABwayPlayerState>();
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(ABwayCharacterWithAbilities::StaticClass(), Location, FRotator::ZeroRotator, Params);
        if (!Player || !Pawn) return nullptr;
        Player->SetGenericTeamId(FGenericTeamId(Team)); Pawn->SetGenericTeamId(FGenericTeamId(Team));
        GS->AddPlayerToTeam(Player, Team - 1);
        Test->TestEqual(TEXT("Fixture player registered in gameplay roster"), GS->GetPlayerTeam(Player), Team - 1);
        if (Controller) Controller->Possess(Pawn);
        Pawn->SetPlayerState(Player);
        auto* ASC = Player->GetLyraAbilitySystemComponent();
        UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
        if (!ASC->GetSet<ULyraHealthSet>()) ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(Player));
        auto* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
        Extension->SetPawnData(NewObject<ULyraPawnData>(World)); Extension->InitializeAbilitySystem(ASC, Player);
        // Isolated fixture baseline, with no regeneration effect or gameplay experience.
        ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 1000);
        ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 100);
        const auto* Teams = World->GetSubsystem<ULyraTeamSubsystem>();
        UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: PawnSetup pawn=%s desired=%d generic=%d resolved=%d subsystem=%d roster=%d overlaps=%d"),
            *Pawn->GetName(), Team, Pawn->GetGenericTeamId().GetId(), Teams ? Teams->FindTeamFromObject(Pawn) : -1, Teams != nullptr,
            GS->GetPlayerTeam(Player), Pawn->GetCapsuleComponent()->GetGenerateOverlapEvents());
        return Pawn;
    }
    ABuildableActor* SpawnShrine(float X)
    {
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr,
            TEXT("/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine.DA_BW_BuildableData_SunShrine"));
        if (!Data || !Data->BuildableActorClass) return nullptr;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Match production spawn ownership before BeginPlay and then initialize stored team.
        Params.Owner = Owner; Params.Instigator = Owner->GetPawn();
        auto* Shrine = World->SpawnActor<ABuildableActor>(Data->BuildableActorClass, FVector(X,0,10000), FRotator::ZeroRotator, Params);
        if (!Shrine) return nullptr;
        Shrine->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
        UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(Shrine, Data, 0);
        Shrine->InitializeBuildable(Owner, 0);
        TArray<USphereComponent*> Spheres; Shrine->GetComponents(Spheres);
        for (auto* Sphere : Spheres) UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Shape name=%s team=%d overlaps=%d radius=%.3f pawnResponse=%d"),
            *Sphere->GetName(), Shrine->GetTeamId().GetId(), Sphere->GetGenerateOverlapEvents(), Sphere->GetScaledSphereRadius(), int32(Sphere->GetCollisionResponseToChannel(ECC_Pawn)));
        return Shrine;
    }
    void Advance() { World->GetTimerManager().Tick(1.05f); }
    void Observe(const TCHAR* Name, float ExpectedDelta)
    {
        const float After = Health(Ally);
        const bool Passed = FMath::IsNearlyEqual(After - Before, ExpectedDelta, .01f);
        Test->TestTrue(FString::Printf(TEXT("%s healing delta %.1f"), Name, ExpectedDelta), Passed);
        Test->TestEqual(FString::Printf(TEXT("%s excludes enemy"), Name), Health(Enemy), 100.f);
        Test->TestEqual(FString::Printf(TEXT("%s excludes outside ally"), Name), Health(Outside), 100.f);
        UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Verify phase=%s before=%.3f after=%.3f expectedDelta=%.3f enemy=%.3f outside=%.3f pass=%d"),
            Name, Before, After, ExpectedDelta, Health(Enemy), Health(Outside), Passed);
        Before = After;
    }
    bool Update() override
    {
        if (Step == 0)
        {
            Instance.Reset(NewObject<UGameInstance>(GEngine)); Instance->InitializeStandalone(); World = Instance->GetWorld();
            if (!Test->TestNotNull(TEXT("Initialized Shrine test world"), World)) return true;
            World->InitializeActorsForPlay(FURL()); GS = World->SpawnActor<ABwayGameState>(); World->SetGameState(GS); World->SetBegunPlay(true);
            Owner = World->SpawnActor<APlayerController>();
            if (!Test->TestNotNull(TEXT("Shrine owner controller"), Owner)) return true;
            OwnerPawn = SpawnPawn(1,FVector(0,-2000,10000),Owner);
            Ally = SpawnPawn(1,FVector(100,0,10000)); Enemy = SpawnPawn(2,FVector(200,0,10000)); Outside = SpawnPawn(1,FVector(1200,0,10000));
            if (!OwnerPawn || !Ally || !Enemy || !Outside) { Test->AddError(TEXT("Shrine fixture pawn setup failed")); return true; }
            Before = Health(Ally); Advance(); Observe(TEXT("no-shrine-baseline"), 0);
        }
        else if (Step == 1)
        {
            First = SpawnShrine(0);
            if (!Test->TestNotNull(TEXT("Actual first Shrine Blueprint"), First)) return true;
            Advance(); Before = Health(Ally); // Complete the authored build delay.
        }
        else if (Step == 2) { Advance(); Observe(TEXT("one-shrine"), 35); }
        else if (Step == 3)
        {
            Second = SpawnShrine(250);
            if (!Test->TestNotNull(TEXT("Actual second Shrine Blueprint"), Second)) return true;
            Advance(); Before = Health(Ally);
        }
        else if (Step == 4) { Advance(); Observe(TEXT("overlapping-shrines"), 35); }
        else if (Step == 5)
        {
            Owner->UnPossess(); OwnerPawn->Destroy(); OwnerPawn = nullptr;
            Advance(); Observe(TEXT("owner-pawn-absent"), 35);
        }
        else if (Step == 6)
        {
            OwnerPawn = SpawnPawn(1,FVector(0,-2000,10000),Owner);
            if (!Test->TestNotNull(TEXT("Replacement owner pawn"), OwnerPawn)) return true;
            Advance(); Observe(TEXT("owner-replaced"), 35);
        }
        else if (Step == 7)
        {
            Test->TestTrue(TEXT("Closest Shrine receives lethal gameplay damage"), UBwayDamageLibrary::ApplyDamageFromSource(Enemy->GetAbilitySystemComponent(),First,1000,Enemy,Enemy));
            Test->TestTrue(TEXT("Closest Shrine destroyed"), First->IsActorBeingDestroyed());
            Test->TestEqual(TEXT("One Shrine remains in registry"), GS->BuildableRegistryComponent->GetAllBuildables().Num(), 1);
            Advance(); Observe(TEXT("nearest-destroyed"),35);
        }
        else if (Step == 8)
        {
            Ally->SetActorLocation(FVector(1200,300,10000),false,nullptr,ETeleportType::TeleportPhysics);
            Advance(); Observe(TEXT("outside-radius"),0);
        }
        else if (Step == 9)
        {
            Ally->SetActorLocation(FVector(100,0,10000),false,nullptr,ETeleportType::TeleportPhysics);
            Advance(); Observe(TEXT("reentered-radius"),35);
        }
        else if (Step == 10)
        {
            Test->TestTrue(TEXT("Last Shrine receives lethal gameplay damage"),UBwayDamageLibrary::ApplyDamageFromSource(Enemy->GetAbilitySystemComponent(),Second,1000,Enemy,Enemy));
            Test->TestTrue(TEXT("Last Shrine destroyed"),Second->IsActorBeingDestroyed());
            Advance(); Observe(TEXT("last-destroyed"),0);
            Test->TestEqual(TEXT("Shrine registry cleanup"),GS->BuildableRegistryComponent->GetAllBuildables().Num(),0);
            return true;
        }
        ++Step; return false;
    }
    FAutomationTestBase* Test;
    TStrongObjectPtr<UGameInstance> Instance;
    UWorld* World = nullptr;
    ABwayGameState* GS = nullptr;
    APlayerController* Owner = nullptr;
    ABwayCharacterWithAbilities* OwnerPawn = nullptr;
    ABwayCharacterWithAbilities* Ally = nullptr;
    ABwayCharacterWithAbilities* Enemy = nullptr;
    ABwayCharacterWithAbilities* Outside = nullptr;
    ABuildableActor* First = nullptr;
    ABuildableActor* Second = nullptr;
    int32 Step = 0;
    float Before = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayShrineAuraContinuityTest,"Breakaway.Buildables.ShrineAuraContinuity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayShrineAuraContinuityTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(BwayShrineAuraTest::FRun(this));
    return true;
}
#endif
