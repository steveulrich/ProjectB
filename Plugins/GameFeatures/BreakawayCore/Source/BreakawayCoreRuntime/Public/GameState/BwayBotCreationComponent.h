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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots")
	TSubclassOf<class AAIController> BotControllerClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAIController>> SpawnedBotList;

	void SpawnInitialBots();
	void SpawnOneBot();
};
