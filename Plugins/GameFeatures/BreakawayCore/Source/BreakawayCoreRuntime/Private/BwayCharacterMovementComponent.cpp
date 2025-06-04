// Fill out your copyright notice in the Description page of Project Settings.
// Source: LyraCharacterMovementComponent_Slide.cpp
#include "BwayCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "LyraGameplayTags.h" // Or wherever tags are defined
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/KismetMathLibrary.h"
#include "Character/LyraCharacter.h" // For CharacterOwner->Jump()

// --- Constructor ---
UBwayCharacterMovementComponent::UBwayCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Sensible defaults can be set here, but primarily configured in BP Details panel
	NavAgentProps.bCanCrouch = true; // Ensure crouching is enabled if slide uses crouch height
}

// --- Initialization ---
void UBwayCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Cache ASC for efficient tag checking
	if (GetCharacterOwner())
	{
		AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetCharacterOwner());
	}
	DefaultGravityScale = GravityScale; // Cache default gravity
}

// --- Movement Mode Change Handling ---
void UBwayCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	// Check if entering the slide mode
	if (IsSliding()) // Use helper function for clarity
	{
		// --- Entering Slide Mode ---
		// Cache defaults before changing them
		DefaultWalkableFloorAngle = GetWalkableFloorAngle();
		DefaultRotationRate = RotationRate;

		// Apply slide-specific settings
		SetWalkableFloorAngle(SlideWalkableFloorAngle); // Change ground interaction
		bOrientRotationToMovement = true; // Character should face velocity
		RotationRate = FRotator(0.f, SlideCharacterRotationSpeed, 0.f); // Set high rotation rate

		StartSlide(); // Handle capsule resize, etc.
		bDidSlideFall = false; // Reset fall flag
	}
	// Check if exiting the slide mode
	else if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == (uint8)ECustomMovementMode::CMOVE_Slide)
	{
		// --- Exiting Slide Mode ---
		// Restore default settings
		SetWalkableFloorAngle(DefaultWalkableFloorAngle);
		RotationRate = DefaultRotationRate;
		// Restore orientation based on the new mode (e.g., true for Walking, false for Falling)
		bOrientRotationToMovement = (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking);

		EndSlide(); // Handle capsule restore

		// Broadcast delegate if used by GA
		// OnSlideEndDelegate.Broadcast();
	}

	// Call Super last AFTER handling custom logic for the new mode
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

// --- Pre-Physics Update ---
void UBwayCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Check for slide end conditions BEFORE physics simulation for this frame
	if (IsSliding())
	{
		if (CheckShouldEndSlide())
		{
			// Determine if jump input caused the end
			bool bJumpPressed = false;
			APlayerController* PC = CharacterOwner? Cast<APlayerController>(CharacterOwner->GetController()) : nullptr;
			UEnhancedInputComponent* EIC = PC? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
			if (EIC && JumpInputAction && CharacterOwner && CharacterOwner->CanJump()) // Check CanJump here
			{
				// Check bool value, assumes Jump IA uses Pressed/Released [11]
				bJumpPressed = EIC->GetBoundActionValue(JumpInputAction).Get<bool>();
			}

			// Transition out of slide mode
			// Determine new mode based on current state (e.g., Falling if airborne)
			const EMovementMode NewMode = IsFalling()? MOVE_Falling : MOVE_Walking;
			SetMovementMode(NewMode); // This will call OnMovementModeChanged to handle cleanup

			// If jump caused the exit, initiate the jump process
			if (bJumpPressed && CharacterOwner)
			{
				// Set flag for ProcessLanded check
				LastSlideJumpTime = GetWorld()->GetTimeSeconds();

				// Trigger the standard Jump action.
				// The modified Jump GA should handle applying the slide cooldown effect.
				CharacterOwner->Jump();
			}

			// Important: Return early as movement mode has changed, physics for the *new* mode will run.
			// Avoid calling Super::UpdateCharacterStateBeforeMovement for the old (sliding) mode.
			return;
		}
	}

	// Call Super for standard state updates (like crouch checks) if not exiting slide
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

