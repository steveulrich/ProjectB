#include "Buildable/BwayBuildablePlacementLibrary.h"

#include "BwayGameState.h"
#include "Buildable/BuildableBase.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBuildablePlacementLibrary)

namespace
{
void ApplyGoldDelta(APlayerState* PlayerState, float GoldDelta)
{
	if (!PlayerState || FMath::IsNearlyZero(GoldDelta))
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
	if (!ASC || !ASC->GetSet<UBwayGoldAttributeSet>())
	{
		return;
	}

	UGameplayEffect* GoldEffect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None);
	GoldEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& Modifier = GoldEffect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UBwayGoldAttributeSet::GetCurrentGoldAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(GoldDelta);

	ASC->ApplyGameplayEffectToSelf(GoldEffect, 1.0f, ASC->MakeEffectContext());
}
}

ABuildableActor* UBwayBuildablePlacementLibrary::SpawnBuildableForPlayer(const UObject* WorldContextObject, APlayerController* PlayerController, UBwayBuildableDataAsset* BuildableData, const FTransform& SpawnTransform)
{
	FText FailureReason;
	if (!CanPlayerPlaceBuildable(WorldContextObject, PlayerController, BuildableData, FailureReason))
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !World->GetAuthGameMode())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerController;
	SpawnParams.Instigator = PlayerController ? PlayerController->GetPawn() : nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	ABuildableActor* Buildable = World->SpawnActor<ABuildableActor>(BuildableData->BuildableActorClass, SpawnTransform, SpawnParams);
	if (!Buildable)
	{
		return nullptr;
	}

	int32 TeamIndex = -1;
	if (const ABwayGameState* BwayGS = World->GetGameState<ABwayGameState>())
	{
		TeamIndex = BwayGS->GetPlayerTeam(PlayerController ? PlayerController->PlayerState : nullptr);
	}

	Buildable->InitializeBuildable(PlayerController, TeamIndex);
	ApplyGoldDelta(PlayerController ? PlayerController->PlayerState : nullptr, -BuildableData->Cost);
	return Buildable;
}

bool UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable(const UObject* WorldContextObject, APlayerController* PlayerController, UBwayBuildableDataAsset* BuildableData, FText& OutFailureReason)
{
	if (!PlayerController || !BuildableData || !BuildableData->BuildableActorClass)
	{
		OutFailureReason = NSLOCTEXT("BreakawayBuildables", "InvalidPlacementData", "Invalid buildable placement data.");
		return false;
	}

	if (BuildableData->MaxActiveBuildablesPerPlayer > 0 && CountActiveBuildablesForPlayer(WorldContextObject, PlayerController) >= BuildableData->MaxActiveBuildablesPerPlayer)
	{
		OutFailureReason = NSLOCTEXT("BreakawayBuildables", "BuildableCapReached", "Maximum active buildables reached.");
		return false;
	}

	if (APlayerState* PlayerState = PlayerController->PlayerState)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState))
		{
			if (const UBwayGoldAttributeSet* GoldSet = ASC->GetSet<UBwayGoldAttributeSet>())
			{
				if (GoldSet->GetCurrentGold() < BuildableData->Cost)
				{
					OutFailureReason = NSLOCTEXT("BreakawayBuildables", "InsufficientGold", "Not enough gold.");
					return false;
				}
			}
			else
			{
				OutFailureReason = NSLOCTEXT("BreakawayBuildables", "GoldSetMissing", "Gold is not initialized.");
				return false;
			}
		}
		else
		{
			OutFailureReason = NSLOCTEXT("BreakawayBuildables", "ASCMissing", "Ability system is not initialized.");
			return false;
		}
	}
	else
	{
		OutFailureReason = NSLOCTEXT("BreakawayBuildables", "PlayerStateMissing", "Player state is not initialized.");
		return false;
	}

	OutFailureReason = FText::GetEmpty();
	return true;
}

int32 UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(const UObject* WorldContextObject, APlayerController* PlayerController)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !PlayerController)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<ABuildableActor> It(World); It; ++It)
	{
		const ABuildableActor* Buildable = *It;
		if (Buildable && Buildable->GetOwner() == PlayerController && !Buildable->IsActorBeingDestroyed())
		{
			++Count;
		}
	}
	return Count;
}
