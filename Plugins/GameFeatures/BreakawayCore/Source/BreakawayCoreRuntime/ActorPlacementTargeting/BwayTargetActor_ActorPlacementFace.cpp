// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayTargetActor_ActorPlacementFace.h"
#include "BwayWorldReticle_ActorVisualization.h"
#include "Engine/World.h"
#include "Abilities/GameplayAbilityTargetActor.h"   // For IsTargetValid()
#include "Engine/EngineTypes.h"     // For FOverlapResult, ECollisionChannel
#include "Engine/OverlapResult.h"
#include "Abilities/GameplayAbilityWorldReticle_ActorVisualization.h" // For base class of our reticle
#include "Abilities/GameplayAbility.h" // For GetCurrentActorInfo
#include "GameFramework/PlayerController.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h" // For footprint check

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayTargetActor_ActorPlacementFace)

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	AGATargetActor_ActorPlacementFace
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

ABwayTargetActor_ActorPlacementFace::ABwayTargetActor_ActorPlacementFace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true; // Enable tick for continuous validation feedback
	// MaxPlacementAngleDegrees, ValidPlacementMaterial, InvalidPlacementMaterial will be set on instance or via ExposeOnSpawn
}

void ABwayTargetActor_ActorPlacementFace::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle->Destroy();
		ActorVisualizationReticle = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayTargetActor_ActorPlacementFace::StartTargeting(UGameplayAbility* InAbility)
{
	Super::StartTargeting(InAbility); // This will create ReticleActor (often a decal)

	if (AActor* VisualizationActor = GetWorld()->SpawnActor(PlacedActorClass))
	{
		// Ensure ActorVisualizationReticle is created if not already, or if previous one was destroyed
		if (!ActorVisualizationReticle.IsValid())
		{
			ActorVisualizationReticle = GetWorld()->SpawnActor<ABwayWorldReticle_ActorVisualization>();
		}
		
		if (ActorVisualizationReticle.IsValid())
		{
			ActorVisualizationReticle->InitializeReticleVisualizationInformation(SourceActor.Get(), VisualizationActor, ValidPlacementMaterial, InvalidPlacementMaterial);
		
			// Attach our visualization reticle to the main reticle actor (e.g., decal) managed by the parent class.
			if (AGameplayAbilityWorldReticle* ParentReticle = ReticleActor.Get())
			{
				ActorVisualizationReticle->AttachToActor(ParentReticle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				ActorVisualizationReticle->SetIsReticleVisible(true); // Make sure it's visible
			}
			else
			{
				// If no parent reticle, our visualization becomes the main one.
				// This path is less common if GroundTrace is configured with a ReticleClass.
				ReticleActor = ActorVisualizationReticle; // Parent class will now manage this
				ActorVisualizationReticle->SetIsReticleVisible(true);
			}
		}
		GetWorld()->DestroyActor(VisualizationActor); // Destroy the temporary template actor
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABwayTargetActor_ActorPlacementFace: PlacedActorClass is not set or failed to spawn temporary actor."));
	}

	// Initial validation state
	bLastTickPlacementValid = false; 
	if (ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle->UpdatePlacementVisuals(bLastTickPlacementValid);
	}
}

FHitResult ABwayTargetActor_ActorPlacementFace::PerformTrace(AActor* InSourceActor)
{
	// Get the initial ground trace from the parent
	FHitResult OriginalGroundHit = Super::PerformTrace(InSourceActor);

	// Perform our detailed validation
	LastValidatedPlacementResult = PerformPlacementValidation(OriginalGroundHit);

	bLastTickPlacementValid = LastValidatedPlacementResult.bIsValid;
	
	if (bLastTickPlacementValid)
	{
		// If valid, return a hit result that uses our adjusted location.
		// The base class's reticle (decal) will snap to this.
		FHitResult AdjustedHit = OriginalGroundHit;
		AdjustedHit.Location = LastValidatedPlacementResult.AdjustedLocation;
		AdjustedHit.ImpactPoint = LastValidatedPlacementResult.AdjustedLocation;
		// Note: ImpactNormal might ideally be re-evaluated at AdjustedLocation if it moved significantly.
		// For now, we assume OriginalGroundHit.ImpactNormal is sufficient for decal orientation.
		return AdjustedHit;
	}
	
	// If not valid by our rules, return the original hit. The decal will be there, but our visual will be "invalid".
	return OriginalGroundHit;
}

void ABwayTargetActor_ActorPlacementFace::ConfirmTargetingAndContinue()
{
	check(ShouldProduceTargetData());
	if (SourceActor)
	{
		bDebug = false;
		FGameplayAbilityTargetDataHandle Handle = CreateTargetData(PerformTrace(SourceActor));
		TargetDataReadyDelegate.Broadcast(Handle);
	}
}

// Override Tick to update the material AFTER Super::Tick() has positioned the reticle based on NewTraceResult's output
void ABwayTargetActor_ActorPlacementFace::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); // This will call AimWithPlayerController -> NewTraceResult, and position ReticleActor

	if (ActorVisualizationReticle.IsValid())
	{
		// bLastTickPlacementValid was set by our NewTraceResult override.
		// Also check if the parent reticle itself is considered valid by the trace logic.
		ActorVisualizationReticle->UpdatePlacementVisuals(bLastTickPlacementValid);
	}
}

