#include "Development/BwayLeechLimitProbe.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Containers/Ticker.h"
#include "Economy/BwayBaseZone.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "LyraGameplayTags.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayLeechLimitProbe)

bool FBwayLeechLimitFrame::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	// A single network property carries the complete case input, including its serial.
	Ar << Serial << Case << Stage << Source << Target << FirstVictim;
	Ar << Raw << Bonus << Armor << Incoming << Players << Before << OriginalPawns;
	bOutSuccess = !Ar.IsError();
	return true;
}

namespace
{
const TCHAR* LeechLimitCaseNames[] = { TEXT("Cap"), TEXT("Full"), TEXT("Immune"), TEXT("Friendly"), TEXT("Self"), TEXT("Overkill"), TEXT("DeadSource") };
ULyraAbilitySystemComponent* ASC(ABwayPlayerState* P) { return P ? P->GetLyraAbilitySystemComponent() : nullptr; }
float Health(ABwayPlayerState* P) { return ASC(P)->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()); }
float Maximum(ABwayPlayerState* P) { return ASC(P)->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()); }
float Leech(ABwayPlayerState* P) { return ASC(P)->GetNumericAttribute(UBwayHeroAttributeSet::GetLifeLeechAttribute()); }
int32 Id(UObject* Object)
{
	const APlayerState* P = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Object);
	return P ? P->GetPlayerId() : INDEX_NONE;
}
TArray<ABwayPlayerState*> Players(UWorld* World)
{
	TArray<ABwayPlayerState*> Result;
	if (ABwayGameState* GS = World->GetGameState<ABwayGameState>())
		for (APlayerState* P : GS->PlayerArray)
			if (ABwayPlayerState* B = Cast<ABwayPlayerState>(P); B && !B->IsABot() && ASC(B) && ASC(B)->GetSet<ULyraHealthSet>() && ASC(B)->GetSet<UBwayHeroAttributeSet>()) Result.Add(B);
	Result.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return Result;
}
ABwayPlayerState* Find(const TArray<ABwayPlayerState*>& All, int32 PlayerId)
{
	for (ABwayPlayerState* P : All) if (P->GetPlayerId() == PlayerId) return P;
	return nullptr;
}
float ExpectedDamage(const FBwayLeechLimitFrame& F)
{
	if (F.Case == 2) return 0;
	const int32 TargetIndex = F.Players.IndexOfByKey(F.Target);
	return TargetIndex != INDEX_NONE ? FMath::Min(F.Before[TargetIndex], FMath::Max(0.f, F.Raw * (1.f - F.Armor * 0.01f) * F.Incoming)) : 0;
}
float ExpectedHeal(const FBwayLeechLimitFrame& F, float SourceMaximum)
{
	if (F.Case != 0 && F.Case != 5) return 0;
	return FMath::Min(SourceMaximum - F.Before[F.Players.IndexOfByKey(F.Source)], ExpectedDamage(F) * F.Bonus);
}
}

struct FBwayLeechLimitState
{
	double Started = FPlatformTime::Seconds(), Changed = 0, StableAt = 0;
	int32 SetupStep = 0, LastSerial = 0, ObservedSerial = 0, DamageEvents = 0, HealEvents = 0;
	double Damage = 0, Healing = 0;
	bool bCollect = false, bCleaned = false, bFinished = false, bImmuneAdded = false, bWrongMessage = false;
	float RespawnDelay = 0;
	TWeakObjectPtr<UBwayRoundManagementComponent> Rounds;
	TArray<TWeakObjectPtr<ABwayPlayerState>> SavedPlayers;
	TArray<float> SavedHealth, SavedGold;
	TMap<int32, FString> LocalOriginalPawns;
	TMap<TWeakObjectPtr<ABwayBaseZone>, float> Bases;
	FGameplayMessageListenerHandle DamageListener, HealListener;
};

ABwayLeechLimitProbe::ABwayLeechLimitProbe()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetLoadOnClient = false;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.f);
	PrimaryActorTick.bCanEverTick = !UE_BUILD_SHIPPING;
	PrimaryActorTick.TickInterval = 0.1f;
}
void ABwayLeechLimitProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayLeechLimitProbe, Frame);
}
void ABwayLeechLimitProbe::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	State = MakeShared<FBwayLeechLimitState>();
	UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Connected role=%s"), HasAuthority() ? TEXT("host") : TEXT("client"));
