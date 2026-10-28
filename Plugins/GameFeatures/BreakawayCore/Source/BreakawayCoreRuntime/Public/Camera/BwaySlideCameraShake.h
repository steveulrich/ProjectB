#pragma once

#include "Camera/CameraShakeBase.h"
#include "BwaySlideCameraShake.generated.h"

/** Small, replaceable local slide motion. Defaults remain provisional until physical feel review. */
UCLASS(EditInlineNew)
class BREAKAWAYCORERUNTIME_API UBwaySlideCameraShakePattern : public UCameraShakePattern
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide Shake", meta=(ClampMin="0.0", ForceUnits="degrees"))
    float PitchAmplitude = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide Shake", meta=(ClampMin="0.0", ForceUnits="degrees"))
    float YawAmplitude = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide Shake", meta=(ClampMin="0.0", ForceUnits="Hz"))
    float PitchFrequency = 4.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide Shake", meta=(ClampMin="0.0", ForceUnits="Hz"))
    float YawFrequency = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide Shake", meta=(ClampMin="0.0", ForceUnits="s"))
    float BlendInSeconds = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide Shake", meta=(ClampMin="0.0", ForceUnits="s"))
    float BlendOutSeconds = 0.2f;

private:
    FCameraShakeState State;
    virtual void GetShakePatternInfoImpl(FCameraShakeInfo& Info) const override;
    virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
    virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& Result) override;
    virtual bool IsFinishedImpl() const override;
    virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
    virtual void TeardownShakePatternImpl() override;
};

UCLASS(Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwaySlideCameraShake : public UCameraShakeBase
{
    GENERATED_BODY()
public:
    UBwaySlideCameraShake(const FObjectInitializer& ObjectInitializer);
};
