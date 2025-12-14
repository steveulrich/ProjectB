// Copyright Epic Games, Inc. All Rights Reserved.

#include "Relic/RelicMovementReplicationComponent.h"
#include "Relic/RelicActor.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogRelicReplication, Log, All);

URelicMovementReplicationComponent::URelicMovementReplicationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	
	// Enable replication for this component
	SetIsReplicatedByDefault(true);
	
	// Default to enabled - will be controlled by relic state
	bIsEnabled = false;
}

void URelicMovementReplicationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// Replicate the server state to all clients
	// Using COND_SimulatedOnly because the server doesn't need this data for itself
	DOREPLIFETIME_CONDITION(URelicMovementReplicationComponent, ServerState, COND_SimulatedOnly);
}

void URelicMovementReplicationComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize the mesh reference
	InitializeTrackedMesh();
	
	// On clients, initialize interpolation state
	if (GetOwnerRole() != ROLE_Authority && TrackedMesh)
	{
		ClientStartState.Location = TrackedMesh->GetComponentLocation();
		ClientStartState.Rotation = TrackedMesh->GetComponentRotation();
		ClientTargetState = ClientStartState;
	}
}

void URelicMovementReplicationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (!bIsEnabled || !TrackedMesh)
	{
		return;
	}

	// Server: Update and replicate state
	if (GetOwnerRole() == ROLE_Authority)
	{
		ServerUpdateTimer += DeltaTime;
		
		if (ServerUpdateTimer >= ServerUpdateInterval)
		{
			ServerUpdateMovementState();
			ServerUpdateTimer = 0.0f;
		}
	}
	// Clients: Interpolate toward server state
	else if (GetOwnerRole() == ROLE_SimulatedProxy)
	{
		ClientInterpolateMovement(DeltaTime);
	}
}

void URelicMovementReplicationComponent::EnableSmoothReplication(bool bEnable)
{
	if (bIsEnabled == bEnable)
	{
		return; // No change
	}

	bIsEnabled = bEnable;
	
	UE_LOG(LogRelicReplication, Log, TEXT("Smooth replication %s for %s"), 
		bEnable ? TEXT("enabled") : TEXT("disabled"),
		*GetNameSafe(GetOwner()));

	if (!bEnable)
	{
		// When disabling, reset timers
		ServerUpdateTimer = 0.0f;
		InterpolationAlpha = 0.0f;
	}
	else
	{
		// When enabling, initialize mesh reference if needed
		if (!TrackedMesh)
		{
			InitializeTrackedMesh();
		}

		// Force an immediate update on server
		if (GetOwnerRole() == ROLE_Authority)
		{
			ServerUpdateMovementState();
		}
	}
}

void URelicMovementReplicationComponent::ForceSnapToServerState()
{
	bForceSnapNextUpdate = true;
	
	UE_LOG(LogRelicReplication, Log, TEXT("Force snap requested for %s"), *GetNameSafe(GetOwner()));
}

void URelicMovementReplicationComponent::OnRep_ServerState()
{
	if (!TrackedMesh || !bIsEnabled)
	{
		return;
	}

	// Store the current state as the start of interpolation
	ClientStartState.Location = TrackedMesh->GetComponentLocation();
	ClientStartState.Rotation = TrackedMesh->GetComponentRotation();
	ClientStartState.LinearVelocity = TrackedMesh->GetPhysicsLinearVelocity();
	ClientStartState.AngularVelocity = TrackedMesh->GetPhysicsAngularVelocityInDegrees();
	ClientStartState.Timestamp = GetWorld()->GetTimeSeconds();
	
	// The new server state becomes our target
	ClientTargetState = ServerState;
	
	// Reset interpolation
	InterpolationAlpha = 0.0f;
	LastServerUpdateTime = GetWorld()->GetTimeSeconds();

	// Check if we should snap instead of interpolate
	if (bForceSnapNextUpdate || ShouldSnapToServerState(ClientStartState.Location))
	{
		UE_LOG(LogRelicReplication, Verbose, TEXT("Snapping to server position (error too large or forced)"));
		
		TrackedMesh->SetWorldLocation(ServerState.Location);
		TrackedMesh->SetWorldRotation(ServerState.Rotation);
		
		// If physics is enabled, set velocities too
		if (TrackedMesh->IsSimulatingPhysics())
		{
			TrackedMesh->SetPhysicsLinearVelocity(ServerState.LinearVelocity);
			TrackedMesh->SetPhysicsAngularVelocityInDegrees(ServerState.AngularVelocity);
		}
		
		ClientStartState = ServerState;
		ClientTargetState = ServerState;
		bForceSnapNextUpdate = false;
	}

	UE_LOG(LogRelicReplication, VeryVerbose, TEXT("Client received server state: Pos=%s, Vel=%s"),
		*ServerState.Location.ToString(),
		*ServerState.LinearVelocity.ToString());
}

