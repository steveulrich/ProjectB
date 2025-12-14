// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RelicMovementReplicationComponent.generated.h"

class UStaticMeshComponent;

/**
 * Compressed movement data for efficient replication
 * Stores position, rotation, and velocities with timestamp for interpolation
 */
USTRUCT()
struct FRelicMovementState
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	
	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;
	
	UPROPERTY()
	FVector LinearVelocity = FVector::ZeroVector;
	
	UPROPERTY()
	FVector AngularVelocity = FVector::ZeroVector;
	
	UPROPERTY()
	float Timestamp = 0.0f;

	FRelicMovementState()
		: Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, LinearVelocity(FVector::ZeroVector)
		, AngularVelocity(FVector::ZeroVector)
		, Timestamp(0.0f)
	{
	}
};

/**
 * Component that handles smooth replication of physics-based relic movement
 * 
 * This component solves the stuttering problem inherent in standard physics replication
 * by implementing client-side interpolation between server states. It:
 * - Captures movement state on server at high frequency
 * - Replicates compressed state data to clients
 * - Interpolates smoothly between states on clients
 * - Handles correction when client prediction diverges
 * 
 * Usage:
 * 1. Attach to RelicActor
 * 2. Call EnableSmoothReplication(true) when physics starts
 * 3. Call EnableSmoothReplication(false) when relic is carried (no physics)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API URelicMovementReplicationComponent : public UActorComponent
{
	GENERATED_BODY()

public:    
	URelicMovementReplicationComponent();

	//~ UActorComponent Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	/**
	 * Enable or disable smooth replication
	 * Call this when the relic's physics state changes (e.g., picked up vs thrown)
	 * 
	 * @param bEnable - True to enable smooth replication, false to disable
	 */
	UFUNCTION(BlueprintCallable, Category = "Replication")
	void EnableSmoothReplication(bool bEnable);

	/**
	 * Force an immediate snap to the server's position
	 * Useful when teleporting or respawning the relic
	 */
	UFUNCTION(BlueprintCallable, Category = "Replication")
	void ForceSnapToServerState();

protected:
	// ========================================
	// Replication
	// ========================================

	/**
	 * Replicated movement state updated by the server
	 * Clients interpolate toward this state
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ServerState)
	FRelicMovementState ServerState;
	
	UFUNCTION()
	void OnRep_ServerState();

	// ========================================
	// Configuration
	// ========================================

	/**
	 * How fast to interpolate toward the server state
	 * Higher = faster convergence but more jittery
	 * Lower = smoother but more lag
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Replication", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float InterpolationSpeed = 15.0f; // Increased from 10.0f for better responsiveness

	/**
	 * Maximum allowed position error before snapping
	 * If client is further than this from server, instantly correct
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Replication", meta = (ClampMin = "50.0", ClampMax = "500.0"))
	float SnapThreshold = 200.0f;

	/**
	 * Position error threshold for applying velocity prediction
	 * Below this threshold, use pure interpolation
	 * Above this, add velocity prediction for more responsive feel
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Replication", meta = (ClampMin = "10.0", ClampMax = "200.0"))
	float PredictionThreshold = 50.0f;

	/**
	 * How much to scale predicted velocity when extrapolating
	 * 0.0 = no prediction, 1.0 = full velocity prediction
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Replication", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityPredictionScale = 0.5f;

	/**
	 * How often the server updates movement state (in seconds)
	 * Lower = more bandwidth, smoother movement
	 * Higher = less bandwidth, more stuttering
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Replication", meta = (ClampMin = "0.01", ClampMax = "0.2"))
	float ServerUpdateInterval = 0.033f; // ~30 Hz

	/**
	 * Rotation interpolation speed (degrees per second)
	 * Higher values make rotation changes more responsive
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Replication", meta = (ClampMin = "90.0", ClampMax = "720.0"))
	float RotationInterpSpeed = 360.0f;

private:
	// ========================================
	// Internal State
	// ========================================

	/** Whether smooth replication is currently active */
	bool bIsEnabled = false;

	/** Reference to the mesh component we're tracking */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TrackedMesh;

	/** Client-side interpolation data */
	FRelicMovementState ClientStartState;
	FRelicMovementState ClientTargetState;
	float InterpolationAlpha = 0.0f;

	/** Timer for server-side state updates */
	float ServerUpdateTimer = 0.0f;

	/** Last server time we received an update (for client-side timing) */
	float LastServerUpdateTime = 0.0f;

	/** Whether we should snap on the next update (after teleport/respawn) */
	bool bForceSnapNextUpdate = false;

	// ========================================
	// Internal Functions
	// ========================================

	/**
	 * Server: Capture current movement state and replicate to clients
	 */
	void ServerUpdateMovementState();

	/**
	 * Client: Interpolate smoothly between states
	 */
	void ClientInterpolateMovement(float DeltaTime);

	/**
	 * Initialize the tracked mesh component reference
	 */
	void InitializeTrackedMesh();

	/**
	 * Check if the position error exceeds the snap threshold
	 */
	bool ShouldSnapToServerState(const FVector& CurrentLocation) const;

	/**
	 * Calculate predicted position based on velocity
	 */
	FVector CalculatePredictedPosition(const FVector& BasePosition, const FVector& Velocity, float DeltaTime) const;
};