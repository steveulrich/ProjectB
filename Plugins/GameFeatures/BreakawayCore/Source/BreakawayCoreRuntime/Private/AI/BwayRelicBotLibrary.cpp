// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BwayRelicBotLibrary.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGoalVolume.h"
#include "BwayPlayerState.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRelicBotLibrary)

int32 UBwayRelicBotLibrary::GetBotTeamIndex(const AController* BotController)
{
	if (!BotController || !BotController->PlayerState)
	{
		return -1;
	}

	if (const ABwayGameState* GameState = BotController->GetWorld() ? BotController->GetWorld()->GetGameState<ABwayGameState>() : nullptr)
	{
		return GameState->GetPlayerTeam(BotController->PlayerState);
	}

	return -1;
}

ARelicActor* UBwayRelicBotLibrary::GetActiveRelic(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	const ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	const UBwayRelicManagerComponent* RelicMgr = GameState ? GameState->RelicManagerComponent : nullptr;
	return RelicMgr ? RelicMgr->GetRelicActor() : nullptr;
}

ABwayGoalVolume* UBwayRelicBotLibrary::GetEnemyGoalVolume(const UObject* WorldContextObject, const AController* BotController)
{
	const int32 MyTeam = GetBotTeamIndex(BotController);
	if (MyTeam < 0)
	{
		return nullptr;
	}

	const int32 EnemyTeam = (MyTeam == 0) ? 1 : 0;
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ABwayGoalVolume> It(World); It; ++It)
	{
		ABwayGoalVolume* Goal = *It;
		if (Goal && Goal->OwningTeam == EnemyTeam)
		{
			return Goal;
		}
	}

	return nullptr;
}

FVector UBwayRelicBotLibrary::GetEnemyGoalLocation(const ABwayGoalVolume* EnemyGoal)
{
	return EnemyGoal ? EnemyGoal->GetActorLocation() : FVector::ZeroVector;
}

bool UBwayRelicBotLibrary::IsSuddenDeathWindowActive(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}

	const ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	const UBwayRoundManagementComponent* RoundMgmt = GameState ? GameState->RoundManagementComponent : nullptr;
	if (!RoundMgmt || RoundMgmt->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return false;
	}

	const int32 WarningSeconds = RoundMgmt->SuddenDeathWarningSeconds;
	if (WarningSeconds <= 0)
	{
		return false;
	}

	return RoundMgmt->GetRoundTimeRemaining() <= WarningSeconds;
}

static void AddLooseRelicRequestTag(ULyraAbilitySystemComponent* ASC, const URelicSettings* Settings)
{
	if (!ASC)
	{
		return;
	}

	FGameplayTag RequestTag = Settings ? Settings->RequestingTag : FGameplayTag();
	if (!RequestTag.IsValid())
	{
		RequestTag = FGameplayTag::RequestGameplayTag(FName("State.RequestingRelic"), /*ErrorIfNotFound*/ false);
	}

	if (RequestTag.IsValid())
	{
		ASC->AddLooseGameplayTag(RequestTag);
	}
}

static ULyraAbilitySystemComponent* GetBotAbilitySystem(ABwayCharacterWithAbilities* BotCharacter)
{
	if (!BotCharacter)
	{
		return nullptr;
	}

	if (ULyraAbilitySystemComponent* PawnASC = BotCharacter->GetLyraAbilitySystemComponent())
	{
		return PawnASC;
	}

	if (APlayerState* PS = BotCharacter->GetPlayerState())
	{
		return Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS));
	}

	return nullptr;
}

void UBwayRelicBotLibrary::ApplyRelicRequestState(ABwayCharacterWithAbilities* BotCharacter)
{
	if (!BotCharacter || !BotCharacter->HasAuthority())
	{
		return;
	}

	ULyraAbilitySystemComponent* ASC = GetBotAbilitySystem(BotCharacter);
	if (!ASC)
	{
		return;
	}

	const ARelicActor* Relic = GetActiveRelic(BotCharacter);
	const URelicSettings* Settings = nullptr;
	if (const UWorld* World = BotCharacter->GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			if (const UBwayRelicManagerComponent* RelicMgr = GameState->RelicManagerComponent)
			{
				Settings = RelicMgr->GetRelicSettings();
			}
		}
	}
	if (!Settings)
	{
		Settings = Relic ? Relic->GetRelicSettings() : nullptr;
	}

	if (Settings && Settings->RequestingGameplayEffectClass)
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(BotCharacter);
		EffectContext.AddInstigator(BotCharacter, BotCharacter);

		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Settings->RequestingGameplayEffectClass, 1.0f, EffectContext);
		if (SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			return;
		}
	}

	AddLooseRelicRequestTag(ASC, Settings);
}

