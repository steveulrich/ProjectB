// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BwayCharacterMovementComponent.h"

#include "TopDownArenaMovementComponent.generated.h"

class UObject;

UCLASS()
class UTopDownArenaMovementComponent : public UBwayCharacterMovementComponent
{
	GENERATED_BODY()

public:

	UTopDownArenaMovementComponent(const FObjectInitializer& ObjectInitializer);

	//~UMovementComponent interface
	virtual float GetMaxSpeed() const override;
	//~End of UMovementComponent interface

};
