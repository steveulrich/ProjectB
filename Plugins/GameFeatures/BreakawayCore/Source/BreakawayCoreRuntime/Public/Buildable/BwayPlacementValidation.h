#pragma once

#include "CoreMinimal.h"
#include "ActorPlacementTargeting/BwayTargetActor_ActorPlacementFace.h"

/** Ephemeral collision query shared by the local preview and authoritative confirmation. */
struct BREAKAWAYCORERUNTIME_API FBwayPlacementQuery
{
	UWorld* World = nullptr;
	AActor* Source = nullptr;
	TSubclassOf<AActor> PlacedClass;
	FBox AuthoredBounds = FBox(ForceInit);
	FVector AimDirection = FVector::ForwardVector;
	TArray<const AActor*> IgnoredActors;
	ECollisionChannel CollisionChannel = ECC_Visibility;
	float MaxRange = 2000.f;
	float RangeTolerance = 0.f;
	float MaxSlopeDegrees = 30.f;
	float MaxFootprintZDifference = 10.f;
	float TraceUp = 20.f;
	float TraceDown = 50.f;
	float GroundClearance = 2.f;

	/** GroundHit comes from a world trace, never directly from a remote client. */
	FPlacementValidationResult ValidateGroundHit(const FHitResult& GroundHit) const;
	/** Re-trace the submitted ground point, ignoring the claimed blocking actor/normal. */
	FPlacementValidationResult ValidateSubmittedHit(const FHitResult& SubmittedHit) const;
	/** Authored occupancy plus any blocking components, including Blueprint templates. */
	bool GetLocalGeometry(TArray<FBox>& OutBlockingBounds, FBox& OutFootprint) const;

private:
	bool IsInRange(const FVector& GroundPoint) const;
	bool IsUsableNormal(const FVector& Normal) const;
};
