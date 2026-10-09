// Exercise the widget's RPC path outside the editor Python execution guard.
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "CoreGlobals.h"
#include "Containers/Ticker.h"
#include "UI/BwayUpgradeShopWidget.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "GameFramework/Pawn.h"

namespace BwayShopTest
{
bool GRemotePurchasePollActive = false;

UWorld* FindClientGameplayWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* Candidate = Context.World();
		if (!Candidate || !Candidate->IsGameWorld() || Candidate->GetNetMode() != NM_Client)
		{
			continue;
		}

		// Startup ExecCmds are dispatched in the frontend world. Resolve the
		// destination world only after travel has produced a gameplay controller
		// and upgrade component, so the timer is owned by the lasting world.
		for (TActorIterator<ABwayPlayerController> It(Candidate); It; ++It)
		{
			ABwayPlayerController* Controller = *It;
			ABwayPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ABwayPlayerState>() : nullptr;
			if (Controller && Controller->IsLocalController() && PlayerState && PlayerState->UpgradeComponent)
			{
				return Candidate;
			}
		}
	}

	return nullptr;
}

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

void OpenAndBuy(UWorld* World)
{
	if (!World || !World->IsGameWorld()) return;
	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		if (GAllowActorScriptExecutionInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayShopTest: Refusing packaged shop probe under editor script guard"));
			return;
		}

		ABwayPlayerController* LocalController = nullptr;
		for (TActorIterator<ABwayPlayerController> It(World); It; ++It)
		{
			if (It->IsLocalController())
			{
				LocalController = *It;
				break;
			}
		}
		if (!LocalController)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayShopTest: Delayed shop probe found no local controller"));
			return;
		}

		if (ABwayPlayerState* PlayerState = LocalController->GetPlayerState<ABwayPlayerState>())
		{
			if (ULyraAbilitySystemComponent* ASC = PlayerState->GetLyraAbilitySystemComponent())
			{
				if (const UBwayGoldAttributeSet* Gold = ASC->GetSet<UBwayGoldAttributeSet>())
				{
					ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 500.0f);
					UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Seeded packaged shop probe gold=%.1f"), Gold->GetCurrentGold());
				}
			}
		}

		LocalController->ToggleUpgradeShop();
		UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Delayed shop open requested world=%s"), *World->GetPathName());
		FTimerHandle PurchaseTimer;
		World->GetTimerManager().SetTimer(PurchaseTimer, FTimerDelegate::CreateWeakLambda(World, [World]()
		{
			for (TObjectIterator<UBwayUpgradeShopWidget> It; It; ++It)
			{
				UBwayUpgradeShopWidget* Widget = *It;
				APlayerController* PC = Widget->GetOwningPlayer();
				if (Widget->GetWorld() != World || !Widget->IsActivated() || !PC || !PC->IsLocalController()) continue;
				Widget->ProcessEvent(Widget->FindFunctionChecked(TEXT("BuySelected")), nullptr);
				UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Delayed widget purchase invoked"));

				FTimerHandle VerifyTimer;
				World->GetTimerManager().SetTimer(VerifyTimer, FTimerDelegate::CreateWeakLambda(World, [World, Widget]()
				{
					APlayerController* OwningPC = Widget ? Widget->GetOwningPlayer() : nullptr;
					ABwayPlayerState* State = OwningPC ? OwningPC->GetPlayerState<ABwayPlayerState>() : nullptr;
					UBwayUpgradeComponent* Shop = State ? State->UpgradeComponent : nullptr;
					ULyraAbilitySystemComponent* ASC = State ? State->GetLyraAbilitySystemComponent() : nullptr;
					const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
					const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
					const FName TargetId = Catalog && Catalog->Upgrades.Num() > 0 ? Catalog->Upgrades[0].Id : NAME_None;
					const int32 Rank = Shop && !TargetId.IsNone() ? Shop->GetOwnedRank(TargetId) : -1;
					const float Balance = Gold ? Gold->GetCurrentGold() : -1.0f;
					FString OwnedIds;
					if (Shop)
					{
						for (const FBwayOwnedUpgrade& Owned : Shop->GetOwnedUpgrades())
						{
							if (!OwnedIds.IsEmpty()) OwnedIds += TEXT(",");
							OwnedIds += FString::Printf(TEXT("%s:%d"), *Owned.Id.ToString(), Owned.Rank);
						}
					}
					UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Packaged purchase state id=%s rank=%d gold=%.1f owned=%s active=%d"),
						*TargetId.ToString(), Rank, Balance, *OwnedIds, Widget && Widget->IsActivated() ? 1 : 0);
					if (Widget && Widget->IsActivated()) Widget->DeactivateWidget();
				}), 0.5f, false);
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("BwayShopTest: Delayed shop probe found no active local shop"));
		}), 0.75f, false);
	}), 2.0f, false);
}

