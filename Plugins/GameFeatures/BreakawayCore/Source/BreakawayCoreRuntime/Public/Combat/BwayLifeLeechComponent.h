#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffect.h"
#include "BwayLifeLeechComponent.generated.h"

struct FLyraVerbMessage;

/** Persistent, authority-only consumer of post-clamp enemy health damage. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayLifeLeechComponent : public UActorComponent
{
	GENERATED_BODY()
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void OnDamageMessage(FGameplayTag Channel, const FLyraVerbMessage& Message);
	FGameplayMessageListenerHandle DamageListenerHandle;
	bool bApplyingLeech = false;
};

/** Instant healing uses the normal health meta-attribute, caps, stats, and feedback. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayLifeLeechHealingEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UBwayLifeLeechHealingEffect();
};
