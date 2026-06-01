// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BwayBTTask_PassRelicForward.h"
#include "AI/BwayRelicBotLibrary.h"
#include "BwayCharacterWithAbilities.h"
#include "AIController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBTTask_PassRelicForward)

UBwayBTTask_PassRelicForward::UBwayBTTask_PassRelicForward()
{
	NodeName = TEXT("Pass Relic Forward");
}

EBTNodeResult::Type UBwayBTTask_PassRelicForward::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ABwayCharacterWithAbilities* Character = Controller ? Cast<ABwayCharacterWithAbilities>(Controller->GetPawn()) : nullptr;
	if (!Character)
	{
		return EBTNodeResult::Failed;
	}

	return UBwayRelicBotLibrary::TryBotPassRelicForward(Character) ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
