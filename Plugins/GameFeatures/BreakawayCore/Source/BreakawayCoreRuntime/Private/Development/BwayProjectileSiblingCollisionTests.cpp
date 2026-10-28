#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwayProjectilePresentationTestTypes.h"
#include "Buildable/BuildableBase.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace BwayProjectileSiblingTest
{
// Separate latent updates give the real world scheduler fresh engine frames.
// No direct hit callback or manually ticked projectile substitutes for a sweep.
class FRun : public IAutomationLatentCommand
{
public:
    FRun(FAutomationTestBase* InTest, FString InCase) : Test(InTest), Case(MoveTemp(InCase)) {}
    virtual ~FRun() override
    {
        if (World)
        {
            if (World->HasBegunPlay()) World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
        }
        if (Instance) Instance->Shutdown();
        if (Instance && World) GEngine->DestroyWorldContext(World);
    }
    virtual bool Update() override
    {
        if (Step++ == 0) return !Setup();
        World->Tick(LEVELTICK_All, 0.05f);
        if (Step == 2)
        {
            if (bPairShouldPass)
            {
                Test->TestFalse(TEXT("First same-pawn shot survives real sweep"), First->IsActorBeingDestroyed());
                Test->TestFalse(TEXT("Second same-pawn shot survives real sweep"), Second->IsActorBeingDestroyed());
                Test->TestTrue(TEXT("Both same-origin shots move at least40cm in one world tick"),
                    First->GetActorLocation().X > 40.f && Second->GetActorLocation().X > 40.f);
                Test->TestEqual(TEXT("First shot has no false damage callback"), First->DamageCalls, 0);
                Test->TestEqual(TEXT("Second shot has no false damage callback"), Second->DamageCalls, 0);
                // The actual engine move-ignore array is strong. Destruction must
                // release both directions immediately, not wait for garbage collection.
                First->Destroy();
                Test->TestFalse(TEXT("Surviving sphere drops destroyed sibling"), Sphere(Second)->GetMoveIgnoreActors().Contains(First));
                Test->TestTrue(TEXT("Ended projectile releases all move-ignore strong references"), Sphere(First)->GetMoveIgnoreActors().IsEmpty());
                First = nullptr;
                return false;
            }
            if (Second)
            {
                Test->TestTrue(TEXT("Unrelated same-origin projectile pair still collides"),
                    First->IsActorBeingDestroyed() || Second->IsActorBeingDestroyed());
                Test->TestTrue(TEXT("Unrelated collision reaches production hit dispatch"), First->DamageCalls + Second->DamageCalls > 0);
            }
            else
            {
                Test->TestTrue(TEXT("Wall/buildable still terminates projectile through real collision"), First->IsActorBeingDestroyed());
                Test->TestEqual(TEXT("Collision reaches damage handler once"), First->DamageCalls, 1);
                Test->TestTrue(TEXT("Actual collision target is the control actor"), First->LastDamageTarget.Get() == Obstacle);
            }
            return true;
        }
        Test->TestFalse(TEXT("Survivor continues after sibling teardown"), Second->IsActorBeingDestroyed());
        Test->TestTrue(TEXT("Survivor advances a second world tick"), Second->GetActorLocation().X > 90.f);
        Test->TestEqual(TEXT("Survivor still has no false damage callback"), Second->DamageCalls, 0);
        return true;
    }
private:
    static USphereComponent* Sphere(ABwayProjectilePresentationTestActor* Actor)
    {
        return Actor ? Cast<USphereComponent>(Actor->GetRootComponent()) : nullptr;
    }
    ABwayProjectilePresentationTestActor* Spawn(APawn* Pawn)
    {
        const FTransform Transform(FVector(0.f, 0.f, 1000.f));
        auto* Shot = World->SpawnActorDeferred<ABwayProjectilePresentationTestActor>(
            ABwayProjectilePresentationTestActor::StaticClass(), Transform, Pawn, Pawn,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Test->TestNotNull(TEXT("Collision-enabled production-base projectile"), Shot)) return nullptr;
        Shot->ConfigureMovement(1000.f, 10.f);
        Shot->bAcceptHit = Case == TEXT("BuildableOverlapPreserved");
        Shot->FinishSpawning(Transform);
        auto* Move = Shot->FindComponentByClass<UProjectileMovementComponent>();
        if (!Test->TestNotNull(TEXT("Production projectile movement"), Move)) return nullptr;
        Test->TestTrue(TEXT("Projectile BeginPlay and scheduled movement tick are active"),
            Shot->HasActorBegunPlay() && Move->IsComponentTickEnabled() && Move->PrimaryComponentTick.IsTickFunctionRegistered());
        Test->TestEqual(TEXT("WorldDynamic blocking response remains unchanged"), Sphere(Shot)->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);
        return Shot;
    }
    bool Setup()
    {
        Instance.Reset(NewObject<UGameInstance>(GEngine));
        Instance->InitializeStandalone();
        World = Instance->GetWorld();
        if (!Test->TestNotNull(TEXT("Scheduled physics world"), World)) return false;
        World->InitializeActorsForPlay(FURL());
        World->SetBegunPlay(true);
        APawn* PawnA = World->SpawnActor<APawn>();
        APawn* PawnB = World->SpawnActor<APawn>();
        if (!Test->TestNotNull(TEXT("First firing pawn"), PawnA) || !Test->TestNotNull(TEXT("Other firing pawn"), PawnB)) return false;
        bPairShouldPass = Case == TEXT("SamePawnCatchUp") || Case == TEXT("LateInstigatorAndCleanup");
        const bool bUnowned = Case == TEXT("UnownedStillBlocks");
        First = Spawn(bUnowned ? nullptr : PawnA);
        if (!First) return false;
        if (bPairShouldPass || bUnowned || Case == TEXT("DifferentPawnStillBlocks"))
        {
            APawn* SecondPawn = Case == TEXT("SamePawnCatchUp") ? PawnA :
                (Case == TEXT("DifferentPawnStillBlocks") ? PawnB : nullptr);
            Second = Spawn(SecondPawn);
            if (!Second) return false;
            Test->TestTrue(TEXT("Catch-up shots start coincident before either moves"),
                First->GetActorLocation().Equals(Second->GetActorLocation(), 0.001f));
            if (Case == TEXT("LateInstigatorAndCleanup"))
            {
                Test->TestFalse(TEXT("Unknown firing pawn does not grant immunity"), Sphere(First)->GetMoveIgnoreActors().Contains(Second));
                Second->ResolveInstigator(PawnA);
            }
            Test->TestEqual(TEXT("First sphere has only intended sibling pair immunity"), Sphere(First)->GetMoveIgnoreActors().Contains(Second), bPairShouldPass);
            Test->TestEqual(TEXT("Second sphere has reciprocal intended pair immunity"), Sphere(Second)->GetMoveIgnoreActors().Contains(First), bPairShouldPass);
            return true;
        }
        if (Case == TEXT("BuildableOverlapPreserved"))
        {
            // Its default 50cm box must not overlap the shot before the fixture
            // installs the intended 5cm swept-contact shape.
            auto* Buildable = World->SpawnActor<ABuildableActor>(FVector(1000.f, 0.f, 1000.f), FRotator::ZeroRotator);
            if (!Test->TestNotNull(TEXT("Native buildable collision control"), Buildable)) return false;
            Buildable->ConfigurePlacementBounds(FVector::ZeroVector, FVector(5.f, 100.f, 100.f));
            Buildable->SetOwner(PawnB);
            Buildable->SetActorLocation(FVector(40.f, 0.f, 1000.f));
            Obstacle = Buildable;
            Test->TestEqual(TEXT("Buildable keeps production WorldDynamic overlap"), Buildable->GetAttackCollision()->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Overlap);
        }
        else
        {
            Obstacle = World->SpawnActor<AActor>();
            if (!Test->TestNotNull(TEXT("Wall control actor"), Obstacle)) return false;
            auto* Box = NewObject<UBoxComponent>(Obstacle);
            Obstacle->SetRootComponent(Box);
            Obstacle->AddInstanceComponent(Box);
            Box->SetBoxExtent(FVector(5.f, 100.f, 100.f));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->SetCollisionObjectType(Case == TEXT("WorldStaticStillBlocks") ? ECC_WorldStatic : ECC_WorldDynamic);
            Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent();
            Obstacle->SetActorLocation(FVector(40.f, 0.f, 1000.f));
            // Even sharing the firing pawn as Owner must not exempt a non-projectile.
            Obstacle->SetOwner(PawnA);
        }
        Test->TestFalse(TEXT("Non-projectile control actor is not movement-ignored"), Sphere(First)->GetMoveIgnoreActors().Contains(Obstacle));
        Test->TestFalse(TEXT("Control projectile survives setup until its real movement tick"), First->IsActorBeingDestroyed());
        Test->TestEqual(TEXT("Control setup has no premature damage callback"), First->DamageCalls, 0);
        return true;
    }
    FAutomationTestBase* Test;
    FString Case;
    int32 Step = 0;
    bool bPairShouldPass = false;
    TStrongObjectPtr<UGameInstance> Instance;
    UWorld* World = nullptr;
    ABwayProjectilePresentationTestActor* First = nullptr;
    ABwayProjectilePresentationTestActor* Second = nullptr;
    AActor* Obstacle = nullptr;
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FBwayProjectileSiblingCollisionTest, "Breakaway.CombatPolish.Projectile.SiblingCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FBwayProjectileSiblingCollisionTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const TCHAR* Name : { TEXT("SamePawnCatchUp"), TEXT("LateInstigatorAndCleanup"), TEXT("DifferentPawnStillBlocks"),
        TEXT("UnownedStillBlocks"), TEXT("WorldStaticStillBlocks"), TEXT("WorldDynamicStillBlocks"), TEXT("BuildableOverlapPreserved") })
    {
        Names.Add(Name);
        Commands.Add(Name);
    }
}
bool FBwayProjectileSiblingCollisionTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(BwayProjectileSiblingTest::FRun(this, Parameters));
    return true;
}
#endif
