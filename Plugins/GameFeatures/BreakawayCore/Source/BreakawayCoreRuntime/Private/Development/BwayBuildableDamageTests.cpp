#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwaySiegeEngineBuildable.h"
#include "Combat/BwayDamageLibrary.h"
#include "Combat/BwayHeroProjectileBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Character/LyraPawnData.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"

namespace BwayBuildableDamageTest
{
const TCHAR* Primaries[] = {
    TEXT("/Script/BreakawayCoreRuntime.BwayGameplayAbility_MeleePrimary"),
    TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsPrimary"),
    TEXT("/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaPrimary"),
    TEXT("/Script/HeroMorganRuntime.BwayGameplayAbility_KorrynPrimary")};

// Actual primary ability activation and projectile movement in an isolated authority
// world. This deliberately does not claim owner prediction or network convergence.
class FRun : public IAutomationLatentCommand
{
public:
    explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FRun() override
    {
        if (World) { GEngine->Exec(World, TEXT("Log LogBwayProjectileCollision Log")); GEngine->Exec(World, TEXT("Log LogBwayDamage Log")); }
        if (World) World->DestroyWorld(false);
        if (Instance) Instance->Shutdown();
        if (World) GEngine->DestroyWorldContext(World);
    }

    bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 45) { Test->AddError(TEXT("Buildable primary damage timeout")); return true; }
        if (!World)
        {
            // Health effects publish through the real game-instance message subsystem.
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(); World = Instance->GetWorld();
            if (!Test->TestNotNull(TEXT("Primary damage world"), World)) return true;
            // Actor::ProcessEvent also requires initialized world actors, even for
            // native UFUNCTION overlap callbacks. Per-actor initialization is insufficient.
            World->InitializeActorsForPlay(FURL());
            World->SetGameState(World->SpawnActor<ABwayGameState>());
            // PrimitiveComponent only delivers overlap delegates in a begun world.
            // Actors/components are initialized below; no match orchestration is simulated.
            World->SetBegunPlay(true);
            if (!Test->TestTrue(TEXT("World can dispatch native overlap callbacks"), World->AreActorsInitialized() && World->HasBegunPlay())) return true;
            GEngine->Exec(World, TEXT("Log LogBwayProjectileCollision Verbose"));
            GEngine->Exec(World, TEXT("Log LogBwayDamage Verbose"));
        }
        if (!Target)
        {
            if (Case == 8) return true;
            if (!Setup()) return true;
            LastTick = CaseStarted = Now;
            return false;
        }
        const float Delta = FMath::Min(float(Now - LastTick), 0.05f); LastTick = Now;
        World->GetTimerManager().Tick(Delta);
        // The automation world does not tick actors globally. Advance only the actual
        // projectile movement components, retaining their sweeps and hit delegates.
        TArray<TWeakObjectPtr<ABwayHeroProjectileBase>> Projectiles;
        for (TActorIterator<ABwayHeroProjectileBase> It(World); It; ++It) Projectiles.Add(*It);
        for (auto Weak : Projectiles)
        {
            auto* Projectile = Weak.Get();
            if (!Projectile || Projectile->IsActorBeingDestroyed()) continue;
            if (!SeenProjectiles.Contains(Weak)) { SeenProjectiles.Add(Weak); ++ProjectileCount; }
            if (!Projectile->HasActorBegunPlay())
            {
                Projectile->InitializeComponents();
                if (!Projectile->IsActorInitialized()) Projectile->PostInitializeComponents();
                Projectile->DispatchBeginPlay();
            }
            if (auto* Movement = Projectile->FindComponentByClass<UProjectileMovementComponent>())
            {
                const FVector BeforeMove = Projectile->GetActorLocation();
                if (MovementSamples++ == 0)
                {
                    UE_LOG(LogTemp, Display, TEXT("BwayBuildableDamage: ProjectileSetup hero=%d position=%s velocity=%s updated=%s target=%s"),
                        Case / 2, *BeforeMove.ToCompactString(), *Movement->Velocity.ToCompactString(), *GetNameSafe(Movement->UpdatedComponent), *Target->GetActorLocation().ToCompactString());
                    auto* Sphere = Cast<USphereComponent>(Projectile->GetRootComponent());
                    auto* Box = Target->GetAttackCollision();
                    TArray<FHitResult> Hits;
                    FCollisionQueryParams Query(SCENE_QUERY_STAT(BuildableProjectileTest), false, Projectile);
                    Query.AddIgnoredActor(Pawn);
                    World->SweepMultiByChannel(Hits, BeforeMove, Target->GetActorLocation() + FVector(150,0,0), FQuat::Identity,
                        Sphere->GetCollisionObjectType(), FCollisionShape::MakeSphere(Sphere->GetScaledSphereRadius()), Query,
                        FCollisionResponseParams(Sphere->GetCollisionResponseToChannels()));
                    UE_LOG(LogTemp, Display, TEXT("BwayBuildableDamage: CollisionDiagnostic worldBegun=%d projectileInit=%d targetInit=%d projectileOverlap=%d targetOverlap=%d response=%d bound=%d radius=%.2f targetQueryHits=%d"),
                        World->HasBegunPlay(), Projectile->IsActorInitialized(), Target->IsActorInitialized(), Sphere->GetGenerateOverlapEvents(), Box->GetGenerateOverlapEvents(),
                        int32(Sphere->GetCollisionResponseToComponent(Box)), Sphere->OnComponentBeginOverlap.IsBound(), Sphere->GetScaledSphereRadius(),
                        Hits.FilterByPredicate([this](const FHitResult& Hit) { return Hit.GetActor() == Target; }).Num());
                }
                Movement->TickComponent(Delta, LEVELTICK_All, nullptr);
                if (MovementSamples <= 3)
                    UE_LOG(LogTemp, Display, TEXT("BwayBuildableDamage: MovementSample hero=%d x=%.3f overlapsTarget=%d targetHealth=%.3f"),
                        Case / 2, Projectile->GetActorLocation().X, Cast<UPrimitiveComponent>(Projectile->GetRootComponent())->IsOverlappingComponent(Target->GetAttackCollision()), Health());
                TravelDistance += FVector::Distance(BeforeMove, Projectile->GetActorLocation());
            }
        }
        if (Now - CaseStarted < 2.0) return false;
        const float After = Health();
        const bool bEnemy = Case % 2 == 1;
        if (Case / 2 > 0) Test->TestTrue(TEXT("Actual projectile was spawned and moved"), ProjectileCount > 0 && TravelDistance > 1);
        Test->TestTrue(TEXT("Primary damages enemy and preserves friendly structure"), bEnemy ? After < Before : FMath::IsNearlyEqual(After, Before));
        const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
        Test->TestTrue(TEXT("Primary ability completed cleanly"), Spec && !Spec->IsActive());
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableDamage: Primary hero=%d enemy=%d before=%.3f after=%.3f ended=%d projectiles=%d travel=%.3f"), Case / 2, bEnemy, Before, After, Spec && !Spec->IsActive(), ProjectileCount, TravelDistance);
        for (auto Weak : Projectiles) if (auto* Projectile = Weak.Get()) Projectile->Destroy();
        ASC->ClearAllAbilities(); Target->Destroy(); Target = nullptr;
        Controller->UnPossess(); Pawn->Destroy(); Player->Destroy(); Controller->Destroy();
        ++Case;
        return false;
    }