bool UBwayRelicBotLibrary::TryBotPickupRelic(ABwayCharacterWithAbilities* BotCharacter)
{
	if (!BotCharacter || !BotCharacter->HasAuthority())
	{
		return false;
	}

	ARelicActor* Relic = GetActiveRelic(BotCharacter);
	if (!Relic || Relic->GetCurrentState() == ERelicState::Carried)
	{
		return false;
	}

	// Must apply request state before CanBePickedUpBy (requires State.RequestingRelic).
	ApplyRelicRequestState(BotCharacter);

	if (!Relic->CanBePickedUpBy(BotCharacter))
	{
		return false;
	}

	if (ULyraAbilitySystemComponent* ASC = GetBotAbilitySystem(BotCharacter))
	{
		FGameplayTag PickupEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Interaction.PickupRelic"), /*ErrorIfNotFound*/ false);
		if (Relic->GetRelicSettings() && Relic->GetRelicSettings()->PickupEventTag.IsValid())
		{
			PickupEventTag = Relic->GetRelicSettings()->PickupEventTag;
		}

		if (PickupEventTag.IsValid())
		{
			FGameplayEventData Payload;
			Payload.EventTag = PickupEventTag;
			Payload.Instigator = BotCharacter;
			Payload.Target = Relic;
			ASC->HandleGameplayEvent(PickupEventTag, &Payload);
		}
	}

	BotCharacter->TryPickupOverlappingRelic();

	return Relic->CurrentCarrier == BotCharacter;
}

bool UBwayRelicBotLibrary::TryBotThrowRelicAtEnemyGoal(ABwayCharacterWithAbilities* BotCharacter)
{
	if (!BotCharacter || !BotCharacter->HasAuthority())
	{
		return false;
	}

	ARelicActor* Relic = GetActiveRelic(BotCharacter);
	if (!Relic || Relic->CurrentCarrier != BotCharacter || Relic->GetCurrentState() != ERelicState::Carried)
	{
		return false;
	}

	const URelicSettings* Settings = Relic->GetRelicSettings();
	if (!Settings)
	{
		return false;
	}

	if (AController* Controller = BotCharacter->GetController())
	{
		if (const ABwayGoalVolume* EnemyGoal = GetEnemyGoalVolume(BotCharacter, Controller))
		{
			FVector ToGoal = GetEnemyGoalLocation(EnemyGoal) - BotCharacter->GetActorLocation();
			ToGoal.Z += FMath::Tan(FMath::DegreesToRadians(Settings->ThrowAngle)) * ToGoal.Size2D();
			const FVector ThrowVelocity = ToGoal.GetSafeNormal() * Settings->ThrowVelocity;
			Relic->Server_ThrowRelic(ThrowVelocity);
			return true;
		}
	}

	return false;
}

ABwayCharacterWithAbilities* UBwayRelicBotLibrary::FindForwardTeammate(const ABwayCharacterWithAbilities* BotCharacter)
{
	if (!BotCharacter)
	{
		return nullptr;
	}

	const AController* Controller = BotCharacter->GetController();
	const int32 MyTeam = GetBotTeamIndex(Controller);
	if (MyTeam < 0)
	{
		return nullptr;
	}

	const ABwayGoalVolume* EnemyGoal = GetEnemyGoalVolume(BotCharacter, Controller);
	const FVector ForwardDir = EnemyGoal
		? (GetEnemyGoalLocation(EnemyGoal) - BotCharacter->GetActorLocation()).GetSafeNormal2D()
		: BotCharacter->GetActorForwardVector().GetSafeNormal2D();

	ABwayCharacterWithAbilities* BestTeammate = nullptr;
	float BestDot = 0.35f;

	const UWorld* World = BotCharacter->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
	{
		AController* OtherController = It->Get();
		if (!OtherController || OtherController == Controller)
		{
			continue;
		}

		if (GetBotTeamIndex(OtherController) != MyTeam)
		{
			continue;
		}

		APawn* OtherPawn = OtherController->GetPawn();
		ABwayCharacterWithAbilities* Teammate = Cast<ABwayCharacterWithAbilities>(OtherPawn);
		if (!Teammate)
		{
			continue;
		}

		const FVector ToTeammate = (Teammate->GetActorLocation() - BotCharacter->GetActorLocation()).GetSafeNormal2D();
		const float Dot = FVector::DotProduct(ForwardDir, ToTeammate);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestTeammate = Teammate;
		}
	}

	return BestTeammate;
}

bool UBwayRelicBotLibrary::TryBotPassRelicForward(ABwayCharacterWithAbilities* BotCharacter)
{
	if (!BotCharacter || !BotCharacter->HasAuthority())
	{
		return false;
	}

	ARelicActor* Relic = GetActiveRelic(BotCharacter);
	if (!Relic || Relic->CurrentCarrier != BotCharacter || Relic->GetCurrentState() != ERelicState::Carried)
	{
		return false;
	}

	const URelicSettings* Settings = Relic->GetRelicSettings();
	if (!Settings)
	{
		return false;
	}

	const ABwayCharacterWithAbilities* Teammate = FindForwardTeammate(BotCharacter);
	if (!Teammate)
	{
		return false;
	}

	FVector ToTeammate = Teammate->GetActorLocation() - BotCharacter->GetActorLocation();
	ToTeammate.Z += FMath::Tan(FMath::DegreesToRadians(Settings->ThrowAngle)) * ToTeammate.Size2D();
	const FVector PassVelocity = ToTeammate.GetSafeNormal() * Settings->ThrowVelocity;
	Relic->Server_PassRelic(PassVelocity);
	return true;
}
