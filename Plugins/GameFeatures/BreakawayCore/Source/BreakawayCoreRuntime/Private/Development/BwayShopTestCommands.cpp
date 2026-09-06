// Exercise the widget's RPC path outside the editor Python execution guard.
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "CoreGlobals.h"
#include "UI/BwayUpgradeShopWidget.h"
#include "BwayPlayerState.h"
#include "Economy/BwayUpgradeComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "GameFramework/Pawn.h"

namespace BwayShopTest
{
void BuySelected(UWorld* World)
{
	if (!World || !World->IsGameWorld()) return;
	// Python calls set GAllowActorScriptExecutionInEditor, which forces even
	// nested RPC calls to execute locally. Wait until a normal game timer tick.
	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		if (GAllowActorScriptExecutionInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayShopTest: Refusing purchase under editor script guard"));
			return;
		}
		for (TObjectIterator<UBwayUpgradeShopWidget> It; It; ++It)
		{
			UBwayUpgradeShopWidget* Widget = *It;
			APlayerController* PC = Widget->GetOwningPlayer();
			if (Widget->GetWorld() != World || !Widget->IsActivated() || !PC || !PC->IsLocalController()) continue;
			UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Native widget purchase world=%s netmode=%d authority=%d scriptguard=%d"),
				*World->GetPathName(), static_cast<int32>(World->GetNetMode()), PC->HasAuthority(), GAllowActorScriptExecutionInEditor);
			Widget->ProcessEvent(Widget->FindFunctionChecked(TEXT("BuySelected")), nullptr);
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("BwayShopTest: No active local shop in requested world"));
	}), 0.2f, false);
}

FAutoConsoleCommandWithWorld BuyCommand(
	TEXT("bway.Test.ShopBuySelected"),
	TEXT("Development test: queue the active local shop purchase on a normal game tick."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&BuySelected));

void DamageRemotePlayer(UWorld* World)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client) return;
	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		if (GAllowActorScriptExecutionInEditor) return;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			ABwayPlayerState* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
			if (!PS || PC->IsLocalController() || !PC->GetPawn()) continue;
			ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent();
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				UE_LOG(LogTemp, Display, TEXT("BwayDeathTest: Granted ability %s active=%d"), *GetNameSafe(Spec.Ability), Spec.IsActive());
			}
			APawn* Pawn = PC->GetPawn();
			ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
			UE_LOG(LogTemp, Display, TEXT("BwayDeathTest: Native damage pawn=%s guard=%d health=%.1f"), *Pawn->GetName(), GAllowActorScriptExecutionInEditor, Health ? Health->GetHealth() : -1.0f);
			UBwayDamageLibrary::ApplyDamageFromSource(ASC, Pawn, 100000.0f, Pawn, Pawn);
			UE_LOG(LogTemp, Display, TEXT("BwayDeathTest: After damage health=%.1f state=%d"), Health ? Health->GetHealth() : -1.0f, Health ? static_cast<int32>(Health->GetDeathState()) : -1);
			return;
		}
	}), 0.2f, false);
}

FAutoConsoleCommandWithWorld DeathCommand(
	TEXT("bway.Test.DamageRemotePlayer"),
	TEXT("Development test: apply lethal damage to a remote human on a normal server tick."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&DamageRemotePlayer));
}
#endif