FGameplayAbilityTargetDataHandle ABwayTargetActor_ActorPlacementFace::CreateTargetData(
	const FHitResult& InHitResult) const
{
	FPlacementValidationResult Validation = PerformPlacementValidation(InHitResult);

	if (bLastTickPlacementValid && LastValidatedPlacementResult.OriginalGroundHit.bBlockingHit)
	{
		// Create target data with the (potentially) adjusted location and original normal.
		FGameplayAbilityTargetData_SingleTargetHit* ReturnData = new FGameplayAbilityTargetData_SingleTargetHit();

		FHitResult FinalHit = LastValidatedPlacementResult.OriginalGroundHit; // Start with original data
		FinalHit.Location = LastValidatedPlacementResult.AdjustedLocation;
		FinalHit.ImpactPoint = LastValidatedPlacementResult.AdjustedLocation;
		// The ability will use FinalHit.Location and FinalHit.ImpactNormal
		// and LastValidatedPlacementResult.PlacementRotation
		
		ReturnData->HitResult = FinalHit;
		// We need to ensure this data is correctly packaged.
		// Consider creating a custom FGameplayAbilityTargetData if you need to pass PlacementRotation directly and cleanly.
		// For now, the ability will have to reconstruct rotation from HitResult.Location/Normal and LastValidatedPlacementResult.PlacementRotation (if accessed).

		return FGameplayAbilityTargetDataHandle(ReturnData);
	}

	// If our validation fails at confirmation time, return an empty handle.
	// This signals to the WaitTargetData node that the confirmed target is invalid.
	return FGameplayAbilityTargetDataHandle();
}

FPlacementValidationResult ABwayTargetActor_ActorPlacementFace::PerformPlacementValidation(
	const FHitResult& GroundHit) const
{
	FPlacementValidationResult Result;
	Result.OriginalGroundHit = GroundHit;
	Result.AdjustedLocation = GroundHit.Location; // Start with the trace hit
	Result.bIsValid = false; // Default to invalid
	
	if (!PlacedActorClass || !SourceActor.Get() || !GroundHit.bBlockingHit)
	{
		return Result;
	}
	
	// 1. Surface Angle Check
	if (!CheckSurfaceAngle(GroundHit.ImpactNormal))
	{
		return Result;
	}

	// Determine initial placement rotation for overhang and obstruction checks
	FVector AimDirection = SourceActor->GetActorForwardVector();
	APlayerController* PC = OwningAbility ? OwningAbility->GetCurrentActorInfo()->PlayerController.Get() : nullptr;
	if (PC)
	{
		AimDirection = PC->GetControlRotation().Vector();
	}
	// Project aim direction onto the plane defined by the ground normal
	FVector ForwardOnPlane = FVector::CrossProduct(GroundHit.ImpactNormal, FVector::CrossProduct(AimDirection, GroundHit.ImpactNormal).GetSafeNormal());
    if (ForwardOnPlane.IsNearlyZero()) // If AimDirection is (anti-)parallel to ImpactNormal
    {
        // Use actor's right vector to find a perpendicular forward vector on the plane
        FVector ActorRight = SourceActor->GetActorRightVector();
        ForwardOnPlane = FVector::CrossProduct(GroundHit.ImpactNormal, FVector::CrossProduct(ActorRight, GroundHit.ImpactNormal).GetSafeNormal());
        if (ForwardOnPlane.IsNearlyZero()) // Still an issue, use a default
        {
            ForwardOnPlane = FVector::CrossProduct(GroundHit.ImpactNormal, FVector::UpVector); // Or any non-parallel vector
             if (ForwardOnPlane.IsNearlyZero()) ForwardOnPlane = FVector::ForwardVector; // Absolute fallback
        }
    }
	Result.PlacementRotation = UKismetMathLibrary::MakeRotFromZX(GroundHit.ImpactNormal, ForwardOnPlane.GetSafeNormal());
	Result.AdjustedLocation = GroundHit.Location; // Start with the trace hit

	// 2. Overhang Check (can adjust Result.AdjustedLocation.Z)
	FVector FootprintAdjustedLocation = Result.AdjustedLocation; // Pass current best location
	if (!CheckOverhangs(GroundHit.Location, Result.PlacementRotation, FootprintAdjustedLocation, GroundHit.ImpactNormal))
	{
		return Result;
	}
	Result.AdjustedLocation = FootprintAdjustedLocation; // Update location based on overhang (snapping)

	// 3. Obstruction Check (uses final AdjustedLocation and PlacementRotation)
	if (!CheckObstructions(Result.AdjustedLocation, Result.PlacementRotation))
	{
		Result.bIsValid = false;
		return Result;
	}

	Result.bIsValid = true;
	return Result;
}

