#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "Development/BwaySlideReviewCourse.h"

namespace BwaySlideContactTest
{
constexpr float Duration = 0.2f;
constexpr float InitialForwardSpeed = 620.f;
constexpr float InitialDownwardSpeed = -334.f;
constexpr float InitialSideSpeed = 300.f;
enum class EScene { FlatFloor, TrueWall, FloorThenGlancingWall, Corner };

// The incoming floor-contact velocity reproduces the ramp-to-flat state seen in
// support-diagnostic01. Only the initial state is seeded; every subsequent step
// uses production physics, real sweeps and the humanoid asset's authored tuning.
class FFixture
{
public:
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture() { if (World) World->DestroyWorld(false); }

	bool Setup(EScene Scene)
	{
		const bool bWithWall = Scene != EScene::FlatFloor;
		UClass* PawnClass = LoadClass<ABwayCharacterWithAbilities>(nullptr,
			TEXT("/BreakawayCore/Characters/B_BW_Character_Humanoid.B_BW_Character_Humanoid_C"));
		if (!Test.TestNotNull(TEXT("Authored humanoid class"), PawnClass)) return false;
		const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test.TestNotNull(TEXT("Contact fixture world"), World)) return false;
		Floor = AddBox(FVector(5000.f, 5000.f, 50.f), FVector(0.f, 0.f, -50.f));
		if (!Test.TestNotNull(TEXT("Real flat collision floor"), Floor)) return false;
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(PawnClass, FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator, Spawn);
		if (!Test.TestNotNull(TEXT("Production authored Bway pawn"), Pawn)) return false;
		Movement = Pawn->GetBwayCharacterMovement();
		if (!Test.TestNotNull(TEXT("Production Bway movement"), Movement)
			|| !Test.TestTrue(TEXT("Valid authority movement data"), Pawn->HasAuthority() && Movement->HasValidData())) return false;
		// Controller absence is fixture plumbing, not a slide-tuning override.
		Movement->bRunPhysicsWithNoController = true;
		Movement->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Slide));
		HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
		Pawn->SetActorLocation(FVector(0.f, 0.f, HalfHeight + 0.25f), false, nullptr, ETeleportType::TeleportPhysics);
		Movement->Velocity = FVector(bWithWall ? InitialSideSpeed : 0.f, InitialForwardSpeed,
			Scene == EScene::TrueWall ? 0.f : InitialDownwardSpeed);
		if (!Test.TestTrue(TEXT("Authored friction and wall braking are positive"),
			Movement->SlideBaseFrictionFactor > 0.f && Movement->SlideWallHitFriction > 0.f)
			|| !Test.TestTrue(TEXT("Authored zero-threshold held slide"), Movement->MinSlideSpeed == 0.f)
			|| !Test.TestTrue(TEXT("Seeded velocity is below the authored slide cap"), Movement->Velocity.Size() < Movement->GetMaxSpeed())) return false;
		FFindFloorResult FloorResult;
		Movement->FindFloor(Pawn->GetActorLocation(), FloorResult, false);
		if (!Test.TestTrue(TEXT("Actual floor sweep resolves the fixture plane"),
			FloorResult.bBlockingHit && FloorResult.IsWalkableFloor() && FloorResult.HitResult.GetComponent() == Floor
			&& FloorResult.HitResult.ImpactNormal.Z > 0.999f && !FloorResult.HitResult.bStartPenetrating)) return false;
		if (bWithWall)
		{
			WallFrontY = Radius + (Scene == EScene::TrueWall ? 0.25f : 4.f);
			Wall = AddBox(FVector(5000.f, 50.f, 5000.f), FVector(0.f, WallFrontY + 50.f, 0.f));
			if (!Test.TestNotNull(TEXT("Real opposing wall"), Wall)) return false;
			FHitResult Hit;
			const FCollisionQueryParams Query(SCENE_QUERY_STAT(BwaySlideContactWall), false, Pawn);
			const FCollisionResponseParams Response(Pawn->GetCapsuleComponent()->GetCollisionResponseToChannels());
			World->SweepSingleByChannel(Hit, Pawn->GetActorLocation(), Pawn->GetActorLocation() + FVector(0.f, 20.f, 0.f),
				FQuat::Identity, Pawn->GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Radius, HalfHeight), Query, Response);
			if (!Test.TestTrue(TEXT("Horizontal capsule sweep meets a genuine opposing wall"),
				Hit.IsValidBlockingHit() && Hit.GetComponent() == Wall && Hit.Normal.Y < -0.999f
				&& FMath::Abs(Hit.Normal.Z) < 0.001f)) return false;
			if (Scene != EScene::TrueWall)
			{
				World->SweepSingleByChannel(Hit, Pawn->GetActorLocation(), Pawn->GetActorLocation() + Movement->Velocity.GetSafeNormal() * 20.f,
					FQuat::Identity, Pawn->GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Radius, HalfHeight), Query, Response);
				if (!Test.TestTrue(TEXT("Incoming capsule sweep hits floor before the farther wall"),
					Hit.IsValidBlockingHit() && Hit.GetComponent() == Floor && Hit.ImpactNormal.Z > 0.999f)) return false;
			}
			if (Scene == EScene::Corner)
			{
				WallFrontX = Radius + 6.f;
				if (!Test.TestNotNull(TEXT("Perpendicular corner wall"),
					AddBox(FVector(50.f, 5000.f, 5000.f), FVector(WallFrontX + 50.f, 0.f, 0.f)))) return false;
			}
		}
		return true;
	}

	void UseNegligibleWallDamping() { Movement->SlideWallHitFriction = 1.e-5f; }
	float GetSideSpeed() const { return Movement->Velocity.X; }

	bool Simulate(int32 Hertz, EScene Scene, bool bNegligibleWallDamping = false)
	{
		const bool bWithWall = Scene != EScene::FlatFloor;
		const float Step = 1.f / Hertz;
		const int32 Steps = FMath::RoundToInt(Duration * Hertz);
		// On a horizontal floor there is no slope acceleration or steering. The
		// independent tangential expectation is the authored flat friction only;
		// floor contact must not trigger the stronger frontal-wall braking path.
		const float ExpectedFreeSpeed = InitialForwardSpeed * FMath::Pow(
			1.f - FMath::Clamp(Movement->SlideBaseFrictionFactor * Step, 0.f, 1.f), Steps);
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			Movement->StartNewPhysics(Step, 0);
			if (!Test.TestTrue(TEXT("Production slide stays on the real nonpenetrating floor"),
				Movement->IsSliding() && Movement->CurrentFloor.bBlockingHit
				&& Movement->CurrentFloor.HitResult.GetComponent() == Floor
				&& !Movement->CurrentFloor.HitResult.bStartPenetrating
				&& Pawn->GetActorLocation().Z >= HalfHeight - 0.1f)) return false;
			if (bWithWall && !Test.TestTrue(TEXT("Capsule does not pass through the wall"),
				Pawn->GetActorLocation().Y + Radius <= WallFrontY + 0.1f)) return false;
			if (Scene == EScene::Corner && !Test.TestTrue(TEXT("Capsule does not pass through the corner wall"),
				Pawn->GetActorLocation().X + Radius <= WallFrontX + 0.1f)) return false;
		}
		Test.AddInfo(FString::Printf(TEXT("SlideContact: scene=%d hz=%d seconds=%.3f flatFriction=%.3f wallFriction=%.3f expectedFreeY=%.6f actualVelocity=%s position=%s floorDistance=%.6f"),
			int32(Scene), Hertz, Duration, Movement->SlideBaseFrictionFactor, Movement->SlideWallHitFriction,
			ExpectedFreeSpeed, *Movement->Velocity.ToString(), *Pawn->GetActorLocation().ToString(), Movement->CurrentFloor.GetDistanceToFloor()));
		if (Scene == EScene::Corner)
		{
			Test.TestTrue(TEXT("Both corner planes stop stored horizontal motion"), Movement->Velocity.Size2D() <= 1.f);
			Test.TestTrue(TEXT("Corner floor contact clears inward vertical motion"), Movement->Velocity.Z >= -1.f);
		}
		else if (bWithWall)
		{
			Test.TestTrue(TEXT("True opposing wall reduces forward speed beyond surface friction"),
				FMath::Abs(Movement->Velocity.Y) < ExpectedFreeSpeed - 1.f);
			// X is unobstructed. Losing only Y to geometric blocking cannot satisfy
			// this assertion, including when the first sweep hit was the floor.
			const float ExpectedFreeSideSpeed = ExpectedFreeSpeed * InitialSideSpeed / InitialForwardSpeed;
			if (bNegligibleWallDamping)
				Test.TestTrue(TEXT("Collision-identical negligible-wall-damping control retains the free tangent"),
					FMath::IsNearlyEqual(Movement->Velocity.X, ExpectedFreeSideSpeed, 0.5f));
			else
				Test.TestTrue(TEXT("Actual wall impact also applies authored damping to the free tangent"),
					Movement->Velocity.X >= -0.1f && Movement->Velocity.X < ExpectedFreeSideSpeed - 1.f);
			Test.TestTrue(TEXT("Glancing wall retains free tangential motion"), Pawn->GetActorLocation().X > 1.f);
		}
		else
		{
			Test.TestTrue(TEXT("Floor contact preserves tangential speed after authored surface friction"),
				FMath::IsNearlyEqual(Movement->Velocity.Y, ExpectedFreeSpeed, 0.5f));
			Test.TestTrue(TEXT("Supported flat contact does not retain velocity into the floor"),
				Movement->Velocity.Z >= -1.f);
		}
		return true;
	}

