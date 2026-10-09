// Synthetic input routing coverage; physical devices and rendered polish remain separate gates.
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "BwayPlayerController.h"
#include "BwayGameState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "UI/BwayUpgradeShopWidget.h"
#include "Components/ComboBoxString.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerInput.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Input/CommonUIActionRouterBase.h"
#include "InputKeyEventArgs.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "BwayPlayerState.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Components/Button.h"
#include "UObject/UnrealType.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "GameFramework/Pawn.h"

namespace BwayShopInputTest
{
struct FWalletObservation
{
	TWeakObjectPtr<ABwayPlayerState> Player;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	FDelegateHandle Handle;
	FName UpgradeId;
	int32 Cost = 0;
	int32 DebitCount = 0;
	bool bUnexpectedDebit = false;
	TWeakObjectPtr<APawn> BeforeDeathPawn;
	FString BeforeDeathPawnName;
	int32 BeforeDeaths = 0;
};

struct FProbe
{
	TArray<TWeakObjectPtr<ABwayPlayerController>> Controllers;
	int32 Index = 0;
	int32 Step = 0;
	double Started = FPlatformTime::Seconds();
	double Changed = 0;
	ECommonInputMode OriginalMode = ECommonInputMode::Game;
	bool bOriginalMoveIgnored = false;
	bool bOriginalLookIgnored = false;
	bool bRestartExercised = false;
	int32 InitialSelection = INDEX_NONE;
	TMap<TWeakObjectPtr<ABwayPlayerController>, ECommonInputType> OriginalInputTypes;
	bool bPurchase = false;
	bool bAuthoritySeeded = false;
	TArray<FWalletObservation> Wallets;
	FName PurchaseId;
	int32 PurchaseCost = 0;
	float BeforeGold = 0;
	float IncomePerSecond = 0;
	bool bRespawn = false;
	int32 AuthorityRespawnIndex = 0;
	double RespawnReadyAt = 0;
	double DamageAt = 0;
	double AuthorityRespawnStableAt = 0;
	TWeakObjectPtr<APawn> BeforeDeathPawn;
	uint32 BeforeDeathPawnId = 0;
	FString BeforeDeathPawnName;
	int32 BeforeDeaths = 0;
	TWeakObjectPtr<UBwayUpgradeShopWidget> RespawnShop;
	bool bObservedDeath = false;
	double LocalRespawnStableAt = 0;
};
TSharedPtr<FProbe> Active;

bool SeedPurchaseWallets(UWorld* World)
{
	if (Active->bAuthoritySeeded) return true;
	TArray<ABwayPlayerState*> Players;
	for (TActorIterator<ABwayPlayerController> It(World); It; ++It)
	{
		ABwayPlayerState* Player = It->GetPlayerState<ABwayPlayerState>();
		UBwayUpgradeComponent* Shop = Player ? Player->UpgradeComponent : nullptr;
		const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
		const ULyraAbilitySystemComponent* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
		const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
		if (!Catalog || Catalog->Upgrades.IsEmpty() || Catalog->Upgrades[0].Ranks.Num() < 2 || !Gold || !Shop->IsPurchaseWindowOpen()) return false;
		if (Shop->GetOwnedRank(Catalog->Upgrades[0].Id) != 0) return false;
		Players.Add(Player);
	}
	if (Players.Num() != 4) return false;
	for (ABwayPlayerState* Player : Players)
	{
		ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
		const FBwayUpgradeDefinition& Definition = Player->UpgradeComponent->GetCatalog()->Upgrades[0];
		const int32 Cost = Definition.Ranks[0].Cost;
		// Fixture funding only, on authority, in an owned disposable match. The
		// purchase itself must still debit through the production widget/RPC/GE.
		ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), float(Cost));
		FWalletObservation& Wallet = Active->Wallets.AddDefaulted_GetRef();
		Wallet.Player = Player;
		Wallet.ASC = ASC;
		Wallet.UpgradeId = Definition.Id;
		Wallet.Cost = Cost;
		Wallet.BeforeDeathPawn = Player->GetPawn();
		Wallet.BeforeDeathPawnName = GetNameSafe(Player->GetPawn());
		Wallet.BeforeDeaths = Player->GetDeaths();
		const TWeakPtr<FProbe> WeakProbe = Active;
		const TWeakObjectPtr<ABwayPlayerState> WeakPlayer = Player;
		Wallet.Handle = ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute()).AddLambda(
			[WeakProbe, WeakPlayer](const FOnAttributeChangeData& Change)
			{
				if (const TSharedPtr<FProbe> Probe = WeakProbe.Pin())
				{
					for (FWalletObservation& Observed : Probe->Wallets)
					{
						if (Observed.Player != WeakPlayer || Change.NewValue >= Change.OldValue) continue;
						const bool bExact = FMath::IsNearlyEqual(Change.OldValue - Change.NewValue, float(Observed.Cost), 0.01f);
						++Observed.DebitCount;
						Observed.bUnexpectedDebit |= !bExact;
						UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: AuthorityDebit player=%d id=%s before=%.2f after=%.2f cost=%d exact=%d count=%d"),
							WeakPlayer.IsValid() ? WeakPlayer->GetPlayerId() : -1, *Observed.UpgradeId.ToString(), Change.OldValue, Change.NewValue, Observed.Cost, bExact, Observed.DebitCount);
					}
				}
			});
		Player->ForceNetUpdate();
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: PurchaseSeed player=%d id=%s cost=%d"), Player->GetPlayerId(), *Definition.Id.ToString(), Cost);
	}
	Active->bAuthoritySeeded = true;
	return true;
}

