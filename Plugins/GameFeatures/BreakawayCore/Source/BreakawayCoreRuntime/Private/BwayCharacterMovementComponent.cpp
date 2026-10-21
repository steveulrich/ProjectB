// Fill out your copyright notice in the Description page of Project Settings.
// Source: LyraCharacterMovementComponent_Slide.cpp
#include "BwayCharacterMovementComponent.h"

#include "AbilitySystemGlobals.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Movement/BwayMovementFeelConfig.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "Misc/ScopeExit.h"

DEFINE_LOG_CATEGORY_STATIC(LogBreakawayMovement, Log, All);

struct FBwaySlideContactContext
{
	float StepSeconds = 0.f;
	float RemainingFraction = 1.f;
	float FrontalWallSeconds = 0.f;
};

namespace BwayMovementCVars
{
	static TAutoConsoleVariable<int32> CVarDebugSlideJump(
		TEXT("bway.Movement.DebugSlideJump"),
		0,
		TEXT("When non-zero, draw slide-jump debug state above the character."),
		ECVF_Cheat);
}

// --- Constructor ---
UBwayCharacterMovementComponent::UBwayCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NavAgentProps.bCanCrouch = true;
}

// --- Initialization ---
void UBwayCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (GetCharacterOwner())
	{
		AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetCharacterOwner());
	}
	DefaultGravityScale = GravityScale;
}

void UBwayCharacterMovementComponent::ApplyMovementFeelConfig(const UBwayMovementFeelConfig* Config)
{
	if (!Config)
	{
		return;
	}

	SlideJumpMomentumBoost = Config->SlideJumpMomentumBoost;
	SlideJumpMinHorizontalSpeed = Config->SlideJumpMinHorizontalSpeed;
	SlideJumpMaxHorizontalSpeed = Config->SlideJumpMaxHorizontalSpeed;
	SlideJumpAirControl = Config->SlideJumpAirControl;
	SlideJumpGravityScale = Config->SlideJumpGravityScale;
	SlideJumpLandedVelocityFactor = Config->SlideJumpLandedVelocityFactor;
	bPreserveSlideJumpUntilLanding = Config->bPreserveSlideJumpUntilLanding;
}

bool UBwayCharacterMovementComponent::ComputeSlideJumpLaunchVelocity(
	FVector InVelocity,
	float JumpZ,
	float MomentumBoost,
	float MinHorizontalSpeed,
	float MaxHorizontalSpeed,
	FVector& OutVelocity)
{
	if (MinHorizontalSpeed > MaxHorizontalSpeed)
	{
		OutVelocity = InVelocity;
		return false;
	}

	OutVelocity = InVelocity;
	OutVelocity.X *= MomentumBoost;
	OutVelocity.Y *= MomentumBoost;

	if (JumpZ > 0.f)
	{
		OutVelocity.Z = FMath::Max(OutVelocity.Z, JumpZ);
	}

	const float HorizSpeedSq = OutVelocity.SizeSquared2D();
	if (HorizSpeedSq > KINDA_SMALL_NUMBER)
	{
		const float HorizSpeed = FMath::Sqrt(HorizSpeedSq);
		const float ClampedSpeed = FMath::Clamp(HorizSpeed, MinHorizontalSpeed, MaxHorizontalSpeed);
		const float Scale = ClampedSpeed / HorizSpeed;
		OutVelocity.X *= Scale;
		OutVelocity.Y *= Scale;
	}

	return true;
}

void UBwayCharacterMovementComponent::ClearSlideJumpState()
{
	bIsSlideJumping = false;
}

