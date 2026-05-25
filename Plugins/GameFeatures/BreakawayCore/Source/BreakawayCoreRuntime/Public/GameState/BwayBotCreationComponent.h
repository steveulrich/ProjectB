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

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAIController>> SpawnedBotList;

	void SpawnInitialBots();
	void SpawnOneBot();
};
