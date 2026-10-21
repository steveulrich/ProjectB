#pragma once

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

// Identical transient collision on each peer. No map/package mutation or net
// movement override. Headless and multiplayer regressions use this same course.
namespace BwaySlideReviewCourse
{
inline FVector Origin(bool bConcave) { return FVector(0.f, bConcave ? 14000.f : 12000.f, 5000.f); }

// These are fixture inputs, independent of observed hit fractions or outcomes.
constexpr float ConcaveStartX = -2.f;
constexpr float ConcaveFloorGap = 0.25f;
constexpr float ConcaveSecondSeamX = 1.5f;
inline FVector ConcaveVelocity() { return FVector(620.f, 0.f, -140.f); }
inline FVector ConcaveStart(float CapsuleHalfHeight)
{
	return Origin(true) + FVector(ConcaveStartX, 0.f, CapsuleHalfHeight + ConcaveFloorGap);
}
inline bool Spawn(UWorld* World, TArray<TWeakObjectPtr<AActor>>& Actors, float CapsuleRadius, const FString& RunToken = TEXT("headless"))
{
	if (!World || RunToken.IsEmpty() || !Actors.IsEmpty() || !FMath::IsFinite(CapsuleRadius) || CapsuleRadius <= 0.f) return false;
	const auto AddPlane = [World, &Actors, &RunToken](const FVector& Point, FVector Normal)
	{
		Normal.Normalize();
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		// Each peer creates the same collision course. Movement-base references
		// therefore need identical actor/component paths, including in PIE.
		// A fresh shared run token also permits retries before old actors are GC'd.
		Params.Name = FName(*FString::Printf(TEXT("BwaySlideReviewFloor_%s_%d"), *RunToken, Actors.Num()));
		Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
		Params.bDeferConstruction = true;
		auto* Actor = World->SpawnActor<AActor>(Params);
		if (!Actor) return false;
		Actors.Add(Actor);
		Actor->SetNetAddressable();
		auto* Box = NewObject<UBoxComponent>(Actor, TEXT("ReviewFloorCollision"));
		Box->SetNetAddressable();
		Actor->SetRootComponent(Box);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetBoxExtent(FVector(3000.f, 600.f, 50.f));
		Box->RegisterComponent();
		const FTransform PlaneTransform(FRotationMatrix::MakeFromZX(Normal, FVector::ForwardVector).Rotator(), Point - Normal * 50.f);
		// A plain deferred AActor had no native root when spawned. Set the new
		// root transform explicitly; FinishSpawning has no cached root transform.
		Actor->SetActorTransform(PlaneTransform);
		Actor->FinishSpawning(PlaneTransform);
		return Actor->GetActorTransform().Equals(PlaneTransform, 0.01f);
	};
	// Three consecutive walkable faces form a concave transition. Real capsule sweeps
	// must reach multiple surfaces; the test fails if the corrected branch is absent.
	return AddPlane(Origin(false), FVector::UpVector)
		&& AddPlane(Origin(true), FVector::UpVector)
		// A capsule's center envelope sits R*(sqrt(1+slope^2)-1) above a
		// sloped plane relative to its flat-plane envelope. Compensate that
		// offset so the middle face is exposed from X=0 through X=1.5 cm.
		// The three real sweeps then meet flat -> 0.2 slope -> 0.4 slope.
		&& AddPlane(Origin(true) + FVector(0.f, 0.f, -CapsuleRadius * (FMath::Sqrt(1.04f) - 1.f)), FVector(-0.2f, 0.f, 1.f))
		&& AddPlane(Origin(true) + FVector(0.f, 0.f, -0.2f * ConcaveSecondSeamX - CapsuleRadius * (FMath::Sqrt(1.16f) - 1.f)), FVector(-0.4f, 0.f, 1.f));
}

inline bool Destroy(TArray<TWeakObjectPtr<AActor>>& Actors)
{
	bool bOkay = true;
	for (const auto& Weak : Actors)
		if (auto* Actor = Weak.Get()) bOkay = Actor->Destroy() && bOkay;
	Actors.Reset();
	return bOkay;
}
}