// --- Movement Mode Change Handling ---
void UBwayCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	UE_LOG(LogBreakawayMovement, Verbose, TEXT("MovementMode Changed: %s"), *UEnum::GetDisplayValueAsText(MovementMode).ToString());

	if (IsSliding())
	{
		DefaultWalkableFloorAngle = GetWalkableFloorAngle();
		DefaultRotationRate = RotationRate;
		bPreSlideOrientRotationToMovement = bOrientRotationToMovement;

		SetWalkableFloorAngle(SlideWalkableFloorAngle);
		bOrientRotationToMovement = true;
		RotationRate = FRotator(0.f, SlideCharacterRotationSpeed, 0.f);

		StartSlide();
	}
	else if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == (uint8)ECustomMovementMode::CMOVE_Slide)
	{
		SetWalkableFloorAngle(DefaultWalkableFloorAngle);
		RotationRate = DefaultRotationRate;
		bOrientRotationToMovement = (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
			&& bPreSlideOrientRotationToMovement;

		if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && MovementMode == MOVE_Falling)
		{
			bIsSlideJumping = true;
		}

		EndSlide();
	}

	// Clear slide-jump on mode interruption / non-preserve leaving Falling.
	// Landing velocity factor is owned by ProcessLanded (captures flag before Super).
	if (bIsSlideJumping && PreviousMovementMode == MOVE_Falling && MovementMode != MOVE_Falling)
	{
		const bool bLandedOnGround = (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking);
		const bool bIsSimProxy = CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy;
		if (!bPreserveSlideJumpUntilLanding || !bLandedOnGround || bIsSimProxy)
		{
			ClearSlideJumpState();
		}
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

// --- Pre-Physics Update ---
void UBwayCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (IsSliding())
	{
		if (CheckShouldEndSlide())
		{
			const EMovementMode NewMode = IsFalling() ? MOVE_Falling : MOVE_Walking;
			SetMovementMode(NewMode);
			return;
		}
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

// --- Slide End Condition Check ---
bool UBwayCharacterMovementComponent::CheckShouldEndSlide()
{
	ACharacter* Owner = GetCharacterOwner();
	if (!Owner) return true;

	if (Velocity.SizeSquared() < FMath::Square(MinSlideSpeed))
	{
		UE_LOG(LogBreakawayMovement, Verbose, TEXT("CheckShouldEndSlide: TRUE Velocity too low"));
		return true;
	}

	UE_LOG(LogBreakawayMovement, VeryVerbose, TEXT("CheckShouldEndSlide: FALSE - continue slide"));
	return false;
}

// --- Speed and Braking Overrides ---
float UBwayCharacterMovementComponent::GetMaxSpeed() const
{
	const AActor* OwnerActor = GetOwner();
	const UAbilitySystemComponent* ASC = OwnerActor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor) : nullptr;
	// Sliding computes its own speed instead of calling the Lyra implementation.
	if (ASC && ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)) return 0.f;

	float BaseMaxSpeed = 0.f;
	if (IsSliding())
	{
		float CurrentMaxSpeedFactor, SlopeAccelFactor, FrictionMultiplier;
		GetCurrentSlideModifiers(CurrentMaxSpeedFactor, SlopeAccelFactor, FrictionMultiplier);
		BaseMaxSpeed = BaseSlideSpeed * CurrentMaxSpeedFactor;
	}
	else
	{
		BaseMaxSpeed = Super::GetMaxSpeed();
	}

	float SpeedMultiplier = 1.f;
	if (ASC)
	{
		if (const UBwayHeroAttributeSet* HeroSet = ASC->GetSet<UBwayHeroAttributeSet>())
		{
			SpeedMultiplier = HeroSet->GetMoveSpeedMultiplier();
		}
	}

	return BaseMaxSpeed * SpeedMultiplier;
}

float UBwayCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (IsSliding())
	{
		return BrakingDecelerationWalking * 5.0f;
	}
	return Super::GetMaxBrakingDeceleration();
}

// --- Custom Physics Entry Point ---
void UBwayCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (CustomMovementMode == (uint8)ECustomMovementMode::CMOVE_Slide)
	{
		UE_LOG(LogBreakawayMovement, VeryVerbose, TEXT("Execute PhysSliding"));
		PhysSliding(deltaTime, Iterations);
	}
	else
	{
		UE_LOG(LogBreakawayMovement, Verbose, TEXT("In different custom mode: %u"), CustomMovementMode);
		Super::PhysCustom(deltaTime, Iterations);
	}
}

