// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorPlacementTargeting/BwayTargetActor_ActorPlacementFace.h"
#include "ActorPlacementTargeting/BwayWorldReticle_ActorVisualization.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Buildable/BuildableBase.h"
#include "Engine/World.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Abilities/GameplayAbilityWorldReticle_ActorVisualization.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"

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
	if (!BuildableDataAsset && InAbility)
	{
		if (APlayerController* PC = InAbility->GetCurrentActorInfo()->PlayerController.Get())
		{
			BuildableDataAsset = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(GetWorld(), PC);
		}
	}

	if (!PlacedActorClass)
	{
		if (BuildableDataAsset)
		{
			PlacedActorClass = BuildableDataAsset->BuildableActorClass;
		}
		else if (InAbility)
		{
			if (APlayerController* PC = InAbility->GetCurrentActorInfo()->PlayerController.Get())
			{
				if (UBwayBuildableDataAsset* ResolvedBuildableData = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(GetWorld(), PC))
				{
					BuildableDataAsset = ResolvedBuildableData;
					PlacedActorClass = ResolvedBuildableData->BuildableActorClass;
				}
			}
		}
	}

	Super::StartTargeting(InAbility); // This will create ReticleActor (often a decal)

	// WaitTargetData only forwards LocalInputConfirm/Cancel when ShouldProduceTargetData() is true,
	// which for the base target actor requires PrimaryPC->IsLocalController(). Repair if missing.
	if (!PrimaryPC)
	{
		if (APawn* SourcePawn = Cast<APawn>(SourceActor.Get()))
		{
			PrimaryPC = Cast<APlayerController>(SourcePawn->GetController());
		}
		if (!PrimaryPC && InAbility && InAbility->GetCurrentActorInfo())
		{
			PrimaryPC = InAbility->GetCurrentActorInfo()->PlayerController.Get();
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("BwayPlacementTarget StartTargeting PrimaryPC=%s SourceActor=%s ShouldProduce=%d"),
		*GetNameSafe(PrimaryPC),
		*GetNameSafe(SourceActor.Get()),
		ShouldProduceTargetData() ? 1 : 0);

	InitializePlacementVisualization(InAbility);
}

bool ABwayTargetActor_ActorPlacementFace::ShouldProduceTargetData() const
{
	if (PrimaryPC && PrimaryPC->IsLocalController())
	{
		return true;
	}

	if (const APawn* SourcePawn = Cast<APawn>(SourceActor.Get()))
	{
		if (const APlayerController* PC = Cast<APlayerController>(SourcePawn->GetController()))
		{
			return PC->IsLocalController();
		}
	}

	return Super::ShouldProduceTargetData();
}

void ABwayTargetActor_ActorPlacementFace::ConfirmTargeting()
{
	UE_LOG(LogTemp, Log,
		TEXT("BwayPlacementTarget ConfirmTargeting ShouldProduce=%d PrimaryPC=%s SourceActor=%s bLastValid=%d"),
		ShouldProduceTargetData() ? 1 : 0,
		*GetNameSafe(PrimaryPC),
		*GetNameSafe(SourceActor.Get()),
		bLastTickPlacementValid ? 1 : 0);

	Super::ConfirmTargeting();
}

void ABwayTargetActor_ActorPlacementFace::CancelTargeting()
{
	UE_LOG(LogTemp, Log, TEXT("BwayPlacementTarget CancelTargeting ShouldProduce=%d"), ShouldProduceTargetData() ? 1 : 0);
	Super::CancelTargeting();
}

void ABwayTargetActor_ActorPlacementFace::InitializePlacementVisualization(UGameplayAbility* InAbility)
{
	if (!TryInitializeVisualizationFromBuildableData(InAbility))
	{
		if (!TryInitializeVisualizationFromActorClass())
		{
			UE_LOG(LogTemp, Warning, TEXT("ABwayTargetActor_ActorPlacementFace: No preview mesh available. Set PrimaryBuildableMesh on the buildable data asset or mesh on %s."),
				PlacedActorClass ? *PlacedActorClass->GetName() : TEXT("(null class)"));
		}
	}

	bLastTickPlacementValid = false;
	if (ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle->UpdatePlacementVisuals(bLastTickPlacementValid);
	}
}

