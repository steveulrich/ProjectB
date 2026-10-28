#include "Camera/BwaySlideCameraShake.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwaySlideCameraShake)

UBwaySlideCameraShake::UBwaySlideCameraShake(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bSingleInstance = false;
    SetRootShakePattern(CreateDefaultSubobject<UBwaySlideCameraShakePattern>(TEXT("SlidePattern")));
}

void UBwaySlideCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& Info) const
{
    Info.Duration = FCameraShakeDuration::Infinite();
    Info.BlendIn = FMath::Max(0.f, BlendInSeconds);
    Info.BlendOut = FMath::Max(0.f, BlendOutSeconds);
}

void UBwaySlideCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
    State.Start(this, Params);
}

void UBwaySlideCameraShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& Result)
{
    const float Weight = State.Update(Params.DeltaTime);
    const float Time = State.GetElapsedTime();
    Result.Rotation.Pitch = FMath::Max(0.f, PitchAmplitude) * FMath::Sin(2.f * PI * FMath::Max(0.f, PitchFrequency) * Time);
    Result.Rotation.Yaw = FMath::Max(0.f, YawAmplitude) * FMath::Sin(2.f * PI * FMath::Max(0.f, YawFrequency) * Time);
    Result.ApplyScale(Weight);
}

bool UBwaySlideCameraShakePattern::IsFinishedImpl() const
{
    return !State.IsPlaying();
}

void UBwaySlideCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
    State.Stop(Params.bImmediately);
}

void UBwaySlideCameraShakePattern::TeardownShakePatternImpl()
{
    State = FCameraShakeState();
}