#endif
}
void ABwayLeechLimitProbe::Fail(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Error, TEXT("BwayLeechLimits: FAIL role=%s serial=%d case=%d error=%s damageEvents=%d healEvents=%d damage=%.4f healing=%.4f"), HasAuthority() ? TEXT("host") : TEXT("client"), Frame.Serial, Frame.Case, Reason, State->DamageEvents, State->HealEvents, State->Damage, State->Healing);
	State->bFinished = true;
	Cleanup();
	SetActorTickEnabled(false);
}
bool ABwayLeechLimitProbe::Setup()
{
	const TArray<ABwayPlayerState*> All = Players(GetWorld());
	ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>();
	UBwayRoundManagementComponent* Rounds = GS ? GS->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
	if (All.Num() != 4 || !Rounds) return false;
	for (ABwayPlayerState* P : All)
		if (!P->GetPawn() || ASC(P)->GetAvatarActor() != P->GetPawn() || !P->UpgradeComponent || !P->UpgradeComponent->GetCatalog() || GS->GetPlayerTeam(P) == INDEX_NONE) return false;
	if (State->SetupStep == 0)
	{
		if (Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return false;
		for (ABwayPlayerState* P : All) if (ASC(P)->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity)) return false;
		ABwayPlayerState* Source = nullptr;
		for (ABwayPlayerState* P : All) if (P->GetPlayerController() && !P->GetPlayerController()->IsLocalController()) { Source = P; break; }
		if (!Source) return false;
		Frame.Source = Source->GetPlayerId();
		for (ABwayPlayerState* P : All) if (GS->GetPlayerTeam(P) != GS->GetPlayerTeam(Source)) { Frame.FirstVictim = P->GetPlayerId(); break; }
		const FBwayUpgradeDefinition* Definition = Source->UpgradeComponent->GetCatalog()->FindUpgrade(TEXT("LifeLeech"));
		if (!Definition || Definition->Ranks.IsEmpty() || Frame.FirstVictim == INDEX_NONE) { Fail(TEXT("invalid-roster-catalog")); return false; }
		Frame.Bonus = Definition->Ranks[0].Magnitude;
		for (ABwayPlayerState* P : All)
		{
			if (!P->UpgradeComponent->GetOwnedUpgrades().IsEmpty()) { Fail(TEXT("unexpected-upgrades")); return false; }
			State->SavedPlayers.Add(P); State->SavedHealth.Add(Health(P)); State->SavedGold.Add(ASC(P)->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()));
			UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Saved player=%d health=%.4f gold=%.4f"), P->GetPlayerId(), Health(P), State->SavedGold.Last());
			Frame.Players.Add(P->GetPlayerId());
		}
		State->Rounds = Rounds; State->RespawnDelay = Rounds->RespawnDelay;
		Rounds->RespawnDelay = 20.f;
		for (TActorIterator<ABwayBaseZone> It(GetWorld()); It; ++It) { State->Bases.Add(*It, It->HealingPerSecond); It->HealingPerSecond = 0; }
		ASC(Source)->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), Definition->Ranks[0].Cost + 100.f);
		State->SetupStep = 1;
		Rounds->EndRound(0, EBwayWinCondition::GoalScored);
		return false;
	}
	ABwayPlayerState* Source = Find(All, Frame.Source);
	if (State->SetupStep == 1)
	{
		if (!Source->UpgradeComponent->IsPurchaseWindowOpen()) return false;
		Source->UpgradeComponent->ServerPurchaseUpgrade(TEXT("LifeLeech"), 0);
		if (Source->UpgradeComponent->GetOwnedRank(TEXT("LifeLeech")) != 1) { Fail(TEXT("purchase-failed")); return false; }
		State->SetupStep = 2;
		return false;
	}
	if (Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return false;
	// EnterPlaying publishes its enum before the GAS phase starts on the next tick.
	// Wait for production protection to end before taking the baseline or adding test tags.
	for (ABwayPlayerState* P : All) if (ASC(P)->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity)) return false;
	Frame.OriginalPawns.Reset();
	for (ABwayPlayerState* P : All) Frame.OriginalPawns.Add(P->GetPawn()->GetName());
	State->DamageListener = UGameplayMessageSubsystem::Get(this).RegisterListener<FLyraVerbMessage>(TAG_Lyra_Damage_Message, this, &ABwayLeechLimitProbe::OnDamageMessage);
	State->HealListener = UGameplayMessageSubsystem::Get(this).RegisterListener<FLyraVerbMessage>(TAG_Lyra_Heal_Message, this, &ABwayLeechLimitProbe::OnHealMessage);
	State->SetupStep = 3;
	UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Setup players=4 source=%d victim=%d remoteSource=1 bonus=%.4f respawnBefore=%.4f respawnDuring=20.0000 bases=%d"), Frame.Source, Frame.FirstVictim, Frame.Bonus, State->RespawnDelay, State->Bases.Num());
	return PrepareCase(0);
}
bool ABwayLeechLimitProbe::PrepareCase(int32 Case)
{
	State->bCollect = false;
	const TArray<ABwayPlayerState*> All = Players(GetWorld());
	ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>();
	ABwayPlayerState* Source = Find(All, Frame.Source);
	if (All.Num() != 4 || !Source || !GS) { Fail(TEXT("roster-lost")); return false; }
	Frame.Case = Case; Frame.Stage = 0; Frame.Target = Frame.FirstVictim;
	if (Case == 3 || Case == 6)
	{
		Frame.Target = INDEX_NONE;
		for (ABwayPlayerState* P : All)
			if (P != Source && Health(P) > 0 && ((GS->GetPlayerTeam(P) == GS->GetPlayerTeam(Source)) == (Case == 3))) { Frame.Target = P->GetPlayerId(); break; }
	}
	if (Case == 4) Frame.Target = Frame.Source;
	ABwayPlayerState* Target = Find(All, Frame.Target);
	if (!Target) { Fail(TEXT("target-missing")); return false; }
	for (ABwayPlayerState* P : All)
		if (Health(P) > 0) ASC(P)->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Maximum(P) * 0.6f);
	ASC(Source)->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Case == 0 ? Maximum(Source) - 0.125f : Case == 1 ? Maximum(Source) : Maximum(Source) * 0.4f);
	if (Case == 5) ASC(Target)->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 3.f);
	if (Case == 6)
	{
		UBwayDamageLibrary::ApplyDamageFromSource(ASC(Target), Source->GetPawn(), Maximum(Source) * 10.f, Target, Target);
		if (Health(Source) != 0) { Fail(TEXT("source-did-not-die")); return false; }
		UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: DeathSetup player=%d health=%.4f death=%d"), Frame.Source, Health(Source), ASC(Source)->HasMatchingGameplayTag(LyraGameplayTags::Status_Death) ? 1 : 0);
	}
	if (ASC(Target)->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity)) { Fail(TEXT("unexpected-immunity")); return false; }
	if (Case == 2) { ASC(Target)->AddLooseGameplayTag(TAG_Gameplay_DamageImmunity); State->bImmuneAdded = true; }
	Frame.Before.Reset();
	for (ABwayPlayerState* P : All) Frame.Before.Add(Health(P));
	Frame.Armor = ASC(Target)->GetNumericAttribute(UBwayHeroAttributeSet::GetArmorAttribute());
	Frame.Incoming = ASC(Target)->GetNumericAttribute(UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute());
	++Frame.Serial; State->Changed = FPlatformTime::Seconds();
	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Plan serial=%d case=%d name=%s source=%d target=%d raw=%.4f armor=%.4f incoming=%.4f bonus=%.4f sourceBefore=%.4f targetBefore=%.4f sourceMax=%.4f"), Frame.Serial, Case, LeechLimitCaseNames[Case], Frame.Source, Frame.Target, Frame.Raw, Frame.Armor, Frame.Incoming, Frame.Bonus, Health(Source), Health(Target), Maximum(Source));
	return true;
}
void ABwayLeechLimitProbe::ApplyCase()
{
	const TArray<ABwayPlayerState*> All = Players(GetWorld());
	ABwayPlayerState* Source = Find(All, Frame.Source); ABwayPlayerState* Target = Find(All, Frame.Target);
	if (!Source || !Target) { Fail(TEXT("damage-roster-lost")); return; }
	State->DamageEvents = State->HealEvents = 0; State->Damage = State->Healing = 0; State->bWrongMessage = false; State->bCollect = true;
	// PlayerState provenance deliberately survives the dead attacker's avatar. No damage is synthesized as a message.
	UBwayDamageLibrary::ApplyDamageFromSource(ASC(Source), Target, Frame.Raw, this, Source);
	if (Frame.Case == 5) UBwayDamageLibrary::ApplyDamageFromSource(ASC(Source), Target, Frame.Raw, this, Source);
	const float Damage = ExpectedDamage(Frame), Healing = ExpectedHeal(Frame, Maximum(Source));
	if (State->bWrongMessage || State->DamageEvents != (Damage > 0 ? 1 : 0) || State->HealEvents != (Healing > 0 ? 1 : 0) || !FMath::IsNearlyEqual(State->Damage, double(Damage), 0.002) || !FMath::IsNearlyEqual(State->Healing, double(Healing), 0.002)) { Fail(TEXT("damage-heal-event-mismatch")); return; }
	UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Result case=%d name=%s source=%d target=%d damage=%.4f healing=%.4f hits=%d heals=%d attempts=%d"), Frame.Case, LeechLimitCaseNames[Frame.Case], Frame.Source, Frame.Target, State->Damage, State->Healing, State->DamageEvents, State->HealEvents, Frame.Case == 5 ? 2 : 1);
	Frame.Stage = 1; ++Frame.Serial; State->Changed = FPlatformTime::Seconds(); ForceNetUpdate();
}
void ABwayLeechLimitProbe::OnDamageMessage(FGameplayTag, const FLyraVerbMessage& Message)
{
	if (!State || !State->bCollect || Id(Message.Target) != Frame.Target) return;
	++State->DamageEvents; State->Damage += Message.Magnitude;
	State->bWrongMessage |= Id(Message.Instigator) != Frame.Source;
	UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Damage case=%d source=%d target=%d amount=%.4f"), Frame.Case, Id(Message.Instigator), Id(Message.Target), Message.Magnitude);
}
void ABwayLeechLimitProbe::OnHealMessage(FGameplayTag, const FLyraVerbMessage& Message)
{
	if (!State || !State->bCollect || Id(Message.Target) != Frame.Source) return;
	++State->HealEvents; State->Healing += Message.Magnitude;
	State->bWrongMessage |= Id(Message.Instigator) != Frame.Source;
	UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Heal case=%d source=%d target=%d amount=%.4f"), Frame.Case, Id(Message.Instigator), Id(Message.Target), Message.Magnitude);
}
bool ABwayLeechLimitProbe::Observe()
{
	const TArray<ABwayPlayerState*> All = Players(GetWorld());
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ABwayPlayerState* Local = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (All.Num() != 4 || !Local || Frame.Players.Num() != 4 || Frame.Before.Num() != 4) return false;
	if (Frame.Serial == 1 && State->LocalOriginalPawns.IsEmpty())
	{
		for (ABwayPlayerState* P : All) if (!P->GetPawn() || ASC(P)->GetAvatarActor() != P->GetPawn()) return false;
		for (ABwayPlayerState* P : All) State->LocalOriginalPawns.Add(P->GetPlayerId(), P->GetPawn()->GetName());
	}
	if (Frame.Serial != State->LastSerial)
	{
		if (Frame.Serial != State->LastSerial + 1 || State->ObservedSerial != State->LastSerial) { Fail(TEXT("case-frame-skipped")); return false; }
		State->LastSerial = Frame.Serial; State->StableAt = 0;
	}
	if (State->ObservedSerial == Frame.Serial) return true;
	ABwayPlayerState* Source = Find(All, Frame.Source);
	if (!Source) return false;
	const float Damage = ExpectedDamage(Frame), Healing = ExpectedHeal(Frame, Maximum(Source));
	bool bMatches = true;
	for (int32 I = 0; I < 4; ++I)
	{
		ABwayPlayerState* P = Find(All, Frame.Players[I]);
		if (!P) return false;
		float Expected = Frame.Before[I];
		if (Frame.Stage == 1) { if (P->GetPlayerId() == Frame.Target) Expected -= Damage; if (P == Source) Expected += Healing; }
		if (Frame.Stage == 2) Expected = Maximum(P);
		const float ExpectedLeech = Frame.Stage == 3 ? 0 : P == Source ? Frame.Bonus : 0;
		const int32 ExpectedRank = Frame.Stage != 3 && P == Source && (HasAuthority() || P == Local) ? 1 : 0;
		bMatches &= FMath::IsNearlyEqual(Health(P), Expected, 0.002f) && FMath::IsNearlyEqual(Leech(P), ExpectedLeech, 0.0001f) && P->UpgradeComponent->GetOwnedRank(TEXT("LifeLeech")) == ExpectedRank;
		// HealthComponent clears its loose death tags when the old pawn uninitializes.
		if (Frame.Stage <= 1 && Expected == 0) bMatches &= !P->GetPawn() || ASC(P)->HasMatchingGameplayTag(LyraGameplayTags::Status_Death);
		if (Frame.Stage == 2 && (P == Source || P->GetPlayerId() == Frame.FirstVictim))
			bMatches &= P->GetPawn() && ASC(P)->GetAvatarActor() == P->GetPawn() && P->GetPawn()->GetName() != State->LocalOriginalPawns.FindRef(P->GetPlayerId()) && !ASC(P)->HasMatchingGameplayTag(LyraGameplayTags::Status_Death);
	}
	if (!bMatches) { State->StableAt = 0; return false; }
	const double Now = FPlatformTime::Seconds();
	if (!State->StableAt) State->StableAt = Now;
	if (Now - State->StableAt < 0.75) return false;
	for (int32 I = 0; I < 4; ++I)
	{
		ABwayPlayerState* P = Find(All, Frame.Players[I]);
		UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Peer serial=%d case=%d stage=%d observer=%d player=%d source=%d target=%d health=%.4f max=%.4f leech=%.4f rank=%d team=%d death=%d pawn=%s original=%s"), Frame.Serial, Frame.Case, Frame.Stage, Local->GetPlayerId(), P->GetPlayerId(), Frame.Source, Frame.Target, Health(P), Maximum(P), Leech(P), P->UpgradeComponent->GetOwnedRank(TEXT("LifeLeech")), P->GetTeamId(), ASC(P)->HasMatchingGameplayTag(LyraGameplayTags::Status_Death) ? 1 : 0, *GetNameSafe(P->GetPawn()), *State->LocalOriginalPawns.FindRef(P->GetPlayerId()));
	}
	State->ObservedSerial = Frame.Serial;
	if (Frame.Stage == 3)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: %s PASS observer=%d cases=7 frames=%d"), HasAuthority() ? TEXT("Host") : TEXT("Client"), Local->GetPlayerId(), State->ObservedSerial);
		State->bFinished = true; SetActorTickEnabled(false);
		if (HasAuthority()) SetLifeSpan(6.f);
	}
	return true;
}
void ABwayLeechLimitProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!State || State->bFinished) return;
	const double Now = FPlatformTime::Seconds();
	if (Now - State->Started > 180) { Fail(TEXT("timeout")); return; }
	if (HasAuthority() && State->SetupStep < 3) { Setup(); return; }
	if (!Frame.Serial) return;
	const bool bObserved = Observe();
	if (!HasAuthority() || State->bFinished || !bObserved || Now - State->Changed < 4) return;
	if (Frame.Stage == 0) { ApplyCase(); return; }
	if (Frame.Stage == 1)
	{
		State->bCollect = false;
		if (State->bImmuneAdded) { ASC(Find(Players(GetWorld()), Frame.Target))->RemoveLooseGameplayTag(TAG_Gameplay_DamageImmunity); State->bImmuneAdded = false; }
		if (Frame.Case < 6) { PrepareCase(Frame.Case + 1); return; }
		const TArray<ABwayPlayerState*> All = Players(GetWorld());
		if (All.Num() != 4) { Fail(TEXT("respawn-roster-lost")); return; }
		for (int32 I = 0; I < 4; ++I)
		{
			ABwayPlayerState* P = Find(All, Frame.Players[I]);
			if (!P || !P->GetPawn() || ASC(P)->GetAvatarActor() != P->GetPawn() || Health(P) <= 0 || ASC(P)->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)) return;
			if ((P->GetPlayerId() == Frame.Source || P->GetPlayerId() == Frame.FirstVictim) && (P->GetPawn()->GetName() == Frame.OriginalPawns[I] || !FMath::IsNearlyEqual(Health(P), Maximum(P), 0.002f))) return;
		}
		for (ABwayPlayerState* P : All) ASC(P)->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Maximum(P));
		Frame.Stage = 2; ++Frame.Serial; State->Changed = Now; ForceNetUpdate();
		UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Respawn players=2 natural=1"));
		return;
	}
	if (Frame.Stage == 2)
	{
		Cleanup(); Frame.Before = State->SavedHealth;
		Frame.Stage = 3; ++Frame.Serial; State->Changed = Now; ForceNetUpdate();
	}
}
void ABwayLeechLimitProbe::Cleanup()
{
	if (!HasAuthority() || !State || State->bCleaned) return;
	State->bCollect = false;
	if (State->DamageListener.IsValid()) State->DamageListener.Unregister();
	if (State->HealListener.IsValid()) State->HealListener.Unregister();
	if (State->bImmuneAdded)
		if (ABwayPlayerState* Target = Find(Players(GetWorld()), Frame.Target)) ASC(Target)->RemoveLooseGameplayTag(TAG_Gameplay_DamageImmunity);
	State->bImmuneAdded = false;
	for (int32 I = 0; I < State->SavedPlayers.Num(); ++I)
		if (ABwayPlayerState* P = State->SavedPlayers[I].Get())
		{
			P->UpgradeComponent->ResetUpgrades();
			if (!ASC(P)->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)) ASC(P)->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), State->SavedHealth[I]);
			ASC(P)->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), State->SavedGold[I]);
			UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Restored player=%d health=%.4f gold=%.4f leech=%.4f owned=%d"), P->GetPlayerId(), Health(P), ASC(P)->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), Leech(P), P->UpgradeComponent->GetOwnedUpgrades().Num());
		}
	for (const auto& Entry : State->Bases) if (ABwayBaseZone* Base = Entry.Key.Get()) Base->HealingPerSecond = Entry.Value;
	if (UBwayRoundManagementComponent* Rounds = State->Rounds.Get()) Rounds->RespawnDelay = State->RespawnDelay;
	State->bCleaned = true;
	UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Cleanup players=%d bases=%d listeners=0 respawnRestored=%.4f"), State->SavedPlayers.Num(), State->Bases.Num(), State->RespawnDelay);
}
void ABwayLeechLimitProbe::EndPlay(const EEndPlayReason::Type Reason)
{
	Cleanup();
	if (State) UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Teardown role=%s finished=%d"), HasAuthority() ? TEXT("host") : TEXT("client"), State->bFinished ? 1 : 0);
	State.Reset(); Super::EndPlay(Reason);
}

