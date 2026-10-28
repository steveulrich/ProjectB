#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

namespace BwaySlideStepDownTest
{
constexpr float StepSeconds = 1.f / 30.f;
constexpr float InitialSpeed = 1600.f;
const float InitialFloorGap = (UCharacterMovementComponent::MIN_FLOOR_DIST + UCharacterMovementComponent::MAX_FLOOR_DIST) * 0.5f;
constexpr float BoundaryMargin = 1.f;

void GetCases(TArray<FString>& Names, TArray<FString>& Commands)
{
	for (const TCHAR* Name : { TEXT("FlatContinuity"), TEXT("Dorado18_28cm"), TEXT("BelowMaxStepHeight"), TEXT("AboveMaxStepHeight"), TEXT("OpenLedge") })
	{
		Names.Add(Name);
		Commands.Add(Name);
	}
}

// A real capsule crosses a platform lip in one production 30 Hz step. Neither
// movement tuning nor floor results are replaced. The initial velocity is a
// fixture seed; authored gravity, friction, sweeps and step-down decide the end.
class FFixture
{
public:
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture() { if (World) World->DestroyWorld(false); }

	bool Setup(const FString& Case)
	{
		const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test.TestNotNull(TEXT("Step-down collision world"), World)) return false;
		UClass* PawnClass = LoadClass<ABwayCharacterWithAbilities>(nullptr,
			TEXT("/BreakawayCore/Characters/B_BW_Character_Humanoid.B_BW_Character_Humanoid_C"));
		if (!Test.TestNotNull(TEXT("Authored humanoid class"), PawnClass)) return false;
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(PawnClass, FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator, Spawn);
		if (!Test.TestNotNull(TEXT("Authored production pawn"), Pawn)) return false;
		Move = Pawn->GetBwayCharacterMovement();
		if (!Test.TestNotNull(TEXT("Production movement component"), Move)
			|| !Test.TestTrue(TEXT("Valid authority movement data"), Pawn->HasAuthority() && Move->HasValidData())) return false;
		Move->bRunPhysicsWithNoController = true;
		Move->SetMovementMode(MOVE_Custom, uint8(ECustomMovementMode::CMOVE_Slide));
		const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
		Radius = Capsule->GetScaledCapsuleRadius();
		HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		if (!Test.TestTrue(TEXT("Authored step height admits the explicit Dorado drop"), Move->MaxStepHeight > 18.28f + BoundaryMargin)
			|| !Test.TestTrue(TEXT("Seed stays below the authored slide cap"), InitialSpeed < Move->GetMaxSpeed())
			|| !Test.TestTrue(TEXT("Authored friction and downward gravity remain enabled"), Move->SlideBaseFrictionFactor > 0.f && Move->GetGravityZ() < 0.f)) return false;

		if (Case == TEXT("FlatContinuity")) Drop = 0.f;
		else if (Case == TEXT("Dorado18_28cm")) Drop = 18.28f;
		else if (Case == TEXT("BelowMaxStepHeight")) Drop = Move->MaxStepHeight - BoundaryMargin;
		else if (Case == TEXT("AboveMaxStepHeight")) Drop = Move->MaxStepHeight + BoundaryMargin;
		else if (Case == TEXT("OpenLedge")) bOpenLedge = true;
		else { Test.AddError(FString::Printf(TEXT("Unknown step-down case: %s"), *Case)); return false; }
		Upper = AddFloor(-1000.f, 0.f);
		if (!bOpenLedge) Lower = AddFloor(1000.f, -Drop);
		if (!Test.TestNotNull(TEXT("Real upper platform"), Upper)
			|| (!bOpenLedge && !Test.TestNotNull(TEXT("Real destination platform"), Lower))) return false;
		Start = FVector(-1.f, 0.f, HalfHeight + InitialFloorGap);
		Pawn->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
		Move->Velocity = FVector(InitialSpeed, 0.f, 0.f);
		FFindFloorResult Floor;
		Move->FindFloor(Start, Floor, false);
		return Test.TestTrue(TEXT("Initial real floor is the nonpenetrating upper face"),
			Floor.bBlockingHit && Floor.HitResult.IsValidBlockingHit() && !Floor.HitResult.bStartPenetrating
			&& Floor.HitResult.GetComponent() == Upper && Floor.HitResult.ImpactNormal.Z > 0.999f
			&& Floor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST)
			&& Test.TestTrue(TEXT("Fixture has no steering input"), Move->GetCurrentAcceleration().IsNearlyZero()
				&& Pawn->GetPendingMovementInputVector().IsNearlyZero());
	}

