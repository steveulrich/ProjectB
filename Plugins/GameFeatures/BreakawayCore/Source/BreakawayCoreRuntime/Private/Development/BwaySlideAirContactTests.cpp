#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

namespace BwaySlideAirContactTest
{
constexpr float InitialPlaneGap = 34.f;
constexpr float VelocityTolerance = 0.1f;
const FVector InitialVelocity(0.f, 500.f, -300.f);

// FindFloor can see a surface well before the upright capsule contacts it. Keep
// the authored humanoid tuning and use real queries/sweeps to distinguish those
// states. Only the initial transform/velocity and controllerless dispatch are
// fixture plumbing; no floor result or production movement is substituted.
class FFixture
{
public:
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture() { if (World) World->DestroyWorld(false); }

	bool Setup(float PitchDegrees, float StartingPlaneGap = InitialPlaneGap, const FVector& SeedVelocity = InitialVelocity)
	{
		UClass* PawnClass = LoadClass<ABwayCharacterWithAbilities>(nullptr,
			TEXT("/BreakawayCore/Characters/B_BW_Character_Humanoid.B_BW_Character_Humanoid_C"));
		if (!Test.TestNotNull(TEXT("Authored humanoid class"), PawnClass)) return false;
		const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test.TestNotNull(TEXT("Air-contact fixture world"), World)) return false;
		AActor* FloorActor = World->SpawnActor<AActor>();
		if (!Test.TestNotNull(TEXT("Real plane actor"), FloorActor)) return false;
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
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(PawnClass, FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator, Spawn);
		if (!Test.TestNotNull(TEXT("Production authored pawn"), Pawn)) return false;
		Movement = Pawn->GetBwayCharacterMovement();
		if (!Test.TestNotNull(TEXT("Production slide movement"), Movement)
			|| !Test.TestTrue(TEXT("Valid authority movement data"), Pawn->HasAuthority() && Movement->HasValidData())) return false;
		Movement->bRunPhysicsWithNoController = true;
		Movement->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Slide));
		const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
		Radius = Capsule->GetScaledCapsuleRadius();
		HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		CapsuleSupport = Radius + (HalfHeight - Radius) * FloorNormal.Z;
		Pawn->SetActorLocation(FVector(0.f, 0.f, (CapsuleSupport + StartingPlaneGap) / FloorNormal.Z),
			false, nullptr, ETeleportType::TeleportPhysics);
		Movement->Velocity = SeedVelocity;
		if (!Test.TestTrue(TEXT("Authored friction and slope acceleration remain enabled"),
			Movement->SlideBaseFrictionFactor > 0.f && Movement->SlideSlopeAccelerationFactor > 0.f)
			|| !Test.TestTrue(TEXT("Fixture uses authored zero-threshold slide"), Movement->MinSlideSpeed == 0.f)
			|| !Test.TestTrue(TEXT("Initial capsule has the declared clearance above the actual plane"),
				FMath::IsNearlyEqual(PlaneGap(Pawn->GetActorLocation()), StartingPlaneGap, 0.01f))
			|| !Test.TestTrue(TEXT("No input or steering acceleration is seeded"),
				Pawn->GetPendingMovementInputVector().IsNearlyZero() && Movement->GetCurrentAcceleration().IsNearlyZero())) return false;
		if (PitchDegrees > 0.f && !Test.TestTrue(TEXT("Ramp can expose authored slope acceleration leakage"),
			PitchDegrees > Movement->MinSlopeAngleForAccel && FMath::Abs(FloorNormal.X) > 0.1f)) return false;
		FFindFloorResult FoundFloor;
		return QueryPlane(TEXT("Initial"), FoundFloor) && ClearPath(TEXT("Initial capsule"), Pawn->GetActorLocation(), Pawn->GetActorLocation());
	}

	bool AirStep(int32 Hertz)
	{
		const FString Label = FString::Printf(TEXT("%d Hz separated step"), Hertz);
		const float Step = 1.f / Hertz;
		const FVector Start = Pawn->GetActorLocation();
		const float Gravity = Movement->GetGravityZ();
		const FVector ExpectedVelocity = InitialVelocity + FVector(0.f, 0.f, Gravity * Step);
		const FVector ExpectedEnd = Start + ExpectedVelocity * Step;
		FFindFloorResult BeforeFloor;
		if (!QueryPlane(Label + TEXT(" before"), BeforeFloor)
			|| !Test.TestTrue(Label + TEXT(": distant floor is outside the engine contact band"),
				BeforeFloor.GetDistanceToFloor() > UCharacterMovementComponent::MAX_FLOOR_DIST + 10.f)
			|| !Test.TestTrue(Label + TEXT(": gravity is downward"), Gravity < -1.f)
			|| !Test.TestTrue(Label + TEXT(": seeded and expected velocity cannot invoke the cap"),
				InitialVelocity.Size() < Movement->GetMaxSpeed() && ExpectedVelocity.Size() < Movement->GetMaxSpeed())
			|| !Test.TestTrue(Label + TEXT(": expected entire step stays clearly separated"), PlaneGap(ExpectedEnd) > 15.f)
			|| !ClearPath(Label + TEXT(" expected path"), Start, ExpectedEnd)) return false;

		Movement->StartNewPhysics(Step, 0);
		const FVector End = Pawn->GetActorLocation();
		const FVector ActualVelocity = Movement->Velocity;
		FFindFloorResult AfterFloor;
		if (!QueryPlane(Label + TEXT(" after"), AfterFloor)
			|| !Test.TestTrue(Label + TEXT(": production dispatch remains in slide mode"), Movement->IsSliding())
			|| !Test.TestTrue(Label + TEXT(": actual capsule also remains clearly separated"),
				PlaneGap(End) > 15.f && AfterFloor.GetDistanceToFloor() > UCharacterMovementComponent::MAX_FLOOR_DIST + 10.f)
			|| !ClearPath(Label + TEXT(" actual path"), Start, End)) return false;
		Test.AddInfo(FString::Printf(TEXT("SlideAirContact: hz=%d normal=%s gapBefore=%.6f gapAfter=%.6f queryBefore=%.6f queryAfter=%.6f gravity=%.6f friction=%.6f slopeAccel=%.6f expected=%s actual=%s"),
			Hertz, *FloorNormal.ToString(), PlaneGap(Start), PlaneGap(End), BeforeFloor.GetDistanceToFloor(),
			AfterFloor.GetDistanceToFloor(), Gravity, Movement->SlideBaseFrictionFactor,
			Movement->SlideSlopeAccelerationFactor, *ExpectedVelocity.ToString(), *ActualVelocity.ToString()));
		Test.TestTrue(Label + TEXT(": distant floor cannot apply horizontal friction or slope acceleration"),
			FMath::IsNearlyEqual(ActualVelocity.X, InitialVelocity.X, VelocityTolerance)
			&& FMath::IsNearlyEqual(ActualVelocity.Y, InitialVelocity.Y, VelocityTolerance));
		Test.TestTrue(Label + TEXT(": vertical velocity integrates gravity without ground forces"),
			FMath::IsNearlyEqual(ActualVelocity.Z, ExpectedVelocity.Z, VelocityTolerance));
		Test.TestTrue(Label + TEXT(": collision-free displacement matches gravity-only integration"), End.Equals(ExpectedEnd, 0.01f));
		return true;
	}

	bool Recontact(int32 Hertz)
	{
		if (!AirStep(Hertz)) return false;
		const float Step = 1.f / Hertz;
		const FString Label = FString::Printf(TEXT("%d Hz air-to-contact"), Hertz);
		bool bReachedPhysicalContact = false;
		int32 SupportedSteps = 0;
		// Follow the actual descent; do not teleport onto the floor or assign a
		// cached hit. A bounded half-second gives every rate time to land.
		for (int32 Index = 0; Index < Hertz / 2 && SupportedSteps < 3; ++Index)
		{
			FFindFloorResult BeforeFloor;
			if (!QueryPlane(Label + TEXT(" before"), BeforeFloor)) return false;
			const float GapBefore = PlaneGap(Pawn->GetActorLocation());
			bReachedPhysicalContact |= GapBefore <= 0.25f;
			const bool bSupported = bReachedPhysicalContact
				&& BeforeFloor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST;
			const FVector BeforeVelocity = Movement->Velocity;
			if (!Test.TestTrue(Label + TEXT(": no horizontal steering input during descent"),
				Movement->GetCurrentAcceleration().SizeSquared2D() < 0.001f
				&& Pawn->GetPendingMovementInputVector().IsNearlyZero())) return false;
			Movement->StartNewPhysics(Step, 0);
			FFindFloorResult AfterFloor;
			if (!QueryPlane(Label + TEXT(" after"), AfterFloor)
				|| !Test.TestTrue(Label + TEXT(": stays sliding without plane penetration"),
					Movement->IsSliding() && PlaneGap(Pawn->GetActorLocation()) >= -0.1f)) return false;
			if (bSupported)
			{
				const float ExpectedForward = BeforeVelocity.Y * (1.f - FMath::Clamp(Movement->SlideBaseFrictionFactor * Step, 0.f, 1.f));
				Test.TestTrue(Label + TEXT(": real recontact resumes authored ground friction"),
					FMath::IsNearlyEqual(Movement->Velocity.Y, ExpectedForward, VelocityTolerance)
					&& Movement->Velocity.Y < BeforeVelocity.Y - VelocityTolerance);
				Test.TestTrue(Label + TEXT(": recontact remains on the same physical plane"),
					PlaneGap(Pawn->GetActorLocation()) <= 0.25f
					&& AfterFloor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST);
				++SupportedSteps;
			}
		}
		Test.AddInfo(FString::Printf(TEXT("SlideAirContactRecontact: hz=%d physicalContact=%d supportedSteps=%d finalGap=%.6f finalVelocity=%s"),
			Hertz, bReachedPhysicalContact ? 1 : 0, SupportedSteps, PlaneGap(Pawn->GetActorLocation()), *Movement->Velocity.ToString()));
		Test.TestTrue(Label + TEXT(": actual descent reaches physical contact"), bReachedPhysicalContact);
		Test.TestEqual(Label + TEXT(": three supported steps prove ground forces resume"), SupportedSteps, 3);
		return true;
	}

	bool GroundedRampStep(int32 Hertz)
	{
		const FString Label = FString::Printf(TEXT("%d Hz grounded authored ramp"), Hertz);
		const float Step = 1.f / Hertz;
		FFindFloorResult BeforeFloor;
		if (!QueryPlane(Label + TEXT(" before"), BeforeFloor)
			|| !Test.TestTrue(Label + TEXT(": real ramp is within the contact band"),
				BeforeFloor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST
				&& PlaneGap(Pawn->GetActorLocation()) <= 0.25f)
			|| !Test.TestTrue(Label + TEXT(": authored walking policy rejects this slide-supported ramp"),
				!BeforeFloor.IsWalkableFloor() && Movement->GetWalkableFloorAngle() < 25.f)) return false;
		const FVector DownSlope = FVector::VectorPlaneProject(FVector::DownVector, FloorNormal).GetSafeNormal();
		const float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FloorNormal.Z));
		const float Friction = Movement->SlideBaseFrictionFactor
			* FMath::Pow(1.f - SlopeAngle / 90.f, Movement->SlideFrictionPower);
		const float Damping = 1.f - FMath::Clamp(Friction * Step, 0.f, 1.f);
		const float AuthoredSlopeAcceleration = Movement->BaseSlideSpeed * Movement->SlideSlopeAccelerationFactor;
		const FVector Gravity(0.f, 0.f, Movement->GetGravityZ());
		const FVector BeforeVelocity = Movement->Velocity;
		// Collision removes the inward component but leaves both plane tangents.
		// Their independent expectations distinguish authored slope acceleration
		// from ordinary projected gravity and cross-slope surface friction.
		const FVector ExpectedPreContactVelocity = (BeforeVelocity + (DownSlope * AuthoredSlopeAcceleration + Gravity) * Step) * Damping;
		const float ExpectedDownhill = FVector::DotProduct(ExpectedPreContactVelocity, DownSlope);
		const float GravityOnlyDownhill = FVector::DotProduct((BeforeVelocity + Gravity * Step) * Damping, DownSlope);
		if (!Test.TestTrue(Label + TEXT(": authored forces remain below the speed cap"),
			ExpectedPreContactVelocity.Size() < Movement->GetMaxSpeed())
			|| !Test.TestTrue(Label + TEXT(": both ground force expectations are observable"),
				ExpectedDownhill > GravityOnlyDownhill + 1.f && BeforeVelocity.Y - ExpectedPreContactVelocity.Y > 0.1f)) return false;
		FHitResult ContactHit;
		const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(BwaySlideGroundedRampContact), false, Pawn);
		const FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());
		const FVector Start = Pawn->GetActorLocation();
		World->SweepSingleByChannel(ContactHit, Start, Start + ExpectedPreContactVelocity * Step,
			Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Radius, HalfHeight), Query, Response);
		if (!Test.TestTrue(Label + TEXT(": full movement-path capsule sweep reaches the real ramp"),
			ContactHit.IsValidBlockingHit() && ContactHit.GetComponent() == Floor
			&& !ContactHit.bStartPenetrating && FVector::DotProduct(ContactHit.ImpactNormal, FloorNormal) > 0.999f)) return false;
		Movement->StartNewPhysics(Step, 0);
		FFindFloorResult AfterFloor;
		if (!QueryPlane(Label + TEXT(" after"), AfterFloor)
			|| !Test.TestTrue(Label + TEXT(": production slide retains real nonpenetrating ramp contact"),
				Movement->IsSliding() && PlaneGap(Pawn->GetActorLocation()) >= -0.1f
				&& PlaneGap(Pawn->GetActorLocation()) <= 0.25f
				&& AfterFloor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST)) return false;
		const float ActualDownhill = FVector::DotProduct(Movement->Velocity, DownSlope);
		Test.AddInfo(FString::Printf(TEXT("SlideAirContactGroundedRamp: hz=%d walkable=%d gap=%.6f friction=%.6f expectedCross=%.6f actualCross=%.6f expectedDown=%.6f actualDown=%.6f gravityOnlyDown=%.6f"),
			Hertz, AfterFloor.IsWalkableFloor() ? 1 : 0, PlaneGap(Pawn->GetActorLocation()), Friction,
			ExpectedPreContactVelocity.Y, Movement->Velocity.Y, ExpectedDownhill, ActualDownhill, GravityOnlyDownhill));
		Test.TestTrue(Label + TEXT(": authored friction still damps the cross-slope tangent"),
			FMath::IsNearlyEqual(Movement->Velocity.Y, ExpectedPreContactVelocity.Y, VelocityTolerance));
		Test.TestTrue(Label + TEXT(": authored slope force still accelerates the downhill tangent"),
			FMath::IsNearlyEqual(ActualDownhill, ExpectedDownhill, 0.5f) && ActualDownhill > GravityOnlyDownhill + 1.f);
		return true;
	}