UButton* BuyButton(UBwayUpgradeShopWidget* Widget)
{
	const FObjectProperty* Property = FindFProperty<FObjectProperty>(Widget->GetClass(), TEXT("BuyButton"));
	return Property ? Cast<UButton>(Property->GetObjectPropertyValue_InContainer(Widget)) : nullptr;
}

TArray<UBwayUpgradeShopWidget*> OpenShops(ABwayPlayerController* PC)
{
	TArray<UBwayUpgradeShopWidget*> Result;
	for (TObjectIterator<UBwayUpgradeShopWidget> It; It; ++It)
	{
		if (It->GetOwningPlayer() == PC && It->IsActivated()) Result.Add(*It);
	}
	return Result;
}

void Key(ABwayPlayerController* PC, FKey Key, EInputEvent Event)
{
	if (Event == IE_Pressed)
	{
		// Controller injection bypasses CommonInput's Slate preprocessor. Mirror
		// the input-method update a physical press performs before game dispatch.
		if (UCommonInputSubsystem* CommonInput = PC->GetLocalPlayer()->GetSubsystem<UCommonInputSubsystem>())
		{
			CommonInput->SetCurrentInputType(Key.IsGamepadKey() ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard);
		}
	}
	const FInputDeviceId Device = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetLocalPlayer()->GetPlatformUserId());
	PC->InputKey(FInputKeyEventArgs(nullptr, Device, Key, Event, FPlatformTime::Cycles64()));
}

void UIKey(ABwayPlayerController* PC, FKey Key)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	const int32 UserIndex = Slate.GetUserIndexForController(PC->GetLocalPlayer()->GetControllerId());
	const FKeyEvent Event(Key, FModifierKeysState(), UserIndex, false, 0, 0);
	const TSharedPtr<SWidget> Focused = Slate.GetUserFocusedWidget(UserIndex);
	const bool bDownHandled = Slate.ProcessKeyDownEvent(Event);
	const bool bUpHandled = Slate.ProcessKeyUpEvent(Event);
	UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: UIKey key=%s user=%d focus=%s down=%d up=%d"),
		*Key.ToString(), UserIndex, Focused ? *Focused->GetTypeAsString() : TEXT("None"), bDownHandled, bUpHandled);
}

bool Finish(const TCHAR* Failure)
{
	if (Active)
	{
		for (const FWalletObservation& Wallet : Active->Wallets)
		{
			if (ULyraAbilitySystemComponent* ASC = Wallet.ASC.Get()) ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute()).Remove(Wallet.Handle);
		}
		for (const auto& Weak : Active->Controllers)
		{
			if (ABwayPlayerController* PC = Weak.Get())
			{
				Key(PC, EKeys::B, IE_Released);
				Key(PC, EKeys::Gamepad_DPad_Up, IE_Released);
				for (UBwayUpgradeShopWidget* Shop : OpenShops(PC)) Shop->DeactivateWidget();
				if (const ECommonInputType* OriginalType = Active->OriginalInputTypes.Find(Weak))
				{
					if (UCommonInputSubsystem* CommonInput = PC->GetLocalPlayer()->GetSubsystem<UCommonInputSubsystem>()) CommonInput->SetCurrentInputType(*OriginalType);
				}
			}
		}
	}
	if (Failure)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayShopInputTest: FAIL step=%d peer=%d reason=%s"), Active ? Active->Step : -1, Active ? Active->Index : -1, Failure);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: PASS peers=%d scope=synthetic-key-open-held-toggle-commonui-back"), Active->Controllers.Num());
	}
	Active.Reset();
	return false;
}

