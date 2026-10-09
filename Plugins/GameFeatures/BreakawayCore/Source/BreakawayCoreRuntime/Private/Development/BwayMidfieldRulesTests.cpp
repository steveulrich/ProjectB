#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "BwayGoalVolume.h"
#include "Engine/World.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
#include "Relic/RelicActor.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayMidfieldGeometryTest,
	"Breakaway.MatchRules.MidfieldGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayMidfieldGeometryTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Goal0 = World->SpawnActor<ABwayGoalVolume>(ABwayGoalVolume::StaticClass(), FTransform::Identity, Spawn);
	auto* Goal1 = World->SpawnActor<ABwayGoalVolume>(ABwayGoalVolume::StaticClass(), FTransform::Identity, Spawn);
	auto* Relic = World->SpawnActor<ARelicActor>(ARelicActor::StaticClass(), FTransform::Identity, Spawn);
	if (!TestNotNull(TEXT("Goal0"), Goal0) || !TestNotNull(TEXT("Goal1"), Goal1) || !TestNotNull(TEXT("Relic"), Relic)) return false;
	Goal0->SetOwningTeam(0);
	Goal1->SetOwningTeam(1);
	for (float Yaw : {90.0f, 35.0f, 180.0f})
	{
		const FVector Center(1300.0f, -750.0f, 230.0f);
		const FVector Axis = FRotator(0.0f, Yaw, 0.0f).Vector();
		const FVector Side = FVector::CrossProduct(Axis, FVector::UpVector);
		Goal0->SetActorLocation(Center - Axis * 4000.0f);
		Goal1->SetActorLocation(Center + Axis * 4000.0f);
		FVector ResolvedCenter, ResolvedAxis;
		TestTrue(TEXT("Distinct goals define a field"), UBwayMidfieldRulesLibrary::TryGetMidfieldFrame(World, ResolvedCenter, ResolvedAxis));
		TestTrue(TEXT("Translated center"), ResolvedCenter.Equals(Center));
		TestTrue(TEXT("Rotated team direction"), ResolvedAxis.Equals(Axis, 0.001f));
		TestEqual(TEXT("Lateral displacement cannot change team 0 half"), UBwayMidfieldRulesLibrary::GetRelicHalfAtWorldLocation(World, Center - Axis * 1000.0f + Side * 5000.0f), 0);
		TestEqual(TEXT("Lateral displacement cannot change team 1 half"), UBwayMidfieldRulesLibrary::GetRelicHalfAtWorldLocation(World, Center + Axis * 1000.0f - Side * 5000.0f), 1);
		TestEqual(TEXT("Height cannot change territory"), UBwayMidfieldRulesLibrary::GetRelicHalfAtWorldLocation(World, Center + Axis * 1000.0f + FVector(0,0,9000)), 1);
		TestEqual(TEXT("Tolerance leaves midfield undecided"), UBwayMidfieldRulesLibrary::GetRelicHalfAtWorldLocation(World, Center + Axis * 0.5f), -1);
		Relic->SetActorLocation(Center + Side * 500.0f);
		Relic->LastPossessingTeam = -1;
		TestEqual(TEXT("Neutral unpossessed relic stays undecided"), UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(Relic), -1);
		Relic->LastPossessingTeam = 0;
		TestEqual(TEXT("Midline retains existing last-possession tiebreak"), UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(Relic), 0);
		Relic->SetActorLocation(Center + Axis * 1000.0f - Side * 5000.0f);
		TestEqual(TEXT("Physical half takes precedence over last possessor"), UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(Relic), 1);
	}
	Goal1->SetActorLocation(Goal0->GetActorLocation());
	TestEqual(TEXT("Coincident goals cannot award a territory win"), UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(Relic), -1);
	Goal1->SetActorLocation(Goal0->GetActorLocation() + FVector(0,8000,0));
	auto* Duplicate = World->SpawnActor<ABwayGoalVolume>(ABwayGoalVolume::StaticClass(), FTransform::Identity, Spawn);
	if (!TestNotNull(TEXT("Duplicate goal"), Duplicate)) return false;
	Duplicate->SetOwningTeam(0);
	TestEqual(TEXT("Ambiguous goal ownership cannot award a territory win"), UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(Relic), -1);
	Duplicate->Destroy();
	Goal1->Destroy();
	TestEqual(TEXT("Missing goal cannot award a territory win"), UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(Relic), -1);
	return true;
}
#endif
