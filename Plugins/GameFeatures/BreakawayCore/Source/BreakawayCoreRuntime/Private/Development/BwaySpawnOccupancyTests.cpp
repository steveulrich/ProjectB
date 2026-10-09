#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/StrongObjectPtr.h"
#include "BwaySpawnTestGeometry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySpawnOccupancyTest, "Breakaway.Spawn.OccupiedTeamStart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySpawnOccupancyTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(); UWorld* World=Instance->GetWorld();
    if (!TestNotNull(TEXT("Spawn test world"),World)) return false;
    World->InitializeActorsForPlay(FURL());
    auto* GS=World->SpawnActor<ABwayGameState>(); World->SetGameState(GS);
    auto* Mode=World->SpawnActor<ABreakawayGameMode>(); Mode->GameState=GS; Mode->DefaultPawnClass=ACharacter::StaticClass();
    auto* Controller=World->SpawnActor<APlayerController>(); auto* PS=World->SpawnActor<ABwayPlayerState>();
    Controller->SetPlayerState(PS); PS->SetOwner(Controller); GS->AddPlayerToTeam(PS,0);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Start=[&](FVector Location,FName Team)
    { auto* S=World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(),Location,FRotator::ZeroRotator,Params); S->PlayerStartTag=Team; return S; };
    auto* Occupied=Start(FVector(0,0,5000),TEXT("Team1Spawn"));
    auto* Clear=Start(FVector(2000,0,5000),TEXT("Team1Spawn"));
    auto* Opponent=Start(FVector(4000,0,5000),TEXT("Team2Spawn"));
    auto* Blocker=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),Occupied->GetActorLocation(),FRotator::ZeroRotator,Params);
    TestTrue(TEXT("Fixture actually blocks the occupied team start"),World->EncroachingBlockingGeometry(GetDefault<ACharacter>(),Occupied->GetActorLocation(),FRotator::ZeroRotator));
    for(int32 I=0;I<32;++I) TestTrue(TEXT("Clear teammate start wins over occupied or enemy starts"),Mode->ChoosePlayerStart_Implementation(Controller)==Clear);
    Blocker->SetActorLocation(Clear->GetActorLocation(),false,nullptr,ETeleportType::TeleportPhysics);
    for(int32 I=0;I<32;++I) TestTrue(TEXT("Occupancy is recomputed for the next player"),Mode->ChoosePlayerStart_Implementation(Controller)==Occupied);
    GS->RemovePlayerFromTeam(PS); GS->AddPlayerToTeam(PS,1);
    TestTrue(TEXT("Selection remains within the assigned team"),Mode->ChoosePlayerStart_Implementation(Controller)==Opponent);
    Opponent->SetActorLocation(FVector(20000,0,5000));
    auto* ClosedObstruction=BwaySpawnTestGeometry::SpawnObstruction(World,Opponent->GetActorLocation());
    FVector Adjusted=Opponent->GetActorLocation();
    TestTrue(TEXT("Closed obstruction overlaps the real spawn capsule"),World->EncroachingBlockingGeometry(GetDefault<ACharacter>(),Adjusted,FRotator::ZeroRotator));
    TestFalse(TEXT("Closed obstruction also rejects engine teleport adjustments"),World->FindTeleportSpot(GetDefault<ACharacter>(),Adjusted,FRotator::ZeroRotator));
    TestNull(TEXT("All team starts blocked never chooses an opposing start"),Mode->ChoosePlayerStart_Implementation(Controller));
    ClosedObstruction->Destroy();
    TestTrue(TEXT("Clearing the obstruction immediately restores the same team start"),Mode->ChoosePlayerStart_Implementation(Controller)==Opponent);
    World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
