// Controlled listener-delivery coverage in an owned, disposable multiplayer match.
#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"

namespace BwayMessageListenerTest
{
bool Running = false;

struct FProbe
{
	double Started = FPlatformTime::Seconds();
	double ReadySince = 0.0;
	int32 Epoch = 0;
	TWeakObjectPtr<ABwayGameState> Pending;
	TWeakObjectPtr<ABwayGameState> Completed;

	bool Stop(const TCHAR* Error = nullptr)
	{
		if (Error) UE_LOG(LogTemp, Error, TEXT("BwayListenerDelivery: FAIL epoch=%d reason=%s"), Epoch, Error);
		Running = false;
		return false;
	}

	bool Exercise(ABwayGameState* GS, const TArray<ABwayPlayerState*>& Players)
	{
		auto* Manager = GS->FindComponentByClass<UBwayRelicManagerComponent>();
		ARelicActor* Relic = Manager ? Manager->GetRelicActor() : nullptr;
		const URelicSettings* Settings = Manager ? Manager->GetRelicSettings() : nullptr;
		if (!Relic || !Settings || !Settings->bFumbleOnDamage) return Stop(TEXT("relic-fumble-setup"));
		TArray<ABwayPlayerState*> Sources;
		TSet<ABwayPlayerState*> Used;
		for (ABwayPlayerState* Target : Players)
		{
			ABwayPlayerState* Source = nullptr;
			for (ABwayPlayerState* Candidate : Players)
				if (!Used.Contains(Candidate) && GS->GetPlayerTeam(Candidate) != GS->GetPlayerTeam(Target)) { Source = Candidate; break; }
			if (!Source) return Stop(TEXT("balanced-opposing-human-teams-required"));
			Sources.Add(Source);
			Used.Add(Source);
		}

		++Epoch;
		UE_LOG(LogTemp, Display, TEXT("BwayListenerDelivery: Begin epoch=%d world=%s players=%d"), Epoch, *GS->GetWorld()->GetName(), Players.Num());
		// These are deliberate production pickup/damage actions. Reset the fixture's
		// relic to the map spawn afterward; no pawn transforms or content are edited.
		ON_SCOPE_EXIT
		{
			Manager->ResetRelic();
			UE_LOG(LogTemp, Display, TEXT("BwayListenerDelivery: Cleanup epoch=%d relicReset=1"), Epoch);
		};
		ABwayPlayerState* CarrierPS = Players[1];
		auto* Carrier = Cast<ABwayCharacterWithAbilities>(CarrierPS->GetPawn());
		const int32 CarrierFumblesBefore = CarrierPS->GetForcedFumbles();
		Relic->OnDropped();
		Relic->OnPickedUp(Carrier);
		if (Relic->CurrentCarrier != Carrier) return Stop(TEXT("pickup-not-carried"));

		for (int32 Index = 0; Index < Players.Num(); ++Index)
		{
			ABwayPlayerState* Target = Players[Index];
			ABwayPlayerState* Source = Sources[Index];
			ULyraAbilitySystemComponent* ASC = Target->GetLyraAbilitySystemComponent();
			const float Before = ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
			const float Expected = UBwayDamageLibrary::ResolveFinalDamage(13.0f, ASC);
			const float AccumulatedDamageBefore = Relic->GetAccumulatedCarrierDamage();
			const int32 AttackerFumblesBefore = Source->GetForcedFumbles();
			const bool Applied = UBwayDamageLibrary::ApplyDamageFromSource(Source->GetLyraAbilitySystemComponent(), Target->GetPawn(), 13.0f, Source->GetPawn(), Source->GetPawn());
			const float After = ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
			if (!Applied || Expected < 1.0f || !FMath::IsNearlyEqual(Before - After, Expected, 0.01f)) return Stop(TEXT("production-damage-delta"));

			// An instant Healing modifier executes the production HealthSet message path.
			UGameplayEffect* Heal = NewObject<UGameplayEffect>();
			Heal->DurationPolicy = EGameplayEffectDurationType::Instant;
			FGameplayModifierInfo& Modifier = Heal->Modifiers.AddDefaulted_GetRef();
			Modifier.Attribute = ULyraHealthSet::GetHealingAttribute();
			Modifier.ModifierOp = EGameplayModOp::Additive;
			Modifier.ModifierMagnitude = FScalableFloat(Before - After);
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(Target->GetPawn(), Target->GetPawn());
			FGameplayEffectSpec Spec(Heal, Context, 1.0f);
			ASC->ApplyGameplayEffectSpecToSelf(Spec);
			const float Healed = ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
			UE_LOG(LogTemp, Display, TEXT("BwayListenerDelivery: Delta epoch=%d player=%d source=%d before=%.3f damaged=%.3f healed=%.3f expected=%.3f"), Epoch, Target->GetPlayerId(), Source->GetPlayerId(), Before, After, Healed, Expected);
			if (!FMath::IsNearlyEqual(Healed, Before, 0.01f)) return Stop(TEXT("production-heal-delta"));
			if (Target == CarrierPS)
			{
				const bool Dropped = !Relic->CurrentCarrier && Relic->GetCurrentState() != ERelicState::Carried;
				const bool ExpectedDrop = AccumulatedDamageBefore + Expected + KINDA_SMALL_NUMBER >= Settings->FumbleDamageThreshold;
				UE_LOG(LogTemp, Display, TEXT("BwayListenerDelivery: Fumble epoch=%d player=%d dropped=%d expectedDrop=%d source=%d before=%d after=%d"),
					Epoch, CarrierPS->GetPlayerId(), Dropped, ExpectedDrop, Source->GetPlayerId(), AttackerFumblesBefore, Source->GetForcedFumbles());
				if (Dropped != ExpectedDrop || Source->GetForcedFumbles() != AttackerFumblesBefore + (ExpectedDrop ? 1 : 0)
					|| CarrierPS->GetForcedFumbles() != CarrierFumblesBefore) return Stop(TEXT("carrier-damage-listener"));
				if (!ExpectedDrop && (Relic->CurrentCarrier != Carrier
					|| !FMath::IsNearlyEqual(Relic->GetAccumulatedCarrierDamage(), AccumulatedDamageBefore + Expected, 0.01f)))
				{
					return Stop(TEXT("carrier-damage-accumulation"));
				}
			}
		}
		UE_LOG(LogTemp, Display, TEXT("BwayListenerDelivery: EpochPassed epoch=%d players=4"), Epoch);
		return true;
	}

