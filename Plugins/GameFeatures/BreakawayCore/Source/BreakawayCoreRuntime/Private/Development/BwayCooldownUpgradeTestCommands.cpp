#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/BwayGameplayAbility_ForGlory.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "UI/BwayMatchAbilitySlotWidget.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace BwayCooldownUpgradeTest
{
const FName UpgradeId(TEXT("CooldownReduction"));
struct FAuthorityCycle
{
	bool bPresent = false;
	int32 Cycles = 0;
	bool bExpired = false;
	bool bExpiryValid = true;
	float Start = 0;
	float Expected = 0;
	FActiveGameplayEffectHandle Handle;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	FDelegateHandle RemovalObserver;
};
struct FProbe
{
	TWeakObjectPtr<ABwayPlayerController> Controller;
	TWeakObjectPtr<UBwayMatchAbilitySlotWidget> Slot;
	FGameplayAbilitySpecHandle Ability;
	TMap<int32, FAuthorityCycle> Authority;
	double Started = FPlatformTime::Seconds();
	double Changed = 0;
	float ActivatedAt = 0;
	float FirstSampleAt = 0;
	float FirstRemaining = 0;
	float BeforeGold = 0;
	float Income = 0;
	float Bonus = 0;
	float BaseDuration = 0;
	int32 Cost = 0;
	int32 Step = 0;
	bool bFunded = false;
	bool bHudWaitReported = false;
};
TSharedPtr<FProbe> Active;

void Removed(const FActiveGameplayEffect& Effect, int32 PlayerId)
{
	if (!Active) return;
	FAuthorityCycle* Cycle = Active->Authority.Find(PlayerId);
	if (!Cycle || Effect.Handle != Cycle->Handle || !Cycle->ASC.IsValid()) return;
	Cycle->bPresent = false;
	if (Cycle->Cycles != 1) return;
	const float Elapsed = Cycle->ASC->GetWorld()->GetTimeSeconds() - Cycle->Start;
	Cycle->bExpired = true;
	Cycle->bExpiryValid = FMath::Abs(Elapsed - Cycle->Expected) <= 1.f;
	UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: Authority EXPIRE player=%d elapsed=%.4f expected=%.4f"), PlayerId, Elapsed, Cycle->Expected);
}

FGameplayAbilitySpec* FindAbility(ULyraAbilitySystemComponent* ASC)
{
	if (!ASC) return nullptr;
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_ForGlory>()) return &Spec;
	}
	return nullptr;
}

float KitDuration(FGameplayAbilitySpec& Spec, ULyraAbilitySystemComponent* ASC)
{
	const UGameplayAbility* Ability = Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get();
	const FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(UBwayGameplayAbility_ArgusBase::StaticClass(), TEXT("KitConfig"));
	if (Property)
	{
		const auto* Reference = Property->ContainerPtrToValuePtr<TSoftObjectPtr<UBwayArgusKitConfig>>(Ability);
		if (const UBwayArgusKitConfig* Config = Reference->LoadSynchronous())
		{
			if (Config->ForGloryCooldown > 0) return Config->ForGloryCooldown;
		}
	}
	const UGameplayEffect* GE = Ability->GetCooldownGameplayEffect();
	return GE ? FGameplayEffectSpec(GE, ASC->MakeEffectContext(), Spec.Level).GetDuration() : 0.f;
}

TArray<FActiveGameplayEffectHandle> Cooldowns(ULyraAbilitySystemComponent* ASC, const FGameplayAbilitySpec& Spec)
{
	const FGameplayTagContainer* Tags = Spec.Ability->GetCooldownTags();
	return Tags ? ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*Tags)) : TArray<FActiveGameplayEffectHandle>();
}

