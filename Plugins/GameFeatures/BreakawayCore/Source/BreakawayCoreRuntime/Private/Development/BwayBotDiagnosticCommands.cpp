#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "AI/BwayRelicBotController.h"
#include "AI/BwayRelicBotBlackboard.h"
#include "AI/BwayRelicBotLibrary.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTasksComponent.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Relic/RelicActor.h"
#include "Tasks/AITask_MoveTo.h"
#include "UObject/UObjectIterator.h"

namespace BwayBotDiagnostics
{
void Dump(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBotDiagnostic: requires a game world"));
		return;
	}
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	ARelicActor* Relic = UBwayRelicBotLibrary::GetActiveRelic(World);
	UE_LOG(LogTemp, Display, TEXT("BwayBotDiagnostic: world=%s netmode=%d nav=%s relic=%s location=%s"),
		*World->GetName(), static_cast<int32>(World->GetNetMode()), *GetNameSafe(Nav),
		*GetNameSafe(Relic), Relic ? *Relic->GetActorLocation().ToString() : TEXT("none"));
	int32 Count = 0;
	for (TActorIterator<ABwayRelicBotController> It(World); It; ++It)
	{
		++Count;
		ABwayRelicBotController* Bot = *It;
		APawn* Pawn = Bot->GetPawn();
		UBlackboardComponent* BB = Bot->GetBlackboardComponent();
		UBehaviorTreeComponent* BT = Cast<UBehaviorTreeComponent>(Bot->GetBrainComponent());
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		UE_LOG(LogTemp, Display, TEXT("BwayBotDiagnostic: taskComponent=%s postponePaths=%d brainRunning=%d brainPaused=%d"),
			*GetPathNameSafe(Bot->GetGameplayTasksComponent()), Bot->ShouldPostponePathUpdates(),
			BT && BT->IsRunning(), BT && BT->IsPaused());
		for (TObjectIterator<UAITask_MoveTo> TaskIt; TaskIt; ++TaskIt)
		{
			if (TaskIt->GetAIController() == Bot)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayBotDiagnostic: moveTask=%s state=%d component=%s"),
					*TaskIt->GetName(), static_cast<int32>(TaskIt->GetState()),
					*GetPathNameSafe(TaskIt->GetGameplayTasksComponent()));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("BwayBotDiagnostic: bot=%s authority=%d pawn=%s location=%s velocity=%s moveStatus=%d mode=%d maxSpeed=%.1f tasks=%s"),
			*Bot->GetName(), Bot->HasAuthority(), *GetNameSafe(Pawn),
			Pawn ? *Pawn->GetActorLocation().ToString() : TEXT("none"),
			Pawn ? *Pawn->GetVelocity().ToString() : TEXT("none"), static_cast<int32>(Bot->GetMoveStatus()),
			Movement ? static_cast<int32>(Movement->MovementMode.GetValue()) : -1,
			Movement ? Movement->GetMaxSpeed() : 0.f, BT ? *BT->DescribeActiveTasks() : TEXT("none"));
		if (BB)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayBotDiagnostic: blackboard team=%d available=%d selfCarrier=%d enemyCarrier=%d relic=%s goal=%s"),
				BB->GetValueAsInt(BwayRelicBotBlackboard::MyTeamIndex),
				BB->GetValueAsBool(BwayRelicBotBlackboard::bRelicAvailable),
				BB->GetValueAsBool(BwayRelicBotBlackboard::bIsRelicCarrier),
				BB->GetValueAsBool(BwayRelicBotBlackboard::bEnemyHasRelic),
				*BB->GetValueAsVector(BwayRelicBotBlackboard::RelicLocation).ToString(),
				*BB->GetValueAsVector(BwayRelicBotBlackboard::EnemyGoalLocation).ToString());
		}
		if (!Nav || !Pawn || !Relic)
		{
			continue;
		}
		const ANavigationData* Data = Nav->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Pawn->GetNavAgentLocation());
		FNavLocation Start, Goal;
		const bool bStart = Data && Nav->ProjectPointToNavigation(Pawn->GetNavAgentLocation(), Start, Data->GetDefaultQueryExtent(), Data);
		const bool bGoal = Data && Nav->ProjectPointToNavigation(Relic->GetActorLocation(), Goal, Data->GetDefaultQueryExtent(), Data);
		UE_LOG(LogTemp, Display, TEXT("BwayBotDiagnostic: navData=%s startProjected=%d goalProjected=%d extent=%s"),
			*GetNameSafe(Data), bStart, bGoal, Data ? *Data->GetDefaultQueryExtent().ToString() : TEXT("none"));
		if (bStart && bGoal)
		{
			const FPathFindingResult Result = Nav->FindPathSync(FPathFindingQuery(Bot, *Data, Start.Location, Goal.Location));
			UE_LOG(LogTemp, Display, TEXT("BwayBotDiagnostic: pathResult=%d partial=%d points=%d"),
				static_cast<int32>(Result.Result), Result.Path.IsValid() && Result.Path->IsPartial(),
				Result.Path.IsValid() ? Result.Path->GetPathPoints().Num() : 0);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("BwayBotDiagnostic: botCount=%d (AI controllers are server-only; client count may be zero)"), Count);
}

FAutoConsoleCommandWithWorld Command(
	TEXT("bway.Debug.RelicBots"),
	TEXT("Read-only snapshot of relic bot navigation, movement and Blackboard state in the current world."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}
#endif