bool ABwayTargetActor_ActorPlacementFace::TryInitializeVisualizationFromBuildableData(UGameplayAbility* InAbility)
{
	APlayerController* PC = InAbility ? InAbility->GetCurrentActorInfo()->PlayerController.Get() : nullptr;
	USkeletalMesh* PreviewMesh = UBwayBuildablePlacementLibrary::ResolvePreviewMeshForPlayer(GetWorld(), PC, BuildableDataAsset);
	if (!PreviewMesh)
	{
		return false;
	}

	if (!ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle = GetWorld()->SpawnActor<ABwayWorldReticle_ActorVisualization>();
	}

	if (!ActorVisualizationReticle.IsValid())
	{
		return false;
	}

	ActorVisualizationReticle->InitializeReticleVisualizationFromPreviewMesh(SourceActor.Get(), PreviewMesh, ValidPlacementMaterial, InvalidPlacementMaterial);

	if (AGameplayAbilityWorldReticle* ParentReticle = ReticleActor.Get())
	{
		ActorVisualizationReticle->AttachToActor(ParentReticle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		ActorVisualizationReticle->SetIsReticleVisible(true);
	}
	else
	{
		ReticleActor = ActorVisualizationReticle;
		ActorVisualizationReticle->SetIsReticleVisible(true);
	}

	return true;
}

bool ABwayTargetActor_ActorPlacementFace::TryInitializeVisualizationFromActorClass()
{
	if (!PlacedActorClass)
	{
		return false;
	}

	AActor* VisualizationActor = GetWorld()->SpawnActor(PlacedActorClass);
	if (!VisualizationActor)
	{
		return false;
	}

	if (!ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle = GetWorld()->SpawnActor<ABwayWorldReticle_ActorVisualization>();
	}

	if (ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle->InitializeReticleVisualizationInformation(SourceActor.Get(), VisualizationActor, ValidPlacementMaterial, InvalidPlacementMaterial);

		if (AGameplayAbilityWorldReticle* ParentReticle = ReticleActor.Get())
		{
			ActorVisualizationReticle->AttachToActor(ParentReticle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			ActorVisualizationReticle->SetIsReticleVisible(true);
		}
		else
		{
			ReticleActor = ActorVisualizationReticle;
			ActorVisualizationReticle->SetIsReticleVisible(true);
		}
	}

	GetWorld()->DestroyActor(VisualizationActor);
	return ActorVisualizationReticle.IsValid();
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
		const FHitResult TraceHit = PerformTrace(SourceActor);
		const FGameplayAbilityTargetDataHandle Handle = CreateTargetData(TraceHit);
		if (Handle.IsValid(0))
		{
			UE_LOG(LogTemp, Log, TEXT("BwayPlacementTarget ConfirmTargetingAndContinue VALID — broadcasting target data"));
			TargetDataReadyDelegate.Broadcast(Handle);
		}
		else
		{
			// Invalid confirm: do not broadcast — WaitTargetData stays active so the player can reposition and retry.
			UE_LOG(LogTemp, Warning,
				TEXT("BwayPlacementTarget ConfirmTargetingAndContinue INVALID — no broadcast (preview stays). bLastValid=%d Hit=%d Actor=%s"),
				bLastTickPlacementValid ? 1 : 0,
				TraceHit.bBlockingHit ? 1 : 0,
				*GetNameSafe(TraceHit.GetActor()));
		}
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
	// Re-validate at confirm time (do not trust only the last tick cache).
	const FPlacementValidationResult Validation = PerformPlacementValidation(InHitResult);
	bLastTickPlacementValid = Validation.bIsValid;
	LastValidatedPlacementResult = Validation;

	if (Validation.bIsValid && Validation.OriginalGroundHit.bBlockingHit)
	{
		FGameplayAbilityTargetData_SingleTargetHit* ReturnData = new FGameplayAbilityTargetData_SingleTargetHit();

		FHitResult FinalHit = Validation.OriginalGroundHit;
		FinalHit.Location = Validation.AdjustedLocation;
		FinalHit.ImpactPoint = Validation.AdjustedLocation;
		ReturnData->HitResult = FinalHit;

		return FGameplayAbilityTargetDataHandle(ReturnData);
	}

	// Invalid confirm: empty handle — WaitTargetData stays alive (see ConfirmTargetingAndContinue).
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

	// Lift pivot so collision bottoms rest on the ground instead of intersecting it.
	const float PivotLift = ComputePivotLiftAlongLocalUp();
	if (PivotLift > 0.f)
	{
		Result.AdjustedLocation += Result.PlacementRotation.Quaternion().GetUpVector() * PivotLift;
	}

	// 3. Obstruction Check (uses final AdjustedLocation and PlacementRotation).
	// Ignore the traced ground actor — pivot-centered collision always overlaps the floor otherwise.
	if (!CheckObstructions(Result.AdjustedLocation, Result.PlacementRotation, GroundHit.GetActor()))
	{
		UE_LOG(LogTemp, Verbose, TEXT("BwayPlacementTarget validation FAIL: obstruction at %s"), *Result.AdjustedLocation.ToCompactString());
		Result.bIsValid = false;
		return Result;
	}

	Result.bIsValid = true;
	return Result;
}

float ABwayTargetActor_ActorPlacementFace::ComputePivotLiftAlongLocalUp() const
{
	if (!PlacedActorClass)
	{
		return ObstructionGroundClearance;
	}

	const AActor* CDO = PlacedActorClass->GetDefaultObject<AActor>();
	if (!CDO)
	{
		return ObstructionGroundClearance;
	}

	float MaxLift = 0.f;

	TArray<UPrimitiveComponent*> CDOPrimitives;
	CDO->GetComponents<UPrimitiveComponent>(CDOPrimitives);

	for (const UPrimitiveComponent* PrimitiveCDO : CDOPrimitives)
	{
		if (!PrimitiveCDO)
		{
			continue;
		}

		const ECollisionEnabled::Type CollisionEnabled = PrimitiveCDO->GetCollisionEnabled();
		if (CollisionEnabled == ECollisionEnabled::NoCollision || CollisionEnabled == ECollisionEnabled::QueryOnly)
		{
			continue;
		}

		const FBoxSphereBounds ActorLocalBounds = PrimitiveCDO->CalcBounds(PrimitiveCDO->GetRelativeTransform());
		const float BottomZ = ActorLocalBounds.Origin.Z - ActorLocalBounds.BoxExtent.Z;
		MaxLift = FMath::Max(MaxLift, -BottomZ);
	}

	return MaxLift + ObstructionGroundClearance;
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
	const FRotator& PlacementRotation,
	AActor* GroundHitActor) const
{
	if (!PlacedActorClass) return false;
	AActor* CDO = PlacedActorClass->GetDefaultObject<AActor>();
	if (!CDO) return false;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BwayTargetActor_ActorPlacementFace_ObstructionCheck), false);
	QueryParams.AddIgnoredActor(this);
	if (ActorVisualizationReticle.IsValid()) QueryParams.AddIgnoredActor(ActorVisualizationReticle.Get());
	if (SourceActor.Get()) QueryParams.AddIgnoredActor(SourceActor.Get());
	if (ReticleActor.IsValid()) QueryParams.AddIgnoredActor(ReticleActor.Get());
	if (GroundHitActor)
	{
		QueryParams.AddIgnoredActor(GroundHitActor);
	}
	QueryParams.bReturnPhysicalMaterial = false;

	TArray<UPrimitiveComponent*> CDOPrimitives;
	CDO->GetComponents<UPrimitiveComponent>(CDOPrimitives);

	if (CDOPrimitives.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CheckObstructions: No primitive components on CDO of %s. Skipping."), *PlacedActorClass->GetName());
		return true;
	}

	const FTransform PlacementTM(PlacementRotation, CenterLocation);

	for (UPrimitiveComponent* PrimitiveCDO : CDOPrimitives)
	{
		if (!PrimitiveCDO)
		{
			continue;
		}

		const ECollisionEnabled::Type CollisionEnabled = PrimitiveCDO->GetCollisionEnabled();
		if (CollisionEnabled == ECollisionEnabled::NoCollision || CollisionEnabled == ECollisionEnabled::QueryOnly)
		{
			continue;
		}

		const FCollisionShape Shape = PrimitiveCDO->GetCollisionShape();
		const FTransform WorldCheckTransform = PrimitiveCDO->GetRelativeTransform() * PlacementTM;

		TArray<FOverlapResult> Overlaps;
		const bool bHasOverlaps = GetWorld()->OverlapMultiByChannel(
			Overlaps,
			WorldCheckTransform.GetLocation(),
			WorldCheckTransform.GetRotation(),
			CollisionChannel,
			Shape,
			QueryParams);

		if (!bHasOverlaps)
		{
			continue;
		}

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* OverlapActor = Overlap.GetActor();
			if (OverlapActor && (OverlapActor == GroundHitActor || OverlapActor == this || OverlapActor == SourceActor.Get()))
			{
				continue;
			}

			// Floor / terrain pieces often aren't the exact ground-hit actor (multi-mesh floors).
			// Skip blockers whose bounds sit at or below the placement plane.
			if (Overlap.Component.IsValid())
			{
				const FBox SphereBounds = Overlap.Component->Bounds.GetBox();
				const float ComponentTopZ = SphereBounds.Max.Z;
				if (ComponentTopZ <= CenterLocation.Z + 2.0f)
				{
					continue;
				}
			}

			if (Overlap.bBlockingHit || (Overlap.Component.IsValid() && Overlap.Component->GetCollisionResponseToChannel(CollisionChannel) == ECR_Block))
			{
				UE_LOG(LogTemp, Log, TEXT("BwayPlacementTarget obstruction with %s.%s"),
					OverlapActor ? *OverlapActor->GetName() : TEXT("(null)"),
					Overlap.Component.IsValid() ? *Overlap.Component->GetName() : TEXT("(null)"));
				return false;
			}
		}
	}
	return true;
}

//Might want to override this function to allow for a radius check against the ground, possibly including a height check. Or might want to do it in ground trace.
//FHitResult AGameplayAbilityTargetActor_ActorPlacement::PerformTrace(AActor* InSourceActor) const