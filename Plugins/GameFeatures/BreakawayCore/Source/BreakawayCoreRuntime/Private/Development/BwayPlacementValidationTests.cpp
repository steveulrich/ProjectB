#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Buildable/BwayPlacementValidation.h"
#include "Buildable/BuildableBase.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayPlacementGeometryTest, "Breakaway.Buildables.AuthorityGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayPlacementGeometryTest::RunTest(const FString&)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Geometry world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto MakeBox = [&](FVector Center, FVector Extent, FRotator Rotation = FRotator::ZeroRotator)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetBoxExtent(Extent);
		Box->RegisterComponent();
		Actor->SetActorLocationAndRotation(Center, Rotation);
		return Box;
	};
	AActor* Source = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Source);
	Source->SetRootComponent(Root); Root->RegisterComponent();
	Source->SetActorLocation(FVector(0, 0, 120));
	UBoxComponent* Floor = MakeBox(FVector(1000, 0, -50), FVector(5000, 5000, 50));
	const FVector GroundPoint(600, 0, 0);
	auto TraceGround = [&](FVector Point)
	{
		FHitResult Hit;
		World->LineTraceSingleByChannel(Hit, Point + FVector(0,0,200), Point - FVector(0,0,200), ECC_Visibility);
		return Hit;
	};
	const TCHAR* DataPaths[] = {
		TEXT("/Hero_Spartacus/Characters/Heroes/Argus/Buildables/DA_BW_BuildableData_SiegeEngine.DA_BW_BuildableData_SiegeEngine"),
		TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_BuildableData_Gunslinger.DA_BW_BuildableData_Gunslinger"),
		TEXT("/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine.DA_BW_BuildableData_SunShrine"),
		TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_BuildableData_Hexweaver.DA_BW_BuildableData_Hexweaver")};
	int32 DefinitionsChecked = 0;
	for (const TCHAR* Path : DataPaths)
	{
		const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr, Path);
		if (!TestNotNull(Path, Data)) continue;
		FBwayPlacementQuery Query;
		Query.World = World; Query.Source = Source; Query.PlacedClass = Data->BuildableActorClass;
		Query.AuthoredBounds = Data->GetPlacementLocalBounds();
		TArray<FBox> Geometry;
		FBox Footprint;
		if (!TestTrue(FString::Printf(TEXT("%s has usable authored geometry"), *Data->GetName()), Query.GetLocalGeometry(Geometry, Footprint))) continue;
		const FHitResult Ground = TraceGround(GroundPoint);
		if (!TestTrue(TEXT("Fixture has actual blocking ground"), Ground.bBlockingHit)) continue;
		const auto Preview = Query.ValidateGroundHit(Ground);
		const auto Authority = Query.ValidateSubmittedHit(Ground);
		TestTrue(TEXT("Authored buildable legal on flat floor in preview"), Preview.bIsValid);
		TestTrue(TEXT("Authored buildable legal on authority"), Authority.bIsValid);
		TestTrue(TEXT("Server uses same pivot exactly once"), Preview.AdjustedLocation.Equals(Authority.AdjustedLocation, 0.001));
		TestTrue(TEXT("Server uses same orientation"), Preview.PlacementRotation.Equals(Authority.PlacementRotation, 0.001));
		ABuildableActor* Existing = World->SpawnActor<ABuildableActor>();
		Existing->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
		Existing->SetActorLocation(Preview.AdjustedLocation);
		TestFalse(TEXT("Buildable occupies its placement volume"), Query.ValidateSubmittedHit(Ground).bIsValid);
		const FHitResult StructureTop = TraceGround(GroundPoint);
		TestTrue(TEXT("Weapon-query body provides an actual top surface"), StructureTop.GetActor() == Existing);
		TestFalse(TEXT("Buildable top cannot authorize a stacked preview"), Query.ValidateGroundHit(StructureTop).bIsValid);
		TestFalse(TEXT("Buildable top cannot authorize stacked server placement"), Query.ValidateSubmittedHit(StructureTop).bIsValid);
		Existing->SetActorLocationAndRotation(Preview.AdjustedLocation + FVector(110,110,0), FRotator(0,45,0));
		TestTrue(TEXT("Separated rotated volumes remain legal despite overlapping world AABBs"), Query.ValidateSubmittedHit(Ground).bIsValid);
		Existing->Destroy();
		UE_LOG(LogTemp, Display, TEXT("BwayPlacementGeometry: Asset name=%s bounds=%d footprint=%s preview=%d authority=%d pivotZ=%.3f"),
			*Data->GetName(), Geometry.Num(), *Footprint.GetExtent().ToCompactString(), Preview.bIsValid, Authority.bIsValid, Authority.AdjustedLocation.Z);
		FHitResult Bad = Ground;
		Bad.Location.X = Bad.ImpactPoint.X = 3000;
		TestFalse(TEXT("Forged out-of-range target rejected"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.Location.Z = Bad.ImpactPoint.Z = 500;
		TestFalse(TEXT("Floating blocking flag cannot authorize placement"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.ImpactNormal = FVector(1,0,0);
		TestFalse(TEXT("Client normal cannot replace traced surface normal"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.ImpactNormal = FVector::ZeroVector;
		TestFalse(TEXT("Zero normal rejected"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.Location.X = std::numeric_limits<double>::quiet_NaN();
		TestFalse(TEXT("NaN location rejected before query"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.ImpactPoint.Y = std::numeric_limits<double>::infinity();
		TestFalse(TEXT("Infinite impact point rejected before query"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.ImpactPoint.X += 30;
		TestFalse(TEXT("Conflicting hit locations rejected"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.bBlockingHit = false;
		TestFalse(TEXT("Nonblocking submission rejected"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.bStartPenetrating = true;
		TestFalse(TEXT("Penetrating submission rejected"), Query.ValidateSubmittedHit(Bad).bIsValid);
		Bad = Ground; Bad.Location = Bad.ImpactPoint = Preview.AdjustedLocation;
		if (!Preview.AdjustedLocation.Equals(GroundPoint, 3.0))
			TestFalse(TEXT("Already lifted transform is not accepted as ground"), Query.ValidateSubmittedHit(Bad).bIsValid);

		Floor->GetOwner()->SetActorLocation(FVector(600,0,-50));
		Floor->SetBoxExtent(FVector(1,1,50), true);
		TestFalse(TEXT("Supported center but unsupported footprint rejected"), Query.ValidateSubmittedHit(Ground).bIsValid);
		Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Forged ground reference cannot replace missing world geometry"), Query.ValidateSubmittedHit(Ground).bIsValid);
		Floor->SetBoxExtent(FVector(5000,5000,50), true);
		Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Floor->GetOwner()->SetActorLocation(FVector(1000,0,-50));
		UBoxComponent* Wall = MakeBox(FVector(300,0,100), FVector(25,250,100));
		TestFalse(TEXT("Blocked avatar-to-ground intent rejected"), Query.ValidateSubmittedHit(Ground).bIsValid);
		Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UBoxComponent* Obstacle = MakeBox(Preview.AdjustedLocation + FVector(Footprint.GetExtent().X * .5,0,30), FVector(10,10,30));
		TestFalse(TEXT("Buildable footprint or volume obstruction rejected"), Query.ValidateSubmittedHit(Ground).bIsValid);
		Obstacle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UBoxComponent* Slope = MakeBox(FVector(600,0,-70.710678), FVector(1000,1000,50), FRotator(45,0,0));
		FHitResult SlopedHit = TraceGround(GroundPoint);
		TestTrue(TEXT("Steep surface trace exists"), SlopedHit.bBlockingHit);
		TestFalse(TEXT("Preview rejects excessive slope"), Query.ValidateGroundHit(SlopedHit).bIsValid);
		TestFalse(TEXT("Server rejects excessive slope"), Query.ValidateSubmittedHit(SlopedHit).bIsValid);
		SlopedHit.ImpactNormal = FVector::UpVector;
		TestFalse(TEXT("Forged upright normal cannot make steep ground legal"), Query.ValidateSubmittedHit(SlopedHit).bIsValid);
		Slope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Floor->GetOwner()->SetActorLocation(FVector(1000,0,-3050));
		const FHitResult Downhill = TraceGround(FVector(600,0,-3000));
		TestTrue(TEXT("Existing vertical ground-drop targeting preserved"), Query.ValidateSubmittedHit(Downhill).bIsValid);
		Floor->GetOwner()->SetActorLocation(FVector(1000,0,-50));
		TestTrue(TEXT("Legal placement restored after obstruction cleanup"), Query.ValidateSubmittedHit(Ground).bIsValid);
		++DefinitionsChecked;
	}
	TestEqual(TEXT("Every current buildable definition exercised"), DefinitionsChecked, 4);
	return true;
}
#endif