	bool Run(const FString& Case)
	{
		const float FrictionRetention = 1.f - FMath::Clamp(Move->SlideBaseFrictionFactor * StepSeconds, 0.f, 1.f);
		const float ExpectedHorizontalSpeed = InitialSpeed * FrictionRetention;
		const float UnsnappedClearance = InitialFloorGap + Move->GetGravityZ() * StepSeconds * StepSeconds * FrictionRetention;
		// Nominal platform height is not the correction distance: the capsule
		// retains clearance and gravity already moves it during this step. Keep
		// the two boundary fixtures on opposite sides of the actual snap limit.
		if (!Test.TestTrue(TEXT("Fixture gravity leaves a clear path above the source top"), UnsnappedClearance > 0.1f)) return false;
		const float UnsnappedDrop = Drop + UnsnappedClearance - UCharacterMovementComponent::MIN_FLOOR_DIST;
		if (Case == TEXT("BelowMaxStepHeight")
			&& !Test.TestTrue(TEXT("Lower boundary is inside the actual correction limit"), UnsnappedDrop < Move->MaxStepHeight)) return false;
		if (Case == TEXT("AboveMaxStepHeight")
			&& !Test.TestTrue(TEXT("Upper boundary exceeds the actual correction limit"), UnsnappedDrop > Move->MaxStepHeight)) return false;
		// A wide enough endpoint makes the source lip incapable of supporting the
		// capsule. Fail setup if a later authored tuning change invalidates this.
		if (!Test.TestTrue(TEXT("Authored step clears the entire source lip"), Start.X + ExpectedHorizontalSpeed * StepSeconds > Radius + 2.f)) return false;
		Move->PhysCustom(StepSeconds, 0);
		const FVector End = Pawn->GetActorLocation();
		const float DestinationGap = End.Z - HalfHeight + Drop;
		Test.AddInfo(FString::Printf(TEXT("SlideStepDown: case=%s dt=%.6f drop=%.6f maxStepHeight=%.6f radius=%.6f start=%s end=%s velocity=%s destinationGap=%.6f floorDistance=%.6f mode=%d custom=%d"),
			*Case, StepSeconds, Drop, Move->MaxStepHeight, Radius, *Start.ToString(), *End.ToString(), *Move->Velocity.ToString(),
			DestinationGap, Move->CurrentFloor.GetDistanceToFloor(), int32(Move->MovementMode), int32(Move->CustomMovementMode)));
		bool bOkay = Test.TestTrue(TEXT("Actual capsule cleared the source lip"), End.X > Radius + 2.f);
		bOkay &= Test.TestTrue(TEXT("Transition preserves horizontal speed after authored friction"),
			FMath::IsNearlyEqual(Move->Velocity.X, ExpectedHorizontalSpeed, 1.f) && FMath::Abs(Move->Velocity.Y) < 0.1f);
		bOkay &= Test.TestTrue(TEXT("Horizontal displacement stays within the single-step budget"),
			FMath::IsNearlyEqual(End.X - Start.X, ExpectedHorizontalSpeed * StepSeconds, 0.5f));
		if (bOpenLedge)
		{
			bOkay &= Test.TestTrue(TEXT("A genuine open ledge switches to falling"), Move->IsFalling());
			bOkay &= Test.TestFalse(TEXT("Open ledge has no cached blocking floor"), Move->CurrentFloor.bBlockingHit);
			bOkay &= Test.TestTrue(TEXT("Open ledge never invents a downward snap"), Start.Z - End.Z < 2.f);
			return bOkay;
		}

		// Independently verify destination geometry even when FindFloor rejects
		// support beyond its search distance. The authored MaxStepHeight remains
		// unchanged; the test never injects a cached floor or synthetic hit.
		FHitResult Destination;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BwaySlideStepDownDestination), false, Pawn);
		const bool bDestinationHit = World->LineTraceSingleByChannel(Destination, End,
			End - FVector(0.f, 0.f, HalfHeight + Move->MaxStepHeight + 10.f), ECC_Visibility, Query);
		bOkay &= Test.TestTrue(TEXT("Independent trace finds the real destination platform"), bDestinationHit
			&& Destination.GetComponent() == Lower && FMath::IsNearlyEqual(Destination.ImpactPoint.Z, -Drop, 0.01f));
		bOkay &= Test.TestTrue(TEXT("Capsule never penetrates the destination"), DestinationGap >= -0.1f);
		if (Case == TEXT("AboveMaxStepHeight"))
		{
			bOkay &= Test.TestTrue(TEXT("Correction beyond MaxStepHeight does not receive a snap"),
				DestinationGap - UCharacterMovementComponent::MIN_FLOOR_DIST > Move->MaxStepHeight);
			bOkay &= Test.TestTrue(TEXT("Rejected step retains only contact-resolved gravity displacement"), Start.Z - End.Z < 2.f);
		}
		else
		{
			bOkay &= Test.TestTrue(TEXT("Supported transition stays in slide mode"), Move->IsSliding());
			bOkay &= Test.TestTrue(TEXT("Destination retains a real nonpenetrating floor result"), Move->CurrentFloor.bBlockingHit
				&& Move->CurrentFloor.HitResult.IsValidBlockingHit() && !Move->CurrentFloor.HitResult.bStartPenetrating
				&& Move->CurrentFloor.HitResult.GetComponent() == Lower);
			bOkay &= Test.TestTrue(TEXT("Accepted destination is inside the engine ground-contact band"),
				Move->CurrentFloor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST
				&& DestinationGap <= UCharacterMovementComponent::MAX_FLOOR_DIST + 0.1f);
			if (Drop > 0.f)
			{
				bOkay &= Test.TestTrue(TEXT("Step-down actually sweeps most of the declared drop"),
					Start.Z - End.Z >= Drop - UCharacterMovementComponent::MAX_FLOOR_DIST);
			}
		}
		return bOkay;
	}

