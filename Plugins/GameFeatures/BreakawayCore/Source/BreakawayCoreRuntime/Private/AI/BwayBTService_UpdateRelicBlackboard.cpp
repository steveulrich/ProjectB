// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BwayBTService_UpdateRelicBlackboard.h"
#include "AI/BwayRelicBotBlackboard.h"
#include "AI/BwayRelicBotLibrary.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGoalVolume.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Relic/RelicActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBTService_UpdateRelicBlackboard)

UBwayBTService_UpdateRelicBlackboard::UBwayBTService_UpdateRelicBlackboard()
{
	Interval = 0.25f;
	RandomDeviation = 0.05f;
	bTickIntervals = true;
}

void UBwayBTService_UpdateRelicBlackboard::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Blackboard || !Controller)
	{
		return;
	}

	const int32 MyTeam = UBwayRelicBotLibrary::GetBotTeamIndex(Controller);
	Blackboard->SetValueAsInt(BwayRelicBotBlackboard::MyTeamIndex, MyTeam);

	ARelicActor* Relic = UBwayRelicBotLibrary::GetActiveRelic(Controller);
	if (Relic)
	{
		Blackboard->SetValueAsVector(BwayRelicBotBlackboard::RelicLocation, Relic->GetActorLocation());

		const bool bAvailable = Relic->GetCurrentState() != ERelicState::Carried
			&& Relic->GetCurrentState() != ERelicState::Scoring
			&& Relic->GetCurrentState() != ERelicState::Resetting;
		Blackboard->SetValueAsBool(BwayRelicBotBlackboard::bRelicAvailable, bAvailable);

		ABwayCharacterWithAbilities* Carrier = Relic->CurrentCarrier;
		Blackboard->SetValueAsObject(BwayRelicBotBlackboard::RelicCarrier, Carrier);

		const bool bIAmCarrier = Carrier == Cast<ABwayCharacterWithAbilities>(Controller->GetPawn());
		Blackboard->SetValueAsBool(BwayRelicBotBlackboard::bIsRelicCarrier, bIAmCarrier);

		bool bEnemyHasRelic = false;
		if (Carrier && MyTeam >= 0 && Carrier->GetPlayerState())
		{
			if (const ABwayGameState* GS = Controller->GetWorld() ? Controller->GetWorld()->GetGameState<ABwayGameState>() : nullptr)
			{
				bEnemyHasRelic = GS->GetPlayerTeam(Carrier->GetPlayerState()) != MyTeam;
			}
		}
		Blackboard->SetValueAsBool(BwayRelicBotBlackboard::bEnemyHasRelic, bEnemyHasRelic);
	}
	else
	{
		Blackboard->ClearValue(BwayRelicBotBlackboard::RelicLocation);
		Blackboard->SetValueAsBool(BwayRelicBotBlackboard::bRelicAvailable, false);
		Blackboard->ClearValue(BwayRelicBotBlackboard::RelicCarrier);
		Blackboard->SetValueAsBool(BwayRelicBotBlackboard::bIsRelicCarrier, false);
		Blackboard->SetValueAsBool(BwayRelicBotBlackboard::bEnemyHasRelic, false);
	}

	if (const ABwayGoalVolume* EnemyGoal = UBwayRelicBotLibrary::GetEnemyGoalVolume(Controller, Controller))
	{
		Blackboard->SetValueAsVector(BwayRelicBotBlackboard::EnemyGoalLocation, UBwayRelicBotLibrary::GetEnemyGoalLocation(EnemyGoal));
	}
	else
	{
		Blackboard->ClearValue(BwayRelicBotBlackboard::EnemyGoalLocation);
	}

	Blackboard->SetValueAsBool(BwayRelicBotBlackboard::bSuddenDeathActive, UBwayRelicBotLibrary::IsSuddenDeathWindowActive(Controller));

	// Keep request state active while pursuing a free relic (mirrors human holding request input).
	if (ABwayCharacterWithAbilities* BotCharacter = Cast<ABwayCharacterWithAbilities>(Controller->GetPawn()))
	{
		const bool bWantsPickup = Blackboard->GetValueAsBool(BwayRelicBotBlackboard::bRelicAvailable)
			&& !Blackboard->GetValueAsBool(BwayRelicBotBlackboard::bIsRelicCarrier)
			&& !Blackboard->GetValueAsBool(BwayRelicBotBlackboard::bEnemyHasRelic);

		if (bWantsPickup)
		{
			UBwayRelicBotLibrary::ApplyRelicRequestState(BotCharacter);
		}
	}
}
