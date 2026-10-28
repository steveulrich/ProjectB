#include "Relic/RelicSettings.h"

URelicSettings::URelicSettings()
{
}

TSoftObjectPtr<USoundBase> URelicSettings::ResolveFeedbackSound(ERelicFeedbackEvent Event) const
{
    switch (Event)
    {
    case ERelicFeedbackEvent::Pickup: return PickupSound;
    case ERelicFeedbackEvent::Drop: return DropSound;
    case ERelicFeedbackEvent::ThrowLaunch: return ThrowLaunchSound.IsNull() ? DropSound : ThrowLaunchSound;
    case ERelicFeedbackEvent::PassLaunch: return PassLaunchSound.IsNull() ? DropSound : PassLaunchSound;
    case ERelicFeedbackEvent::PreparedCatch: return PreparedCatchSound.IsNull() ? PickupSound : PreparedCatchSound;
    case ERelicFeedbackEvent::Fumble: return FumbleSound.IsNull() ? DropSound : FumbleSound;
    }
    return {};
}

TSoftObjectPtr<UNiagaraSystem> URelicSettings::ResolveFeedbackEffect(ERelicFeedbackEvent Event) const
{
    switch (Event)
    {
    case ERelicFeedbackEvent::Pickup: return PickupBurstEffect;
    case ERelicFeedbackEvent::Drop: return DropBurstEffect;
    case ERelicFeedbackEvent::ThrowLaunch: return ThrowLaunchEffect.IsNull() ? DropBurstEffect : ThrowLaunchEffect;
    case ERelicFeedbackEvent::PassLaunch: return PassLaunchEffect.IsNull() ? DropBurstEffect : PassLaunchEffect;
    case ERelicFeedbackEvent::PreparedCatch: return PreparedCatchEffect.IsNull() ? PickupBurstEffect : PreparedCatchEffect;
    case ERelicFeedbackEvent::Fumble: return FumbleEffect.IsNull() ? DropBurstEffect : FumbleEffect;
    }
    return {};
}