float UBwayCharacterMovementComponent::GetSlideIntensity() const
{
	if (!IsSliding()) return 0.0f;

	const float MaxSpeed = GetMaxSpeed();
	if (MaxSpeed <= KINDA_SMALL_NUMBER) return 0.0f;

	return FMath::Clamp(Velocity.Size() / MaxSpeed, 0.0f, 1.0f);
}

// --- Core Slide Physics Implementation ---
void UBwayCharacterMovementComponent::PhysSliding(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	RestorePreAdditiveRootMotionVelocity();

	if (!HasValidData() || deltaTime < MIN_TICK_TIME || Iterations >= MaxSimulationIterations)
	{
		UE_LOG(LogBreakawayMovement, Verbose, TEXT("PhysSlide - invalid data, tiny timestep, or max iterations exceeded"));
		return;
	}

	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (!CurrentFloor.bBlockingHit)
	{
		UE_LOG(LogBreakawayMovement, Verbose, TEXT("PhysSlide - not on walkable floor"));
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	UE_LOG(LogBreakawayMovement, VeryVerbose, TEXT("PhysSlide - Hit Z: %f"), CurrentFloor.HitResult.ImpactNormal.Z);

	const float SlopeAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(CurrentFloor.HitResult.ImpactNormal.Z));
	// FindFloor also finds nearby geometry while the capsule is airborne. Use
	// the engine's contact distance, without rejecting authored slide ramps
	// through the stricter walking-angle test, before applying ground forces.
	const float FloorDistance = CurrentFloor.GetDistanceToFloor();
	const bool bHasSlideGroundContact = CurrentFloor.HitResult.IsValidBlockingHit()
		&& FMath::IsFinite(FloorDistance) && FloorDistance <= MAX_FLOOR_DIST;
	constexpr float MinSlideFloorZ = 0.80f;
	const FVector StartFloorNormal = CurrentFloor.HitResult.ImpactNormal;
	const FVector InputAccelDir = Acceleration.GetSafeNormal();

	if (bHasSlideGroundContact && StartFloorNormal.Z >= MinSlideFloorZ
		&& FVector::DotProduct(Velocity, StartFloorNormal) > KINDA_SMALL_NUMBER)
	{
		const float PreTangentSpeed = Velocity.Size();
		const FVector TangentDir = FVector::VectorPlaneProject(Velocity, StartFloorNormal).GetSafeNormal();
		if (!TangentDir.IsNearlyZero())
		{
			Velocity = TangentDir * PreTangentSpeed;
		}
	}

	Acceleration = FVector::ZeroVector;

	if (bHasSlideGroundContact)
	{
		ApplySlideSlopeAcceleration(deltaTime, SlopeAngleDegrees);
	}
	// Gravity is acceleration; the velocity integration below applies the timestep.
	Acceleration.Z += GetGravityZ();

	Velocity += Acceleration * deltaTime;

	if (bHasSlideGroundContact)
	{
		ApplySlideFriction(deltaTime, SlopeAngleDegrees);
	}
	ApplySlideSteering(deltaTime, InputAccelDir);

	Velocity = Velocity.GetClampedToMaxSize(GetMaxSpeed());

	const float PreMoveSurfaceSpeed = (bHasSlideGroundContact && StartFloorNormal.Z >= MinSlideFloorZ)
		? FVector::VectorPlaneProject(Velocity, StartFloorNormal).Size()
		: Velocity.Size2D();

	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
#if !UE_BUILD_SHIPPING
	FBwaySlideReviewObservation Review;
	bool bReviewConcave = false;
	Review.Start = OldLocation; Review.BeforeVelocity = Velocity; Review.DeltaSeconds = deltaTime;
	ON_SCOPE_EXIT
	{
		if (bReviewConcave && SlideReviewObserver.IsBound() && HasValidData())
		{
			Review.End = UpdatedComponent->GetComponentLocation(); Review.AfterVelocity = Velocity;
			SlideReviewObserver.Broadcast(Review);
		}
	};
#endif
	FHitResult Hit(1.f);
	FVector Adjusted = Velocity * deltaTime;
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		FBwaySlideContactContext Contacts;
		Contacts.StepSeconds = deltaTime;
		TGuardValue<FBwaySlideContactContext*> ContactScope(SlideContactContext, &Contacts);
		const float FirstRemaining = 1.f - Hit.Time;
		const FVector FirstFloorNormal = Hit.ImpactNormal;
		HandleImpact(Hit, deltaTime, Adjusted);
		const float SlideTimeApplied = SlideAlongSurface(Adjusted, FirstRemaining, Hit.Normal, Hit, true);
		if (!HasValidData()) return;

		bool bConcaveFloorTransition = false;
		FVector ConcaveFloorNormal = FVector::ZeroVector;
		if (Hit.bBlockingHit && Hit.Time < 1.f && Contacts.FrontalWallSeconds <= 0.f
			&& FirstFloorNormal.Z >= MinSlideFloorZ && Hit.ImpactNormal.Z >= MinSlideFloorZ
			&& FVector::DotProduct(FirstFloorNormal, Hit.ImpactNormal) < 0.999f)
		{
			bConcaveFloorTransition = true;
			ConcaveFloorNormal = Hit.ImpactNormal;
			const float SecondRemaining = FirstRemaining * (1.f - SlideTimeApplied);
#if !UE_BUILD_SHIPPING
			bReviewConcave = true;
			Review.FirstHitFraction = 1.f - FirstRemaining;
			Review.SlideConsumedFraction = SlideTimeApplied;
			Review.FinalHitFraction = Hit.Time;
			Review.ContinuationFraction = SecondRemaining;
			Review.FirstNormal = FirstFloorNormal; Review.FinalNormal = Hit.ImpactNormal;
#endif
			const FVector SecondTangentDir = FVector::VectorPlaneProject(Velocity, ConcaveFloorNormal).GetSafeNormal();
			if (!SecondTangentDir.IsNearlyZero() && SecondRemaining > KINDA_SMALL_NUMBER)
			{
				FHitResult SecondHit(1.f);
				const FVector SecondDelta = SecondTangentDir * (PreMoveSurfaceSpeed * deltaTime);
				SafeMoveUpdatedComponent(SecondDelta * SecondRemaining, UpdatedComponent->GetComponentQuat(), true, SecondHit);
				if (SecondHit.Time < 1.f)
				{
					HandleImpact(SecondHit, deltaTime * SecondRemaining, SecondDelta);
					SlideAlongSurface(SecondDelta, SecondRemaining * (1.f - SecondHit.Time), SecondHit.Normal, SecondHit, true);
					if (!HasValidData()) return;
				}
			}
		}
		else if (bHasSlideGroundContact && Contacts.FrontalWallSeconds <= 0.f
			&& StartFloorNormal.Z >= MinSlideFloorZ && FirstFloorNormal.Z >= MinSlideFloorZ
			&& FVector::DotProduct(StartFloorNormal, FirstFloorNormal) < 0.999f)
		{
			bConcaveFloorTransition = true;
			ConcaveFloorNormal = FirstFloorNormal;
		}

		if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			// SlideAlongSurface resolves displacement through every contact, but does
			// not update Velocity. Retaining inward gravity makes a later floor hit
			// look like a frontal wall and repeatedly brakes horizontal motion.
			Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
			if (bConcaveFloorTransition && Contacts.FrontalWallSeconds <= 0.f)
			{
				const FVector TransitionTangentDir = FVector::VectorPlaneProject(Velocity, ConcaveFloorNormal).GetSafeNormal();
				if (!TransitionTangentDir.IsNearlyZero())
				{
					Velocity = TransitionTangentDir * FMath::Max(Velocity.Size(), PreMoveSurfaceSpeed);
				}
			}
		}
		if (Contacts.FrontalWallSeconds > 0.f)
		{
			float CurrentMaxSpeedFactor, SlopeAccelFactor, CurrentFrictionMultiplier;
			GetCurrentSlideModifiers(CurrentMaxSpeedFactor, SlopeAccelFactor, CurrentFrictionMultiplier);
			Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, Contacts.FrontalWallSeconds, SlideWallHitFriction * CurrentFrictionMultiplier);
		}
	}
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (bHasSlideGroundContact && !bJustTeleported && CurrentFloor.bBlockingHit
		&& CurrentFloor.HitResult.IsValidBlockingHit() && !CurrentFloor.HitResult.bStartPenetrating
		&& CurrentFloor.HitResult.ImpactNormal.Z >= MinSlideFloorZ)
	{
		const float PostFloorDist = CurrentFloor.GetDistanceToFloor();
		bool bSteppedDown = false;
		if (FMath::IsFinite(PostFloorDist) && PostFloorDist > MAX_FLOOR_DIST && PostFloorDist <= MaxStepHeight)
		{
			FHitResult StepDownHit(1.f);
			const float StepDownDrop = FMath::Max(0.f, PostFloorDist - MIN_FLOOR_DIST);
			SafeMoveUpdatedComponent(FVector(0.f, 0.f, -StepDownDrop), UpdatedComponent->GetComponentQuat(), true, StepDownHit);
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
			bSteppedDown = true;
		}
		if (CurrentFloor.bBlockingHit && CurrentFloor.HitResult.IsValidBlockingHit()
			&& !CurrentFloor.HitResult.bStartPenetrating && CurrentFloor.HitResult.ImpactNormal.Z >= MinSlideFloorZ
			&& (bSteppedDown || FVector::DotProduct(Velocity, CurrentFloor.HitResult.ImpactNormal) > KINDA_SMALL_NUMBER))
		{
			const float CurrentSpeed = FMath::Max(Velocity.Size(), PreMoveSurfaceSpeed);
			const FVector TangentDir = FVector::VectorPlaneProject(Velocity, CurrentFloor.HitResult.ImpactNormal).GetSafeNormal();
			if (!TangentDir.IsNearlyZero())
			{
				Velocity = TangentDir * CurrentSpeed;
			}
		}
	}
	if (!CurrentFloor.bBlockingHit)
	{
		SetMovementMode(MOVE_Falling);
	}
	else if (Velocity.SizeSquared() < KINDA_SMALL_NUMBER && Acceleration.IsNearlyZero())
	{
		Velocity = FVector::ZeroVector;
		Acceleration = FVector::ZeroVector;
	}

	if (HasAnimRootMotion())
	{
		//PreAdditiveRootMotionVelocity = Velocity;
	}
}

void UBwayCharacterMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	if (SlideContactContext && Hit.IsValidBlockingHit() && CharacterOwner && UpdatedComponent)
	{
		// Each subsequent sweep consumes a fraction of the previously remaining
		// move. Capture intermediate walls here: the final Hit may be overwritten
		// by a free tangent move inside SlideAlongSurface/TwoWallAdjust.
		SlideContactContext->RemainingFraction *= 1.f - FMath::Clamp(Hit.Time, 0.f, 1.f);
		const auto* Capsule = CharacterOwner->GetCapsuleComponent();
		const float LowerHemisphereTop = UpdatedComponent->GetComponentLocation().Z
			- (Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
		const bool bSupportingContact = Hit.ImpactNormal.Z > KINDA_SMALL_NUMBER
			&& Hit.ImpactPoint.Z <= LowerHemisphereTop + KINDA_SMALL_NUMBER;
		if (!bSupportingContact && FVector::DotProduct(MoveDelta.GetSafeNormal(), Hit.Normal) < -0.5f)
		{
			SlideContactContext->FrontalWallSeconds = FMath::Max(SlideContactContext->FrontalWallSeconds,
				SlideContactContext->StepSeconds * SlideContactContext->RemainingFraction);
		}
	}
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

// --- Slope Acceleration Logic ---
void UBwayCharacterMovementComponent::ApplySlideSlopeAcceleration(float DeltaTime, float SlopeAngleDegrees)
{
	float CurrentMaxSpeedFactor, CurrentSlopeAccelFactor, FrictionMultiplier;
	GetCurrentSlideModifiers(CurrentMaxSpeedFactor, CurrentSlopeAccelFactor, FrictionMultiplier);

	if (SlopeAngleDegrees >= MinSlopeAngleForAccel)
	{
		const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;
		const FVector RightVector = FVector::CrossProduct(FloorNormal, FVector::UpVector);
		const FVector DownSlopeDirection = FVector::CrossProduct(FloorNormal, RightVector).GetSafeNormal();

		if (!DownSlopeDirection.IsNearlyZero())
		{
			const float SlopeAccelMagnitude = BaseSlideSpeed * CurrentSlopeAccelFactor;
			Acceleration += DownSlopeDirection * SlopeAccelMagnitude;
		}
	}
}

// --- Steering Logic (Direct Velocity Rotation) ---
void UBwayCharacterMovementComponent::ApplySlideSteering(float DeltaTime, const FVector& InputAccelDir)
{
	if (InputAccelDir.IsNearlyZero() || Velocity.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector CurrentVelDir2D = Velocity.GetSafeNormal2D();
	const FVector InputDir2D = InputAccelDir.GetSafeNormal2D();

	if (CurrentVelDir2D.IsNearlyZero() || InputDir2D.IsNearlyZero()) return;

	const float Dot = FVector::DotProduct(CurrentVelDir2D, InputDir2D);
	const float SteerThresholdCosine = FMath::Cos(FMath::DegreesToRadians(5.0f));

	if (Dot < SteerThresholdCosine)
	{
		const float AngleRad = FMath::Acos(Dot);
		const float CrossZ = FVector::CrossProduct(CurrentVelDir2D, InputDir2D).Z;
		const float RotationSign = FMath::Sign(CrossZ);

		const float MaxAngleThisFrame = SlideSteerSpeed * DeltaTime;
		const float ClampedAngleRad = FMath::Min(AngleRad, MaxAngleThisFrame);

		Velocity = Velocity.RotateAngleAxisRad(ClampedAngleRad * RotationSign, FVector::UpVector);
	}
}

// --- Friction Logic ---
void UBwayCharacterMovementComponent::ApplySlideFriction(float DeltaTime, float SlopeAngleDegrees)
{
	float CurrentMaxSpeedFactor, SlopeAccelFactor, CurrentFrictionMultiplier;
	GetCurrentSlideModifiers(CurrentMaxSpeedFactor, SlopeAccelFactor, CurrentFrictionMultiplier);

	const float ClampedSlopeFactor = FMath::Clamp(SlopeAngleDegrees / 90.0f, 0.0f, 1.0f);
	const float SlopePowerFactor = FMath::Pow(1.0f - ClampedSlopeFactor, SlideFrictionPower);

	float FrictionToApply = SlideBaseFrictionFactor * SlopePowerFactor * CurrentFrictionMultiplier;

	if (FrictionToApply > KINDA_SMALL_NUMBER && Velocity.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaTime, FrictionToApply);
	}
}

// --- Falling Physics Override ---
void UBwayCharacterMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	const float PreviousGravityScale = GravityScale;
	const float PreviousAirControl = AirControl;

	if (bIsSlideJumping)
	{
		GravityScale = SlideJumpGravityScale;
		AirControl = SlideJumpAirControl;
	}

	Super::PhysFalling(deltaTime, Iterations);

	GravityScale = PreviousGravityScale;
	AirControl = PreviousAirControl;
}