const TCHAR* AdvanceAuthorityRespawns(FProbe& P, double Now)
{
	if (!P.bAuthoritySeeded || P.RespawnReadyAt == 0 || P.AuthorityRespawnIndex == P.Wallets.Num()) return nullptr;
	for (const FWalletObservation& Wallet : P.Wallets)
	{
		if (Wallet.bUnexpectedDebit || Wallet.DebitCount > 1) return TEXT("incorrect debit before respawn");
		if (Wallet.DebitCount != 1) { P.RespawnReadyAt = Now; return nullptr; }
	}
	// Let all remote purchase observers enter their open-shop death checkpoint.
	if (Now - P.RespawnReadyAt < 3.0) return nullptr;
	FWalletObservation& Victim = P.Wallets[P.AuthorityRespawnIndex];
	ABwayPlayerState* Player = Victim.Player.Get();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	if (!Player || !Victim.ASC.IsValid()) return TEXT("respawn authority player lost");
	if (P.DamageAt == 0)
	{
		if (!Pawn || Pawn != Victim.BeforeDeathPawn.Get() || !Health || Health->IsDeadOrDying()) return TEXT("unexpected pawn before native damage");
		P.DamageAt = Now;
		// Kill one player at a time through production damage/death/respawn, so
		// the disposable fixture never substitutes a team wipe or round reset.
		UBwayDamageLibrary::ApplyDamageFromSource(Victim.ASC.Get(), Pawn, 100000.0f, Pawn, Pawn);
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: RespawnDamage player=%d old=%s health=%.2f deathsBefore=%d"),
			Player->GetPlayerId(), *Victim.BeforeDeathPawnName, Health->GetHealth(), Victim.BeforeDeaths);
		if (Health->GetHealth() > 0) return TEXT("native damage did not kill the target");
		return nullptr;
	}
	if (Now - P.DamageAt > 40.0) return TEXT("authority replacement pawn timed out");
	const bool bReplaced = Pawn && Pawn != Victim.BeforeDeathPawn.Get() && Health && !Health->IsDeadOrDying()
		&& Health->GetHealth() > 0 && Player->GetDeaths() == Victim.BeforeDeaths + 1;
	if (!bReplaced) { P.AuthorityRespawnStableAt = 0; return nullptr; }
	if (P.AuthorityRespawnStableAt == 0) P.AuthorityRespawnStableAt = Now;
	if (Now - P.AuthorityRespawnStableAt < 2.0) return nullptr;
	UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: AuthorityRespawn PASS player=%d old=%s new=%s deathsBefore=%d deathsAfter=%d"),
		Player->GetPlayerId(), *Victim.BeforeDeathPawnName, *GetNameSafe(Pawn), Victim.BeforeDeaths, Player->GetDeaths());
	++P.AuthorityRespawnIndex;
	P.DamageAt = 0;
	P.AuthorityRespawnStableAt = 0;
	return nullptr;
}

