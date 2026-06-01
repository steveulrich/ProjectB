// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BwayBTTask_ThrowRelicAtGoal.generated.h"

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayBTTask_ThrowRelicAtGoal : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBwayBTTask_ThrowRelicAtGoal();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