// --- Landing Logic Override ---
void UBwayCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	const bool bApplySlideJumpLanding = bIsSlideJumping;

	Super::ProcessLanded(Hit, remainingTime, Iterations);

	if (bApplySlideJumpLanding)
	{
		Velocity.X *= SlideJumpLandedVelocityFactor;
		Velocity.Y *= SlideJumpLandedVelocityFactor;
		ClearSlideJumpState();
	}
}

bool UBwayCharacterMovementComponent::CanAttemptJump() const
{
	// Lyra removes crouch gate but still requires Walking/Falling; allow jump from slide.
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling() || IsSliding());
}

bool UBwayCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	const FVector ReviewBefore = Velocity;
	const bool bReviewWasJumping = CharacterOwner && CharacterOwner->bWasJumping;
#endif
	const bool bJumpingFromSlide = (IsSliding() || bIsSlideJumping)
		&& CharacterOwner && !CharacterOwner->bWasJumping;

	if (!Super::DoJump(bReplayingMoves, DeltaTime))
	{
		return false;
	}

	if (bJumpingFromSlide)
	{
		FVector LaunchVelocity;
		if (ComputeSlideJumpLaunchVelocity(
			Velocity,
			/*JumpZ already applied by Super*/ 0.f,
			SlideJumpMomentumBoost,
			SlideJumpMinHorizontalSpeed,
			SlideJumpMaxHorizontalSpeed,
			LaunchVelocity))
		{
			Velocity = LaunchVelocity;
		}

		bIsSlideJumping = true;
	}