void SeedRemoteGold(UWorld* World)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client) return;
	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		if (GAllowActorScriptExecutionInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayShopTest: Refusing remote seed under editor script guard"));
			return;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || PC->IsLocalController()) continue;
			ABwayPlayerState* PlayerState = PC->GetPlayerState<ABwayPlayerState>();
			ULyraAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetLyraAbilitySystemComponent() : nullptr;
			if (const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr)
			{
				ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 500.0f);
				UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Seeded remote player gold player=%s gold=%.1f"),
					*GetNameSafe(PlayerState), Gold->GetCurrentGold());
				return;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("BwayShopTest: No remote player available for shop seed"));
	}), 5.0f, false);
}

void RunOpenAndBuyRemote(UWorld* World)
{
	if (!World || !World->IsGameWorld()) return;
	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		if (GAllowActorScriptExecutionInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayShopTest: Refusing remote purchase under editor script guard"));
			return;
		}
		ABwayPlayerController* LocalController = nullptr;
		for (TActorIterator<ABwayPlayerController> It(World); It; ++It)
		{
			if (It->IsLocalController())
			{
				LocalController = *It;
				break;
			}
		}
		if (!LocalController)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayShopTest: No local controller for remote purchase"));
			return;
		}

		LocalController->ToggleUpgradeShop();
		UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Remote delayed shop open requested world=%s netmode=%d"),
			*World->GetPathName(), static_cast<int32>(World->GetNetMode()));
		// CommonUI activation and replicated round phase changes can take a few
		// frames. Poll one timer-owned widget until it is actionable instead of
		// treating a single early frame as a purchase failure.
		const TSharedRef<FTimerHandle> PurchaseTimer = MakeShared<FTimerHandle>();
		const TSharedRef<float> Waited = MakeShared<float>(0.0f);
		World->GetTimerManager().SetTimer(*PurchaseTimer, FTimerDelegate::CreateWeakLambda(World, [World, PurchaseTimer, Waited]()
		{
			*Waited += 0.25f;
			for (TObjectIterator<UBwayUpgradeShopWidget> It; It; ++It)
			{
				UBwayUpgradeShopWidget* Widget = *It;
				APlayerController* PC = Widget->GetOwningPlayer();
				if (Widget->GetWorld() != World || !Widget->IsActivated() || !PC || !PC->IsLocalController()) continue;
				ABwayPlayerState* State = PC->GetPlayerState<ABwayPlayerState>();
				UBwayUpgradeComponent* Shop = State ? State->UpgradeComponent : nullptr;
				const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
				const FName TargetId = Catalog && Catalog->Upgrades.Num() > 0 ? Catalog->Upgrades[0].Id : NAME_None;
				const FBwayUpgradeDefinition* Definition = Catalog && !TargetId.IsNone() ? Catalog->FindUpgrade(TargetId) : nullptr;
				const int32 ExpectedCost = Definition && Definition->Ranks.Num() > 0 ? Definition->Ranks[0].Cost : -1;
				ULyraAbilitySystemComponent* ASC = State ? State->GetLyraAbilitySystemComponent() : nullptr;
				const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
				if (!Shop || !Gold || ExpectedCost < 0 || !Shop->IsPurchaseWindowOpen() || Gold->GetCurrentGold() < ExpectedCost) continue;

				const float BeforeBalance = Gold->GetCurrentGold();
				TWeakObjectPtr<UBwayUpgradeShopWidget> WeakWidget(Widget);
				World->GetTimerManager().ClearTimer(*PurchaseTimer);
				Widget->ProcessEvent(Widget->FindFunctionChecked(TEXT("BuySelected")), nullptr);
				UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Remote widget purchase invoked id=%s beforeGold=%.1f cost=%d"),
					*TargetId.ToString(), BeforeBalance, ExpectedCost);

				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWidget, TargetId, BeforeBalance, ExpectedCost](float)
				{
					UBwayUpgradeShopWidget* VerifiedWidget = WeakWidget.Get();
					if (!VerifiedWidget)
					{
						UE_LOG(LogTemp, Error, TEXT("BwayShopTest: Remote purchase verification lost the shop widget"));
						return false;
					}
					APlayerController* OwningPC = VerifiedWidget ? VerifiedWidget->GetOwningPlayer() : nullptr;
					ABwayPlayerState* State = OwningPC ? OwningPC->GetPlayerState<ABwayPlayerState>() : nullptr;
					UBwayUpgradeComponent* Shop = State ? State->UpgradeComponent : nullptr;
					ULyraAbilitySystemComponent* ASC = State ? State->GetLyraAbilitySystemComponent() : nullptr;
					const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
					const int32 Rank = Shop && !TargetId.IsNone() ? Shop->GetOwnedRank(TargetId) : -1;
					const float Balance = Gold ? Gold->GetCurrentGold() : -1.0f;
					const float ExpectedAfterPurchase = BeforeBalance - static_cast<float>(ExpectedCost);
					const bool bWalletDeltaMatches = FMath::IsFinite(BeforeBalance) && FMath::IsFinite(Balance) &&
						Balance < BeforeBalance && FMath::IsNearlyEqual(Balance, ExpectedAfterPurchase, 5.0f);
					UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Remote purchase state id=%s rank=%d beforeGold=%.1f gold=%.1f cost=%d expectedGold=%.1f walletDeltaOk=%d active=%d"),
						*TargetId.ToString(), Rank, BeforeBalance, Balance, ExpectedCost, ExpectedAfterPurchase, bWalletDeltaMatches ? 1 : 0, VerifiedWidget && VerifiedWidget->IsActivated() ? 1 : 0);
					if (Rank >= 1 && bWalletDeltaMatches)
					{
						UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Remote purchase PASS id=%s rank=%d gold=%.1f beforeGold=%.1f cost=%d"),
							*TargetId.ToString(), Rank, Balance, BeforeBalance, ExpectedCost);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("BwayShopTest: Remote purchase FAIL id=%s rank=%d gold=%.1f"),
							*TargetId.ToString(), Rank, Balance);
					}
					if (VerifiedWidget && VerifiedWidget->IsActivated()) VerifiedWidget->DeactivateWidget();
					return false;
				}), 1.5f);
				return;
			}
			if (*Waited >= 8.0f)
			{
				World->GetTimerManager().ClearTimer(*PurchaseTimer);
				UE_LOG(LogTemp, Error, TEXT("BwayShopTest: Remote purchase found no actionable local shop after %.1fs"), *Waited);
			}
		}), 0.25f, true);
	}), 0.5f, false);
}

