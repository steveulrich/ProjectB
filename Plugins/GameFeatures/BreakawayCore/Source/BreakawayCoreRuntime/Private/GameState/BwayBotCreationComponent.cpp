#include "GameState/BwayBotCreationComponent.h"

#include "GameModes/LyraGameMode.h"
#include "GameFramework/PlayerState.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Player/LyraPlayerBotController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBotCreationComponent)

UBwayBotCreationComponent::UBwayBotCreationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumBotsToCreate = 7;
	BotControllerClass = ALyraPlayerBotController::StaticClass();
}

void UBwayBotCreationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UBwayBotCreationComponent::SpawnInitialBots);
	}
}

void UBwayBotCreationComponent::SpawnInitialBots()
{
	for (int32 Index = SpawnedBotList.Num(); Index < NumBotsToCreate; ++Index)
	{
		SpawnOneBot();
	}
}

void UBwayBotCreationComponent::SpawnOneBot()
{
	if (!BotControllerClass)
	{
		return;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.OverrideLevel = GetComponentLevel();
	SpawnInfo.ObjectFlags |= RF_Transient;

	AAIController* NewController = GetWorld()->SpawnActor<AAIController>(BotControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (!NewController)
	{
		return;
	}

	if (NewController->PlayerState)
	{
		NewController->PlayerState->SetPlayerName(FString::Printf(TEXT("Breakaway Bot %d"), SpawnedBotList.Num() + 1));
	}

	if (ALyraGameMode* GameMode = GetWorld()->GetAuthGameMode<ALyraGameMode>())
	{
		GameMode->GenericPlayerInitialization(NewController);
		GameMode->RestartPlayer(NewController);
	}

	if (APawn* Pawn = NewController->GetPawn())
	{
		if (ULyraPawnExtensionComponent* PawnExtComponent = Pawn->FindComponentByClass<ULyraPawnExtensionComponent>())
		{
			PawnExtComponent->CheckDefaultInitialization();
		}
	}

	SpawnedBotList.Add(NewController);
}
