#include "Buildable/BwayPlacementValidation.h"
#include "Buildable/BuildableBase.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"

namespace
{
bool Finite(const FVector& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

FCollisionQueryParams QueryParams(const FBwayPlacementQuery& Query)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BwayPlacementValidation), true);
	if (Query.Source) Params.AddIgnoredActor(Query.Source);
	for (const AActor* Actor : Query.IgnoredActors) if (Actor) Params.AddIgnoredActor(Actor);
	return Params;
}

bool OccupanciesOverlap(const FBox& A, const FTransform& AT, const FBox& B, const FTransform& BT)
{
	const FVector Delta = BT.TransformPosition(B.GetCenter()) - AT.TransformPosition(A.GetCenter());
	const FVector AE = A.GetExtent() * AT.GetScale3D().GetAbs();
	const FVector BE = B.GetExtent() * BT.GetScale3D().GetAbs();
	const FVector AA[] = {AT.GetUnitAxis(EAxis::X), AT.GetUnitAxis(EAxis::Y), AT.GetUnitAxis(EAxis::Z)};
	const FVector BA[] = {BT.GetUnitAxis(EAxis::X), BT.GetUnitAxis(EAxis::Y), BT.GetUnitAxis(EAxis::Z)};
	auto Separated = [&](const FVector& Axis)
	{
		if (Axis.IsNearlyZero()) return false;
		const double RadiusA = AE.X * FMath::Abs(Axis.Dot(AA[0])) + AE.Y * FMath::Abs(Axis.Dot(AA[1])) + AE.Z * FMath::Abs(Axis.Dot(AA[2]));
		const double RadiusB = BE.X * FMath::Abs(Axis.Dot(BA[0])) + BE.Y * FMath::Abs(Axis.Dot(BA[1])) + BE.Z * FMath::Abs(Axis.Dot(BA[2]));
		return FMath::Abs(Delta.Dot(Axis)) >= RadiusA + RadiusB;
	};
	for (const FVector& Axis : AA) if (Separated(Axis)) return false;
	for (const FVector& Axis : BA) if (Separated(Axis)) return false;
	for (const FVector& AAxis : AA) for (const FVector& BAxis : BA)
		if (Separated(FVector::CrossProduct(AAxis, BAxis))) return false;
	return true;
}
}

bool FBwayPlacementQuery::IsInRange(const FVector& Point) const
{
	if (!Source || !Finite(Point) || !Finite(Source->GetActorLocation()) || !FMath::IsFinite(MaxRange)
		|| MaxRange <= 0 || !FMath::IsFinite(RangeTolerance) || RangeTolerance < 0) return false;
	const FVector Delta = Point - Source->GetActorLocation();
	// GroundTrace clips the aim ray to range, then traces vertically down up to 99,999 cm.
	// Preserve downhill targeting instead of imposing an incompatible spherical ground limit.
	return Delta.SizeSquared2D() <= FMath::Square(double(MaxRange + RangeTolerance))
		&& Delta.Z <= MaxRange + RangeTolerance && Delta.Z >= -99999.0 - MaxRange;
}

bool FBwayPlacementQuery::IsUsableNormal(const FVector& Normal) const
{
	return Finite(Normal) && !Normal.IsNearlyZero() && FMath::IsFinite(MaxSlopeDegrees)
		&& FVector::DotProduct(Normal.GetSafeNormal(), FVector::UpVector)
			>= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(MaxSlopeDegrees, 0.f, 89.f)));
}

bool FBwayPlacementQuery::GetLocalGeometry(TArray<FBox>& Bounds, FBox& Footprint) const
{
	Bounds.Reset();
	Footprint = FBox(ForceInit);
	if (!PlacedClass) return false;
	if (!AuthoredBounds.IsValid || !Finite(AuthoredBounds.Min) || !Finite(AuthoredBounds.Max)
		|| AuthoredBounds.GetExtent().GetMin() <= UE_SMALL_NUMBER) return false;
	Bounds.Add(AuthoredBounds);
	Footprint += AuthoredBounds;
	TArray<const UPrimitiveComponent*> Primitives;
	AActor::GetActorClassDefaultComponents(PlacedClass, Primitives);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		// The runtime box mirrors AuthoredBounds, which is already included above.
		if (Primitive && Primitive->GetFName() == TEXT("AttackCollision")) continue;
		if (!Primitive || !CollisionEnabledHasQuery(Primitive->GetCollisionEnabled())
			|| Primitive->GetCollisionResponseToChannel(CollisionChannel) != ECR_Block) continue;
		const FTransform Relative = Primitive->GetRelativeTransform();
		FBox Box = Primitive->CalcBounds(Relative).GetBox();
		if (!Box.IsValid || !Finite(Box.Min) || !Finite(Box.Max) || Box.GetExtent().GetMin() <= UE_SMALL_NUMBER) continue;
		Bounds.Add(Box);
		Footprint += Box;
	}
	return !Bounds.IsEmpty();
}