bool Finish(const TCHAR* Error)
{
	if (Active)
	{
		for (const auto& Pair : Active->Authority)
		{
			if (Pair.Value.ASC.IsValid()) Pair.Value.ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Pair.Value.RemovalObserver);
		}
	}
	if (Error)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayCooldownUpgradeTest: FAIL step=%d reason=%s"), Active ? Active->Step : -1, Error);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: PASS scope=purchase-activation-cooldown-hud-expiry-reactivation"));
	}
	// Only fixture state is retained. Funding belongs to the owned disposable
	// match; real ability cooldowns expire normally, without forced GE removal.
	Active.Reset();
	return false;
}

bool HudMatches(UBwayMatchAbilitySlotWidget* Slot, bool bCooling, float Remaining, float Duration)
{
	if (!Slot || !Slot->GetCachedWidget().IsValid() || !Slot->IsVisible()) return false;
	const auto& VM = Slot->GetSlotViewModel();
	const UTextBlock* Timer = Cast<UTextBlock>(Slot->GetWidgetFromName(TEXT("Text_CooldownTimer")));
	const UProgressBar* Bar = Cast<UProgressBar>(Slot->GetWidgetFromName(TEXT("ProgressBar_Cooldown")));
	const UImage* Overlay = Cast<UImage>(Slot->GetWidgetFromName(TEXT("Image_CooldownOverlay")));
	const UImage* Icon = Cast<UImage>(Slot->GetWidgetFromName(TEXT("Image_Icon")));
	if (!Timer || !Bar || !Icon || !VM.bIsAvailable || VM.bIsOnCooldown != bCooling) return false;
	// The authored slot dims its icon. The separate overlay is an optional
	// native binding and is absent from this placeholder layout.
	const FLinearColor ExpectedTint = bCooling ? FLinearColor(0.22f, 0.22f, 0.22f, 1.f) : FLinearColor::White;
	if (!Icon->GetColorAndOpacity().Equals(ExpectedTint, 0.001f)) return false;
	if (!bCooling)
	{
		return VM.CountdownTime == 0.f && VM.CooldownPercent == 0.f && Timer->GetText().IsEmpty()
			&& Timer->GetVisibility() == ESlateVisibility::Collapsed && Bar->GetVisibility() == ESlateVisibility::Collapsed
			&& (!Overlay || Overlay->GetVisibility() == ESlateVisibility::Collapsed) && Bar->GetPercent() == 0.f;
	}
	return FMath::Abs(VM.CountdownTime - Remaining) <= 0.35f && Duration > 0.f
		&& FMath::Abs(VM.CooldownPercent - Remaining / Duration) <= 0.03f
		&& FMath::IsNearlyEqual(Bar->GetPercent(), VM.CooldownPercent, 0.001f)
		&& Timer->GetText().ToString() == FText::AsNumber(FMath::Max(1, FMath::CeilToInt(VM.CountdownTime))).ToString()
		&& Timer->IsVisible() && Bar->IsVisible() && (!Overlay || Overlay->IsVisible());
}

