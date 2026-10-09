#pragma once

#include "Camera/CameraShakeBase.h"
#include "BwaySlideCameraShakeTestTypes.generated.h"

// Transient automation patterns use the engine's real duration/blending state.
// They produce no visual offsets and are never referenced by gameplay assets.
UCLASS(Transient, NotBlueprintable)
class UBwaySlideTestShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()
public:
	UPROPERTY()
	float Duration = -1.f;
	int32 StopCount = 0;
	bool bLastStopImmediate = false;
private:
	FCameraShakeState State;
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& Info) const override
	{
		Info.Duration = Duration > 0.f ? FCameraShakeDuration(Duration) : FCameraShakeDuration::Infinite();
		Info.BlendOut = 0.25f;
	}
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override
	{
		StopCount = 0;
		bLastStopImmediate = false;
		State.Start(this, Params);
	}
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& Result) override
	{
		State.Update(Params.DeltaTime);
	}
	virtual bool IsFinishedImpl() const override { return !State.IsPlaying(); }
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override
	{
		++StopCount;
		bLastStopImmediate = Params.bImmediately;
		State.Stop(Params.bImmediately);
	}
	virtual void TeardownShakePatternImpl() override { State = FCameraShakeState(); }
};

UCLASS(Transient, NotBlueprintable)
class UBwaySlideTestShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UBwaySlideTestShake(const FObjectInitializer& Initializer) : Super(Initializer)
	{
		bSingleInstance = false;
		SetRootShakePattern(CreateDefaultSubobject<UBwaySlideTestShakePattern>(TEXT("Pattern")));
	}
};

UCLASS(Transient, NotBlueprintable)
class UBwaySlideAlternateTestShake : public UBwaySlideTestShake
{
	GENERATED_BODY()
};

UCLASS(Transient, NotBlueprintable)
class UBwayUnrelatedTestShake : public UBwaySlideTestShake
{
	GENERATED_BODY()
};

UCLASS(Transient, NotBlueprintable)
class UBwaySlideFiniteTestShake : public UBwaySlideTestShake
{
	GENERATED_BODY()
public:
	UBwaySlideFiniteTestShake(const FObjectInitializer& Initializer) : Super(Initializer)
	{
		CastChecked<UBwaySlideTestShakePattern>(GetRootShakePattern())->Duration = 0.1f;
	}
};

UCLASS(Transient, NotBlueprintable)
class UBwaySlideSingleInstanceTestShake : public UBwaySlideTestShake
{
	GENERATED_BODY()
public:
	UBwaySlideSingleInstanceTestShake(const FObjectInitializer& Initializer) : Super(Initializer)
	{
		bSingleInstance = true;
	}
};