bool ABwayTargetActor_ActorPlacementFace::CheckSurfaceAngle(const FVector& SurfaceNormal) const
{
	if (SurfaceNormal.IsNearlyZero()) return false; // Invalid normal
	const float AngleRadians = FMath::Acos(FVector::DotProduct(SurfaceNormal.GetSafeNormal(), FVector::UpVector));
	const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);
	return AngleDegrees <= MaxPlacementAngleDegrees;
}

bool ABwayTargetActor_ActorPlacementFace::CheckOverhangs(const FVector& CenterLocationOnGround,
	const FRotator& InitialPlacementRotation,
	FVector& OutAdjustedFootprintCenter,
	const FVector& SurfaceNormalForTrace) const
{
	OutAdjustedFootprintCenter = CenterLocationOnGround; // Default to original
	if (!PlacedActorClass) return false;

	AActor* CDO = PlacedActorClass->GetDefaultObject<AActor>();
	if (!CDO) return false;

	UBoxComponent* FootprintBox = CDO->FindComponentByClass<UBoxComponent>(); // Or a specifically tagged component
    if (!FootprintBox) {
        // Fallback: Try to get any primitive component's bounds. This is less accurate.
        UPrimitiveComponent* PrimCDO = CDO->FindComponentByClass<UPrimitiveComponent>();
        if(PrimCDO)
        {
            // This is a rough approximation using the entire component bounds.
            // A dedicated, smaller "footprint" box component on the buildable is better.
            FVector BoxExtent = PrimCDO->CalcBounds(FTransform::Identity).BoxExtent;
             FootprintBox = NewObject<UBoxComponent>(); // Temporary for extents calculation
             FootprintBox->SetBoxExtent(FVector(BoxExtent.X, BoxExtent.Y, 1.0f)); // Flatten Z for footprint
        } else {
		    UE_LOG(LogTemp, Warning, TEXT("CheckOverhangs: No UBoxComponent for footprint found on CDO of %s. Skipping check."), *PlacedActorClass->GetName());
		    return true; // Or false if strict
        }
	}

	FVector HalfExtents = FootprintBox->GetUnscaledBoxExtent(); // Use unscaled, as transform will scale
	HalfExtents.Z = 0; // We only care about XY footprint for ground check points

	TArray<FVector> LocalSamplePoints;
	LocalSamplePoints.Add(FVector( HalfExtents.X,  HalfExtents.Y, 0));
	LocalSamplePoints.Add(FVector( HalfExtents.X, -HalfExtents.Y, 0));
	LocalSamplePoints.Add(FVector(-HalfExtents.X,  HalfExtents.Y, 0));
	LocalSamplePoints.Add(FVector(-HalfExtents.X, -HalfExtents.Y, 0));
	if (LocalSamplePoints.Num() == 0) return true; // No points to check if extents are zero

	const FTransform PlacementTransform(InitialPlacementRotation, CenterLocationOnGround);
	float MinZ = BIG_NUMBER, MaxZ = -BIG_NUMBER;
	int32 ValidHits = 0;
	float AccumulatedZ = 0.f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BwayTargetActor_ActorPlacementFace_OverhangCheck), true, this);
	QueryParams.bReturnPhysicalMaterial = false;
	
	for (const FVector& LocalPoint : LocalSamplePoints)
	{
		FVector WorldPoint = PlacementTransform.TransformPosition(LocalPoint);
		FHitResult Hit;
		// Trace relative to the surface normal
		FVector TraceStart = WorldPoint + SurfaceNormalForTrace * FootprintTraceUpOffset; 
		FVector TraceEnd = WorldPoint - SurfaceNormalForTrace * FootprintTraceDownLength;   

		if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, this->CollisionChannel, QueryParams))
		{
			if (Hit.bBlockingHit)
			{
				MinZ = FMath::Min(MinZ, Hit.ImpactPoint.Z);
				MaxZ = FMath::Max(MaxZ, Hit.ImpactPoint.Z);
				AccumulatedZ += Hit.ImpactPoint.Z;
				ValidHits++;
			} else { return false; }
		} else { return false; }
	}

	if (ValidHits < LocalSamplePoints.Num()) return false;
	if ((MaxZ - MinZ) > MaxFootprintZDifference) return false;

	// Adjust the center Z to the average height of the sampled ground points if desired
	OutAdjustedFootprintCenter.Z = (MinZ + MaxZ) / 2.0f; 
	// Alternatively, to ensure bottom touches: OutAdjustedFootprintCenter.Z = MaxZ (if buildable pivot is at its base & MaxZ is highest ground point)
	// Or, if buildable pivot is center, and you want its base at MaxZ, OutAdjustedFootprintCenter.Z = MaxZ + BuildPivotToBottomOffsetZ;
	// The current (MinZ+MaxZ)/2 approach averages it out.

	return true;
}

