// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BwayBTTask_PassRelicForward.generated.h"

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayBTTask_PassRelicForward : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBwayBTTask_PassRelicForward();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
