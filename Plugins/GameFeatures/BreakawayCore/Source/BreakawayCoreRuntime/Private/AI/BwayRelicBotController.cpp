// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BwayRelicBotController.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRelicBotController)

ABwayRelicBotController::ABwayRelicBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	bStopAILogicOnUnposses = false;
}

void ABwayRelicBotController::ConfigureRelicAI(UBehaviorTree* InBehaviorTree, UBlackboardData* InBlackboard)
{
	if (InBehaviorTree)
	{
		RelicBehaviorTree = InBehaviorTree;
	}

	if (InBlackboard)
	{
		RelicBlackboard = InBlackboard;
	}
}

void ABwayRelicBotController::StartRelicBotLogic()
{
	RunRelicBehaviorTreeIfReady();
}

void ABwayRelicBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RunRelicBehaviorTreeIfReady();
}

void ABwayRelicBotController::RunRelicBehaviorTreeIfReady()
{
	if (!HasAuthority() || !GetPawn())
	{
		return;
	}

	if (!RelicBehaviorTree)
	{
		return;
	}

	if (RelicBlackboard)
	{
		UBlackboardComponent* BlackboardComp = nullptr;
		UseBlackboard(RelicBlackboard, BlackboardComp);
	}

	if (RunBehaviorTree(RelicBehaviorTree))
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRelicBotController: %s running %s"), *GetName(), *GetNameSafe(RelicBehaviorTree));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRelicBotController: %s failed to run %s"), *GetName(), *GetNameSafe(RelicBehaviorTree));
	}
}
