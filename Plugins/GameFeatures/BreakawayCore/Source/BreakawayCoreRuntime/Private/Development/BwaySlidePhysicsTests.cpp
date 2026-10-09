#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace BwaySlidePhysicsTest
{
constexpr float Duration = 0.5f;
constexpr float NegligibleWallFriction = 1.e-5f;

struct FResult
{
	FVector Displacement = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Gravity = 0.f;
	float DownhillDistance = 0.f;
	float DownhillSpeed = 0.f;
};

// Real authority pawn, collision scene, floor queries and production physics.
// Only fixture instances disable base friction/extra slope acceleration and
// reduce impact damping to a bounded negligible amount. Production is untouched.
class FFixture
{
public:
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture() { if (World) World->DestroyWorld(false); }

	bool Setup(float PitchDegrees, float GravityScale)
	{
		const UWorld::InitializationValues Settings = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test.TestNotNull(TEXT("Physics fixture world"), World)) return false;
		AActor* FloorActor = World->SpawnActor<AActor>();
		if (!Test.TestNotNull(TEXT("Collision floor actor"), FloorActor)) return false;
		Floor = NewObject<UBoxComponent>(FloorActor);
		FloorActor->SetRootComponent(Floor);
		Floor->SetCollisionObjectType(ECC_WorldStatic);
		Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Floor->SetCollisionResponseToAllChannels(ECR_Block);
		Floor->SetBoxExtent(FVector(5000.f, 5000.f, 50.f));
		Floor->RegisterComponent();
		const FRotator Rotation(PitchDegrees, 0.f, 0.f);
		FloorNormal = Rotation.RotateVector(FVector::UpVector);
		FloorActor->SetActorLocationAndRotation(-FloorNormal * 50.f, Rotation);

		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator, Spawn);
		if (!Test.TestNotNull(TEXT("Production Bway character"), Pawn)) return false;
		Movement = Pawn->GetBwayCharacterMovement();
		if (!Test.TestNotNull(TEXT("Production Bway CMC"), Movement)
			|| !Test.TestTrue(TEXT("Movement has valid authority data"), Pawn->HasAuthority() && Movement->HasValidData())) return false;
		Movement->SlideBaseFrictionFactor = 0.f;
		// A positive epsilon isolates gravity from any genuine wall damping while
		// retaining real collision. VInterpTo interprets zero speed as an immediate
		// snap; epsilon bounds damping error below 0.003 cm/s at normal gravity.
		Movement->SlideWallHitFriction = NegligibleWallFriction;
		Movement->SlideSlopeAccelerationFactor = 0.f;
		Movement->MinSlideSpeed = 0.f;
		Movement->GravityScale = GravityScale;
		Movement->bRunPhysicsWithNoController = true;
		Movement->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Slide));
		Movement->Velocity = FVector::ZeroVector;

		// Place the actual upright capsule just above the real plane, accounting
		// for its hemispheres on the ramp. No cached/fabricated floor result.
		const auto* Capsule = Pawn->GetCapsuleComponent();
		CapsuleSupport = Capsule->GetScaledCapsuleRadius()
			+ (Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius()) * FloorNormal.Z;
		const FVector Start(0.f, 0.f, (CapsuleSupport + 0.25f) / FloorNormal.Z);
		Pawn->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
		StartLocation = Pawn->GetActorLocation();
		FFindFloorResult FoundFloor;
		Movement->FindFloor(StartLocation, FoundFloor, false);
		if (!Test.TestTrue(TEXT("Actual fixture floor is blocking and walkable"), FoundFloor.bBlockingHit && FoundFloor.IsWalkableFloor())
			|| !Test.TestTrue(TEXT("Floor sweep hit the fixture collision"), FoundFloor.HitResult.GetComponent() == Floor)
			|| !Test.TestTrue(TEXT("Actual floor normal matches flat/ramp geometry"), FVector::DotProduct(FoundFloor.HitResult.ImpactNormal, FloorNormal) > 0.999f)
			|| !Test.TestFalse(TEXT("Initial capsule does not penetrate floor"), FoundFloor.HitResult.bStartPenetrating)) return false;
		Downhill = FVector::VectorPlaneProject(FVector::DownVector, FoundFloor.HitResult.ImpactNormal).GetSafeNormal();
		return true;
	}

	bool Simulate(int32 Hertz, FResult& Out, float SimDuration = Duration)
	{
		const float Step = 1.f / Hertz;
		const int32 Steps = FMath::RoundToInt(SimDuration * Hertz);
		const float Gravity = Movement->GetGravityZ();
		if (!Test.TestTrue(TEXT("Gravity integration remains below production speed cap"),
			FMath::Abs(Gravity) * SimDuration < Movement->GetMaxSpeed())) return false;
		if (!Test.TestTrue(TEXT("Fixture impact damping is below 0.01 cm/s error bound"),
			FMath::Abs(Gravity) * FMath::Square(SimDuration) * NegligibleWallFriction < 0.01f)) return false;
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			// Engine dispatch reaches unmodified PhysCustom/PhysSliding; no copied
			// integration, mock movement override or manually assigned hit result.
			Movement->StartNewPhysics(Step, 0);
			const FString Sample = FString::Printf(TEXT("%dHz step%d"), Hertz, Index);
			if (!Test.TestTrue(Sample + TEXT(": remains sliding on floor"), Movement->IsSliding())
				|| !Test.TestTrue(Sample + TEXT(": real walkable floor retained"), Movement->CurrentFloor.IsWalkableFloor())
				|| !Test.TestTrue(Sample + TEXT(": same collision floor"), Movement->CurrentFloor.HitResult.GetComponent() == Floor)
				|| !Test.TestFalse(Sample + TEXT(": no floor penetration"), Movement->CurrentFloor.HitResult.bStartPenetrating)
				|| !Test.TestTrue(Sample + TEXT(": capsule stays above surface"), FVector::DotProduct(Pawn->GetActorLocation(), FloorNormal) - CapsuleSupport >= -0.1f)) return false;
		}
		Out.Displacement = Pawn->GetActorLocation() - StartLocation;
		Out.Velocity = Movement->Velocity;
		Out.Gravity = Gravity;
		Out.DownhillDistance = FVector::DotProduct(Out.Displacement, Downhill);
		Out.DownhillSpeed = FVector::DotProduct(Out.Velocity, Downhill);
		Test.AddInfo(FString::Printf(TEXT("SlidePhysics: hz=%d seconds=%.3f gravity=%.3f velocity=%s displacement=%s downhill=%.3f floorZ=%.3f"),
			Hertz, SimDuration, Gravity, *Out.Velocity.ToString(), *Out.Displacement.ToString(), Out.DownhillDistance, FloorNormal.Z));
		return true;
	}

	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	UBoxComponent* Floor = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	UBwayCharacterMovementComponent* Movement = nullptr;
	FVector StartLocation = FVector::ZeroVector;
	FVector FloorNormal = FVector::UpVector;
	FVector Downhill = FVector::ZeroVector;
	float CapsuleSupport = 0.f;
};

