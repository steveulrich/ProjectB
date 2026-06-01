// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BwayBTTask_ThrowRelicAtGoal.h"
#include "AI/BwayRelicBotLibrary.h"
#include "BwayCharacterWithAbilities.h"
#include "AIController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBTTask_ThrowRelicAtGoal)

UBwayBTTask_ThrowRelicAtGoal::UBwayBTTask_ThrowRelicAtGoal()
{
	NodeName = TEXT("Throw Relic At Goal");
}

EBTNodeResult::Type UBwayBTTask_ThrowRelicAtGoal::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ABwayCharacterWithAbilities* Character = Controller ? Cast<ABwayCharacterWithAbilities>(Controller->GetPawn()) : nullptr;
	if (!Character)
	{
		return EBTNodeResult::Failed;
	}

	return UBwayRelicBotLibrary::TryBotThrowRelicAtEnemyGoal(Character) ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