#if !UE_BUILD_SHIPPING
	if (SlideReviewObserver.IsBound())
	{
		FBwaySlideReviewObservation Review;
		Review.bJump = true; Review.bWasJumping = bReviewWasJumping; Review.bBoostApplied = bJumpingFromSlide;
		Review.DeltaSeconds = DeltaTime; Review.BeforeVelocity = ReviewBefore; Review.AfterVelocity = Velocity;
		Review.Start = Review.End = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
		SlideReviewObserver.Broadcast(Review);
	}
#endif

	return true;
}

void UBwayCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (BwayMovementCVars::CVarDebugSlideJump.GetValueOnGameThread() != 0 && CharacterOwner && GetWorld())
	{
		const FString DebugText = FString::Printf(
			TEXT("SlideJump:%d Slide:%d AirCtrl:%.2f Grav:%.2f Speed2D:%.0f"),
			bIsSlideJumping ? 1 : 0,
			IsSliding() ? 1 : 0,
			bIsSlideJumping ? SlideJumpAirControl : AirControl,
			bIsSlideJumping ? SlideJumpGravityScale : GravityScale,
			Velocity.Size2D());

		DrawDebugString(
			GetWorld(),
			CharacterOwner->GetActorLocation() + FVector(0.f, 0.f, 100.f),
			DebugText,
			nullptr,
			FColor::Cyan,
			0.f,
			true);
	}
}

void UBwayCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bIsSlideJumping = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UBwayCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UBwayCharacterMovementComponent* MutableThis = const_cast<UBwayCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_BwayCharacter(*this);
	}

	return ClientPredictionData;
}

// --- Modifier Calculation ---
void UBwayCharacterMovementComponent::GetCurrentSlideModifiers(float& OutMaxSpeedFactor, float& OutSlopeAccelFactor, float& OutFrictionMultiplier) const
{
	OutMaxSpeedFactor = SlideMaxSpeedFactor;
	OutSlopeAccelFactor = SlideSlopeAccelerationFactor;
	OutFrictionMultiplier = 1.0f;

	if (AbilitySystemComponent && CarryingLootTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(CarryingLootTag))
	{
		OutMaxSpeedFactor *= LootMaxSpeedFactorMultiplier;
		OutSlopeAccelFactor *= LootSlopeAccelFactorMultiplier;
		OutFrictionMultiplier *= LootFrictionMultiplier;
	}
}

// --- Slide Start/End Helpers ---
void UBwayCharacterMovementComponent::StartSlide()
{
	if (CharacterOwner)
	{
		CharacterOwner->Crouch(true);
	}
}

void UBwayCharacterMovementComponent::EndSlide()
{
	if (CharacterOwner)
	{
		CharacterOwner->UnCrouch(true);
	}
}