bool FrameRateCase(FAutomationTestBase& Test, float PitchDegrees)
{
	const int32 Rates[] = {30, 60, 120};
	FResult Results[UE_ARRAY_COUNT(Rates)];
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rates); ++Index)
	{
		FFixture Fixture(Test);
		if (!Fixture.Setup(PitchDegrees, 1.f) || !Fixture.Simulate(Rates[Index], Results[Index])) return false;
		const auto& Result = Results[Index];
		Test.TestTrue(TEXT("Fixture gravity is downward"), Result.Gravity < -1.f);
		if (PitchDegrees == 0.f)
		{
			Test.TestTrue(TEXT("Supported flat contact removes inward gravity velocity"), Result.Velocity.IsNearlyZero(0.1f));
			Test.TestTrue(TEXT("Flat collision prevents horizontal slide and floor fall-through"),
				Result.Displacement.SizeSquared2D() < 0.01f && FMath::Abs(Result.Displacement.Z) < 1.f);
		}
		else
		{
			const float ExpectedTangentSpeed = FMath::Abs(Result.Gravity) * Duration * FMath::Sin(FMath::DegreesToRadians(PitchDegrees));
			Test.TestTrue(TEXT("Gravity accelerates the supported capsule along the real ramp tangent"),
				FMath::IsNearlyEqual(Result.DownhillSpeed, ExpectedTangentSpeed, 0.5f));
			Test.TestTrue(TEXT("Stored ramp velocity does not retain an inward floor component"),
				FMath::Abs(FVector::DotProduct(Result.Velocity, Fixture.FloorNormal)) < 0.5f);
			// Independent kinematic distance, with tolerance for first-order
			// integration (worst step1/30s) and real swept-capsule contact offsets.
			const float ContinuousDistance = 0.5f * FMath::Abs(Result.Gravity) * FMath::Sin(FMath::DegreesToRadians(PitchDegrees)) * FMath::Square(Duration);
			Test.TestTrue(TEXT("Real capsule gains downhill distance from gravity"), Result.DownhillDistance > ContinuousDistance * 0.8f);
			Test.TestTrue(TEXT("Ramp trajectory is within5cm of continuous gravity motion"), FMath::Abs(Result.DownhillDistance - ContinuousDistance) < 5.f);
		}
	}
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Rates); ++Index)
	{
		Test.TestTrue(TEXT("Equal elapsed time gives equal contact-resolved velocity across rates"), Results[0].Velocity.Equals(Results[Index].Velocity, 0.1f));
		Test.TestTrue(TEXT("Actual capsule trajectories converge within5cm across rates"), Results[0].Displacement.Equals(Results[Index].Displacement, 5.f));
	}
	FFixture NoGravity(Test);
	FResult Control;
	if (!NoGravity.Setup(PitchDegrees, 0.f) || !NoGravity.Simulate(60, Control)) return false;
	Test.TestTrue(TEXT("Zero-gravity control cannot create motion on the same collision"), Control.Displacement.IsNearlyZero(0.01f) && Control.Velocity.IsNearlyZero(0.01f));
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideGravityFlatRateTest, "Breakaway.CombatPolish.Slide.Physics.FlatFrameRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideGravityFlatRateTest::RunTest(const FString&)
{
	return BwaySlidePhysicsTest::FrameRateCase(*this, 0.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideGravityRampRateTest, "Breakaway.CombatPolish.Slide.Physics.RampFrameRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideGravityRampRateTest::RunTest(const FString&)
{
	return BwaySlidePhysicsTest::FrameRateCase(*this, 30.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideGravityFloorLossTest, "Breakaway.CombatPolish.Slide.Physics.FloorLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideGravityFloorLossTest::RunTest(const FString&)
{
	BwaySlidePhysicsTest::FFixture Fixture(*this);
	if (!Fixture.Setup(0.f, 1.f)) return false;
	Fixture.Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fixture.Movement->StartNewPhysics(1.f / 60.f, 0);
	TestTrue(TEXT("Actual missing floor exits slide through production physics"), Fixture.Movement->IsFalling());
	TestFalse(TEXT("Floor-loss transition clears slide custom mode"), Fixture.Movement->IsSliding());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideOffGroundGravityRateTest, "Breakaway.CombatPolish.Slide.Physics.OffGroundFrameRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideOffGroundGravityRateTest::RunTest(const FString&)
{
	using namespace BwaySlidePhysicsTest;
	constexpr float AirSeconds = 0.2f;
	for (const int32 Hertz : {30, 60, 120})
	{
		FFixture Fixture(*this);
		if (!Fixture.Setup(0.f, 1.f)) return false;
		// Floor remains inside the real query range, but the capsule cannot reach
		// it during this short interval. This isolates actual off-contact sliding
		// without disabling collision or replacing the production integration.
		Fixture.StartLocation += FVector(0.f, 0.f, 40.f);
		Fixture.Pawn->SetActorLocation(Fixture.StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
		FResult Result;
		if (!Fixture.Simulate(Hertz, Result, AirSeconds)) return false;
		TestTrue(TEXT("Capsule stayed genuinely off the queried floor"), Fixture.Movement->CurrentFloor.GetDistanceToFloor() > 10.f);
		TestTrue(TEXT("Off-contact gravity is integrated exactly once over elapsed time"),
			FMath::IsNearlyEqual(Result.Velocity.Z, Result.Gravity * AirSeconds, 0.1f));
		TestTrue(TEXT("Off-contact gravity creates no horizontal velocity"), Result.Velocity.SizeSquared2D() < 0.01f);
		const float Step = 1.f / Hertz;
		const int32 Steps = FMath::RoundToInt(AirSeconds * Hertz);
		const float ExpectedEulerDistance = Result.Gravity * Step * Step * Steps * (Steps + 1) * 0.5f;
		TestTrue(TEXT("Actual off-contact displacement matches single-integration kinematics"),
			FMath::IsNearlyEqual(Result.Displacement.Z, ExpectedEulerDistance, 0.1f));
	}
	return true;
}
#endif
