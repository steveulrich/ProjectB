// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BwayBTTask_TryPickupRelic.h"
#include "AI/BwayRelicBotLibrary.h"
#include "BwayCharacterWithAbilities.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBTTask_TryPickupRelic)

UBwayBTTask_TryPickupRelic::UBwayBTTask_TryPickupRelic()
{
	NodeName = TEXT("Try Pickup Relic");
}

EBTNodeResult::Type UBwayBTTask_TryPickupRelic::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ABwayCharacterWithAbilities* Character = Controller ? Cast<ABwayCharacterWithAbilities>(Controller->GetPawn()) : nullptr;
	if (!Character)
	{
		return EBTNodeResult::Failed;
	}

	return UBwayRelicBotLibrary::TryBotPickupRelic(Character) ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