void UBwayCharacterMovementComponent::BindAbilitySystem(UAbilitySystemComponent* InASC)
{
	UnbindAbilitySystem();

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		return;
	}

	MoveSpeedMultiplierChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()).AddUObject(
			this, &UBwayCharacterMovementComponent::HandleMoveSpeedMultiplierChanged);
	MoveSpeedRatingChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UBwayHeroAttributeSet::GetMoveSpeedRatingAttribute()).AddUObject(
			this, &UBwayCharacterMovementComponent::HandleMoveSpeedRatingChanged);
	if (const UBwayHeroAttributeSet* HeroSet = AbilitySystemComponent->GetSet<UBwayHeroAttributeSet>())
	{
		ApplyMoveSpeedRating(HeroSet->GetMoveSpeedRating());
	}
}

void UBwayCharacterMovementComponent::UnbindAbilitySystem()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()).Remove(MoveSpeedMultiplierChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UBwayHeroAttributeSet::GetMoveSpeedRatingAttribute()).Remove(MoveSpeedRatingChangedHandle);
	}
	MoveSpeedMultiplierChangedHandle.Reset();
	MoveSpeedRatingChangedHandle.Reset();
	AbilitySystemComponent = nullptr;
}

void UBwayCharacterMovementComponent::HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& ChangeData)
{
	(void)ChangeData;
}

void UBwayCharacterMovementComponent::HandleMoveSpeedRatingChanged(const FOnAttributeChangeData& ChangeData)
{
	ApplyMoveSpeedRating(ChangeData.NewValue);
}

void UBwayCharacterMovementComponent::ApplyMoveSpeedRating(float SpeedRating)
{
	// A new attribute set starts at zero until the authority initializes the hero.
	// Preserve the pawn's authored fallback until a usable replicated rating arrives.
	if (FMath::IsFinite(SpeedRating) && SpeedRating > 0.f)
	{
		MaxWalkSpeed = UBwayHeroStatsLibrary::ConvertSpeedRatingToMaxWalkSpeed(SpeedRating);
	}
	// GetMaxSpeed applies MoveSpeedMultiplier once, separately from this base value.
}

// ---------------------------------------------------------------------------
// Prediction: FSavedMove_BwayCharacter
// ---------------------------------------------------------------------------

void FSavedMove_BwayCharacter::Clear()
{
	Super::Clear();
	bSavedIsSlideJumping = 0;
}

void FSavedMove_BwayCharacter::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UBwayCharacterMovementComponent* MoveComp = Cast<UBwayCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedIsSlideJumping = MoveComp->bIsSlideJumping ? 1 : 0;
	}
}

void FSavedMove_BwayCharacter::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UBwayCharacterMovementComponent* MoveComp = Cast<UBwayCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		MoveComp->bIsSlideJumping = bSavedIsSlideJumping != 0;
	}
}

void FSavedMove_BwayCharacter::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	Super::PostUpdate(C, PostUpdateMode);

	if (const UBwayCharacterMovementComponent* MoveComp = Cast<UBwayCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedIsSlideJumping = MoveComp->bIsSlideJumping ? 1 : 0;
	}
}

bool FSavedMove_BwayCharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_BwayCharacter* NewBwayMove = static_cast<const FSavedMove_BwayCharacter*>(NewMove.Get());
	if (bSavedIsSlideJumping != NewBwayMove->bSavedIsSlideJumping)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

uint8 FSavedMove_BwayCharacter::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedIsSlideJumping)
	{
		Result |= FLAG_Custom_0;
	}
	return Result;
}

FNetworkPredictionData_Client_BwayCharacter::FNetworkPredictionData_Client_BwayCharacter(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_BwayCharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_BwayCharacter());
}