private:
	float PlaneGap(const FVector& Location) const { return FVector::DotProduct(Location, FloorNormal) - CapsuleSupport; }

	bool QueryPlane(const FString& Label, FFindFloorResult& OutFloor) const
	{
		Movement->FindFloor(Pawn->GetActorLocation(), OutFloor, false);
		// The authored humanoid rejects ramps for walking. A slide query still
		// returns the real blocking plane; walkability is not contact evidence.
		return Test.TestTrue(Label + TEXT(": fresh floor query resolves the exact nonpenetrating plane"),
			OutFloor.bBlockingHit && OutFloor.HitResult.IsValidBlockingHit()
			&& OutFloor.HitResult.GetComponent() == Floor && !OutFloor.HitResult.bStartPenetrating
			&& FMath::IsFinite(OutFloor.GetDistanceToFloor())
			&& FVector::DotProduct(OutFloor.HitResult.ImpactNormal, FloorNormal) > 0.999f);
	}

	bool ClearPath(const FString& Label, const FVector& Start, const FVector& End) const
	{
		const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(BwaySlideAirContact), false, Pawn);
		const FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
		const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
		const FQuat Rotation = Capsule->GetComponentQuat();
		FHitResult Hit;
		const bool bBlocked = World->SweepSingleByChannel(Hit, Start, End, Rotation, Channel, Shape, Query, Response);
		const bool bStartOverlap = World->OverlapBlockingTestByChannel(Start, Rotation, Channel, Shape, Query, Response);
		const bool bEndOverlap = World->OverlapBlockingTestByChannel(End, Rotation, Channel, Shape, Query, Response);
		return Test.TestTrue(Label + TEXT(": full capsule sweep and endpoints have no contact or penetration"),
			!bBlocked && !Hit.bBlockingHit && !Hit.bStartPenetrating && !bStartOverlap && !bEndOverlap);
	}

	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	UBwayCharacterMovementComponent* Movement = nullptr;
	UBoxComponent* Floor = nullptr;
	FVector FloorNormal = FVector::UpVector;
	float Radius = 0.f, HalfHeight = 0.f, CapsuleSupport = 0.f;
};

