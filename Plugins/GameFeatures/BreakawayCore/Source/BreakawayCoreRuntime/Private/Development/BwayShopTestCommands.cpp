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
}
#endif