private:
	UBoxComponent* AddFloor(float CenterX, float TopZ)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor) return nullptr;
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetBoxExtent(FVector(1000.f, 500.f, 50.f));
		Box->RegisterComponent();
		Actor->SetActorLocation(FVector(CenterX, 0.f, TopZ - 50.f));
		return Box;
	}

	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	UBwayCharacterMovementComponent* Move = nullptr;
	UBoxComponent* Upper = nullptr;
	UBoxComponent* Lower = nullptr;
	FVector Start = FVector::ZeroVector;
	float Radius = 0.f;
	float HalfHeight = 0.f;
	float Drop = 0.f;
	bool bOpenLedge = false;
};

bool RunCase(FAutomationTestBase& Test, const FString& Case)
{
	FFixture Fixture(Test);
	return Fixture.Setup(Case) && Fixture.Run(Case);
}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FBwayMovementSlideStepDownTest, "Breakaway.Movement.Slide.StepDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FBwayMovementSlideStepDownTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	BwaySlideStepDownTest::GetCases(Names, Commands);
}
bool FBwayMovementSlideStepDownTest::RunTest(const FString& Parameters)
{
	return BwaySlideStepDownTest::RunCase(*this, Parameters);
}

// Keep the historical CombatPolish prefix useful while the movement contract
// selects its own prefix. Both registrations execute the same five real cases.
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FBwayCombatPolishSlideStepDownTest, "Breakaway.CombatPolish.Slide.Physics.StepDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FBwayCombatPolishSlideStepDownTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	BwaySlideStepDownTest::GetCases(Names, Commands);
}
bool FBwayCombatPolishSlideStepDownTest::RunTest(const FString& Parameters)
{
	return BwaySlideStepDownTest::RunCase(*this, Parameters);
}
#endif
