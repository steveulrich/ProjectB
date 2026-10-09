#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
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
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace BwayJailLifecycleTest
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
    ABwayCharacterWithAbilities* SpawnPawn(int32 Team, FVector Location, APlayerController* PC = nullptr)
    {
        auto* PS = World->SpawnActor<ABwayPlayerState>();
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(ABwayCharacterWithAbilities::StaticClass(), Location, FRotator::ZeroRotator, Params);
        if (!PS || !Pawn) return nullptr;
        PS->SetGenericTeamId(FGenericTeamId(Team)); Pawn->SetGenericTeamId(FGenericTeamId(Team)); GS->AddPlayerToTeam(PS, Team - 1);
        if (PC) { PS->SetOwner(PC); PC->SetPlayerState(PS); PC->Possess(Pawn); }
        Pawn->SetPlayerState(PS);
        auto* ASC = PS->GetLyraAbilitySystemComponent(); UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
        if (!ASC->GetSet<ULyraHealthSet>()) ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(PS));
        auto* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
        Extension->SetPawnData(NewObject<ULyraPawnData>(World)); Extension->InitializeAbilitySystem(ASC, PS);
        ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 1000);
        ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 1000);
        return Pawn;
    }
    ABuildableActor* SpawnTrap(float X = 0)
    {
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr,
            TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_BuildableData_Gunslinger.DA_BW_BuildableData_Gunslinger"));
        if (!Data || !Data->BuildableActorClass) return nullptr;
        FActorSpawnParameters Params; Params.Owner = Owner; Params.Instigator = Owner->GetPawn();
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* SpawnedTrap = World->SpawnActor<ABuildableActor>(Data->BuildableActorClass, FVector(X,0,10000), FRotator::ZeroRotator, Params);
        if (!SpawnedTrap) return nullptr;
        SpawnedTrap->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
        UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(SpawnedTrap, Data, 0); SpawnedTrap->InitializeBuildable(Owner, 0);
        return SpawnedTrap;
    }
    void Move(ABwayCharacterWithAbilities* Pawn, FVector Position)
    { Pawn->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics); }
    bool Jailed(ABwayCharacterWithAbilities* Pawn) const
    { return Pawn->GetAbilitySystemComponent()->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Jailed); }
    ABuildableActor* CageFor(ABwayCharacterWithAbilities* Pawn) const
    {
        for (TActorIterator<ABuildableActor> It(World); It; ++It)
            if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->GetAttachParentActor() == Pawn) return *It;
        return nullptr;
    }
    float Health(ABuildableActor* Actor) const
    { return Actor->GetAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()); }
    bool Update() override
    {
        if (Step == 0)
        {
            Instance.Reset(NewObject<UGameInstance>(GEngine)); Instance->InitializeStandalone(); World = Instance->GetWorld();
            if (!Test->TestNotNull(TEXT("Initialized Jail test world"), World)) return true;
            World->InitializeActorsForPlay(FURL()); GS = World->SpawnActor<ABwayGameState>(); World->SetGameState(GS); World->SetBegunPlay(true);
            Owner = World->SpawnActor<APlayerController>(); VictimController = World->SpawnActor<APlayerController>();
            if (!Owner || !VictimController) { Test->AddError(TEXT("Jail fixture controller setup failed")); return true; }
            // This isolated world has no GameMode login or net driver. Initialize
            // the victim as the local authority player before testing LocalPredicted GAS.
            VictimController->SetAsLocalPlayerController();
            OwnerPawn = SpawnPawn(1, FVector(0,-3000,10000), Owner);
            Ally = SpawnPawn(1,FVector(2000,1000,10000)); EnemyA = SpawnPawn(2,FVector(2000,-1000,10000),VictimController);
            EnemyB = SpawnPawn(2,FVector(2000,-1500,10000)); DeadEnemy = SpawnPawn(2,FVector(2000,-2000,10000));
            if (!OwnerPawn || !Ally || !EnemyA || !EnemyB || !DeadEnemy) { Test->AddError(TEXT("Jail fixture pawn setup failed")); return true; }
            OwnerPlayer = OwnerPawn->GetPlayerState<ABwayPlayerState>();
            Trap = SpawnTrap(); if (!Test->TestNotNull(TEXT("Actual Jail Blueprint"), Trap)) return true;
            Test->TestEqual(TEXT("Placed floor trap counts for its player"), GS->BuildableRegistryComponent->GetBuildableCountForPlayer(OwnerPlayer), 1);
            World->GetTimerManager().Tick(1.05f);
            if (OwnerMode > 0) Owner->UnPossess();
            if (OwnerMode == 2) OwnerPawn->Destroy();
            Move(Ally, FVector(0,0,10000));
            const bool FriendlySafe = IsValid(Trap) && !Trap->IsActorBeingDestroyed() && !Jailed(Ally);
            UE_LOG(LogTemp, Display, TEXT("BwayJailLifecycle: Friendly ownerMode=%d trapValid=%d allyJailed=%d registered=%d"),
                OwnerMode, IsValid(Trap) && !Trap->IsActorBeingDestroyed(), Jailed(Ally), GS->BuildableRegistryComponent->GetAllBuildables().Num());
            if (!Test->TestTrue(TEXT("Friendly overlap never consumes trap or jails ally across owner lifetime"), FriendlySafe)) return true;
            Move(Ally,FVector(400,100,10000));
            auto* DeadHealth = ULyraHealthComponent::FindHealthComponent(DeadEnemy);
            DeadEnemy->GetAbilitySystemComponent()->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0);
            DeadHealth->StartDeath(); DeadHealth->FinishDeath(); Move(DeadEnemy,FVector(0,0,10000));
            if (!Test->TestTrue(TEXT("Dead enemy cannot consume trap"), IsValid(Trap) && !Trap->IsActorBeingDestroyed())) return true;
            Move(DeadEnemy,FVector(2000,-2000,10000));
            Move(EnemyB,FVector(400,0,10000));
            if (!Test->TestTrue(TEXT("Outside trigger but inside capture does not trigger alone"), IsValid(Trap) && !Trap->IsActorBeingDestroyed())) return true;
            Move(EnemyA,FVector(0,0,10000));
        }
        else if (Step == 1)
        {
            CageA = CageFor(EnemyA); CageB = CageFor(EnemyB);
            UE_LOG(LogTemp, Display, TEXT("BwayJailLifecycle: Discovery ownerMode=%d first=%s second=%s registered=%d"),
                OwnerMode, *GetNameSafe(CageA), *GetNameSafe(CageB), GS->BuildableRegistryComponent->GetAllBuildables().Num());
            Test->TestTrue(TEXT("Trigger consumes the floor trap"), !IsValid(Trap) || Trap->IsActorBeingDestroyed());
            if (!Test->TestTrue(TEXT("Each nearby living enemy gets its own cage"), CageA && CageB && CageA != CageB)) return true;
            Test->TestEqual(TEXT("Both actual cages tracked in match registry"), GS->BuildableRegistryComponent->GetAllBuildables().Num(), 2);
            Test->TestFalse(TEXT("Friendly inside capture radius is not jailed"), Jailed(Ally));
            Test->TestTrue(TEXT("Both captured enemies receive jail status"), Jailed(EnemyA) && Jailed(EnemyB));
            Test->TestEqual(TEXT("Production cage health"), Health(CageA), 450.f);
            Test->TestEqual(TEXT("Jail stops maximum movement speed"), EnemyA->GetCharacterMovement()->GetMaxSpeed(), 0.f);
            Test->TestEqual(TEXT("Consumed trap and cages do not occupy placer quota"), GS->BuildableRegistryComponent->GetBuildableCountForPlayer(OwnerPlayer), 0);
            Test->TestEqual(TEXT("PlayerState without a visible controller never counts ownerless cages"),
                GS->BuildableRegistryComponent->GetBuildableCountForPlayer(World->SpawnActor<ABwayPlayerState>()), 0);
            Test->TestTrue(TEXT("Cages are non-persistent"), !CageA->ShouldPersistBetweenRounds() && !CageB->ShouldPersistBetweenRounds());
            Before = Health(CageA);
            Test->TestFalse(TEXT("Friendly damage cannot hurt cage"), UBwayDamageLibrary::ApplyDamageToEnemyBuildable(Ally, CageA, 1000, Ally));
            Test->TestEqual(TEXT("Friendly damage preserves cage health"), Health(CageA), Before);
            auto* ASC = Cast<ULyraAbilitySystemComponent>(EnemyA->GetAbilitySystemComponent());
            if (!Test->TestTrue(TEXT("Victim ability owner is locally controlled"), ASC->AbilityActorInfo->IsLocallyControlled())) return true;
            auto* Primary = LoadClass<UGameplayAbility>(nullptr, TEXT("/Script/BreakawayCoreRuntime.BwayGameplayAbility_MeleePrimary"));
            if (!Test->TestNotNull(TEXT("Production melee primary"), Primary)) return true;
            Ability = ASC->GiveAbility(FGameplayAbilitySpec(Primary,1));
            Test->TestTrue(TEXT("Jailed victim can activate actual primary locally"), ASC->TryActivateAbility(Ability, false));
            UE_LOG(LogTemp, Display, TEXT("BwayJailLifecycle: Capture ownerMode=%d cages=%d firstHealth=%.3f jailed=%d allyJailed=%d maxSpeed=%.3f"),
                OwnerMode, GS->BuildableRegistryComponent->GetAllBuildables().Num(), Before, Jailed(EnemyA), Jailed(Ally), EnemyA->GetCharacterMovement()->GetMaxSpeed());
        }
        else if (Step == 2) { World->GetTimerManager().Tick(.6f); }
        else if (Step == 3)
        {
            auto* ASC = Cast<ULyraAbilitySystemComponent>(EnemyA->GetAbilitySystemComponent());
            const auto* Spec = ASC->FindAbilitySpecFromHandle(Ability);
            Test->TestTrue(TEXT("Primary ends while jailed"), Spec && !Spec->IsActive());
            Test->TestTrue(TEXT("Jailed victim primary damages enemy cage"), Health(CageA) < Before);
            Test->TestTrue(TEXT("Enemy lethal damage accepted"), UBwayDamageLibrary::ApplyDamageToEnemyBuildable(EnemyA,CageA,2000,EnemyA));
            Test->TestTrue(TEXT("Destroyed cage releases victim movement and tag"), !Jailed(EnemyA) && EnemyA->GetCharacterMovement()->GetMaxSpeed() > 0);
            Test->TestEqual(TEXT("Other victim cage remains"), GS->BuildableRegistryComponent->GetAllBuildables().Num(),1);
            ULyraHealthComponent::FindHealthComponent(EnemyB)->StartDeath();
            Test->TestTrue(TEXT("Victim death destroys cage and removes jail effect"), !Jailed(EnemyB) && CageFor(EnemyB) == nullptr);
            Test->TestEqual(TEXT("Both cages removed from registry"), GS->BuildableRegistryComponent->GetAllBuildables().Num(),0);
            Move(Ally,FVector(2000,1000,10000));
            auto* RoundTrap = SpawnTrap(2000); if (!Test->TestNotNull(TEXT("Round-reset capture trap"), RoundTrap)) return true;
            Move(EnemyA,FVector(2000,0,10000));
        }
        else if (Step == 4)
        {
            if (!Test->TestTrue(TEXT("Second capture applies jail"), CageFor(EnemyA) && Jailed(EnemyA))) return true;
            auto* Survivor = SpawnTrap(4000); if (!Test->TestNotNull(TEXT("Untriggered persistent trap"), Survivor)) return true;
            GS->GetRoundManagement()->ResetRoundState();
            Test->TestTrue(TEXT("Production round reset releases captured victim"), !Jailed(EnemyA) && CageFor(EnemyA) == nullptr);
            Test->TestTrue(TEXT("Untriggered floor trap survives round cleanup"), IsValid(Survivor) && !Survivor->IsActorBeingDestroyed());
            Test->TestEqual(TEXT("Only persistent trap remains"), GS->BuildableRegistryComponent->GetAllBuildables().Num(),1);
            Test->TestEqual(TEXT("Persistent trap retains its placing player identity"), GS->BuildableRegistryComponent->GetBuildableCountForPlayer(OwnerPlayer),1);
            UE_LOG(LogTemp, Display, TEXT("BwayJailLifecycle: Complete ownerMode=%d released=%d survivor=%d registered=%d"),
                OwnerMode, !Jailed(EnemyA), IsValid(Survivor), GS->BuildableRegistryComponent->GetAllBuildables().Num());
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
    APlayerController* VictimController = nullptr;
    ABwayPlayerState* OwnerPlayer = nullptr;
    ABwayCharacterWithAbilities *OwnerPawn = nullptr, *Ally = nullptr, *EnemyA = nullptr, *EnemyB = nullptr, *DeadEnemy = nullptr;
    ABuildableActor *Trap = nullptr, *CageA = nullptr, *CageB = nullptr;
    FGameplayAbilitySpecHandle Ability;
    float Before = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayJailLiveOwnerTest,"Breakaway.Buildables.JailCapture.LiveOwner",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayJailLiveOwnerTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(BwayJailLifecycleTest::FRun(this,0)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayJailUnpossessedOwnerTest,"Breakaway.Buildables.JailCapture.UnpossessedOwner",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayJailUnpossessedOwnerTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(BwayJailLifecycleTest::FRun(this,1)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayJailDestroyedOwnerTest,"Breakaway.Buildables.JailCapture.DestroyedOwner",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayJailDestroyedOwnerTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(BwayJailLifecycleTest::FRun(this,2)); return true; }
#endif