// --- Slide End Condition Check ---
bool UBwayCharacterMovementComponent::CheckShouldEndSlide()
{
	// Already checked IsSliding() before calling this in UpdateCharacterStateBeforeMovement

	ACharacter* Owner = GetCharacterOwner();
	if (!Owner) return true;

	// 1. Check Speed: End if speed drops below minimum threshold
	if (Velocity.SizeSquared() < FMath::Square(MinSlideSpeed))
	{
		return true;
	}

	APlayerController* PC = Cast<APlayerController>(Owner->GetController());
	UEnhancedInputComponent* EIC = PC? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	if (!EIC) return true; // Cannot check inputs, safer to end

	// 2. Check Input Hold State: End if slide input is released [11, 15]
	if (SlideInputAction &&!EIC->GetBoundActionValue(SlideInputAction).Get<bool>())
	{
		return true;
	}

	// 3. Check Jump Input Press: End if jump is pressed AND character can currently jump [11]
	if (JumpInputAction && EIC->GetBoundActionValue(JumpInputAction).Get<bool>())
	{
		// Check CanJump to ensure jump is valid (e.g., grounded, not blocked by other abilities)
		if (Owner->CanJump())
		{
			return true;
		}
	}

	// 4. Check if Falling: This is handled implicitly by PhysSliding transitioning to MOVE_Falling.
	// No explicit check needed here as PhysSliding runs after this check.

	return false; // Conditions met to continue sliding
}

// --- Speed and Braking Overrides ---
float UBwayCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsSliding())
	{
		float CurrentMaxSpeedFactor, SlopeAccelFactor, FrictionMultiplier;
		GetCurrentSlideModifiers(CurrentMaxSpeedFactor, SlopeAccelFactor, FrictionMultiplier);
		return BaseSlideSpeed * CurrentMaxSpeedFactor;
	}
	return Super::GetMaxSpeed();
}

float UBwayCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	// Standard braking deceleration is not used in PhysSliding due to custom friction.
	// Return a high value if needed by other systems, but it won't directly affect slide physics.
	if (IsSliding())
	{
		return BrakingDecelerationWalking * 5.0f; // Arbitrary high value
	}
	return Super::GetMaxBrakingDeceleration();
}

// --- Custom Physics Entry Point ---
void UBwayCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (CustomMovementMode == (uint8)ECustomMovementMode::CMOVE_Slide)
	{
		PhysSliding(deltaTime, Iterations);
	}
	else
	{
		Super::PhysCustom(deltaTime, Iterations); // Handle other custom modes if any
	}
}

// --- Core Slide Physics Implementation ---
void UBwayCharacterMovementComponent::PhysSliding(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	RestorePreAdditiveRootMotionVelocity();

	if (!HasValidData() || deltaTime < MIN_TICK_TIME || Iterations >= MaxSimulationIterations) return;

	// --- Ground Check ---
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (!CurrentFloor.IsWalkableFloor())
	{
		// Became airborne - Transition to Falling
		bDidSlideFall = true; // Mark that this fall originated from a slide
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(deltaTime, Iterations); // Immediately recalculate physics for the new mode
		return;
	}

	// --- Calculate Physics Inputs ---
	const float SlopeAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(CurrentFloor.HitResult.ImpactNormal.Z));
	const FVector InputAccelDir = Acceleration.GetSafeNormal(); // Raw input direction for steering

	// --- Apply Forces and Steering ---
	// Reset acceleration for this frame's calculations
	Acceleration = FVector::ZeroVector;

	// Order matters: Apply driving forces first, then resistance (friction)
	ApplySlideSlopeAcceleration(deltaTime, SlopeAngleDegrees); // Adds to Acceleration
	Acceleration.Z += GetGravityZ() * deltaTime; // Adds standard gravity force to Acceleration

	// Apply accumulated acceleration (slope + gravity) to velocity
	Velocity += Acceleration * deltaTime;

	// Apply friction *after* acceleration forces
	ApplySlideFriction(deltaTime, SlopeAngleDegrees); // Modifies Velocity directly or via opposing accel

	// Apply steering *after* forces and friction (directly rotates Velocity)
	ApplySlideSteering(deltaTime, InputAccelDir);

	// Clamp speed after all modifications
	Velocity = Velocity.GetClampedToMaxSize(GetMaxSpeed());

	// --- Perform Movement Update ---
	Iterations++;
	bJustTeleported = false;
	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FHitResult Hit(1.f);
	FVector Adjusted = Velocity * deltaTime;
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);

	// --- Handle Movement Results ---
	if (Hit.Time < 1.f) // Hit something
	{
		// Apply wall friction if hitting a wall significantly opposing movement
		float CurrentMaxSpeedFactor, SlopeAccelFactor, CurrentFrictionMultiplier;
		GetCurrentSlideModifiers(CurrentMaxSpeedFactor, SlopeAccelFactor, CurrentFrictionMultiplier);
		if (FVector::DotProduct(Velocity.GetSafeNormal(), Hit.Normal) < -0.5f) // Check angle of impact
		{
			// Apply high wall friction using VInterpTo for immediate effect
			Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, deltaTime * (1.f - Hit.Time), SlideWallHitFriction * CurrentFrictionMultiplier);
		}

		// Standard impact handling and surface sliding
		HandleImpact(Hit, deltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	// Re-check ground state after movement, might have slid off an edge
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (!CurrentFloor.IsWalkableFloor())
	{
		bDidSlideFall = true; // Mark fall origin
		SetMovementMode(MOVE_Falling);
		// No need for StartNewPhysics here, will happen next tick
	}
	else if (Velocity.SizeSquared() < KINDA_SMALL_NUMBER && Acceleration.IsNearlyZero())
	{
		// Came to a stop naturally
		Velocity = FVector::ZeroVector;
		Acceleration = FVector::ZeroVector;
		// CheckShouldEndSlide will likely catch this via MinSlideSpeed next frame, but zeroing here is cleaner.
	}

	// Store root motion if applicable
	if (HasAnimRootMotion())
	{
		//PreAdditiveRootMotionVelocity = Velocity;
	}
}

