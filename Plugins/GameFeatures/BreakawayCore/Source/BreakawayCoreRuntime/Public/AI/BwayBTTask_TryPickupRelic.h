// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BwayBTTask_TryPickupRelic.generated.h"

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayBTTask_TryPickupRelic : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBwayBTTask_TryPickupRelic();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