bool ABwayTargetActor_ActorPlacementFace::CheckObstructions(const FVector& CenterLocation,
	const FRotator& PlacementRotation) const
{
	if (!PlacedActorClass) return false;
	AActor* CDO = PlacedActorClass->GetDefaultObject<AActor>();
	if (!CDO) return false;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BwayTargetActor_ActorPlacementFace_ObstructionCheck), false);
	QueryParams.AddIgnoredActor(this);
	if (ActorVisualizationReticle.IsValid()) QueryParams.AddIgnoredActor(ActorVisualizationReticle.Get());
	if (SourceActor.Get()) QueryParams.AddIgnoredActor(SourceActor.Get());
    QueryParams.bReturnPhysicalMaterial = false;

	// Iterate all primitive components on the CDO to form the collision check shape
    TArray<UPrimitiveComponent*> CDOPrimitives;
    CDO->GetComponents<UPrimitiveComponent>(CDOPrimitives);

    if (CDOPrimitives.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("CheckObstructions: No primitive components on CDO of %s. Skipping."), *PlacedActorClass->GetName());
        return true; // No collision to check
    }

	for (UPrimitiveComponent* PrimitiveCDO : CDOPrimitives)
	{
        if (!PrimitiveCDO || PrimitiveCDO->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            continue;
        }

		FCollisionShape Shape = PrimitiveCDO->GetCollisionShape(); // Gets shape with CDO's scale
		FTransform CDOCompRelativeTransform = PrimitiveCDO->GetRelativeTransform();
		FTransform WorldCheckTransform = CDOCompRelativeTransform * FTransform(PlacementRotation, CenterLocation);
		
		TArray<FOverlapResult> Overlaps;
		bool bHasBlockingOverlaps = GetWorld()->OverlapMultiByChannel(
			Overlaps,
			WorldCheckTransform.GetLocation(),
			WorldCheckTransform.GetRotation(),
			CollisionChannel, // Use a specific channel for buildable obstructions if needed
			Shape,
			QueryParams
		);

		if (bHasBlockingOverlaps)
		{
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (Overlap.GetActor()) // Could add more filtering here (e.g., ignore other friendly buildables if allowed)
				{
					UE_LOG(LogTemp, Log, TEXT("Obstruction with %s"), *Overlap.GetActor()->GetName());
					return false;
				}
			}
		}
	}
	return true;
}

//Might want to override this function to allow for a radius check against the ground, possibly including a height check. Or might want to do it in ground trace.
//FHitResult AGameplayAbilityTargetActor_ActorPlacement::PerformTrace(AActor* InSourceActor) const