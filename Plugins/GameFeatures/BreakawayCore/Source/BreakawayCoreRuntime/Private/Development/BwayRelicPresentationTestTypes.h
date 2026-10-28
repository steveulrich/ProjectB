#pragma once

#include "Relic/RelicActor.h"
#include "BwayRelicPresentationTestTypes.generated.h"

// Observe resolved presentation while running the actual pickup/pass/drop paths.
UCLASS(Transient, NotBlueprintable)
class ABwayRelicPresentationTestActor : public ARelicActor
{
    GENERATED_BODY()
public:
    int32 AudioCalls = 0;
    int32 InteractionCalls = 0;
    ERelicState LastAudioState = ERelicState::Neutral;
    ERelicFeedbackEvent LastEvent = ERelicFeedbackEvent::Pickup;
    USoundBase* LastSound = nullptr;
    UNiagaraSystem* LastEffect = nullptr;

    void PrimeCarrierSet() { LoadedRelicAbilitySet = NewObject<ULyraAbilitySet>(this); }
    void UseSimulatedRole() { SetRole(ROLE_SimulatedProxy); }
    void Receive(ERelicFeedbackEvent Event, USoundBase* Sound, UNiagaraSystem* Effect, uint32 EventId)
    {
        Multicast_PlayInteractionFeedback_Implementation(Event, Sound, Effect, FVector(1.f, 2.f, 3.f), EventId);
    }
protected:
    virtual void PlayLocalStateAudio(ERelicState State, USoundBase* Sound, const FVector&, uint32) override
    {
        ++AudioCalls;
        LastAudioState = State;
        LastSound = Sound;
    }
    virtual void PlayLocalInteractionVFX(ERelicFeedbackEvent Event, UNiagaraSystem* Effect, const FVector&, uint32) override
    {
        ++InteractionCalls;
        LastEvent = Event;
        LastEffect = Effect;
    }
};
