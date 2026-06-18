// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BwayMatchPhaseTypes.generated.h"

/** Top-level match flow phase owned by UBwayRoundManagementComponent (11-3+). */
UENUM(BlueprintType)
enum class EBwayMatchPhase : uint8
{
	None		UMETA(DisplayName = "None"),
	Prematch	UMETA(DisplayName = "Prematch"),
	Warmup		UMETA(DisplayName = "Warmup"),
	Playing		UMETA(DisplayName = "Playing"),
	PostRound	UMETA(DisplayName = "PostRound"),
	PostMatch	UMETA(DisplayName = "PostMatch")
};
