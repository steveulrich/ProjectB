// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterWithAbilities.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "InputAction.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "BwayCharacterMovementComponent.generated.h"

class UAbilitySystemComponent;
class UBwayMovementFeelConfig;

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
	virtual bool CanAttemptJump() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
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
	float SlideBaseFrictionFactor = 8.0f;

	/** High friction factor applied immediately upon hitting a wall during slide. Lua: 20.0 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0"))
	float SlideWallHitFriction = 20.0f;

	/** Maximum slope angle considered walkable during slide (degrees). Lua: maxClimbAngleDuringSlide */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits="degrees"))
	float SlideWalkableFloorAngle = 60.0f;

	/** Legacy slide gravity scale (kept for existing content). Prefer SlideJumpGravityScale for slide-jump. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0"))
	float SlideGravityScale = 1.0f;

	/** Minimum speed below which the character automatically stops sliding (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding", meta = (ClampMin = "0.0", ForceUnits="cm/s"))
	float MinSlideSpeed = 100.0f;

	/** Multiplier applied to horizontal velocity when landing from a slide-jump. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Slide Jump", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideJumpLandedVelocityFactor = 0.5f;

	/** Multiplier applied to horizontal velocity when jumping out of a slide. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Slide Jump", meta = (ClampMin = "1.0"))
	float SlideJumpMomentumBoost = 1.2f;

	/** Minimum horizontal speed after a successful slide-jump (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Slide Jump", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float SlideJumpMinHorizontalSpeed = 400.0f;

	/** Maximum horizontal speed after a successful slide-jump (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Slide Jump", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float SlideJumpMaxHorizontalSpeed = 2000.0f;

	/** Absolute AirControl while slide-jumping (normal jumps use CMC AirControl). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Slide Jump", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideJumpAirControl = 0.15f;

	/** GravityScale applied only while slide-jump state is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Slide Jump", meta = (ClampMin = "0.0"))
	float SlideJumpGravityScale = 1.0f;

	/** If true, keep slide-jump state until landing (or hard mode interrupt). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Slide Jump")
	bool bPreserveSlideJumpUntilLanding = true;

	// --- Slide Juice (Camera) ---
	/** Maximum FOV offset to add when sliding at maximum speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Juice", meta = (ClampMin = "0.0", ForceUnits="degrees"))
	float SlideFOVOffsetMax = 15.0f;

	/** Speed at which the FOV offset interpolates towards the target value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Juice", meta = (ClampMin = "0.0"))
	float SlideFOVInterpSpeed = 5.0f;

	/** Camera shake to play during sliding. Intensity should be modulated by slide speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Juice")
	TSubclassOf<class UCameraShakeBase> SlideCameraShakeClass;

	// --- Loot Modifiers (Optional) ---
	/** Gameplay Tag checked on the ASC to determine if loot modifiers should apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Modifiers")
	FGameplayTag CarryingLootTag;

	/** Multiplier applied to SlideMaxSpeedFactor when carrying loot. Lua: 0.7 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Modifiers", meta = (ClampMin = "0.0"))
	float LootMaxSpeedFactorMultiplier = 0.7f;

	/** Multiplier applied to SlideSlopeAccelerationFactor when carrying loot. Lua: 0.9 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Modifiers", meta = (ClampMin = "0.0"))
	float LootSlopeAccelFactorMultiplier = 0.9f;

	/** Multiplier applied to the calculated friction factor when carrying loot. Lua: 1.5 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Movement: Sliding|Modifiers", meta = (ClampMin = "0.0"))
	float LootFrictionMultiplier = 1.5f;

	// --- Public Functions ---
	/** Checks conditions to determine if the slide should end. */
	UFUNCTION(BlueprintCallable, Category = "Character Movement: Sliding")
	virtual bool CheckShouldEndSlide();

	/** Returns true if currently in the custom sliding movement mode. */
	UFUNCTION(BlueprintPure, Category = "Character Movement: Sliding")
	bool IsSliding() const { return IsCustomMovementMode((uint8)ECustomMovementMode::CMOVE_Slide); }

	/** Returns true while in the predicted slide-jump aerial state. */
	UFUNCTION(BlueprintPure, Category = "Character Movement: Sliding|Slide Jump")
	bool IsSlideJumping() const { return bIsSlideJumping; }

	/** Returns a normalized value (0.0 to 1.0) representing the current slide intensity based on velocity. */
	UFUNCTION(BlueprintPure, Category = "Character Movement: Sliding")
	float GetSlideIntensity() const;

	/** Applies tunable slide-jump feel values from a data asset (component defaults remain as fallbacks). */
	UFUNCTION(BlueprintCallable, Category = "Character Movement: Sliding|Slide Jump")
	void ApplyMovementFeelConfig(const UBwayMovementFeelConfig* Config);

	/**
	 * Pure helper: boosts and clamps horizontal speed for a slide-jump launch.
	 * If JumpZ > 0, sets OutVelocity.Z = max(InVelocity.Z, JumpZ). Returns false if MinH > MaxH.
	 */
	UFUNCTION(BlueprintPure, Category = "Character Movement: Sliding|Slide Jump")
	static bool ComputeSlideJumpLaunchVelocity(
		FVector InVelocity,
		float JumpZ,
		float MomentumBoost,
		float MinHorizontalSpeed,
		float MaxHorizontalSpeed,
		UPARAM(ref) FVector& OutVelocity);

protected:
	//~ Begin UObject Interface
	virtual void InitializeComponent() override;
	//~ End UObject Interface

	//~ Begin UCharacterMovementComponent Interface
	virtual void PhysSliding(float deltaTime, int32 Iterations);
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

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

	void ClearSlideJumpState();

	// Cached default values restored when exiting slide
	float DefaultWalkableFloorAngle;
	FRotator DefaultRotationRate;
	float DefaultGravityScale;

	/** Predicted slide-jump aerial state (carried via compressed move flags). */
	bool bIsSlideJumping = false;

	/** Cached Ability System Component for tag checking. */
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle MoveSpeedMultiplierChangedHandle;

	void HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& ChangeData);

	friend class FSavedMove_BwayCharacter;

	// -- END NEW SLIDE -- //

public:
	/** Bind ASC attribute change callbacks for MoveSpeedMultiplier (called from character ASC init). */
	void BindAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindAbilitySystem();

};

/** Saved move that carries slide-jump state for client prediction. */
class FSavedMove_BwayCharacter : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	FSavedMove_BwayCharacter()
		: bSavedIsSlideJumping(0)
	{
	}

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual void PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual uint8 GetCompressedFlags() const override;

	uint8 bSavedIsSlideJumping : 1;
};

class FNetworkPredictionData_Client_BwayCharacter : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	FNetworkPredictionData_Client_BwayCharacter(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