	bool Tick(float)
	{
		if (FPlatformTime::Seconds() - Started > 360.0) return Stop(TEXT("two-gameplay-lifetimes-timeout"));
		if (!GEngine) return true;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || World->WorldType != EWorldType::Game) continue;
			if (World->GetNetMode() == NM_Client) return Stop(); // Existing local diagnostics observe delivery.
			if (World->GetNetMode() != NM_ListenServer) continue;
			auto* GS = World->GetGameState<ABwayGameState>();
			auto* Round = GS ? GS->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
			if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || GS == Completed.Get()) continue;
			if (Pending.Get() != GS) { Pending = GS; ReadySince = FPlatformTime::Seconds(); }
			if (FPlatformTime::Seconds() - ReadySince < 4.0) return true;
			TArray<ABwayPlayerState*> Players;
			for (APlayerState* PS : GS->PlayerArray)
				if (auto* P = Cast<ABwayPlayerState>(PS); P && !P->IsABot() && !P->IsInactive())
				{
					auto* ASC = P->GetLyraAbilitySystemComponent();
					if (!Cast<ABwayCharacterWithAbilities>(P->GetPawn()) || !ASC || ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) < 100.0f) return true;
					Players.Add(P);
				}
			if (Players.Num() != 4) return true;
			Players.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
			if (!Exercise(GS, Players)) return false;
			Completed = GS;
			if (Epoch == 2) { UE_LOG(LogTemp, Display, TEXT("BwayListenerDelivery: COMPLETE epochs=2")); return Stop(); }
		}
		return true;
	}
};

FAutoConsoleCommand Command(TEXT("bway.Test.ListenerDelivery"), TEXT("Development-only: controlled damage/heal and carrier-fumble delivery in two disposable browser-flow matches."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (Running) return;
		Running = true;
		TSharedRef<FProbe> Probe = MakeShared<FProbe>();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Delta) { return Probe->Tick(Delta); }), 0.25f);
	}));
}
#endif
