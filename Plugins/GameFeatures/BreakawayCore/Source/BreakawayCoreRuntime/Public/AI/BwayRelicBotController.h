// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularAIController.h"
#include "BwayRelicBotController.generated.h"

class UBehaviorTree;
class UBlackboardData;

/**
 * Bot controller for Capture-the-Relic. Runs BT_BW_RelicBot when configured.
 * Inherits AModularAIController (not ALyraPlayerBotController) so the Game Feature
 * plugin can link without importing non-exported LyraGame symbols across DLLs.
 * Team assignment uses ABwayGameState via BreakawayGameMode::AssignControllerToTeam.
 */
UCLASS(Blueprintable)
class BREAKAWAYCORERUNTIME_API ABwayRelicBotController : public AModularAIController
{
	GENERATED_BODY()

public:
	ABwayRelicBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assign behavior tree assets (from BotCreationComponent or Blueprint defaults). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic Bot")
	void ConfigureRelicAI(UBehaviorTree* InBehaviorTree, UBlackboardData* InBlackboard);

	/** Start or restart behavior-tree logic (safe to call after round respawn). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic Bot")
	void StartRelicBotLogic();

	/** Stop behavior-tree logic before match travel / teardown (prevents BT aux-node crash on GC). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic Bot")
	void StopRelicBotLogic();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Relic Bot")
	TObjectPtr<UBehaviorTree> RelicBehaviorTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Relic Bot")
	TObjectPtr<UBlackboardData> RelicBlackboard;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	void RunRelicBehaviorTreeIfReady();
};
