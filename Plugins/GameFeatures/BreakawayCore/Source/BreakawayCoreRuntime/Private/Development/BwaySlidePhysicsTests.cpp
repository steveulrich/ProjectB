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

namespace BwayDoradoRampBenchmark
{
constexpr float RampPitchDeg = 25.f;
constexpr float L1_CrestY = -3150.f;
constexpr float L2_MidwayY = -2840.f;
constexpr float L3_BottomY = -2425.f;
constexpr float L4_SeamY = -1705.f;
constexpr float UpperTopZ = 330.f;

struct FDoradoSplitResult
{
	float TimeL1 = 0.f;
	float TimeL2 = -1.f;
	float TimeL3 = -1.f;
	float TimeL4 = -1.f;
	float TimeReturnToRunSpeed = -1.f;
	float SpeedAtL1 = 0.f;
	float SpeedAtL2 = 0.f;
	float MinSpeedMiddleLanding = 99999.f;
	float PeakSpeedRamp1 = 0.f;
	float PeakSpeedRamp2 = 0.f;
	float PeakDownhillSpeed = 0.f;
	float SpeedBeforeL3 = 0.f;
	float SpeedAfterL3 = 0.f;
	float VelZAfterL3 = 0.f;
	float SpeedAtL4 = 0.f;
	float MaxYReached = -99999.f;
};

class FDoradoRampFixture
{
public:
	explicit FDoradoRampFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FDoradoRampFixture() { if (World) World->DestroyWorld(false); }

	static void AddSegment(AActor* Owner, const FVector& TopStart, const FVector& TopEnd, float HalfWidth = 420.f, float HalfThickness = 40.f)
	{
		const FVector Delta = TopEnd - TopStart;
		const float Length = Delta.Size();
		const FVector Tangent = Delta / Length;
		const FVector Normal = FVector(0.f, -Tangent.Z, Tangent.Y).GetSafeNormal();
		const FMatrix Axes(FVector::ForwardVector, Tangent, Normal, FVector::ZeroVector);
		UBoxComponent* Box = NewObject<UBoxComponent>(Owner);
		Box->SetupAttachment(Owner->GetRootComponent());
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetBoxExtent(FVector(HalfWidth, Length * 0.5f, HalfThickness));
		Box->SetWorldLocationAndRotation((TopStart + TopEnd) * 0.5f - Normal * HalfThickness, Axes.Rotator());
		Box->RegisterComponent();
	}

	bool Setup()
	{
		const UWorld::InitializationValues Settings = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test.TestNotNull(TEXT("Dorado benchmark world"), World)) return false;

		AActor* RampActor = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(RampActor);
		RampActor->SetRootComponent(Root);
		Root->RegisterComponent();

		const float TanPitch = FMath::Tan(FMath::DegreesToRadians(RampPitchDeg));
		const float Ramp1EndY = -2965.f;
		const float MiddleEndY = -2730.f;
		const float MiddleZ = UpperTopZ - (Ramp1EndY - L1_CrestY) * TanPitch;
		const float LowerZ = MiddleZ - (L3_BottomY - MiddleEndY) * TanPitch;
		LowerFloorZ = LowerZ;

		const FVector P0(0.f, -3650.f, UpperTopZ);
		const FVector P1(0.f, L1_CrestY, UpperTopZ);
		const FVector P2(0.f, Ramp1EndY, MiddleZ);
		const FVector P3(0.f, MiddleEndY, MiddleZ);
		const FVector P4(0.f, L3_BottomY, LowerZ);
		const FVector P5(0.f, 500.f, LowerZ);

		const FVector RampDownhillDir = (P2 - P1).GetSafeNormal();
		AddSegment(RampActor, P0, P1);
		AddSegment(RampActor, P1, P2 + RampDownhillDir * 25.f);
		AddSegment(RampActor, P2 - FVector(0.f, 25.f, 0.f), P3);
		AddSegment(RampActor, P3, P4 + RampDownhillDir * 25.f);
		AddSegment(RampActor, P4 - FVector(0.f, 25.f, 0.f), P5);

		UClass* PawnClass = StaticLoadClass(ABwayCharacterWithAbilities::StaticClass(), nullptr,
			TEXT("/BreakawayCore/Characters/B_BW_Character_Humanoid.B_BW_Character_Humanoid_C"));
		if (!PawnClass)
		{
			PawnClass = ABwayCharacterWithAbilities::StaticClass();
		}

		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(PawnClass, FVector(0.f, L1_CrestY, UpperTopZ + 120.f), FRotator(0.f, 90.f, 0.f), Spawn);
		if (!Test.TestNotNull(TEXT("Dorado benchmark pawn"), Pawn)) return false;
		Movement = Pawn->GetBwayCharacterMovement();
		if (!Test.TestNotNull(TEXT("Dorado benchmark CMC"), Movement)) return false;
		Movement->bRunPhysicsWithNoController = true;
		return true;
	}

