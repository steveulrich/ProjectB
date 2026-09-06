#pragma once

#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "BwayBaseZone.generated.h"

class UBoxComponent;
class APlayerState;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayEffect_BaseHealing : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UBwayGameplayEffect_BaseHealing();
	static const FName HealingMagnitudeName;
};

/** Team base: authoritative healing and spatial shop access, independent of presentation art. */
UCLASS(Blueprintable)
class BREAKAWAYCORERUNTIME_API ABwayBaseZone : public AActor
{
	GENERATED_BODY()
public:
	ABwayBaseZone();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Uses current pawn position and canonical Breakaway team membership on every query. */
	UFUNCTION(BlueprintPure, Category="Base")
	bool CanUseBase(const APlayerState* Player) const;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Base")
	TObjectPtr<UBoxComponent> Bounds;
	/** Breakaway indices: 0 and 1. Invalid indices disable the base. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Base", meta=(ClampMin="0", ClampMax="1"))
	int32 TeamIndex = 0;
	/** Provisional tuning; final-alpha rate has not been recovered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Base", meta=(ClampMin="0"))
	float HealingPerSecond = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Base")
	bool bEnabled = true;
};
