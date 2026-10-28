#pragma once

#include "Combat/BwayHeroProjectileBase.h"
#include "GameFramework/Pawn.h"
#include "BwayProjectilePresentationTestTypes.generated.h"

// The fixture captures presentation sinks while retaining production event,
// collision, lifespan, round-removal and camera-shake behavior.
UCLASS(Transient, NotBlueprintable)
class ABwayProjectilePresentationTestActor : public ABwayHeroProjectileBase
{
	GENERATED_BODY()
public:
	mutable int32 CueCalls = 0;
	mutable int32 FallbackCalls = 0;
	int32 DamageCalls = 0;
	TWeakObjectPtr<AActor> LastDamageTarget;
	void ResolveInstigator(APawn* Pawn) { SetInstigator(Pawn); OnRep_Instigator(); }
	bool bAcceptHit = true;
	bool bFinishRoundDuringDamage = false;
	bool bTryReentrantHit = false;
	bool bAliveAfterRoundCallback = false;

	void Hit(bool bBlocking = false) { ProcessHit(FHitResult(), bBlocking); }
	void Timeout() { LifeSpanExpired(); }
	void RoundCleanup() { RemoveForRoundTransition(); }
	void RepeatSpawn() { DispatchPresentationEvent(EBwayProjectilePresentationEvent::Spawn, GetActorLocation(), FVector::UpVector); }
	uint8 ReceivedEvents() const { return ReceivedPresentationEvents; }
	void UseSimulatedRole() { SetRole(ROLE_SimulatedProxy); }
	void Receive(EBwayProjectilePresentationEvent Event, FGameplayTag Cue, USoundBase* Sound,
		APawn* Pawn = nullptr, TSubclassOf<UCameraShakeBase> Shake = nullptr)
	{
		Multicast_PlayPresentationEvent_Implementation(Event, Cue, nullptr, Sound, GetActorLocation(), FVector::UpVector,
			Pawn, Pawn, Shake, 1.f);
	}
protected:
	virtual bool HandleDamageHit(const FHitResult& HitResult) override
	{
		++DamageCalls;
		LastDamageTarget = HitResult.GetActor();
		if (bTryReentrantHit) ProcessHit(HitResult, true);
		if (bFinishRoundDuringDamage)
		{
			RemoveForRoundTransition();
			bAliveAfterRoundCallback = !IsActorBeingDestroyed();
		}
		return bAcceptHit;
	}
	virtual void ExecuteProjectileCue(FGameplayTag, const FVector&, const FVector&, APawn*, AActor*) const override
	{
		++CueCalls;
	}
	virtual void PlayLocalFeedback(UNiagaraSystem* Niagara, USoundBase* Sound, const FVector&, const FVector&) const override
	{
		if (Niagara || Sound) ++FallbackCalls;
	}
};