#if !UE_BUILD_SHIPPING
namespace
{
TWeakObjectPtr<ABwayLeechLimitProbe> ActiveLimitProbe;
bool bLimitStarting = false;
FAutoConsoleCommand StartLimit(TEXT("bway.Test.LeechLimitsHost"), TEXT("Development four-player life-leech boundary fixture."), FConsoleCommandDelegate::CreateLambda([]
{
	if (bLimitStarting || ActiveLimitProbe.IsValid()) return;
	bLimitStarting = true;
	const double Start = FPlatformTime::Seconds();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Start](float)
	{
		if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld() && World->GetNetMode() != NM_Client && World->GetMapName().Contains(TEXT("L_BW_Dorado")))
			{
				ActiveLimitProbe = World->SpawnActor<ABwayLeechLimitProbe>(); bLimitStarting = false; return false;
			}
		}
		if (FPlatformTime::Seconds() - Start > 90) { bLimitStarting = false; UE_LOG(LogTemp, Error, TEXT("BwayLeechLimits: FAIL role=host error=world-timeout")); return false; }
		return true;
	}), 0.1f);
}));
FAutoConsoleCommand ObserveLimit(TEXT("bway.Test.LeechLimitsClient"), TEXT("Development boundary observer runs on the replicated probe."), FConsoleCommandDelegate::CreateLambda([] { UE_LOG(LogTemp, Display, TEXT("BwayLeechLimits: Await replicated probe")); }));
}
#endif
