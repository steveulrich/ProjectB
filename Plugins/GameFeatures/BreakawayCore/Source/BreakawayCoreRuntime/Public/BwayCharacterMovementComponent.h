// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterWithAbilities.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "InputAction.h" // Add this include
#include "GameplayTagContainer.h"
#include "BwayCharacterMovementComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDashStartDelegate);

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None			UMETA(Hidden),
	CMOVE_Slide			UMETA(DisplayName = "Slide"),
	CMOVE_WallRun		UMETA(DisplayName = "Wall Run"),
	CMOVE_MAX			UMETA(Hidden),
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayCharacterMovementComponent : public ULyraCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UBwayCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	// -- SLIDE -- //
	//~ UCharacterMovementComponent Interface
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;
	virtual bool IsCustomMovementMode(uint8 TestCustomMovementMode) const { return MovementMode == MOVE_Custom && CustomMovementMode == TestCustomMovementMode; }
	//~ End UCharacterMovementComponent Interface

	// --- Slide Parameters (Derived from Lua, exposed for tuning) ---

	/** Base speed value used for slide calculations (cm/s). Corresponds to Lua's eS_slideSpeed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ForceUnits="cm/s"))
	float BaseSlideSpeed = 600.0f;

	/** Factor multiplied by BaseSlideSpeed to determine maximum slide speed. Lua: 3.5 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0"))
	float SlideMaxSpeedFactor = 3.5f;

	/** Factor multiplied by BaseSlideSpeed to determine slope acceleration magnitude. Lua: 9.0 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0"))
	float SlideSlopeAccelerationFactor = 9.0f;

	/** Minimum slope angle required to apply downward slope acceleration. Lua: 1 degree */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits="degrees"))
	float MinSlopeAngleForAccel = 1.0f;

	/** Rate at which the velocity vector rotates towards the input direction during slide (radians/sec). Lua: 2.0 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ForceUnits="rad/s"))
	float SlideSteerSpeed = 2.0f;

	/** Rate at which the character mesh rotates to face the current velocity direction (degrees/sec). Lua: 10800 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ForceUnits="deg/s"))
	float SlideCharacterRotationSpeed = 10800.0f;

	/** Exponent used in the slope-based friction calculation (Higher = more sensitive to slope). Lua: 15.0 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.1"))
	float SlideFrictionPower = 15.0f;

	/** Base friction factor used in slope calculation. Adjusted by slope power curve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0"))
	float SlideBaseFrictionFactor = 8.0f; // Needs tuning, replaces implicit Lua base

	/** High friction factor applied immediately upon hitting a wall during slide. Lua: 20.0 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0"))
	float SlideWallHitFriction = 20.0f;

	/** Maximum slope angle considered walkable during slide (degrees). Lua: maxClimbAngleDuringSlide */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits="degrees"))
	float SlideWalkableFloorAngle = 60.0f;

	/** Gravity scale multiplier applied when falling *if* the fall originated from a slide and input is held. Lua: fSlideGravity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0"))
	float SlideGravityScale = 1.0f;

	/** Minimum speed below which the character automatically stops sliding (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ForceUnits="cm/s"))
	float MinSlideSpeed = 100.0f;

	/** Multiplier applied to horizontal velocity when landing shortly after a slide-jump. Lua: 0.5 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideJumpLandedVelocityFactor = 0.5f;

	/** Time window after a slide-jump during which the landing velocity penalty applies (seconds). Corresponds to Lua's fSlideJumpTimer check. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ForceUnits="s"))
	float SlideJumpLandingGracePeriod = 0.2f;

	// --- Loot Modifiers (Optional) ---
	/** Gameplay Tag checked on the ASC to determine if loot modifiers should apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Modifiers")
	FGameplayTag CarryingLootTag; // Assign State.Condition.CarryingLoot in BP

	/** Multiplier applied to SlideMaxSpeedFactor when carrying loot. Lua: 0.7 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Modifiers", meta = (ClampMin = "0.0"))
	float LootMaxSpeedFactorMultiplier = 0.7f;

	/** Multiplier applied to SlideSlopeAccelerationFactor when carrying loot. Lua: 0.9 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Modifiers", meta = (ClampMin = "0.0"))
	float LootSlopeAccelFactorMultiplier = 0.9f;

	/** Multiplier applied to the calculated friction factor when carrying loot. Lua: 1.5 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Modifiers", meta = (ClampMin = "0.0"))
	float LootFrictionMultiplier = 1.5f;

	// --- Input Action References (For internal checks) ---
	/** Input Action asset for the slide input. */
	UPROPERTY(EditDefaultsOnly, Category = "Character Movement: Sliding|Input")
	TObjectPtr<UInputAction> SlideInputAction; // Assign IA_Slide in BP

	/** Input Action asset for the jump input. */
	UPROPERTY(EditDefaultsOnly, Category = "Character Movement: Sliding|Input")
	TObjectPtr<UInputAction> JumpInputAction; // Assign Lyra's Jump IA in BP

	// --- Public Functions ---
	/** Checks conditions to determine if the slide should end. */
	UFUNCTION(BlueprintCallable, Category = "Character Movement: Sliding")
	virtual bool CheckShouldEndSlide();

	/** Returns true if currently in the custom sliding movement mode. */
	UFUNCTION(BlueprintPure, Category = "Character Movement: Sliding")
	bool IsSliding() const { return IsCustomMovementMode((uint8)ECustomMovementMode::CMOVE_Slide); }

	// Optional delegate if needed by GA
	// UPROPERTY(BlueprintAssignable, Category = "Character Movement: Sliding")
	// FSlideEndDelegate OnSlideEndDelegate;

protected:
	//~ Begin UObject Interface
	virtual void InitializeComponent() override;
	//~ End UObject Interface

	/** Contains the core physics logic for the CMOVE_Sliding custom movement mode. */
	virtual void PhysSliding(float deltaTime, int32 Iterations);

	/** Applies friction based on the Lua-derived model (slope power curve, wall hits). */
	virtual void ApplySlideFriction(float DeltaTime, float SlopeAngleDegrees);

	/** Applies steering by directly rotating the velocity vector towards input, based on Lua logic. */
	virtual void ApplySlideSteering(float DeltaTime, const FVector& InputAccelDir);

	/** Applies acceleration down the slope based on Lua parameters. */
	virtual void ApplySlideSlopeAcceleration(float DeltaTime, float SlopeAngleDegrees);

	/** Calculates the current effective speed/accel/friction modifiers based on game state (e.g., carrying loot). */
	virtual void GetCurrentSlideModifiers(float& OutMaxSpeedFactor, float& OutSlopeAccelFactor, float& OutFrictionMultiplier) const;

	/** Performs setup when entering the slide state (e.g., capsule resize). */
	virtual void StartSlide();

	/** Performs cleanup when exiting the slide state (e.g., capsule restore). */
	virtual void EndSlide();

	// Cached default values restored when exiting slide
	float DefaultWalkableFloorAngle;
	FRotator DefaultRotationRate;
	float DefaultGravityScale;

	// State tracking
	bool bDidSlideFall = false; // Flag set when transitioning from Slide to Fall

	// State for slide-jump landing penalty
	float LastSlideJumpTime = -1.0f;

	/** Cached Ability System Component for tag checking. */
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	// -- END NEW SLIDE -- //

};