// --- Slope Acceleration Logic ---
void UBwayCharacterMovementComponent::ApplySlideSlopeAcceleration(float DeltaTime, float SlopeAngleDegrees)
{
	float CurrentMaxSpeedFactor, CurrentSlopeAccelFactor, FrictionMultiplier;
	GetCurrentSlideModifiers(CurrentMaxSpeedFactor, CurrentSlopeAccelFactor, FrictionMultiplier);

	if (SlopeAngleDegrees >= MinSlopeAngleForAccel)
	{
		// Calculate direction down the slope (perpendicular to floor normal and horizontal plane)
		const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;
		const FVector RightVector = FVector::CrossProduct(FloorNormal, FVector::UpVector);
		const FVector DownSlopeDirection = FVector::CrossProduct(RightVector, FloorNormal).GetSafeNormal();

		if (!DownSlopeDirection.IsNearlyZero())
		{
			// Calculate acceleration magnitude based on parameters
			const float SlopeAccelMagnitude = BaseSlideSpeed * CurrentSlopeAccelFactor;
			// Add to the Acceleration vector for standard integration in PhysSliding
			// This approach integrates better with CMC's prediction than directly adding to Velocity
			Acceleration += DownSlopeDirection * SlopeAccelMagnitude;
		}
	}
	// Note: Acceleration is applied to Velocity later in PhysSliding
}

// --- Steering Logic (Direct Velocity Rotation) ---
void UBwayCharacterMovementComponent::ApplySlideSteering(float DeltaTime, const FVector& InputAccelDir)
{
	// No steering if no input or not moving
	if (InputAccelDir.IsNearlyZero() || Velocity.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector CurrentVelDir2D = Velocity.GetSafeNormal2D();
	const FVector InputDir2D = InputAccelDir.GetSafeNormal2D();

	if (CurrentVelDir2D.IsNearlyZero() || InputDir2D.IsNearlyZero()) return;

	// Check if input direction is significantly different from velocity direction
	// Lua used cos(5 deg), meaning steer if angle > 5 degrees.
	const float Dot = FVector::DotProduct(CurrentVelDir2D, InputDir2D);
	const float SteerThresholdCosine = FMath::Cos(FMath::DegreesToRadians(5.0f));

	if (Dot < SteerThresholdCosine)
	{
		// Calculate angle between vectors
		const float AngleRad = FMath::Acos(Dot); // Angle is always positive [0, PI]

		// Determine rotation direction (sign) using cross product Z component
		const float CrossZ = FVector::CrossProduct(CurrentVelDir2D, InputDir2D).Z;
		const float RotationSign = FMath::Sign(CrossZ);

		// Clamp maximum rotation angle based on steer speed and delta time
		const float MaxAngleThisFrame = SlideSteerSpeed * DeltaTime;
		const float ClampedAngleRad = FMath::Min(AngleRad, MaxAngleThisFrame);

		// Rotate the full 3D velocity vector around the Z axis
		// This direct velocity modification replicates the Lua behavior but may challenge prediction
		Velocity = Velocity.RotateAngleAxisRad(ClampedAngleRad * RotationSign, FVector::UpVector);
	}
}

// --- Friction Logic ---
void UBwayCharacterMovementComponent::ApplySlideFriction(float DeltaTime, float SlopeAngleDegrees)
{
	float CurrentMaxSpeedFactor, SlopeAccelFactor, CurrentFrictionMultiplier;
	GetCurrentSlideModifiers(CurrentMaxSpeedFactor, SlopeAccelFactor, CurrentFrictionMultiplier);

	// Use the base friction factor defined in properties, scaled by the Lua power curve
	float FrictionToApply = 0.f;
	const float ClampedSlopeFactor = FMath::Clamp(SlopeAngleDegrees / 90.0f, 0.0f, 1.0f);
	// Lua: math.pow(1.0 - clamp(slopeAngle / 90.0, 0.0, 1.0), 15.0)
	// This factor approaches 1 on flat ground and 0 on 90-degree slopes.
	const float SlopePowerFactor = FMath::Pow(1.0f - ClampedSlopeFactor, SlideFrictionPower);

	// Combine base friction, slope factor, and loot modifier
	FrictionToApply = SlideBaseFrictionFactor * SlopePowerFactor * CurrentFrictionMultiplier;

	// Apply friction using VInterpTo (approximates exponential decay like Lua's VariableInterpolate)
	if (FrictionToApply > KINDA_SMALL_NUMBER && Velocity.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		// VInterpTo provides a smooth, frame-rate somewhat independent deceleration towards zero.
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaTime, FrictionToApply);
	}
}