FPlacementValidationResult FBwayPlacementQuery::ValidateGroundHit(const FHitResult& GroundHit) const
{
	FPlacementValidationResult Result;
	Result.OriginalGroundHit = GroundHit;
	if (!World || !Source || !GroundHit.bBlockingHit || GroundHit.bStartPenetrating
		|| !IsInRange(GroundHit.ImpactPoint) || !Finite(GroundHit.Location) || !IsUsableNormal(GroundHit.ImpactNormal)
		|| !Finite(AimDirection) || !FMath::IsFinite(TraceUp) || TraceUp <= 0
		|| !FMath::IsFinite(TraceDown) || TraceDown <= 0 || !FMath::IsFinite(MaxFootprintZDifference)
		|| MaxFootprintZDifference < 0 || !FMath::IsFinite(GroundClearance) || GroundClearance < 0
		|| Cast<APawn>(GroundHit.GetActor()) || Cast<ABuildableActor>(GroundHit.GetActor())) return Result;

	TArray<FBox> Bounds;
	FBox Footprint;
	if (!GetLocalGeometry(Bounds, Footprint)) return Result;
	const FVector Normal = GroundHit.ImpactNormal.GetSafeNormal();
	FVector Forward = FVector::VectorPlaneProject(AimDirection, Normal).GetSafeNormal();
	if (Forward.IsNearlyZero()) Forward = FVector::VectorPlaneProject(Source->GetActorRightVector(), Normal).GetSafeNormal();
	if (Forward.IsNearlyZero()) return Result;
	Result.PlacementRotation = UKismetMathLibrary::MakeRotFromZX(Normal, Forward);
	const FTransform GroundTransform(Result.PlacementRotation, GroundHit.ImpactPoint);
	const FCollisionQueryParams Params = QueryParams(*this);
	float MinZ = BIG_NUMBER, MaxZ = -BIG_NUMBER;
	// Include the center as well as corners: a ring of supports cannot authorize empty space.
	const FVector Samples[] = {FVector::ZeroVector,
		FVector(Footprint.Min.X, Footprint.Min.Y, 0), FVector(Footprint.Min.X, Footprint.Max.Y, 0),
		FVector(Footprint.Max.X, Footprint.Min.Y, 0), FVector(Footprint.Max.X, Footprint.Max.Y, 0)};
	for (const FVector& Sample : Samples)
	{
		const FVector Point = GroundTransform.TransformPosition(Sample);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Point + Normal * TraceUp, Point - Normal * TraceDown, CollisionChannel, Params)
			|| Hit.bStartPenetrating || !IsUsableNormal(Hit.ImpactNormal) || Cast<APawn>(Hit.GetActor())
			|| Cast<ABuildableActor>(Hit.GetActor())) return Result;
		MinZ = FMath::Min(MinZ, float(Hit.ImpactPoint.Z));
		MaxZ = FMath::Max(MaxZ, float(Hit.ImpactPoint.Z));
	}
	if (MaxZ - MinZ > MaxFootprintZDifference) return Result;
	FVector Center = GroundHit.ImpactPoint;
	Center.Z = (MinZ + MaxZ) * 0.5f;
	Center += Normal * (FMath::Max(0.0, -Footprint.Min.Z) + GroundClearance);
	Result.AdjustedLocation = Center;
	const FTransform Placement(Result.PlacementRotation, Center);
	// Effect spheres and cosmetic meshes need not block collision. Their authored
	// occupancy still prevents stacking structures, using full oriented-box tests.
	for (TActorIterator<ABuildableActor> It(World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && OccupanciesOverlap(AuthoredBounds, Placement,
			It->GetPlacementLocalBounds(), It->GetActorTransform())) return Result;
	}
	for (const FBox& Box : Bounds)
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, Placement.TransformPosition(Box.GetCenter()),
			Placement.GetRotation(), CollisionChannel, FCollisionShape::MakeBox(Box.GetExtent()), Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.bBlockingHit) return Result;
		}
	}
	Result.bIsValid = true;
	return Result;
}

FPlacementValidationResult FBwayPlacementQuery::ValidateSubmittedHit(const FHitResult& Submitted) const
{
	FPlacementValidationResult Invalid;
	if (!World || !Submitted.bBlockingHit || Submitted.bStartPenetrating || !IsInRange(Submitted.Location)
		|| !Finite(Submitted.ImpactPoint) || !Finite(Submitted.ImpactNormal)
		|| Submitted.ImpactNormal.IsNearlyZero() || !Submitted.Location.Equals(Submitted.ImpactPoint, 3.0)
		|| !FMath::IsFinite(TraceUp) || TraceUp <= 0 || !FMath::IsFinite(TraceDown) || TraceDown <= 0) return Invalid;
	const FCollisionQueryParams Params = QueryParams(*this);
	FHitResult Ground;
	if (!World->LineTraceSingleByChannel(Ground, Submitted.Location + FVector::UpVector * TraceUp,
		Submitted.Location - FVector::UpVector * TraceDown, CollisionChannel, Params)
		|| !Ground.ImpactPoint.Equals(Submitted.Location, 3.0)
		|| FVector::DotProduct(Ground.ImpactNormal.GetSafeNormal(), Submitted.ImpactNormal.GetSafeNormal()) < 0.98) return Invalid;
	// Recheck visibility from the authoritative avatar, without trusting TraceStart or the hit actor.
	FHitResult Occluder;
	if (World->LineTraceSingleByChannel(Occluder, Source->GetActorLocation(), Ground.ImpactPoint + Ground.ImpactNormal * 2.f,
		CollisionChannel, Params) && !Occluder.ImpactPoint.Equals(Ground.ImpactPoint, 3.0)) return Invalid;
	return ValidateGroundHit(Ground);
}