	void PlaceOnSurfaceAtY(float Y, float SurfaceZ, float PitchDeg = 0.f)
	{
		const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
		const float NormalZ = FMath::Cos(FMath::DegreesToRadians(PitchDeg));
		const float Support = Capsule->GetScaledCapsuleRadius()
			+ (Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius()) * NormalZ;
		const FVector Loc(0.f, Y, SurfaceZ + (Support + 1.9f) / NormalZ);
		Pawn->SetActorLocationAndRotation(Loc, FRotator(0.f, 90.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		Movement->FindFloor(Pawn->GetActorLocation(), Movement->CurrentFloor, false);
	}

	FDoradoSplitResult RunDownhillSlide(bool bUseLegacyTuning)
	{
		FDoradoSplitResult Out;
		if (bUseLegacyTuning)
		{
			Movement->BaseSlideSpeed = 500.f;
			Movement->SlideMaxSpeedFactor = 3.5f;
			Movement->SlideSlopeAccelerationFactor = 6.0f;
			Movement->SlideBaseFrictionFactor = 4.0f;
			Movement->SlideFrictionPower = 7.0f;
		}
		else
		{
			Movement->BaseSlideSpeed = 600.f;
			Movement->SlideMaxSpeedFactor = 3.0f;
			Movement->SlideSlopeAccelerationFactor = 2.4f;
			Movement->SlideBaseFrictionFactor = 0.80f;
			Movement->SlideFrictionPower = 4.0f;
		}
		Movement->MinSlideSpeed = 0.f;
		Movement->SetMovementMode(MOVE_Walking);
		PlaceOnSurfaceAtY(L1_CrestY + 2.f, UpperTopZ - 2.f * FMath::Tan(FMath::DegreesToRadians(RampPitchDeg)), RampPitchDeg);
		Movement->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Slide));
		PlaceOnSurfaceAtY(L1_CrestY + 2.f, UpperTopZ - 2.f * FMath::Tan(FMath::DegreesToRadians(RampPitchDeg)), RampPitchDeg);

		const FVector RampDownhill(0.f, FMath::Cos(FMath::DegreesToRadians(RampPitchDeg)), -FMath::Sin(FMath::DegreesToRadians(RampPitchDeg)));
		Movement->Velocity = RampDownhill * Movement->BaseSlideSpeed;
		Out.SpeedAtL1 = Movement->Velocity.Size();

		constexpr float Step = 1.f / 60.f;
		float PrevY = Pawn->GetActorLocation().Y;
		float PrevSpeed = Out.SpeedAtL1;
		for (int32 Tick = 1; Tick <= 240; ++Tick)
		{
			const float PrevTime = (Tick - 1) * Step;
			const float CurrTime = Tick * Step;
			Movement->StartNewPhysics(Step, 0);
			const float CurrY = Pawn->GetActorLocation().Y;
			const float CurrSpeed = Movement->Velocity.Size();
			Out.MaxYReached = FMath::Max(Out.MaxYReached, CurrY);

			if (CurrY <= -2965.f)
			{
				Out.PeakSpeedRamp1 = FMath::Max(Out.PeakSpeedRamp1, CurrSpeed);
			}
			else if (CurrY > -2965.f && CurrY <= -2730.f)
			{
				Out.MinSpeedMiddleLanding = FMath::Min(Out.MinSpeedMiddleLanding, CurrSpeed);
			}
			else if (CurrY > -2730.f && CurrY <= L3_BottomY)
			{
				Out.PeakSpeedRamp2 = FMath::Max(Out.PeakSpeedRamp2, CurrSpeed);
				Out.SpeedBeforeL3 = CurrSpeed;
			}
			Out.PeakDownhillSpeed = FMath::Max(Out.PeakDownhillSpeed, CurrSpeed);

			auto InterpolateCrossTime = [&](float TargetY) -> float
			{
				const float Alpha = (CurrY > PrevY) ? FMath::Clamp((TargetY - PrevY) / (CurrY - PrevY), 0.f, 1.f) : 0.f;
				return FMath::Lerp(PrevTime, CurrTime, Alpha);
			};

			if (Out.TimeL2 < 0.f && CurrY >= L2_MidwayY)
			{
				Out.TimeL2 = InterpolateCrossTime(L2_MidwayY);
				Out.SpeedAtL2 = CurrSpeed;
			}
			if (Out.TimeL3 < 0.f && CurrY >= L3_BottomY)
			{
				Out.TimeL3 = InterpolateCrossTime(L3_BottomY);
				Out.SpeedAfterL3 = CurrSpeed;
				Out.VelZAfterL3 = Movement->Velocity.Z;
			}
			if (Out.TimeL4 < 0.f && CurrY >= L4_SeamY)
			{
				Out.TimeL4 = InterpolateCrossTime(L4_SeamY);
				Out.SpeedAtL4 = CurrSpeed;
			}
			if (Out.TimeL3 > 0.f && Out.TimeReturnToRunSpeed < 0.f && CurrSpeed <= 600.f)
			{
				const float Alpha = (PrevSpeed > CurrSpeed) ? FMath::Clamp((PrevSpeed - 600.f) / (PrevSpeed - CurrSpeed), 0.f, 1.f) : 0.f;
				Out.TimeReturnToRunSpeed = FMath::Lerp(PrevTime, CurrTime, Alpha);
			}

			PrevY = CurrY;
			PrevSpeed = CurrSpeed;
		}
		return Out;
	}

	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	UBwayCharacterMovementComponent* Movement = nullptr;
	float LowerFloorZ = 0.f;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideDoradoRampBenchmarkTest, "Breakaway.CombatPolish.Slide.Physics.DoradoRampBenchmark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideDoradoRampBenchmarkTest::RunTest(const FString&)
{
	using namespace BwayDoradoRampBenchmark;
	FDoradoRampFixture Fixture(*this);
	if (!Fixture.Setup()) return false;

	// 1. Verify automatic upgrade of legacy Blueprint defaults on spawn
	TestEqual(TEXT("Tuned BaseSlideSpeed"), Fixture.Movement->BaseSlideSpeed, 600.f);
	TestEqual(TEXT("Tuned SlideMaxSpeedFactor"), Fixture.Movement->SlideMaxSpeedFactor, 3.0f);
	TestEqual(TEXT("Tuned SlideSlopeAccelerationFactor"), Fixture.Movement->SlideSlopeAccelerationFactor, 2.4f);
	TestEqual(TEXT("Tuned SlideBaseFrictionFactor"), Fixture.Movement->SlideBaseFrictionFactor, 0.80f);
	TestEqual(TEXT("Tuned SlideFrictionPower"), Fixture.Movement->SlideFrictionPower, 4.0f);
	TestEqual(TEXT("Tuned SlideUphillFrictionMultiplier"), Fixture.Movement->SlideUphillFrictionMultiplier, 3.5f);
	TestEqual(TEXT("Tuned SlideFOVOffsetMax"), Fixture.Movement->SlideFOVOffsetMax, 12.0f);
	TestEqual(TEXT("Tuned SlideFOVInterpSpeed"), Fixture.Movement->SlideFOVInterpSpeed, 6.0f);

	// 2. Normal Run splits on L_BW_Dorado route (600 cm/s standard hero walk speed)
	constexpr float NormalRunSpeed = 600.f;
	const float RunT_L1_L2 = (L2_MidwayY - L1_CrestY) / NormalRunSpeed;
	const float RunT_L2_L3 = (L3_BottomY - L2_MidwayY) / NormalRunSpeed;
	const float RunT_L3_L4 = (L4_SeamY - L3_BottomY) / NormalRunSpeed;
	const float RunT_Total = (L4_SeamY - L1_CrestY) / NormalRunSpeed;

	// 3. Before vs. After Downhill Slide Benchmark on L_BW_Dorado double-flight ramp
	const FDoradoSplitResult Before = Fixture.RunDownhillSlide(/*bUseLegacyTuning=*/true);
	const FDoradoSplitResult After = Fixture.RunDownhillSlide(/*bUseLegacyTuning=*/false);

	const float Before_S1 = Before.TimeL2 - Before.TimeL1;
	const float Before_S2 = Before.TimeL3 - Before.TimeL2;
	const float After_S1 = After.TimeL2 - After.TimeL1;
	const float After_S2 = After.TimeL3 - After.TimeL2;
	const float After_S3 = After.TimeL4 - After.TimeL3;
	const float After_Total = After.TimeL4 - After.TimeL1;

	AddInfo(FString::Printf(
		TEXT("DoradoNormalRun: T_L1_L2=%.3fs T_L2_L3=%.3fs T_L3_L4=%.3fs Total=%.3fs (Speed=%.1f cm/s)"),
		RunT_L1_L2, RunT_L2_L3, RunT_L3_L4, RunT_Total, NormalRunSpeed));
	AddInfo(FString::Printf(
		TEXT("DoradoSlideBEFORE: T_L1_L2=%.3fs T_L2_L3=%.3fs T_L3=%.3fs T_L4=%.3fs (MaxY=%.1f) SpeedL1=%.1f PeakRamp1=%.1f MinMid=%.1f PeakRamp2=%.1f SpeedAfterL3=%.1f VelZAfterL3=%.1f"),
		Before_S1, Before_S2, Before.TimeL3, Before.TimeL4, Before.MaxYReached,
		Before.SpeedAtL1, Before.PeakSpeedRamp1, Before.MinSpeedMiddleLanding, Before.PeakSpeedRamp2, Before.SpeedAfterL3, Before.VelZAfterL3));
	AddInfo(FString::Printf(
		TEXT("DoradoSlideAFTER: T_L1_L2=%.3fs (saved %.3fs) T_L2_L3=%.3fs (saved %.3fs) T_L3_L4=%.3fs (saved %.3fs) Total=%.3fs (saved %.3fs) SpeedL1=%.1f PeakRamp1=%.1f MinMid=%.1f PeakDownhill=%.1f SpeedAfterL3=%.1f VelZAfterL3=%.1f SpeedAtL4=%.1f ReturnToRunTime=%.3fs"),
		After_S1, RunT_L1_L2 - After_S1,
		After_S2, RunT_L2_L3 - After_S2,
		After_S3, RunT_L3_L4 - After_S3,
		After_Total, RunT_Total - After_Total,
		After.SpeedAtL1, After.PeakSpeedRamp1, After.MinSpeedMiddleLanding, After.PeakDownhillSpeed,
		After.SpeedAfterL3, After.VelZAfterL3, After.SpeedAtL4, After.TimeReturnToRunSpeed));

	TestTrue(TEXT("Before-tuning slide stalled on flat runout before reaching L4"), Before.TimeL4 < 0.f && Before.MaxYReached < L4_SeamY);
	TestTrue(TEXT("After-tuning slide reaches all 4 landmarks L1->L2->L3->L4"),
		After.TimeL2 > 0.f && After.TimeL3 > After.TimeL2 && After.TimeL4 > After.TimeL3);
	TestTrue(TEXT("After-tuning slide saves time on Section 1 (L1->L2)"), After_S1 < RunT_L1_L2);
	TestTrue(TEXT("After-tuning slide saves time on Section 2 (L2->L3)"), After_S2 < RunT_L2_L3);
	TestTrue(TEXT("After-tuning slide saves time on Section 3 (L3->L4)"), After_S3 < RunT_L3_L4);
	TestTrue(TEXT("After-tuning peak downhill speed is in Breakaway reference band (1150..1500 cm/s)"),
		After.PeakDownhillSpeed >= 1150.f && After.PeakDownhillSpeed <= 1500.f);
	TestTrue(TEXT("After-tuning bottom transition retains >95% of ramp speed with zero vertical pop/hangover"),
		After.SpeedAfterL3 >= After.SpeedBeforeL3 * 0.95f && FMath::Abs(After.VelZAfterL3) < 1.0f);
	TestTrue(TEXT("After-tuning slide carries momentum past L4 above normal walk speed and decays back to 600 cm/s around 1.7..2.0s"),
		After.SpeedAtL4 > NormalRunSpeed && After.TimeReturnToRunSpeed >= 1.65f && After.TimeReturnToRunSpeed <= 2.05f);

	// 4. Flat-ground slide verification: smooth decay without acceleration
	Fixture.Movement->SetMovementMode(MOVE_Walking);
	Fixture.PlaceOnSurfaceAtY(-1500.f, Fixture.LowerFloorZ, 0.f);
	Fixture.Movement->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Slide));
	Fixture.PlaceOnSurfaceAtY(-1500.f, Fixture.LowerFloorZ, 0.f);
	Fixture.Movement->Velocity = FVector(0.f, NormalRunSpeed, 0.f);
	float PrevFlatSpeed = NormalRunSpeed;
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		Fixture.Movement->StartNewPhysics(1.f / 60.f, 0);
		const float CurrFlatSpeed = Fixture.Movement->Velocity.Size2D();
		TestTrue(TEXT("Flat slide speed monotonically decays without accelerating"), CurrFlatSpeed < PrevFlatSpeed);
		PrevFlatSpeed = CurrFlatSpeed;
	}
	AddInfo(FString::Printf(TEXT("DoradoFlatSlide: StartSpeed=600.0 EndSpeedAfter0.5s=%.1f"), PrevFlatSpeed));

	// 5. Uphill slide verification: quick decay (<0.35s) and zero backward runaway
	Fixture.Movement->SetMovementMode(MOVE_Walking);
	const float UphillStartY = -2460.f;
	const float UphillStartZ = Fixture.LowerFloorZ + (L3_BottomY - UphillStartY) * FMath::Tan(FMath::DegreesToRadians(RampPitchDeg));
	Fixture.PlaceOnSurfaceAtY(UphillStartY, UphillStartZ, RampPitchDeg);
	Fixture.Movement->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Slide));
	Fixture.PlaceOnSurfaceAtY(UphillStartY, UphillStartZ, RampPitchDeg);
	const FVector RampUphill(0.f, -FMath::Cos(FMath::DegreesToRadians(RampPitchDeg)), FMath::Sin(FMath::DegreesToRadians(RampPitchDeg)));
	Fixture.Movement->Velocity = RampUphill * NormalRunSpeed;
	float UphillStopTime = -1.f;
	float MinUphillY = UphillStartY;
	for (int32 Tick = 1; Tick <= 30; ++Tick)
	{
		Fixture.Movement->StartNewPhysics(1.f / 60.f, 0);
		MinUphillY = FMath::Min(MinUphillY, Fixture.Pawn->GetActorLocation().Y);
		if (UphillStopTime < 0.f && Fixture.Movement->Velocity.SizeSquared() < 1.f)
		{
			UphillStopTime = Tick * (1.f / 60.f);
		}
	}
	const float UphillTravel2D = UphillStartY - MinUphillY;
	AddInfo(FString::Printf(TEXT("DoradoUphillSlide: StopTime=%.3fs UphillTravel2D=%.1fcm FinalVel=%s"),
		UphillStopTime, UphillTravel2D, *Fixture.Movement->Velocity.ToString()));
	TestTrue(TEXT("Uphill slide decays to stop in under 0.35s"), UphillStopTime > 0.f && UphillStopTime <= 0.35f);
	TestTrue(TEXT("Uphill slide does not reverse-accelerate backward down the ramp"),
		Fixture.Movement->Velocity.IsNearlyZero(0.1f) && Fixture.Pawn->GetActorLocation().Y <= MinUphillY + 1.f);

	// 6. Slide-jump cancel verification: jumping mid-slide exits CMOVE_Slide into MOVE_Falling with momentum boost
	Fixture.Movement->SetMovementMode(MOVE_Walking);
	Fixture.PlaceOnSurfaceAtY(-2650.f, Fixture.LowerFloorZ + (L3_BottomY - (-2650.f)) * FMath::Tan(FMath::DegreesToRadians(RampPitchDeg)), RampPitchDeg);
	Fixture.Movement->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Slide));
	const FVector RampDownhill(0.f, FMath::Cos(FMath::DegreesToRadians(RampPitchDeg)), -FMath::Sin(FMath::DegreesToRadians(RampPitchDeg)));
	Fixture.Movement->Velocity = RampDownhill * 1100.f;
	const float PreJumpSpeed2D = Fixture.Movement->Velocity.Size2D();
	const bool bJumped = static_cast<UCharacterMovementComponent*>(Fixture.Movement)->DoJump(false, 1.f / 60.f);
	const float PostJumpSpeed2D = Fixture.Movement->Velocity.Size2D();
	AddInfo(FString::Printf(TEXT("DoradoSlideJumpCancel: bJumped=%d PreSpeed2D=%.1f PostSpeed2D=%.1f PostVelZ=%.1f IsSlideJumping=%d"),
		bJumped ? 1 : 0, PreJumpSpeed2D, PostJumpSpeed2D, Fixture.Movement->Velocity.Z, Fixture.Movement->IsSlideJumping() ? 1 : 0));
	TestTrue(TEXT("DoJump succeeds from downhill slide and enters MOVE_Falling + SlideJumping"),
		bJumped && Fixture.Movement->IsFalling() && !Fixture.Movement->IsSliding() && Fixture.Movement->IsSlideJumping());
	TestTrue(TEXT("Slide-jump boosts horizontal speed by SlideJumpMomentumBoost"),
		FMath::IsNearlyEqual(PostJumpSpeed2D, PreJumpSpeed2D * Fixture.Movement->SlideJumpMomentumBoost, 2.f));

	return true;
}
#endif
