#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "BwayGameplayEffect_RawlinsJail.generated.h"

/**
 * Infinite GE applied while a victim is encaged by Rawlins Jail.
 * Grants Gameplay.MovementStopped + State.Status.Jailed. Abilities remain usable.
 * TargetTags component is added in PostInitProperties — not in the constructor.
 */
UCLASS()
class HERORAWLINSRUNTIME_API UGE_Bway_RawlinsJailed : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Bway_RawlinsJailed();
	virtual void PostInitProperties() override;

protected:
	UPROPERTY()
	TArray<FGameplayTag> PendingGrantedTags;
};