// --- Falling Physics Override ---
void UBwayCharacterMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	// Check if slide input is still held AND if the fall started during a slide attempt
	bool bApplySlideGravity = false;
	if (bDidSlideFall) // Check flag set when transitioning from Slide->Fall
	{
		APlayerController* PC = CharacterOwner? Cast<APlayerController>(CharacterOwner->GetController()) : nullptr;
		UEnhancedInputComponent* EIC = PC? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
		if (EIC && SlideInputAction && EIC->GetBoundActionValue(SlideInputAction).Get<bool>())
		{
			bApplySlideGravity = true;
		}
	}

	// Apply custom gravity scale if conditions met
	if (bApplySlideGravity)
	{
		GravityScale = SlideGravityScale;
	}
	else
	{
		// Ensure default gravity scale is used otherwise
		GravityScale = DefaultGravityScale;
		bDidSlideFall = false; // Reset flag if input released or fall didn't start from slide
	}

	// Execute standard falling physics with potentially modified gravity scale
	Super::PhysFalling(deltaTime, Iterations);

	// IMPORTANT: Restore default gravity scale AFTER Super call so it doesn't persist
	GravityScale = DefaultGravityScale;
}

// --- Landing Logic Override ---
void UBwayCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	Super::ProcessLanded(Hit, remainingTime, Iterations);

	// Check if landing occurred shortly after a slide-jump
	if (LastSlideJumpTime > 0 && (GetWorld()->GetTimeSeconds() - LastSlideJumpTime) <= SlideJumpLandingGracePeriod)
	{
		// Apply velocity penalty as per Lua logic
		Velocity.X *= SlideJumpLandedVelocityFactor;
		Velocity.Y *= SlideJumpLandedVelocityFactor;
		// Velocity.Z is already handled by landing logic in Super

		LastSlideJumpTime = -1.0f; // Consume the timer
	}

	// Reset slide fall flag on landing
	bDidSlideFall = false;
}

// --- Modifier Calculation ---
void UBwayCharacterMovementComponent::GetCurrentSlideModifiers(float& OutMaxSpeedFactor, float& OutSlopeAccelFactor, float& OutFrictionMultiplier) const
{
	// Start with base values
	OutMaxSpeedFactor = SlideMaxSpeedFactor;
	OutSlopeAccelFactor = SlideSlopeAccelerationFactor;
	OutFrictionMultiplier = 1.0f; // Base friction multiplier is 1

	// Check ASC for the carrying loot tag
	if (AbilitySystemComponent && CarryingLootTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(CarryingLootTag))
	{
		// Apply multipliers if tag is present
		OutMaxSpeedFactor *= LootMaxSpeedFactorMultiplier;
		OutSlopeAccelFactor *= LootSlopeAccelFactorMultiplier;
		OutFrictionMultiplier *= LootFrictionMultiplier;
	}
}

// --- Slide Start/End Helpers ---
void UBwayCharacterMovementComponent::StartSlide()
{
	// Example: Shrink capsule height for slide pose
	if (CharacterOwner)
	{
		CharacterOwner->Crouch(true); // Use standard crouch mechanism for capsule size change
		// Adjust CrouchedHalfHeight if needed, or ensure it's set appropriately for sliding
	}
	// Optional: Apply initial boost if desired (simpler than setting specific speed)
	// Velocity += Velocity.GetSafeNormal() * SlidingInitialBoost;
}

void UBwayCharacterMovementComponent::EndSlide()
{
	// Example: Restore capsule height
	if (CharacterOwner)
	{
		CharacterOwner->UnCrouch(true);
	}
}