private:
	UBoxComponent* AddBox(const FVector& Extent, const FVector& Location)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor) return nullptr;
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetBoxExtent(Extent);
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Box;
	}

	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	UBwayCharacterMovementComponent* Movement = nullptr;
	UBoxComponent* Floor = nullptr;
	UBoxComponent* Wall = nullptr;
	float HalfHeight = 0.f, Radius = 0.f, WallFrontY = 0.f, WallFrontX = 0.f;
};

bool Run(FAutomationTestBase& Test, EScene Scene)
{
	for (const int32 Hertz : {30, 60, 120})
	{
		FFixture Fixture(Test);
		if (!Fixture.Setup(Scene) || !Fixture.Simulate(Hertz, Scene)) return false;
		if (Scene == EScene::TrueWall || Scene == EScene::FloorThenGlancingWall)
		{
			// The main fixture retains authored tuning. This separate control changes
			// only wall damping, proving that geometry alone cannot pass the test.
			FFixture Control(Test);
			if (!Control.Setup(Scene)) return false;
			Control.UseNegligibleWallDamping();
			if (!Control.Simulate(Hertz, Scene, true)) return false;
			Test.TestTrue(TEXT("Authored wall damping slows more than collision-identical control"),
				Fixture.GetSideSpeed() < Control.GetSideSpeed() - 1.f);
		}
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideFlatContactTest, "Breakaway.CombatPolish.Slide.Physics.Contact.FlatFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideFlatContactTest::RunTest(const FString&)
{
	return BwaySlideContactTest::Run(*this, BwaySlideContactTest::EScene::FlatFloor);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideTrueWallContactTest, "Breakaway.CombatPolish.Slide.Physics.Contact.TrueWall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideTrueWallContactTest::RunTest(const FString&)
{
	return BwaySlideContactTest::Run(*this, BwaySlideContactTest::EScene::TrueWall);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideSecondaryWallContactTest, "Breakaway.CombatPolish.Slide.Physics.Contact.FloorThenGlancingWall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideSecondaryWallContactTest::RunTest(const FString&)
{
	return BwaySlideContactTest::Run(*this, BwaySlideContactTest::EScene::FloorThenGlancingWall);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideCornerContactTest, "Breakaway.CombatPolish.Slide.Physics.Contact.Corner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideCornerContactTest::RunTest(const FString&)
{
	return BwaySlideContactTest::Run(*this, BwaySlideContactTest::EScene::Corner);
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideConcaveTimeBudgetTest,
	"Breakaway.CombatPolish.Slide.Physics.Contact.ConcaveTimeBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideConcaveTimeBudgetTest::RunTest(const FString&)
{
	const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Concave fixture world"), World)) return false;
	TArray<TWeakObjectPtr<AActor>> Course;
	const auto Finish = [World, &Course]() { BwaySlideReviewCourse::Destroy(Course); World->DestroyWorld(false); };
	UClass* PawnClass = LoadClass<ABwayCharacterWithAbilities>(nullptr,
		TEXT("/BreakawayCore/Characters/B_BW_Character_Humanoid.B_BW_Character_Humanoid_C"));
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Pawn = PawnClass ? World->SpawnActor<ABwayCharacterWithAbilities>(PawnClass, FVector(0, 0, 8000), FRotator::ZeroRotator, Spawn) : nullptr;
	if (!TestNotNull(TEXT("Authored production pawn"), Pawn)) { Finish(); return false; }
	if (!TestTrue(TEXT("Shared real collision course"), BwaySlideReviewCourse::Spawn(World, Course, Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius())))
	{ Finish(); return false; }
	auto* Move = Pawn->GetBwayCharacterMovement();
	Move->bRunPhysicsWithNoController = true;
	int32 Branches = 0, Decisive = 0;
	const FDelegateHandle Observer = Move->SlideReviewObserver.AddLambda([this, &Branches, &Decisive](const FBwaySlideReviewObservation& O)
	{
		if (O.bJump) return;
		++Branches;
		const float R = 1.f - O.FirstHitFraction;
		const float Budget = O.FirstHitFraction + R * O.SlideConsumedFraction + O.ContinuationFraction;
		const float LegacyBudget = O.FirstHitFraction + R * O.SlideConsumedFraction + R * (1.f - O.FinalHitFraction);
		TestTrue(TEXT("Actual concave continuation stays inside one simulation step"), FMath::IsFinite(Budget) && Budget <= 1.0001f && O.ContinuationFraction >= 0.f);
		TestTrue(TEXT("Independent planar displacement bound"), FVector::Dist2D(O.Start, O.End) <= O.BeforeVelocity.Size() * O.DeltaSeconds + 1.f);
		if (LegacyBudget > 1.001f && O.SlideConsumedFraction > O.FinalHitFraction + 0.001f) ++Decisive;
		AddInfo(FString::Printf(TEXT("SlideConcaveContact: dt=%.7f firstHit=%.7f consumed=%.7f finalHit=%.7f continuation=%.7f budget=%.7f legacyBudget=%.7f firstNormal=%s finalNormal=%s start=%s end=%s beforeVelocity=%s afterVelocity=%s"),
			O.DeltaSeconds, O.FirstHitFraction, O.SlideConsumedFraction, O.FinalHitFraction, O.ContinuationFraction, Budget, LegacyBudget,
			*O.FirstNormal.ToString(), *O.FinalNormal.ToString(), *O.Start.ToString(), *O.End.ToString(), *O.BeforeVelocity.ToString(), *O.AfterVelocity.ToString()));
	});
	for (int32 Hertz : {30, 60, 120})
	{
		Branches = 0; Decisive = 0;
		Move->StopMovementImmediately(); Move->SetMovementMode(MOVE_Custom, uint8(ECustomMovementMode::CMOVE_Slide));
		const FVector InitialPosition = BwaySlideReviewCourse::ConcaveStart(Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Pawn->SetActorLocation(InitialPosition, false, nullptr, ETeleportType::TeleportPhysics);
		Move->FindFloor(InitialPosition, Move->CurrentFloor, false);
		if (!TestTrue(TEXT("Concave initial state is supported by the real flat face"), Move->CurrentFloor.IsWalkableFloor()
			&& !Move->CurrentFloor.HitResult.bStartPenetrating && Move->CurrentFloor.HitResult.ImpactNormal.Z > 0.999f))
		{ Move->SlideReviewObserver.Remove(Observer); Finish(); return false; }
		Move->Velocity = BwaySlideReviewCourse::ConcaveVelocity();
		AddInfo(FString::Printf(TEXT("SlideConcaveSeed: hz=%d radius=%.6f halfHeight=%.6f initial=%s velocity=%s floorDistance=%.6f floorNormal=%s friction=%.6f gravity=%.6f speedCap=%.6f"),
			Hertz, Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
			*InitialPosition.ToString(), *Move->Velocity.ToString(), Move->CurrentFloor.GetDistanceToFloor(), *Move->CurrentFloor.HitResult.ImpactNormal.ToString(),
			Move->SlideBaseFrictionFactor, Move->GetGravityZ(), Move->GetMaxSpeed()));
		// One production step with actual capsule sweeps; no fabricated hit or
		// repeated phase search can stand in for the required three-contact path.
		Move->PhysCustom(1.f / Hertz, 0);
		AddInfo(FString::Printf(TEXT("SlideConcaveEnd: hz=%d position=%s velocity=%s floorDistance=%.6f floorNormal=%s"),
			Hertz, *Pawn->GetActorLocation().ToString(), *Move->Velocity.ToString(), Move->CurrentFloor.GetDistanceToFloor(), *Move->CurrentFloor.HitResult.ImpactNormal.ToString()));
		AddInfo(FString::Printf(TEXT("SlideConcave: hz=%d realBranches=%d legacyOverrunControls=%d"), Hertz, Branches, Decisive));
		TestTrue(FString::Printf(TEXT("%d Hz reaches actual concave branch"), Hertz), Branches > 0);
		TestTrue(FString::Printf(TEXT("%d Hz rejects old last-hit timing on real contacts"), Hertz), Decisive > 0);
	}
	Move->SlideReviewObserver.Remove(Observer);
	Finish();
	return true; // The framework retains every failed TestTrue assertion above.
}
#endif