bool Tick(float)
{
	if (!Active) return false;
	FProbe& P = *Active;
	const double Now = FPlatformTime::Seconds();
	if (Now - P.Started > (P.bRespawn ? 180.0 : 45.0)) return Finish(TEXT("timeout"));
	if (P.Controllers.IsEmpty())
	{
		// Startup ExecCmds precede the gameplay controller. Resolve it on a
		// normal tick only after Playing and the input mappings are ready.
		bool bPIE = false;
		TArray<TWeakObjectPtr<ABwayPlayerController>> Ready;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld()) continue;
			bPIE |= World->WorldType == EWorldType::PIE;
			if (P.bRespawn && bPIE) return Finish(TEXT("respawn shop gate requires separate packaged processes"));
			const ABwayGameState* GS = World->GetGameState<ABwayGameState>();
			if (!GS || !GS->GetRoundManagement() || GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) continue;
			if (P.bPurchase && World->GetNetMode() != NM_Client && !SeedPurchaseWallets(World)) continue;
			for (TActorIterator<ABwayPlayerController> It(World); It; ++It)
			{
				if (!It->IsLocalController() || !It->GetLocalPlayer() || !It->GetUpgradeShopInputAction()) continue;
				if (P.bPurchase)
				{
					const ABwayPlayerState* Player = It->GetPlayerState<ABwayPlayerState>();
					const UBwayUpgradeComponent* Shop = Player ? Player->UpgradeComponent : nullptr;
					const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
					const ULyraAbilitySystemComponent* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
					const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
					if (!Catalog || Catalog->Upgrades.IsEmpty() || Catalog->Upgrades[0].Ranks.Num() < 2 || !Gold || !Shop->IsPurchaseWindowOpen()) continue;
					if (Gold->GetCurrentGold() < Catalog->Upgrades[0].Ranks[0].Cost || Shop->GetOwnedRank(Catalog->Upgrades[0].Id) != 0) continue;
				}
				const auto* Input = It->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
				if (Input && Input->QueryKeysMappedToAction(It->GetUpgradeShopInputAction()).Contains(EKeys::B)) Ready.Add(*It);
			}
		}
		if (Ready.Num() < (bPIE ? 4 : 1)) return true;
		P.Controllers = MoveTemp(Ready);
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Ready peers=%d pie=%d"), P.Controllers.Num(), bPIE);
	}
	ABwayPlayerController* PC = P.Controllers.IsValidIndex(P.Index) ? P.Controllers[P.Index].Get() : nullptr;
	if (!PC || !PC->GetWorld() || !PC->GetLocalPlayer()) return Finish(TEXT("controller lost"));
	UCommonUIActionRouterBase* Router = PC->GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>();
	UEnhancedInputLocalPlayerSubsystem* Input = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Router || !Input || !PC->GetUpgradeShopInputAction()) return Finish(TEXT("input setup missing"));
	if (P.bRespawn)
	{
		if (const TCHAR* Failure = AdvanceAuthorityRespawns(P, Now)) return Finish(Failure);
	}
	const auto Shops = OpenShops(PC);
	if (P.Step == 0)
	{
		const auto Keys = Input->QueryKeysMappedToAction(PC->GetUpgradeShopInputAction());
		const UEnhancedInputComponent* Component = Cast<UEnhancedInputComponent>(PC->InputComponent);
		int32 Bindings = 0;
		if (Component) for (const auto& Binding : Component->GetActionEventBindings())
		{
			if (Binding->GetAction() == PC->GetUpgradeShopInputAction()) ++Bindings;
		}
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Setup peer=%d bindings=%d keyboard=%d gamepad=%d shops=%d"),
			P.Index, Bindings, Keys.Contains(EKeys::B), Keys.Contains(EKeys::Gamepad_DPad_Up), Shops.Num());
		if (!Keys.Contains(EKeys::B) || !Keys.Contains(EKeys::Gamepad_DPad_Up) || Bindings != 1 || !Shops.IsEmpty()) return Finish(TEXT("mapping/binding/initial modal mismatch"));
		P.OriginalMode = Router->GetActiveInputMode();
		P.bOriginalMoveIgnored = PC->IsMoveInputIgnored();
		P.bOriginalLookIgnored = PC->IsLookInputIgnored();
		if (const UCommonInputSubsystem* CommonInput = PC->GetLocalPlayer()->GetSubsystem<UCommonInputSubsystem>()) P.OriginalInputTypes.Add(PC, CommonInput->GetCurrentInputType());
		Key(PC, EKeys::B, IE_Pressed);
		P.Step = 1;
		P.Changed = Now;
		return true;
	}
	if (Now - P.Changed < 0.65) return true;
	if (P.Step == 1 || P.Step == 3)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Observe peer=%d step=%d shops=%d mode=%d keyDown=%d actionValue=%s world=%s"),
			P.Index, P.Step, Shops.Num(), int32(Router->GetActiveInputMode()), PC->IsInputKeyDown(P.Step == 1 ? EKeys::B : EKeys::Gamepad_DPad_Up),
			*Input->GetPlayerInput()->GetActionValue(PC->GetUpgradeShopInputAction()).ToString(), *PC->GetWorld()->GetPathName());
		if (Shops.Num() != 1 || Router->GetActiveInputMode() != ECommonInputMode::Menu) return Finish(TEXT("key did not open exactly one modal"));
		UWidget* Focus = Shops[0]->GetDesiredFocusTarget();
		const bool bSelectorFocus = Cast<UComboBoxString>(Focus) && Focus->HasUserFocus(PC);
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Open peer=%d netmode=%d key=%s count=%d menu=1 selectorFocus=%d moveIgnored=%d lookIgnored=%d"),
			P.Index, int32(PC->GetNetMode()), P.Step == 1 ? TEXT("B") : TEXT("DPadUp"), Shops.Num(), bSelectorFocus, PC->IsMoveInputIgnored(), PC->IsLookInputIgnored());
		if (!bSelectorFocus || !PC->IsMoveInputIgnored() || !PC->IsLookInputIgnored()) return Finish(TEXT("focus or gameplay input blocking mismatch"));
		if (P.Step == 1 && !P.bRestartExercised)
		{
			if (!PC->GetPawn()) return Finish(TEXT("restart probe has no possessed pawn"));
			P.bRestartExercised = true;
			// Exercise the real client restart locally with the existing pawn.
			// Repeat the reset to catch unbalanced UI input-lock counts on close.
			PC->ClientRestart_Implementation(PC->GetPawn());
			PC->ResetIgnoreInputFlags();
			PC->RestoreGameplayInputMode();
			UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Restart peer=%d activeShop=1 resets=2"), P.Index);
			P.Changed = Now;
			return true;
		}
		const FKey Pressed = P.Step == 1 ? EKeys::B : EKeys::Gamepad_DPad_Up;
		Key(PC, Pressed, IE_Released);
		if (P.Step == 1)
		{
			const auto Slate = Shops[0]->GetCachedWidget();
			if (!Slate.IsValid() || !Slate->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::B, FModifierKeysState(), 0, false, 0, 0)).IsEventHandled()) return Finish(TEXT("mapped toggle not handled"));
		}
		else
		{
			UComboBoxString* Selector = CastChecked<UComboBoxString>(Focus);
			P.InitialSelection = Selector->GetSelectedIndex();
			if (Selector->GetOptionCount() < 2 || P.InitialSelection != 0) return Finish(TEXT("navigation probe requires initial first option and at least two choices"));
			UIKey(PC, EKeys::Gamepad_FaceButton_Bottom);
			P.Step = 5;
			P.Changed = Now;
			return true;
		}
		++P.Step;
		P.Changed = Now;
		return true;
	}
	if (P.Step >= 5 && P.Step <= 10)
	{
		if (Shops.Num() != 1 || Router->GetActiveInputMode() != ECommonInputMode::Menu) return Finish(TEXT("navigation closed or lost the shop"));
		UComboBoxString* Selector = Cast<UComboBoxString>(Shops[0]->GetDesiredFocusTarget());
		if (!Selector) return Finish(TEXT("navigation selector missing"));
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Navigation step=%d open=%d selection=%d selectorFocus=%d"),
			P.Step, Selector->IsOpen(), Selector->GetSelectedIndex(), Selector->HasUserFocus(PC));
		if (P.Step == 5 || P.Step == 8)
		{
			if (!Selector->IsOpen()) return Finish(TEXT("gamepad Accept did not open the selector"));
			UIKey(PC, P.Step == 5 ? EKeys::Gamepad_DPad_Down : EKeys::Gamepad_DPad_Up);
		}
		else if (P.Step == 6 || P.Step == 9)
		{
			if (!Selector->IsOpen()) return Finish(TEXT("direction dismissed the selector"));
			UIKey(PC, EKeys::Gamepad_FaceButton_Bottom);
		}
		else
		{
			const int32 Expected = P.Step == 7 ? P.InitialSelection + 1 : P.InitialSelection;
			if (Selector->IsOpen() || Selector->GetSelectedIndex() != Expected || !Selector->HasUserFocus(PC)) return Finish(TEXT("gamepad selection or focus restoration mismatch"));
			if (P.Step == 7) UIKey(PC, EKeys::Gamepad_FaceButton_Bottom);
			else
			{
				UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Navigation PASS peer=%d down=1 up=1 accept=1 selectorFocus=1"), P.Index);
				if (P.bPurchase)
				{
					UButton* Buy = BuyButton(Shops[0]);
					if (!Buy || !Buy->GetIsEnabled()) return Finish(TEXT("purchase navigation has no enabled Buy button"));
					UIKey(PC, EKeys::Gamepad_DPad_Down);
					P.Step = 11;
					P.Changed = Now;
					return true;
				}
				Router->ProcessInput(EKeys::Escape, IE_Pressed);
				Router->ProcessInput(EKeys::Escape, IE_Released);
				P.Step = 4;
				P.Changed = Now;
				return true;
			}
		}
		++P.Step;
		P.Changed = Now;
		return true;
	}
	if (P.Step == 11 || P.Step == 12)
	{
		if (Shops.Num() != 1 || Router->GetActiveInputMode() != ECommonInputMode::Menu) return Finish(TEXT("purchase lost the active shop"));
		UButton* Buy = BuyButton(Shops[0]);
		UWidget* Selector = Shops[0]->GetDesiredFocusTarget();
		ABwayPlayerState* Player = PC->GetPlayerState<ABwayPlayerState>();
		UBwayUpgradeComponent* Shop = Player ? Player->UpgradeComponent : nullptr;
		ULyraAbilitySystemComponent* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
		const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
		if (!Buy || !Selector || !Shop || !Gold || !Shop->GetCatalog()) return Finish(TEXT("purchase state missing"));
		if (P.Step == 11)
		{
			if (!Buy->HasUserFocus(PC) || !Buy->GetIsEnabled()) return Finish(TEXT("Down did not focus enabled Buy"));
			const FBwayUpgradeDefinition& Definition = Shop->GetCatalog()->Upgrades[0];
			P.PurchaseId = Definition.Id;
			P.PurchaseCost = Definition.Ranks[0].Cost;
			P.BeforeGold = Gold->GetCurrentGold();
			P.IncomePerSecond = Gold->GetGoldPerSecond();
			if (Shop->GetOwnedRank(P.PurchaseId) != 0) return Finish(TEXT("purchase fixture expected unowned first rank"));
			UIKey(PC, EKeys::Gamepad_FaceButton_Bottom);
			const int32 ImmediateRank = Shop->GetOwnedRank(P.PurchaseId);
			const bool bPendingObserved = PC->GetNetMode() == NM_Client && ImmediateRank == 0;
			UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: PurchaseSubmitted peer=%d netmode=%d id=%s before=%.2f cost=%d income=%.2f disabled=%d selectorFocus=%d pending=%d rank=%d"),
				P.Index, int32(PC->GetNetMode()), *P.PurchaseId.ToString(), P.BeforeGold, P.PurchaseCost, P.IncomePerSecond, !Buy->GetIsEnabled(), Selector->HasUserFocus(PC), bPendingObserved, ImmediateRank);
			if (Buy->GetIsEnabled() || !Selector->HasUserFocus(PC) || (PC->GetNetMode() == NM_Client && !bPendingObserved)) return Finish(TEXT("purchase did not preserve focus while pending or disabled"));
			P.Step = 12;
			P.Changed = Now;
			return true;
		}
		const int32 Rank = Shop->GetOwnedRank(P.PurchaseId);
		const float Balance = Gold->GetCurrentGold();
		const float Expected = P.BeforeGold - P.PurchaseCost;
		// One-second passive GE ticks plus two ticks for the replicated baseline
		// boundary. Authority separately requires one exact catalog-price debit.
		const float Tolerance = FMath::Max(0.0f, P.IncomePerSecond) * (FMath::CeilToInt(Now - P.Changed) + 2) + 0.01f;
		const bool bWallet = FMath::IsFinite(Balance) && Balance < P.BeforeGold && Balance >= Expected - 0.01f && Balance <= Expected + Tolerance;
		if ((Rank != 1 || !bWallet) && Now - P.Changed < 8.0) return true;
		if (Rank != 1 || !bWallet || Buy->GetIsEnabled() || !Selector->HasUserFocus(PC)) return Finish(TEXT("replicated purchase or disabled-button focus mismatch"));
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Purchase PASS peer=%d netmode=%d id=%s rank=%d before=%.2f after=%.2f cost=%d tolerance=%.2f walletDeltaOk=1 disabled=1 selectorFocus=1"),
			P.Index, int32(PC->GetNetMode()), *P.PurchaseId.ToString(), Rank, P.BeforeGold, Balance, P.PurchaseCost, Tolerance);
		if (P.bRespawn)
		{
			APawn* Pawn = PC->GetPawn();
			if (!Pawn) return Finish(TEXT("no pawn for respawn baseline"));
			P.BeforeDeathPawn = Pawn;
			P.BeforeDeathPawnId = Pawn->GetUniqueID();
			P.BeforeDeathPawnName = Pawn->GetName();
			P.BeforeDeaths = Player->GetDeaths();
			P.RespawnShop = Shops[0];
			P.RespawnReadyAt = Now;
			P.Step = 13;
			P.Changed = Now;
			UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: RespawnBaseline peer=%d netmode=%d old=%s deaths=%d shopOpen=1"),
				P.Index, int32(PC->GetNetMode()), *P.BeforeDeathPawnName, P.BeforeDeaths);
			return true;
		}
		UIKey(PC, EKeys::Gamepad_FaceButton_Right);
		P.Step = 4;
		P.Changed = Now;
		return true;
	}
	if (P.Step >= 13 && P.Step <= 16)
	{
		if (Shops.Num() != 1 || Shops[0] != P.RespawnShop.Get() || Router->GetActiveInputMode() != ECommonInputMode::Menu
			|| !PC->IsMoveInputIgnored() || !PC->IsLookInputIgnored()) return Finish(TEXT("respawn lost modal or input locks"));
		UComboBoxString* Selector = Cast<UComboBoxString>(Shops[0]->GetDesiredFocusTarget());
		ABwayPlayerState* Player = PC->GetPlayerState<ABwayPlayerState>();
		if (!Selector || !Player || !Player->UpgradeComponent || Player->UpgradeComponent->GetOwnedRank(P.PurchaseId) != 1) return Finish(TEXT("respawn lost selector or purchased rank"));
		if (P.Step == 13)
		{
			if (!Selector->HasUserFocus(PC)) return Finish(TEXT("death/respawn lost selector focus"));
			const ULyraHealthComponent* OldHealth = ULyraHealthComponent::FindHealthComponent(P.BeforeDeathPawn.Get());
			if (!P.bObservedDeath && OldHealth && OldHealth->IsDeadOrDying() && OldHealth->GetHealth() <= 0)
			{
				P.bObservedDeath = true;
				UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: DeathObserved peer=%d netmode=%d old=%s shopOpen=1 selectorFocus=1 moveIgnored=1 lookIgnored=1"),
					P.Index, int32(PC->GetNetMode()), *P.BeforeDeathPawnName);
			}
			APawn* Pawn = PC->GetPawn();
			const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
			const ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
			const bool bReady = P.bObservedDeath && Pawn && Pawn->GetUniqueID() != P.BeforeDeathPawnId && Health
				&& !Health->IsDeadOrDying() && Health->GetHealth() > 0 && ASC && ASC->GetAvatarActor() == Pawn
				&& Player->GetDeaths() == P.BeforeDeaths + 1;
			if (!bReady) { P.LocalRespawnStableAt = 0; return true; }
			if (P.LocalRespawnStableAt == 0) P.LocalRespawnStableAt = Now;
			if (Now - P.LocalRespawnStableAt < 1.0) return true;
			const UEnhancedInputComponent* Component = Cast<UEnhancedInputComponent>(PC->InputComponent);
			int32 Bindings = 0;
			if (Component) for (const auto& Binding : Component->GetActionEventBindings()) if (Binding->GetAction() == PC->GetUpgradeShopInputAction()) ++Bindings;
			const auto Keys = Input->QueryKeysMappedToAction(PC->GetUpgradeShopInputAction());
			if (Bindings != 1 || !Keys.Contains(EKeys::B) || !Keys.Contains(EKeys::Gamepad_DPad_Up)) return Finish(TEXT("replacement pawn lost or duplicated shop bindings"));
			UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Respawn PASS peer=%d netmode=%d old=%s new=%s deathsBefore=%d deathsAfter=%d sameShop=1 selectorFocus=1 moveIgnored=1 lookIgnored=1 bindings=1 rank=1"),
				P.Index, int32(PC->GetNetMode()), *P.BeforeDeathPawnName, *Pawn->GetName(), P.BeforeDeaths, Player->GetDeaths());
			UIKey(PC, EKeys::Gamepad_FaceButton_Bottom);
		}
		else if (P.Step == 14)
		{
			if (!Selector->IsOpen()) return Finish(TEXT("replacement pawn gamepad Accept did not open selector"));
			UIKey(PC, EKeys::Gamepad_DPad_Down);
		}
		else if (P.Step == 15)
		{
			if (!Selector->IsOpen()) return Finish(TEXT("replacement pawn direction closed selector"));
			UIKey(PC, EKeys::Gamepad_FaceButton_Bottom);
		}
		else
		{
			if (Selector->IsOpen() || Selector->GetSelectedIndex() != P.InitialSelection + 1 || !Selector->HasUserFocus(PC)) return Finish(TEXT("replacement pawn selector navigation failed"));
			UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: RespawnNavigation PASS peer=%d netmode=%d accept=1 down=1 selection=1 selectorFocus=1"), P.Index, int32(PC->GetNetMode()));
			UIKey(PC, EKeys::Gamepad_FaceButton_Right);
			P.Step = 4;
			P.Changed = Now;
			return true;
		}
		++P.Step;
		P.Changed = Now;
		return true;
	}
	if (!Shops.IsEmpty() || Router->GetActiveInputMode() != P.OriginalMode || PC->IsMoveInputIgnored() != P.bOriginalMoveIgnored || PC->IsLookInputIgnored() != P.bOriginalLookIgnored) return Finish(TEXT("close did not restore prior gameplay input"));
	if (P.Step == 2)
	{
		Key(PC, EKeys::Gamepad_DPad_Up, IE_Pressed);
		P.Step = 3;
		P.Changed = Now;
		return true;
	}
	if (P.bAuthoritySeeded)
	{
		if (P.bRespawn && P.AuthorityRespawnIndex != P.Wallets.Num()) return true;
		for (const FWalletObservation& Wallet : P.Wallets)
		{
			if (Wallet.bUnexpectedDebit || Wallet.DebitCount > 1) return Finish(TEXT("authority observed an incorrect or duplicate debit"));
			if (Wallet.DebitCount != 1 || !Wallet.Player.IsValid() || Wallet.Player->UpgradeComponent->GetOwnedRank(Wallet.UpgradeId) != 1) return true;
		}
		UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: AuthorityPurchase PASS peers=%d exactDebits=%d"), P.Wallets.Num(), P.Wallets.Num());
	}
	UE_LOG(LogTemp, Display, TEXT("BwayShopInputTest: Peer PASS index=%d netmode=%d restored=1"), P.Index, int32(PC->GetNetMode()));
	if (++P.Index == P.Controllers.Num()) return Finish(nullptr);
	P.Step = 0;
	P.bRestartExercised = false;
	return true;
}

void Start()
{
	if (Active) return;
	Active = MakeShared<FProbe>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.1f);
}

void StartPurchase()
{
	if (Active) return;
	Start();
	Active->bPurchase = true;
}

void StartRespawn()
{
	if (Active) return;
	StartPurchase();
	Active->bRespawn = true;
}

FAutoConsoleCommand RespawnCommand(TEXT("bway.Test.ShopRespawnFocus"), TEXT("Development-only: four packaged players buy through Slate, retain shop focus through sequential production deaths/replacement pawns, then navigate and close."), FConsoleCommandDelegate::CreateStatic(&StartRespawn));

FAutoConsoleCommand Command(TEXT("bway.Test.ShopInputRoutes"), TEXT("Synthetic keyboard/gamepad shop input across local gameplay worlds."), FConsoleCommandDelegate::CreateStatic(&Start));
FAutoConsoleCommand PurchaseCommand(TEXT("bway.Test.ShopPurchaseFocus"), TEXT("Four-player disposable-match fixture: fund catalog-priced purchases on authority, navigate/activate Buy through Slate, verify pending focus and exact authoritative debits."), FConsoleCommandDelegate::CreateStatic(&StartPurchase));
}
#endif
