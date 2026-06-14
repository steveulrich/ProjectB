#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "BwayBotCreationComponent.generated.h"

UCLASS(Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayBotCreationComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayBotCreationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	/** Runtime override from UBwayMatchFlowConfig / URL (-1 = use defaults / scaling). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Bots")
	void SetNumBotsOverride(int32 InNumBots);

	/** Spawn any missing bots, then respawn all bots at team spawn points (round reset). Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Bots")
	void EnsureBotsForRound();

	/** Destroy and respawn every existing bot at team spawn points. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Bots")
	void RestartAllBots();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots")
	int32 NumBotsToCreate = 7;

	/** When enabled, spawns enough bots to reach TargetPlayerCount (4v4 = 8). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots")
	bool bScaleBotsToTargetPlayerCount = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots")
	int32 TargetPlayerCount = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots")
	TSubclassOf<class AAIController> BotControllerClass;

	/** Behavior tree asset for relic bots (e.g. BT_BW_RelicBot). Loaded when each bot spawns or restarts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots|Relic AI")
	TSoftObjectPtr<class UBehaviorTree> RelicBehaviorTreeAsset;

	/** Blackboard asset paired with RelicBehaviorTreeAsset (e.g. BB_BW_RelicBot). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots|Relic AI")
	TSoftObjectPtr<class UBlackboardData> RelicBlackboardAsset;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAIController>> SpawnedBotList;

	/** -1 = use NumBotsToCreate / TargetPlayerCount scaling. */
	int32 NumBotsOverride = -1;

	void OnExperienceLoaded(const class ULyraExperienceDefinition* Experience);

	void ApplyMatchRulesFromExperience(const class ULyraExperienceDefinition* Experience);
	void TrimExcessBots(int32 TargetBotCount);

	int32 GetTargetBotCount() const;
	int32 GetUrlNumBotsOverride(int32 CurrentDefault) const;
	void SpawnMissingBots();
	void SpawnOneBot();
	void ApplyRelicAIToBot(class AAIController* BotController);
};