bool Tick(float)
{
	if (!Active) return false;
	FProbe& P = *Active;
	const double Now = FPlatformTime::Seconds();
	if (Now - P.Started > 100.0) return Finish(TEXT("timeout"));
	UWorld* World = P.Controller.IsValid() ? P.Controller->GetWorld() : nullptr;
	if (!World)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (!Candidate || !Candidate->IsGameWorld()) continue;
			if (Candidate->WorldType == EWorldType::PIE) return Finish(TEXT("requires separate packaged processes"));
			const ABwayGameState* GS = Candidate->GetGameState<ABwayGameState>();
			if (!GS || !GS->GetRoundManagement() || GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) continue;
			for (TActorIterator<ABwayPlayerController> It(Candidate); It; ++It)
			{
				if (It->IsLocalController() && It->GetLocalPlayer() && It->GetPawn()) { P.Controller = *It; World = Candidate; break; }
			}
		}
		if (!World) return true;
	}
	ABwayPlayerController* PC = P.Controller.Get();
	ABwayPlayerState* Owner = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	ULyraAbilitySystemComponent* ASC = Owner ? Owner->GetLyraAbilitySystemComponent() : nullptr;
	UBwayUpgradeComponent* Shop = Owner ? Owner->UpgradeComponent : nullptr;
	const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
	const FBwayUpgradeDefinition* Definition = Catalog ? Catalog->FindUpgrade(UpgradeId) : nullptr;
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	FGameplayAbilitySpec* Spec = FindAbility(ASC);
	if (!Definition || Definition->Ranks.IsEmpty() || !Gold || !Spec) return true;
	TArray<ABwayPlayerState*> Players;
	TSet<int32> Ids;
	for (TActorIterator<ABwayPlayerState> It(World); It; ++It)
	{
		if (!It->GetPawn() || !It->UpgradeComponent || !It->GetLyraAbilitySystemComponent()->GetSet<UBwayHeroAttributeSet>()) return true;
		Players.Add(*It); Ids.Add(It->GetPlayerId());
	}
	if (Players.Num() != 4 || Ids.Num() != 4) return true;
	if (P.Cost == 0)
	{
		P.Cost = Definition->Ranks[0].Cost; P.Bonus = Definition->Ranks[0].Magnitude;
		P.BaseDuration = KitDuration(*Spec, ASC); P.Ability = Spec->Handle;
		if (P.Cost <= 0 || P.Bonus <= 0 || P.Bonus >= 1 || !FMath::IsFinite(P.Bonus) || P.BaseDuration < 5 || P.BaseDuration > 40) return Finish(TEXT("unsupported catalog or kit duration"));
	}
	const float Expected = P.BaseDuration * (1.f - P.Bonus);
	const float WorldTime = World->GetTimeSeconds();
	const int32 Mode = int32(World->GetNetMode());
	if (Mode != NM_Client)
	{
		for (ABwayPlayerState* Player : Players)
		{
			ULyraAbilitySystemComponent* PeerASC = Player->GetLyraAbilitySystemComponent();
			FGameplayAbilitySpec* PeerSpec = FindAbility(PeerASC);
			if (!PeerSpec) return true;
			const auto Handles = Cooldowns(PeerASC, *PeerSpec);
			FAuthorityCycle& Cycle = P.Authority.FindOrAdd(Player->GetPlayerId());
			if (!Cycle.RemovalObserver.IsValid())
			{
				Cycle.ASC = PeerASC;
				Cycle.RemovalObserver = PeerASC->OnAnyGameplayEffectRemovedDelegate().AddStatic(&Removed, Player->GetPlayerId());
			}
			if (!Cycle.bExpiryValid) return Finish(TEXT("authority expiry timing incorrect"));
			if (Handles.Num() > 1) return Finish(TEXT("duplicate authoritative cooldown"));
			if (Handles.Num() == 1 && !Cycle.bPresent)
			{
				const float Base = KitDuration(*PeerSpec, PeerASC);
				const float Duration = PeerASC->GetActiveGameplayEffect(Handles[0])->GetDuration();
				const float Reduction = PeerASC->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute());
				if (!FMath::IsNearlyEqual(Duration, Base * (1.f - P.Bonus), 0.01f) || !FMath::IsNearlyEqual(Reduction, P.Bonus, 0.001f) || Player->UpgradeComponent->GetOwnedRank(UpgradeId) != 1) return Finish(TEXT("authoritative cooldown does not match purchased reduction"));
				Cycle.bPresent = true; Cycle.Start = WorldTime; Cycle.Expected = Base * (1.f - P.Bonus); Cycle.Handle = Handles[0]; ++Cycle.Cycles;
				if (Cycle.Cycles > 2) return Finish(TEXT("unexpected extra authority activation"));
				UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: Authority BEGIN player=%d cycle=%d base=%.4f bonus=%.4f duration=%.4f rank=1"), Player->GetPlayerId(), Cycle.Cycles, Base, P.Bonus, Duration);
			}
		}
	}
	if (P.Step == 0)
	{
		if (Mode != NM_Client && !P.bFunded)
		{
			for (ABwayPlayerState* Player : Players)
			{
				if (!Player->UpgradeComponent->IsPurchaseWindowOpen() || Player->UpgradeComponent->GetOwnedRank(UpgradeId) != 0 || !Player->GetLyraAbilitySystemComponent()->GetSet<UBwayGoldAttributeSet>()) return true;
			}
			for (ABwayPlayerState* Player : Players)
			{
				Player->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), float(P.Cost));
				Player->ForceNetUpdate();
			}
			P.bFunded = true;
		}
		if (Gold->GetCurrentGold() < P.Cost || !Shop->IsPurchaseWindowOpen()) return true;
		if (Shop->GetOwnedRank(UpgradeId) != 0 || ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()) != 0.f) return Finish(TEXT("owner baseline already upgraded"));
		if (P.Changed == 0) { P.Changed = Now; return true; }
		if (Now - P.Changed < 2.f) return true;
		P.BeforeGold = Gold->GetCurrentGold(); P.Income = Gold->GetGoldPerSecond();
		Shop->ServerPurchaseUpgrade(UpgradeId, 0);
		P.Step = 1; P.Changed = Now;
		return true;
	}
	if (P.Step == 1)
	{
		const float Tolerance = FMath::Max(0.f, P.Income) * (FMath::CeilToInt(Now - P.Changed) + 2) + 0.01f;
		const float Balance = Gold->GetCurrentGold();
		if (Shop->GetOwnedRank(UpgradeId) != 1 || !FMath::IsNearlyEqual(ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()), P.Bonus, 0.001f)
			|| Balance >= P.BeforeGold || Balance < P.BeforeGold - P.Cost - 0.01f || Balance > P.BeforeGold - P.Cost + Tolerance) return true;
		UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: Purchase PASS player=%d netmode=%d before=%.2f after=%.2f cost=%d tolerance=%.2f bonus=%.4f base=%.4f"), Owner->GetPlayerId(), Mode, P.BeforeGold, Balance, P.Cost, Tolerance, P.Bonus, P.BaseDuration);
		P.Step = 2;
	}
	if (P.Step == 2)
	{
		if (!P.Slot.IsValid())
		{
			for (TObjectIterator<UBwayMatchAbilitySlotWidget> It; It; ++It)
			{
				if (It->GetWorld() == World && It->GetOwningPlayer() == PC && It->GetCachedWidget().IsValid()
					&& Spec->GetDynamicSpecSourceTags().HasTagExact(It->GetSlotViewModel().DisplayInfo.InputActionTag)) { P.Slot = *It; break; }
			}
		}
		if (!HudMatches(P.Slot.Get(), false, 0, Expected))
		{
			if (!P.bHudWaitReported && Now - P.Changed > 5.f)
			{
				P.bHudWaitReported = true;
				const auto* Slot = P.Slot.Get();
				UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: HudWait player=%d slot=%s visible=%d cached=%d available=%d cooling=%d countdown=%.4f percent=%.4f"),
					Owner->GetPlayerId(), *GetNameSafe(Slot), Slot && Slot->IsVisible(), Slot && Slot->GetCachedWidget().IsValid(),
					Slot && Slot->GetSlotViewModel().bIsAvailable, Slot && Slot->GetSlotViewModel().bIsOnCooldown,
					Slot ? Slot->GetSlotViewModel().CountdownTime : -1.f, Slot ? Slot->GetSlotViewModel().CooldownPercent : -1.f);
			}
			return true;
		}
		if (!ASC->TryActivateAbility(P.Ability)) return Finish(TEXT("initial real ability activation rejected"));
		P.ActivatedAt = WorldTime; P.Step = 3;
		UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: Activate player=%d netmode=%d expected=%.4f"), Owner->GetPlayerId(), Mode, Expected);
		return true;
	}
	const auto Handles = Cooldowns(ASC, *Spec);
	if ((P.Step == 3 || P.Step == 4 || P.Step == 6) && WorldTime - P.ActivatedAt >= 1.5f && Handles.Num() == 1)
	{
		const FActiveGameplayEffect* GE = ASC->GetActiveGameplayEffect(Handles[0]);
		const float Duration = GE->GetDuration();
		const float Remaining = GE->GetTimeRemaining(WorldTime);
		if (!FMath::IsNearlyEqual(Duration, Expected, 0.01f)) return Finish(TEXT("settled owner duration mismatch"));
		if (!HudMatches(P.Slot.Get(), true, Remaining, Duration)) return true;
		if (P.Step == 3)
		{
			if (Spec->IsActive()) return true;
			if (Spec->Ability->CheckCooldown(P.Ability, ASC->AbilityActorInfo.Get()) || ASC->TryActivateAbility(P.Ability)) return Finish(TEXT("ability reused during cooldown"));
			P.FirstSampleAt = WorldTime; P.FirstRemaining = Remaining;
			P.Step = 4;
			UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: RejectedDuringCooldown player=%d netmode=%d inactive=1"), Owner->GetPlayerId(), Mode);
		}
		else if (P.Step == 4 && WorldTime - P.FirstSampleAt >= 2.f)
		{
			if (Remaining >= P.FirstRemaining || FMath::Abs((P.FirstRemaining - Remaining) - (WorldTime - P.FirstSampleAt)) > 0.35f) return Finish(TEXT("cooldown countdown did not advance"));
			P.Step = 5;
		}
		else if (P.Step == 6)
		{
			P.Step = 7;
			UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: Reactivation PASS player=%d netmode=%d duration=%.4f"), Owner->GetPlayerId(), Mode, Duration);
		}
		else return true;
		const auto& VM = P.Slot->GetSlotViewModel();
		UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: HUD player=%d netmode=%d sample=%d duration=%.4f remaining=%.4f countdown=%.4f percent=%.4f elapsed=%.4f controls=1"), Owner->GetPlayerId(), Mode, P.Step == 4 ? 1 : (P.Step == 5 ? 2 : 3), Duration, Remaining, VM.CountdownTime, VM.CooldownPercent, WorldTime - P.ActivatedAt);
	}
	if (P.Step == 5 && Handles.IsEmpty() && Spec->Ability->CheckCooldown(P.Ability, ASC->AbilityActorInfo.Get()) && HudMatches(P.Slot.Get(), false, 0, Expected))
	{
		const float Elapsed = WorldTime - P.ActivatedAt;
		if (FMath::Abs(Elapsed - Expected) > 1.5f) return Finish(TEXT("owner expiry outside latency/tick tolerance"));
		UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: Expiry PASS player=%d netmode=%d elapsed=%.4f expected=%.4f controls=1"), Owner->GetPlayerId(), Mode, Elapsed, Expected);
		if (!ASC->TryActivateAbility(P.Ability)) return Finish(TEXT("ability rejected after ready HUD and cooldown expiry"));
		P.ActivatedAt = WorldTime; P.Step = 6;
		return true;
	}
	if (P.Step == 7)
	{
		if (Mode != NM_Client)
		{
			if (P.Authority.Num() != 4) return true;
			for (const auto& Pair : P.Authority) { if (Pair.Value.Cycles != 2 || !Pair.Value.bExpired) return true; }
			UE_LOG(LogTemp, Display, TEXT("BwayCooldownUpgradeTest: Authority PASS owners=4 cycles=8 expiries=4"));
		}
		return Finish(nullptr);
	}
	return true;
}

void Start()
{
	if (Active) return;
	Active = MakeShared<FProbe>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.1f);
}
FAutoConsoleCommand Command(TEXT("bway.Test.CooldownUpgrade"), TEXT("Development-only four-process Argus purchase, real ForGlory activation, authoritative durations and live HUD expiry; owned disposable match required."), FConsoleCommandDelegate::CreateStatic(&Start));
}
#endif
