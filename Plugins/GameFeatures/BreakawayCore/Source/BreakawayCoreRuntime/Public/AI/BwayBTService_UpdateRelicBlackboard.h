// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BwayBTService_UpdateRelicBlackboard.generated.h"

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayBTService_UpdateRelicBlackboard : public UBTService
{
	GENERATED_BODY()

public:
	UBwayBTService_UpdateRelicBlackboard();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