bool PollRemotePurchaseWorld(float DeltaTime)
{
	static float Elapsed = 0.0f;
	if (!GRemotePurchasePollActive)
	{
		Elapsed = 0.0f;
		return false;
	}

	Elapsed += DeltaTime;
	if (UWorld* World = FindClientGameplayWorld())
	{
		GRemotePurchasePollActive = false;
		Elapsed = 0.0f;
		RunOpenAndBuyRemote(World);
		return false;
	}

	if (Elapsed >= 60.0f)
	{
		GRemotePurchasePollActive = false;
		Elapsed = 0.0f;
		UE_LOG(LogTemp, Error, TEXT("BwayShopTest: Remote purchase timed out waiting for the post-travel gameplay world"));
		return false;
	}

	return true;
}

void OpenAndBuyRemote()
{
	if (GRemotePurchasePollActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayShopTest: Remote purchase probe already waiting for a gameplay world"));
		return;
	}

	GRemotePurchasePollActive = true;
	UE_LOG(LogTemp, Display, TEXT("BwayShopTest: Remote purchase probe waiting for post-travel gameplay world"));
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&PollRemotePurchaseWorld), 0.25f);
}

FAutoConsoleCommandWithWorld OpenAndBuyCommand(
	TEXT("bway.Test.ShopOpenAndBuy"),
	TEXT("Development test: open the local upgrade shop after startup and invoke one purchase."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&OpenAndBuy));

FAutoConsoleCommandWithWorld SeedRemoteGoldCommand(
	TEXT("bway.Test.ShopSeedRemote"),
	TEXT("Development test: seed one remote owner's authority wallet for a purchase probe."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&SeedRemoteGold));

FAutoConsoleCommand OpenAndBuyRemoteCommand(
	TEXT("bway.Test.ShopOpenAndBuyRemote"),
	TEXT("Development test: wait for post-travel gameplay, invoke one local widget purchase, and verify owner replication from authority."),
	FConsoleCommandDelegate::CreateStatic(&OpenAndBuyRemote));

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
