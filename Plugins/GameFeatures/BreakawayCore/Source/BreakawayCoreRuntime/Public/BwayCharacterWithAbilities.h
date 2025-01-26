// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacterWithAbilities.h"
#include "BwayCharacterWithAbilities.generated.h"

UCLASS(config=Game)
class BREAKAWAYCORERUNTIME_API ABwayCharacterWithAbilities : public ALyraCharacterWithAbilities
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Movement) UBwayCharacterMovementComponent* BwayCharacterMovementComponent;
public:
	explicit ABwayCharacterWithAbilities(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure) FORCEINLINE UBwayCharacterMovementComponent* GetBwayCharacterMovement() const { return BwayCharacterMovementComponent; }
	FCollisionQueryParams GetIgnoreCharacterParams() const;
};
