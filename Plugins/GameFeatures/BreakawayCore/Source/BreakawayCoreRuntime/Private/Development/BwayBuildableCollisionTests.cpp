#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Buildable/BwaySiegeEngineBuildable.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "BwayGameState.h"
#include "Physics/LyraCollisionChannels.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/SkeletalMeshComponent.h"

namespace BwayBuildableCollisionTest
{
const TCHAR* Paths[] = {
    TEXT("/Hero_Spartacus/Characters/Heroes/Argus/Buildables/DA_BW_BuildableData_SiegeEngine.DA_BW_BuildableData_SiegeEngine"),
    TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_BuildableData_Gunslinger.DA_BW_BuildableData_Gunslinger"),
    TEXT("/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine.DA_BW_BuildableData_SunShrine"),
    TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_BuildableData_Hexweaver.DA_BW_BuildableData_Hexweaver"),
    TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_JailCage.BP_BW_Gunslinger_JailCage_C")};

class FRun : public IAutomationLatentCommand
{
public:
    explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FRun() override
    {
        if (World) World->DestroyWorld(false);
        if (Instance) Instance->Shutdown();
        if (World) GEngine->DestroyWorldContext(World);
    }

    bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 20) { Test->AddError(TEXT("Buildable collision fixture timed out")); return true; }
        if (!World)
        {
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(); World = Instance->GetWorld();
            if (!Test->TestNotNull(TEXT("Isolated collision world"), World)) return true;
            World->InitializeActorsForPlay(FURL());
            World->SetGameState(World->SpawnActor<ABwayGameState>());
            World->SetBegunPlay(true);
            if (!Test->TestTrue(TEXT("Blueprint callbacks enabled by initialized world actors"), World->AreActorsInitialized())) return true;
        }
        if (!Actor)
        {
            if (Index == UE_ARRAY_COUNT(Paths)) return true;
            UClass* ActorClass = nullptr;
            if (Index < 4)
            {
                Data.Reset(LoadObject<UBwayBuildableDataAsset>(nullptr, Paths[Index]));
                if (!Test->TestNotNull(Paths[Index], Data.Get())) return true;
                ActorClass = Data->BuildableActorClass;
            }
            else ActorClass = LoadClass<ABuildableActor>(nullptr, Paths[Index]);
            if (!Test->TestNotNull(TEXT("Authored actor class"), ActorClass)) return true;
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Actor = World->SpawnActor<ABuildableActor>(ActorClass, FVector(Index * 1000, 0, 1000), FRotator::ZeroRotator, Params);
            if (!Test->TestNotNull(TEXT("Actual authored Blueprint actor"), Actor)) return true;
            if (!Actor->IsActorInitialized()) static_cast<AActor*>(Actor)->PostInitializeComponents();
            if (Data)
            {
                Actor->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
                UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(Actor, Data.Get(), 0);
            }
            if (!Actor->HasActorBegunPlay()) Actor->DispatchBeginPlay();
            Actor->InitializeBuildable(nullptr, 0);
            SpawnedAt = LastTick = Now;
            return false;
        }
        World->GetTimerManager().Tick(float(Now - LastTick)); LastTick = Now;
        if (Now - SpawnedAt < 1.5) return false;
        Test->TestTrue(TEXT("Blueprint BeginPlay executed"), Actor->HasActorBegunPlay());
        const bool bFloorTrap = Index == 1;
        const FString Label = Actor->GetClass()->GetName();
        Test->TestEqual(TEXT("Construction damageability preserves armed floor trap immunity"), Actor->CanBeDamaged(), !bFloorTrap);
        // Exercise the reflected notification used on clients, including derived rules.
        if (UFunction* Notify = Actor->FindFunction(TEXT("OnRep_BuildState")))
        {
            Actor->ProcessEvent(Notify, nullptr);
            Test->TestEqual(TEXT("Replicated build state preserves authored damageability"), Actor->CanBeDamaged(), !bFloorTrap);
        }
        else Test->AddError(TEXT("Build state replication notification is missing"));
        TInlineComponentArray<UPrimitiveComponent*> Components; Actor->GetComponents(Components);
        for (const auto* Component : Components)
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableCollision: Component asset=%s component=%s class=%s root=%d enabled=%d object=%d pawn=%d dynamic=%d visibility=%d weapon=%d overlaps=%d"),
                *Label, *Component->GetName(), *Component->GetClass()->GetName(), Component == Actor->GetRootComponent(),
                int32(Component->GetCollisionEnabled()), int32(Component->GetCollisionObjectType()), int32(Component->GetCollisionResponseToChannel(ECC_Pawn)),
                int32(Component->GetCollisionResponseToChannel(ECC_WorldDynamic)), int32(Component->GetCollisionResponseToChannel(ECC_Visibility)),
                int32(Component->GetCollisionResponseToChannel(Lyra_TraceChannel_Weapon)), Component->GetGenerateOverlapEvents());
        const FVector Center = Actor->GetActorTransform().TransformPosition(Actor->GetPlacementLocalBounds().GetCenter());
        const FVector Start = Center - FVector(150, 0, 0), End = Center + FVector(150, 0, 0);
        for (const auto Channel : {ECC_Visibility, Lyra_TraceChannel_Weapon, Lyra_TraceChannel_Weapon_Capsule, Lyra_TraceChannel_Weapon_Multi})
        {
            FHitResult Hit;
            World->LineTraceSingleByChannel(Hit, Start, End, Channel);
            const bool Found = Hit.bBlockingHit && Hit.GetActor() == Actor;
            Test->TestEqual(FString::Printf(TEXT("%s attack/visibility channel %d"), *Label, int32(Channel)), Found, !bFloorTrap);
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableCollision: Trace asset=%s channel=%d hit=%d"), *Label, int32(Channel), Found);
        }
        for (const auto Channel : {ECC_Pawn, ECC_WorldDynamic})
        {
            TArray<FHitResult> Hits;
            World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeSphere(10));
            const bool Found = Hits.ContainsByPredicate([this](const FHitResult& Hit) { return Hit.GetActor() == Actor; });
            Test->TestEqual(FString::Printf(TEXT("%s melee/projectile channel %d"), *Label, int32(Channel)), Found, !bFloorTrap || Channel == ECC_Pawn);
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableCollision: Sweep asset=%s channel=%d hit=%d"), *Label, int32(Channel), Found);
        }
        if (!bFloorTrap && Index < 4)
        {
            // Nonzero authored centers and asymmetric extents must survive cosmetic replacement.
            const FVector BoundsCenter(20, 30, 40), BoundsExtent(25, 35, 45);
            const FVector Pivot = Actor->GetActorLocation();
            Actor->ConfigurePlacementBounds(BoundsCenter, BoundsExtent);
            Actor->GetMesh()->SetSkeletalMesh(nullptr);
            const FVector QueryCenter = Actor->GetActorTransform().TransformPosition(BoundsCenter);
            FHitResult Hit;
            World->LineTraceSingleByChannel(Hit, QueryCenter - FVector(100,0,0), QueryCenter + FVector(100,0,0), Lyra_TraceChannel_Weapon);
            Test->TestTrue(TEXT("Weapon hits exact authored body without a visual mesh"), Hit.GetComponent() == Actor->GetAttackCollision());
            Test->TestTrue(TEXT("Gameplay shape applies authored center and extent"), Actor->GetAttackCollision()->GetComponentLocation().Equals(QueryCenter)
                && Actor->GetAttackCollision()->GetUnscaledBoxExtent().Equals(BoundsExtent));
            Test->TestTrue(TEXT("Changing gameplay bounds preserves actor pivot"), Actor->GetActorLocation().Equals(Pivot));
            Actor->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
        }
        if (Actor->IsA<ABwaySiegeEngineBuildable>())
        {
            AActor* Wall = World->SpawnActor<AActor>();
            auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(20, 150, 150)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent(); Wall->SetActorLocation(Center + FVector(250, 0, 0));
            // Exercise the actual native tick, including collision expiry, at a long frame.
            Actor->Tick(0.5f);
            const bool Stopped = Actor->IsActorBeingDestroyed() && Actor->GetActorLocation().X < Center.X + 250;
            Test->TestTrue(TEXT("Native Siege movement stops and expires at a world obstacle"), Stopped);
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableCollision: WorldSweep hit=%d"), Stopped);
            Wall->Destroy();
        }
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableCollision: AssetComplete asset=%s components=%d begun=%d"), *Label, Components.Num(), Actor->HasActorBegunPlay());
        Actor->Destroy(); Actor = nullptr; Data.Reset(); ++Index;
        return false;
    }
private:
    FAutomationTestBase* Test;
    TStrongObjectPtr<UGameInstance> Instance;
    UWorld* World = nullptr;
    ABuildableActor* Actor = nullptr;
    TStrongObjectPtr<UBwayBuildableDataAsset> Data;
    int32 Index = 0;
    double Started = FPlatformTime::Seconds(), SpawnedAt = 0, LastTick = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayBuildableCollisionTest, "Breakaway.Buildables.CollisionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayBuildableCollisionTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(BwayBuildableCollisionTest::FRun(this));
    return true;
}
#endif