private:
    float Health() const { return Target->GetAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()); }
    bool Setup()
    {
        ProjectileCount = MovementSamples = 0; TravelDistance = 0; SeenProjectiles.Reset();
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Player = World->SpawnActor<ABwayPlayerState>();
        Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(ABwayCharacterWithAbilities::StaticClass(), FVector(0,0,1000), FRotator::ZeroRotator, Params);
        Controller = World->SpawnActor<APlayerController>();
        if (!Player || !Pawn || !Controller) { Test->AddError(TEXT("Primary source setup failed")); return false; }
        if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
        Player->SetGenericTeamId(FGenericTeamId(1));
        Pawn->SetGenericTeamId(FGenericTeamId(1));
        Controller->Possess(Pawn); Pawn->SetPlayerState(Player);
        ASC = Player->GetLyraAbilitySystemComponent();
        UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
        if (!ASC->GetSet<ULyraHealthSet>()) ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(Player));
        auto* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
        Extension->SetPawnData(NewObject<ULyraPawnData>(World));
        Extension->InitializeAbilitySystem(ASC, Player);
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr,
            TEXT("/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine.DA_BW_BuildableData_SunShrine"));
        if (!Test->TestNotNull(TEXT("Production shrine definition"), Data)) return false;
        Target = World->SpawnActor<ABuildableActor>(Data->BuildableActorClass, FVector(150,0,1000), FRotator::ZeroRotator, Params);
        if (!Test->TestNotNull(TEXT("Production shrine Blueprint"), Target)) return false;
        if (!Target->IsActorInitialized()) static_cast<AActor*>(Target)->PostInitializeComponents();
        Target->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
        if (!Target->HasActorBegunPlay()) Target->DispatchBeginPlay();
        Target->InitializeBuildable(nullptr, Case % 2);
        Before = Health();
        // Also exercise the shared GAS boundary: AActor immunity is otherwise ignored by GAS.
        Target->SetCanBeDamaged(false);
        Test->TestFalse(TEXT("GAS damage rejects invulnerable buildable"), UBwayDamageLibrary::ApplyDamageFromSource(ASC, Target, 25, Pawn, Pawn));
        Test->TestEqual(TEXT("Invulnerability preserved health"), Health(), Before);
        Target->SetCanBeDamaged(true);
        UClass* AbilityClass = LoadClass<UGameplayAbility>(nullptr, Primaries[Case / 2]);
        if (!Test->TestNotNull(TEXT("Production primary class"), AbilityClass)) return false;
        Handle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
        return Test->TestTrue(TEXT("Real primary activation accepted"), ASC->TryActivateAbility(Handle));
    }
    FAutomationTestBase* Test;
    TStrongObjectPtr<UGameInstance> Instance;
    UWorld* World = nullptr;
    ABuildableActor* Target = nullptr;
    ABwayPlayerState* Player = nullptr;
    ABwayCharacterWithAbilities* Pawn = nullptr;
    APlayerController* Controller = nullptr;
    ULyraAbilitySystemComponent* ASC = nullptr;
    FGameplayAbilitySpecHandle Handle;
    int32 Case = 0;
    int32 ProjectileCount = 0, MovementSamples = 0;
    double TravelDistance = 0;
    TSet<TWeakObjectPtr<ABwayHeroProjectileBase>> SeenProjectiles;
    float Before = 0;
    double Started = FPlatformTime::Seconds(), CaseStarted = 0, LastTick = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayBuildablePrimaryDamageTest, "Breakaway.Buildables.PrimaryDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayBuildablePrimaryDamageTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(BwayBuildableDamageTest::FRun(this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySiegeDamageTest, "Breakaway.Buildables.SiegeDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySiegeDamageTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone();
    UWorld* World = Instance->GetWorld();
    if (!TestNotNull(TEXT("Siege damage world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* GameState = World->SpawnActor<ABwayGameState>(); World->SetGameState(GameState);
    World->SetBegunPlay(true);
    if (!TestNotNull(TEXT("Buildable registry"), GameState->BuildableRegistryComponent.Get())) return false;
    auto Spawn = [&](UClass* Class, FVector Position, int32 Team)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor = World->SpawnActor<ABuildableActor>(Class, Position, FRotator::ZeroRotator, Params);
        if (!Actor->IsActorInitialized()) static_cast<AActor*>(Actor)->PostInitializeComponents();
        if (!Actor->HasActorBegunPlay()) Actor->DispatchBeginPlay();
        Actor->InitializeBuildable(nullptr, Team);
        return Actor;
    };
    auto Health = [](ABuildableActor* Actor)
    {
        return Actor->GetAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
    };
    auto* Siege = Spawn(ABwaySiegeEngineBuildable::StaticClass(), FVector(0,0,1000), 0);
    auto* Enemy = Spawn(ABuildableActor::StaticClass(), FVector(140,100,1000), 1);
    auto* Friend = Spawn(ABuildableActor::StaticClass(), FVector(140,-100,1000), 0);
    auto* Neutral = Spawn(ABuildableActor::StaticClass(), FVector(140,80,1000), 0);
    Neutral->SetTeamId(FGenericTeamId::NoTeam);
    const float EnemyBefore = Health(Enemy), FriendBefore = Health(Friend), NeutralBefore = Health(Neutral);
    float Elapsed = 0;
    for (float Delta : {0.1f, 0.02f, 1.f / 60.f})
    {
        Siege->Tick(Delta); Elapsed += Delta;
        TestTrue(TEXT("Native Siege DPS is 200 per second"), FMath::IsNearlyEqual(Health(Enemy), EnemyBefore - 200.f * Elapsed, 0.01f));
        TestEqual(TEXT("Native Siege preserves friendly structure"), Health(Friend), FriendBefore);
        TestEqual(TEXT("Native Siege preserves unknown-team structure"), Health(Neutral), NeutralBefore);
        TestFalse(TEXT("Dynamic structure overlaps do not expire Siege before its DPS"), Siege->IsActorBeingDestroyed());
    }
    TestTrue(TEXT("Siege moved at production 900 cm/s"), FMath::IsNearlyEqual(Siege->GetActorLocation().X, 900.f * Elapsed, 0.01f));
    TestTrue(TEXT("Lethal production damage accepted"), UBwayDamageLibrary::ApplyDamageFromSource(Enemy->GetAbilitySystemComponent(), Siege, 300, Enemy, Enemy));
    TestTrue(TEXT("Siege is destroyed when health is depleted"), Siege->IsActorBeingDestroyed());
    TestFalse(TEXT("Destroyed Siege releases registry occupancy"), GameState->BuildableRegistryComponent->GetAllBuildables().Contains(Siege));
    UE_LOG(LogTemp, Display, TEXT("BwayBuildableDamage: Siege elapsed=%.4f enemyDamage=%.4f friendlyDamage=%.4f neutralDamage=%.4f destroyed=%d"),
        Elapsed, EnemyBefore - Health(Enemy), FriendBefore - Health(Friend), NeutralBefore - Health(Neutral), Siege->IsActorBeingDestroyed());
    return true;
}
#endif