void URelicMovementReplicationComponent::ServerUpdateMovementState()
{
	if (!TrackedMesh)
	{
		return;
	}

	// Capture current state
	ServerState.Location = TrackedMesh->GetComponentLocation();
	ServerState.Rotation = TrackedMesh->GetComponentRotation();
	ServerState.LinearVelocity = TrackedMesh->GetPhysicsLinearVelocity();
	ServerState.AngularVelocity = TrackedMesh->GetPhysicsAngularVelocityInDegrees();
	ServerState.Timestamp = GetWorld()->GetTimeSeconds();

	// This will trigger OnRep_ServerState on clients via replication
	MARK_PROPERTY_DIRTY_FROM_NAME(URelicMovementReplicationComponent, ServerState, this);

	UE_LOG(LogRelicReplication, VeryVerbose, TEXT("Server updated state: Pos=%s, Vel=%s"),
		*ServerState.Location.ToString(),
		*ServerState.LinearVelocity.ToString());
}

void URelicMovementReplicationComponent::ClientInterpolateMovement(float DeltaTime)
{
	if (!TrackedMesh)
	{
		return;
	}

	// Don't interpolate if physics is disabled (relic is being carried)
	if (!TrackedMesh->IsSimulatingPhysics())
	{
		return;
	}

	// Update interpolation alpha
	InterpolationAlpha = FMath::Clamp(InterpolationAlpha + (DeltaTime * InterpolationSpeed), 0.0f, 1.0f);

	// Calculate position error
	FVector CurrentLocation = TrackedMesh->GetComponentLocation();
	FVector TargetLocation = ClientTargetState.Location;
	float PositionError = FVector::Distance(CurrentLocation, TargetLocation);

	// Determine target position (with optional velocity prediction)
	FVector InterpolatedLocation;
	
	if (PositionError > PredictionThreshold)
	{
		// Use velocity prediction for large errors to feel more responsive
		float TimeSinceLastUpdate = GetWorld()->GetTimeSeconds() - LastServerUpdateTime;
		FVector PredictedLocation = CalculatePredictedPosition(
			ClientTargetState.Location,
			ClientTargetState.LinearVelocity,
			TimeSinceLastUpdate
		);
		
		InterpolatedLocation = FMath::Lerp(ClientStartState.Location, PredictedLocation, InterpolationAlpha);
		
		UE_LOG(LogRelicReplication, VeryVerbose, TEXT("Using velocity prediction: Error=%.2f, Predicted=%s"),
			PositionError,
			*PredictedLocation.ToString());
	}
	else
	{
		// Use pure interpolation for small errors (smoother)
		InterpolatedLocation = FMath::Lerp(ClientStartState.Location, ClientTargetState.Location, InterpolationAlpha);
	}

	// Interpolate rotation
	FRotator InterpolatedRotation = FMath::RInterpTo(
		TrackedMesh->GetComponentRotation(),
		ClientTargetState.Rotation,
		DeltaTime,
		RotationInterpSpeed
	);

	// Apply the interpolated transform
	// Instead of disabling physics, guide it with velocity for smoother movement
	if (TrackedMesh->IsSimulatingPhysics())
	{
		// Calculate desired velocity to reach target position
		FVector ToTarget = InterpolatedLocation - CurrentLocation;
		float DistanceToTarget = ToTarget.Size();
		
		// Only apply correction if we're significantly off target
		// Otherwise let physics run naturally
		if (DistanceToTarget > 5.0f)
		{
			// Calculate correction velocity (blended with server velocity)
			FVector CorrectionVelocity = ToTarget / FMath::Max(DeltaTime, 0.001f);
			FVector InterpolatedVelocity = FMath::Lerp(
				ClientStartState.LinearVelocity,
				ClientTargetState.LinearVelocity,
				InterpolationAlpha
			);
			
			// Blend correction with server velocity (70% correction, 30% server)
			FVector FinalVelocity = FMath::Lerp(InterpolatedVelocity, CorrectionVelocity, 0.7f);
			
			// Clamp velocity to prevent excessive corrections
			float MaxCorrectionSpeed = 2000.0f;
			if (FinalVelocity.SizeSquared() > MaxCorrectionSpeed * MaxCorrectionSpeed)
			{
				FinalVelocity = FinalVelocity.GetSafeNormal() * MaxCorrectionSpeed;
			}
			
			TrackedMesh->SetPhysicsLinearVelocity(FinalVelocity);
		}
		else
		{
			// Close enough - just use server velocity
			FVector InterpolatedVelocity = FMath::Lerp(
				ClientStartState.LinearVelocity,
				ClientTargetState.LinearVelocity,
				InterpolationAlpha
			);
			TrackedMesh->SetPhysicsLinearVelocity(InterpolatedVelocity);
		}
		
		// Apply rotation smoothly
		TrackedMesh->SetWorldRotation(InterpolatedRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		// Physics not enabled - use direct position setting (shouldn't happen for thrown relics)
		TrackedMesh->SetWorldLocation(InterpolatedLocation, false, nullptr, ETeleportType::TeleportPhysics);
		TrackedMesh->SetWorldRotation(InterpolatedRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	UE_LOG(LogRelicReplication, VeryVerbose, TEXT("Client interpolated: Pos=%s, Alpha=%.2f, Error=%.2f"),
		*InterpolatedLocation.ToString(),
		InterpolationAlpha,
		PositionError);
}

void URelicMovementReplicationComponent::InitializeTrackedMesh()
{
	if (TrackedMesh)
	{
		return; // Already initialized
	}

	ARelicActor* RelicOwner = Cast<ARelicActor>(GetOwner());
	if (!RelicOwner)
	{
		UE_LOG(LogRelicReplication, Error, TEXT("RelicMovementReplicationComponent owner is not a RelicActor!"));
		return;
	}

	// Get the mesh component from the relic
	TrackedMesh = RelicOwner->FindComponentByClass<UStaticMeshComponent>();
	
	if (!TrackedMesh)
	{
		UE_LOG(LogRelicReplication, Error, TEXT("RelicActor has no StaticMeshComponent!"));
		return;
	}

	UE_LOG(LogRelicReplication, Log, TEXT("Initialized tracked mesh: %s"), *GetNameSafe(TrackedMesh));
}

bool URelicMovementReplicationComponent::ShouldSnapToServerState(const FVector& CurrentLocation) const
{
	float Distance = FVector::Distance(CurrentLocation, ServerState.Location);
	
	bool bShouldSnap = Distance > SnapThreshold;
	
	if (bShouldSnap)
	{
		UE_LOG(LogRelicReplication, Warning, TEXT("Position error %.2f exceeds snap threshold %.2f - snapping to server"),
			Distance,
			SnapThreshold);
	}
	
	return bShouldSnap;
}

FVector URelicMovementReplicationComponent::CalculatePredictedPosition(const FVector& BasePosition, const FVector& Velocity, float DeltaTime) const
{
	// Simple linear prediction with scaling factor
	// We scale down the prediction to avoid over-shooting
	FVector PredictedPosition = BasePosition + (Velocity * DeltaTime * VelocityPredictionScale);
	
	return PredictedPosition;
}