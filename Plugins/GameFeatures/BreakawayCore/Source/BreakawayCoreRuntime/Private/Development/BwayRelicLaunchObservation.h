#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/BodyInstance.h"

// Development-fixture observation only. No physics or gameplay state is changed.
namespace BwayRelicFlight
{
// Match the effective engine DOF mode, not dormant generic plane-lock flags.
// Reject existing constraints and unsupported constrained modes; observation never changes physics.
inline bool HasSupportedLaunchDegreesOfFreedom(const FBodyInstance& Body)
{
    if (Body.DOFConstraint) return false;
    const EDOFMode::Type Mode = FBodyInstance::ResolveDOFMode(Body.DOFMode);
    if (Mode == EDOFMode::None) return true;
    if (Mode != EDOFMode::SixDOF) return false;
    return !Body.bLockXTranslation && !Body.bLockYTranslation && !Body.bLockZTranslation
        && !Body.bLockXRotation && !Body.bLockYRotation && !Body.bLockZRotation;
}

struct FLaunchStep
{
    double Seconds = 0.0;
    double Damping = 0.0;
    FVector Gravity = FVector::ZeroVector;
};

struct FLaunchObservation
{
    TArray<FLaunchStep> Steps;
    double Multiplier = 1.0;
    FVector GravityVelocity = FVector::ZeroVector;
    double Seconds = 0.0;
    bool bInvalid = false;

    bool AddStep(double Dt, double Damping, const FVector& Gravity)
    {
        // The live probe supports bounded synchronous, nonsubstepped flight only.
        if (bInvalid || !FMath::IsFinite(Dt) || Dt <= 0.0 || Dt > 0.05
            || !FMath::IsFinite(Damping) || Damping < 0.0 || Gravity.ContainsNaN()
            || Steps.Num() >= 4 || Seconds + Dt > 0.15)
        { bInvalid = true; return false; }
        const double Drag = FMath::Max(0.0, 1.0 - Damping * Dt);
        if (Multiplier * Drag < 0.5) { bInvalid = true; return false; }
        // Chaos applies gravity and pending launch impulse before linear drag.
        Multiplier *= Drag;
        GravityVelocity = (GravityVelocity + Gravity * Dt) * Drag;
        Seconds += Dt;
        Steps.Add({Dt, Damping, Gravity});
        return true;
    }

    bool Reconstruct(const FVector& Observed, FVector& Initial) const
    {
        if (bInvalid || Steps.IsEmpty() || Observed.ContainsNaN() || Observed.IsNearlyZero()) return false;
        Initial = (Observed - GravityVelocity) / Multiplier;
        return !Initial.ContainsNaN();
    }
};
}