bool Run(FAutomationTestBase& Test, float PitchDegrees, bool bRecontact = false)
{
	for (const int32 Hertz : {30, 60, 120})
	{
		FFixture Fixture(Test);
		if (!Fixture.Setup(PitchDegrees)) return false;
		if (bRecontact ? !Fixture.Recontact(Hertz) : !Fixture.AirStep(Hertz)) return false;
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideAirFlatForceTest, "Breakaway.CombatPolish.Slide.Physics.AirContact.FlatForces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideAirFlatForceTest::RunTest(const FString&)
{
	return BwaySlideAirContactTest::Run(*this, 0.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideAirRampForceTest, "Breakaway.CombatPolish.Slide.Physics.AirContact.RampForces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideAirRampForceTest::RunTest(const FString&)
{
	return BwaySlideAirContactTest::Run(*this, 25.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideAirRecontactTest, "Breakaway.CombatPolish.Slide.Physics.AirContact.Recontact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideAirRecontactTest::RunTest(const FString&)
{
	return BwaySlideAirContactTest::Run(*this, 0.f, true);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideGroundedRampForceTest, "Breakaway.CombatPolish.Slide.Physics.AirContact.GroundedRampForces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideGroundedRampForceTest::RunTest(const FString&)
{
	for (const int32 Hertz : {30, 60, 120})
	{
		BwaySlideAirContactTest::FFixture Fixture(*this);
		if (!Fixture.Setup(25.f, 0.1f, FVector(0.f, 500.f, -50.f)) || !Fixture.GroundedRampStep(Hertz)) return false;
	}
	return true;
}
#endif
