#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Containers/Ticker.h"
#include "Economy/BwayBaseZone.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HAL/PlatformTime.h"
#include "LyraGameplayTags.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "UObject/UnrealType.h"

namespace BwayAllKitLeechTest
{
const TCHAR* Heroes[] = { TEXT("Argus"), TEXT("Rawlins"), TEXT("Alona"), TEXT("Korryn") };
const TCHAR* PrimaryFamilies[] = { TEXT("BwayGameplayAbility_MeleePrimary"), TEXT("BwayGameplayAbility_RawlinsPrimary"), TEXT("BwayGameplayAbility_AlonaPrimary"), TEXT("BwayGameplayAbility_KorrynPrimary") };
struct FRestore
{
	TWeakObjectPtr<ABwayPlayerState> Player;
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	FTransform Transform;
	FRotator Rotation;
	EMovementMode Movement = MOVE_None;
	uint8 CustomMovement = 0;
	float Health = 0, Gold = 0;
	TArray<FActiveGameplayEffectHandle> Effects;
};
struct FProbe
{
	bool bRemoteArgus = false;
	bool bClient = false, bConnected = false, bSelected = false, bSetup = false, bPurchased = false;
	bool bBaseline = false, bActivated = false, bAfter = false, bPlanned = false;
	int32 Index = 0, Source = INDEX_NONE, Victim = INDEX_NONE, ExpectedHits = 0, DamageEvents = 0, HealEvents = 0;
	float SourceBefore = 0, VictimBefore = 0, Bonus = 0, ExpectedDamage = 0;
	double Started = FPlatformTime::Seconds(), StageStarted = 0, BaselineAt = 0, AfterAt = 0, LastDamageAt = 0;
	double Damage = 0, Healing = 0;
	FString HealthKey;
	TArray<FRestore> Restore;
	TMap<TWeakObjectPtr<ABwayBaseZone>, float> Bases;
	FGameplayMessageListenerHandle DamageListener, HealListener;
	TWeakObjectPtr<APlayerController> LocalController;
	FRotator LocalRotation;
};
TSharedPtr<FProbe> Active;

UWorld* FindWorld(bool bClient)
{
	if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && World->IsGameWorld() && (World->GetNetMode() == NM_Client) == bClient) return World;
	}
	return nullptr;
}
float Health(const ABwayPlayerState* Player) { return Player->GetLyraAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()); }
float MaxHealth(const ABwayPlayerState* Player) { return Player->GetLyraAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()); }
int32 Id(UObject* Object)
{
	const APlayerState* Player = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Object);
	return Player ? Player->GetPlayerId() : INDEX_NONE;
}
FGameplayAbilitySpec* Primary(ULyraAbilitySystemComponent* ASC, int32 Index)
{
	if (!ASC || Index < 0 || Index >= 4) return nullptr;
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		if (Spec.Ability) for (const UClass* Class = Spec.Ability->GetClass(); Class; Class = Class->GetSuperClass())
			if (Class->GetName() == PrimaryFamilies[Index]) return &Spec;
	return nullptr;
}
bool Finish(const TSharedPtr<FProbe>& Probe, const TCHAR* Error = nullptr)
{
	Probe->DamageListener.Unregister(); Probe->HealListener.Unregister();
	int32 Restored = 0, Bases = 0;
	if (!Probe->bClient)
	{
		for (const FRestore& Saved : Probe->Restore)
		{
			ABwayPlayerState* Player = Saved.Player.Get();
			ABwayCharacterWithAbilities* Pawn = Saved.Pawn.Get();
			if (!Player || !Pawn) continue;
			ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
			Player->UpgradeComponent->ResetUpgrades();
			// The disposable fixture owns these new effects, including projectile armor shred.
			for (FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
				if (!Saved.Effects.Contains(Handle)) ASC->RemoveActiveGameplayEffect(Handle);
			ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Saved.Health);
			ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), Saved.Gold);
			Pawn->SetActorTransform(Saved.Transform, false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->GetCharacterMovement()->StopMovementImmediately();
			Pawn->GetCharacterMovement()->SetMovementMode(Saved.Movement, Saved.CustomMovement);
			if (APlayerController* PC = Player->GetPlayerController()) { PC->SetControlRotation(Saved.Rotation); PC->ClientSetRotation(Saved.Rotation, true); }
			++Restored;
		}
		for (const auto& Saved : Probe->Bases) if (ABwayBaseZone* Base = Saved.Key.Get()) { Base->HealingPerSecond = Saved.Value; ++Bases; }
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Cleanup players=%d bases=%d listeners=0"), Restored, Bases);
	}
	if (APlayerController* PC = Probe->LocalController.Get()) PC->SetControlRotation(Probe->LocalRotation);
	if (Error) { UE_LOG(LogTemp, Error, TEXT("BwayAllKitLeech: FAIL role=%s index=%d error=%s damageEvents=%d healEvents=%d damage=%.4f healing=%.4f"), Probe->bClient ? TEXT("client") : TEXT("host"), Probe->Index, Error, Probe->DamageEvents, Probe->HealEvents, Probe->Damage, Probe->Healing); }
	else { UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: %s PASS kits=4"), Probe->bClient ? TEXT("Client") : TEXT("Host")); }
	Active.Reset();
	return false;
}
void LogPeers(const FProbe& Probe, const TArray<ABwayPlayerState*>& Players, int32 Observer, const TCHAR* Stage)
{
	for (const ABwayPlayerState* Player : Players)
	{
		const UBwayHeroAttributeSet* Hero = Player->GetLyraAbilitySystemComponent()->GetSet<UBwayHeroAttributeSet>();
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Peer index=%d hero=%s stage=%s observer=%d player=%d source=%d victim=%d health=%.4f maxHealth=%.4f leech=%.4f armor=%.4f incoming=%.4f team=%d rank=%d owned=%d"),
			Probe.Index, Heroes[Probe.Index], Stage, Observer, Player->GetPlayerId(), Probe.Source, Probe.Victim, Health(Player), MaxHealth(Player), Hero->GetLifeLeech(), Hero->GetArmor(), Hero->GetIncomingDamageMultiplier(), Player->GetTeamId(), Player->UpgradeComponent->GetOwnedRank(TEXT("LifeLeech")), Player->UpgradeComponent->GetOwnedUpgrades().Num());
	}
}
bool ReadFloat(const UObject* Object, const TCHAR* Name, float& Value)
{
	const FFloatProperty* Property = Object ? FindFProperty<FFloatProperty>(Object->GetClass(), Name) : nullptr;
	if (!Property) return false;
	Value = Property->GetPropertyValue_InContainer(Object);
	return FMath::IsFinite(Value);
}
bool BeginAttack(FProbe& Probe, const TArray<ABwayPlayerState*>& Players, ABwayPlayerState* Source, ABwayPlayerState* Victim)
{
	if (Probe.Index == 0)
	{
		// Opening the purchase window leads through production round reset, which
		// replaces the pawns and their granted effects. Snapshot the live avatars
		// before moving them; retain the pre-funding health/wallet restoration.
		for (FRestore& Saved : Probe.Restore)
		{
			ABwayPlayerState* Player = Saved.Player.Get();
			ABwayCharacterWithAbilities* Pawn = Player ? Cast<ABwayCharacterWithAbilities>(Player->GetPawn()) : nullptr;
			if (!Pawn || !Player->GetPlayerController()) return false;
			Saved.Pawn = Pawn; Saved.Transform = Pawn->GetActorTransform();
			Saved.Rotation = Player->GetPlayerController()->GetControlRotation();
			Saved.Movement = Pawn->GetCharacterMovement()->MovementMode;
			Saved.CustomMovement = Pawn->GetCharacterMovement()->CustomMovementMode;
			Saved.Effects = Player->GetLyraAbilitySystemComponent()->GetActiveEffects(FGameplayEffectQuery());
		}
	}
	ULyraAbilitySystemComponent* ASC = Source->GetLyraAbilitySystemComponent();
	FGameplayAbilitySpec* Spec = Primary(ASC, Probe.Index);
	const FSoftObjectProperty* Property = Spec && Spec->Ability ? FindFProperty<FSoftObjectProperty>(Spec->Ability->GetClass(), TEXT("KitConfig")) : nullptr;
	UObject* Config = Property ? Property->GetPropertyValue_InContainer(Spec->Ability).LoadSynchronous() : nullptr;
	float Base = 0, Scaling = 0;
	if (!ReadFloat(Config, TEXT("PrimaryBaseDamage"), Base) || !ReadFloat(Config, TEXT("PrimaryDamageScaling"), Scaling)) return false;
	const UBwayHeroAttributeSet* SourceHero = ASC->GetSet<UBwayHeroAttributeSet>();
	const UBwayHeroAttributeSet* TargetHero = Victim->GetLyraAbilitySystemComponent()->GetSet<UBwayHeroAttributeSet>();
	const FIntProperty* Shots = FindFProperty<FIntProperty>(Config->GetClass(), TEXT("PrimaryShotCount"));
	Probe.ExpectedHits = Shots ? Shots->GetPropertyValue_InContainer(Config) : 1;
	const float Raw = Base + Scaling * SourceHero->GetAttackStrength();
	Probe.ExpectedDamage = FMath::Max(0.0f, Raw * (1.0f - TargetHero->GetArmor() * 0.01f) * TargetHero->GetIncomingDamageMultiplier()) * Probe.ExpectedHits;
	if (Probe.ExpectedHits < 1 || Probe.ExpectedHits > 4 || Probe.ExpectedDamage <= 0 || Probe.ExpectedDamage >= Probe.VictimBefore - 1) return false;
	const FVector Anchor = Probe.Restore[0].Transform.GetLocation();
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		ABwayPlayerState* Player = Players[Index];
		ABwayCharacterWithAbilities* Pawn = CastChecked<ABwayCharacterWithAbilities>(Player->GetPawn());
		Pawn->GetCharacterMovement()->StopMovementImmediately();
		Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
		const FVector Location = Player == Source ? Anchor : (Player == Victim ? Anchor + FVector(180, 0, 0) : Anchor + FVector(0, 2000 + 300 * Index, 0));
		Pawn->SetActorLocationAndRotation(Location, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		if (APlayerController* PC = Player->GetPlayerController()) { PC->SetControlRotation(FRotator::ZeroRotator); PC->ClientSetRotation(FRotator::ZeroRotator, true); }
		Player->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Player == Source ? Probe.SourceBefore : (Player == Victim ? Probe.VictimBefore : MaxHealth(Player)));
	}
	Probe.Damage = 0; Probe.Healing = 0; Probe.DamageEvents = 0; Probe.HealEvents = 0;
	Probe.StageStarted = FPlatformTime::Seconds(); Probe.LastDamageAt = 0;
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Plan index=%d hero=%s source=%d victim=%d ability=%s config=%s base=%.4f scaling=%.4f attack=%.4f raw=%.4f hits=%d armor=%.4f incoming=%.4f expectedDamage=%.4f sourceBefore=%.4f victimBefore=%.4f bonus=%.4f"),
		Probe.Index, Heroes[Probe.Index], Probe.Source, Probe.Victim, *Spec->Ability->GetClass()->GetPathName(), *Config->GetPathName(), Base, Scaling, SourceHero->GetAttackStrength(), Raw, Probe.ExpectedHits, TargetHero->GetArmor(), TargetHero->GetIncomingDamageMultiplier(), Probe.ExpectedDamage, Probe.SourceBefore, Probe.VictimBefore, Probe.Bonus);
	return true;
}
bool Tick(const TSharedPtr<FProbe>& Probe)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - Probe->Started > 180) return Finish(Probe, TEXT("overall-timeout"));
	UWorld* World = FindWorld(Probe->bClient);
	ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ABwayPlayerState* Local = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (!GS || !Local) return true;
	if (!Probe->bConnected) { Probe->bConnected = true; UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Connected role=%s"), Probe->bClient ? TEXT("client") : TEXT("host")); }
	if (World->GetMapName().Contains(TEXT("HeroSelect_Staging")))
	{
		if (Probe->bClient || Probe->bSelected || GS->PlayerArray.Num() != 4 || !GS->HeroSelectionManager || !GS->HeroSelectionManager->IsSelectionActive()) return true;
		TArray<ABwayPlayerState*> Players;
		for (APlayerState* State : GS->PlayerArray) { ABwayPlayerState* Player = Cast<ABwayPlayerState>(State); if (!Player || Player->IsABot()) return Finish(Probe, TEXT("invalid-selection-roster")); Players.Add(Player); }
		Players.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
		if (Probe->bRemoteArgus)
		{
			if (World->GetNetMode() != NM_ListenServer || !PC->IsLocalController() || !Players.Contains(Local)) return Finish(Probe, TEXT("remote-argus-requires-local-listen-host"));
			const int32 RemoteIndex = Players.IndexOfByPredicate([Local](const ABwayPlayerState* Player) { return Player != Local; });
			if (RemoteIndex == INDEX_NONE) return Finish(Probe, TEXT("remote-argus-player-missing"));
			if (Players[0] == Local) Players.Swap(0, RemoteIndex);
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: RoleOrder variant=remote-argus host=%d argus=%d argusRemote=1 order=%d,%d,%d,%d"),
				Local->GetPlayerId(), Players[0]->GetPlayerId(), Players[0]->GetPlayerId(), Players[1]->GetPlayerId(), Players[2]->GetPlayerId(), Players[3]->GetPlayerId());
		}
		Probe->bSelected = true;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FPrimaryAssetId Hero = UBwayHeroRegistry::ResolveHeroIdByName(Heroes[Index]);
			if (!Hero.IsValid() || !GS->HeroSelectionManager->AssignHeroToPlayer(Players[Index], Hero, true) || Players[Index]->GetSelectedHeroId() != Hero) return Finish(Probe, TEXT("hero-selection-rejected"));
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Selected index=%d hero=%s player=%d asset=%s"), Index, Heroes[Index], Players[Index]->GetPlayerId(), *Hero.ToString());
		}
		return true;
	}
	if (!World->GetMapName().Contains(TEXT("L_BW_Dorado")) || GS->PlayerArray.Num() != 4) return true;
	UBwayRoundManagementComponent* Rounds = GS->GetRoundManagement();
	const UBwayUpgradeCatalog* Catalog = Local->UpgradeComponent ? Local->UpgradeComponent->GetCatalog() : nullptr;
	const FBwayUpgradeDefinition* Leech = Catalog ? Catalog->FindUpgrade(TEXT("LifeLeech")) : nullptr;
	if (!Rounds || !Leech || Leech->Ranks.IsEmpty()) return true;
	Probe->Bonus = Leech->Ranks[0].Magnitude;
	TArray<ABwayPlayerState*> Players; Players.SetNumZeroed(4);
	for (APlayerState* State : GS->PlayerArray)
	{
		ABwayPlayerState* Player = Cast<ABwayPlayerState>(State);
		ULyraAbilitySystemComponent* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
		if (!Player || !Cast<ABwayCharacterWithAbilities>(Player->GetPawn()) || !ASC || ASC->GetAvatarActor() != Player->GetPawn() || !ASC->GetSet<UBwayHeroAttributeSet>() || !ASC->GetSet<ULyraHealthSet>() || !Player->UpgradeComponent || Health(Player) <= 0 || GS->GetPlayerTeam(Player) == INDEX_NONE) return true;
		for (int32 Index = 0; Index < 4; ++Index) if (Player->GetSelectedHeroId() == UBwayHeroRegistry::ResolveHeroIdByName(Heroes[Index])) { if (Players[Index]) return Finish(Probe, TEXT("duplicate-hero")); Players[Index] = Player; }
	}
	if (Players.Contains(nullptr)) return true;
	if (!Probe->LocalController.IsValid()) { Probe->LocalController = PC; Probe->LocalRotation = PC->GetControlRotation(); }
	const bool bPlaying = Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing;
	if (!Probe->bClient && !Probe->bSetup)
	{
		if (!bPlaying) return true;
		for (ABwayPlayerState* Player : Players)
		{
			if (!Player->UpgradeComponent->GetOwnedUpgrades().IsEmpty()) return Finish(Probe, TEXT("unexpected-owned-upgrades"));
			ABwayCharacterWithAbilities* Pawn = CastChecked<ABwayCharacterWithAbilities>(Player->GetPawn());
			ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
			FRestore& Saved = Probe->Restore.AddDefaulted_GetRef(); Saved.Player = Player; Saved.Pawn = Pawn; Saved.Transform = Pawn->GetActorTransform();
			Saved.Rotation = Player->GetPlayerController()->GetControlRotation(); Saved.Movement = Pawn->GetCharacterMovement()->MovementMode; Saved.CustomMovement = Pawn->GetCharacterMovement()->CustomMovementMode;
			Saved.Health = Health(Player); Saved.Gold = ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()); Saved.Effects = ASC->GetActiveEffects(FGameplayEffectQuery());
			ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), Leech->Ranks[0].Cost + 100.0f);
		}
		for (TActorIterator<ABwayBaseZone> It(World); It; ++It) { Probe->Bases.Add(*It, It->HealingPerSecond); It->HealingPerSecond = 0; }
		TWeakPtr<FProbe> Weak = Probe;
		Probe->DamageListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(TAG_Lyra_Damage_Message, [Weak](FGameplayTag, const FLyraVerbMessage& Message)
		{
			const TSharedPtr<FProbe> P = Weak.Pin();
			if (!P || !P->bPlanned || P->Index >= 4 || Id(Message.Target) != P->Victim) return;
			P->DamageEvents++; P->Damage += Message.Magnitude; P->LastDamageAt = FPlatformTime::Seconds();
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Damage index=%d hero=%s source=%d victim=%d amount=%.4f"), P->Index, Heroes[P->Index], Id(Message.Instigator), Id(Message.Target), Message.Magnitude);
		});
		Probe->HealListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(TAG_Lyra_Heal_Message, [Weak](FGameplayTag, const FLyraVerbMessage& Message)
		{
			const TSharedPtr<FProbe> P = Weak.Pin();
			if (!P || !P->bPlanned || P->Index >= 4 || Id(Message.Target) != P->Source) return;
			P->HealEvents++; P->Healing += Message.Magnitude;
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Heal index=%d hero=%s source=%d target=%d amount=%.4f"), P->Index, Heroes[P->Index], Id(Message.Instigator), Id(Message.Target), Message.Magnitude);
		});
		Probe->bSetup = true;
		Rounds->EndRound(0, EBwayWinCondition::GoalScored);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Setup players=4 bases=%d"), Probe->Bases.Num());
		return true;
	}
	if (!Probe->bClient && !Probe->bPurchased)
	{
		if (!Local->UpgradeComponent->IsPurchaseWindowOpen()) return true;
		for (ABwayPlayerState* Player : Players) { Player->UpgradeComponent->ServerPurchaseUpgrade(TEXT("LifeLeech"), 0); if (Player->UpgradeComponent->GetOwnedRank(TEXT("LifeLeech")) != 1) return Finish(Probe, TEXT("purchase-failed")); }
		Probe->bPurchased = true;
		return true;
	}
	if (!bPlaying) return true;
	for (ABwayPlayerState* Player : Players)
	{
		const int32 Rank = !Probe->bClient || Player == Local ? 1 : 0;
		if (Player->UpgradeComponent->GetOwnedRank(TEXT("LifeLeech")) != Rank || !FMath::IsNearlyEqual(Player->GetLyraAbilitySystemComponent()->GetNumericAttribute(UBwayHeroAttributeSet::GetLifeLeechAttribute()), Probe->Bonus, 0.001f)) return true;
	}
	if (Probe->Index >= 4) return Probe->bClient || Now - Probe->StageStarted >= 8 ? Finish(Probe) : true;
	ABwayPlayerState* Source = Players[Probe->Index];
	ABwayPlayerState* Victim = nullptr;
	for (ABwayPlayerState* Player : Players) if (GS->GetPlayerTeam(Player) != GS->GetPlayerTeam(Source)) { Victim = Player; break; }
	if (!Victim) return Finish(Probe, TEXT("no-opposing-target"));
	Probe->Source = Source->GetPlayerId(); Probe->Victim = Victim->GetPlayerId();
	Probe->SourceBefore = MaxHealth(Source) * 0.4f; Probe->VictimBefore = MaxHealth(Victim) * 0.8f;
	if (!Probe->bClient && !Probe->bPlanned)
	{
		if (!BeginAttack(*Probe, Players, Source, Victim)) return Finish(Probe, TEXT("invalid-primary-plan"));
		Probe->bPlanned = true;
	}
	bool bBaseline = FMath::IsNearlyEqual(Health(Source), Probe->SourceBefore, 0.02f) && FMath::IsNearlyEqual(Health(Victim), Probe->VictimBefore, 0.02f);
	if (!Probe->bBaseline && bBaseline)
	{
		if (!Probe->BaselineAt) Probe->BaselineAt = Now;
		if (Now - Probe->BaselineAt >= 1)
		{
			LogPeers(*Probe, Players, Local->GetPlayerId(), TEXT("Baseline")); Probe->bBaseline = true;
		}
	}
	else if (!Probe->bBaseline) Probe->BaselineAt = 0;
	if (Local == Source && Probe->bBaseline && !Probe->bActivated && Now - Probe->BaselineAt >= 3)
	{
		PC->SetControlRotation(FRotator::ZeroRotator);
		FGameplayAbilitySpec* Spec = Primary(Local->GetLyraAbilitySystemComponent(), Probe->Index);
		if (!Spec) return Finish(Probe, TEXT("missing-granted-primary"));
		const FString Class = Spec->Ability->GetClass()->GetPathName();
		Probe->bActivated = true;
		const bool bActivated = Local->GetLyraAbilitySystemComponent()->TryActivateAbility(Spec->Handle, true);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Activate index=%d hero=%s owner=%d client=%d ability=%s activated=%d"), Probe->Index, Heroes[Probe->Index], Local->GetPlayerId(), Probe->bClient ? 1 : 0, *Class, bActivated);
		if (!bActivated) return Finish(Probe, TEXT("activation-rejected"));
	}
	const float Damage = Probe->VictimBefore - Health(Victim), Healing = Health(Source) - Probe->SourceBefore;
	const bool bHealed = Probe->bBaseline && Damage > 0.1f && FMath::IsNearlyEqual(Healing, Damage * Probe->Bonus, 0.02f);
	const FString Key = FString::Printf(TEXT("%.4f:%.4f"), Health(Source), Health(Victim));
	if (!bHealed || Key != Probe->HealthKey) { Probe->HealthKey = Key; Probe->AfterAt = bHealed ? Now : 0; }
	if (!Probe->bAfter && bHealed && Probe->AfterAt && Now - Probe->AfterAt >= (Probe->bClient ? 2 : 8))
	{
		if (!Probe->bClient || Local == Source)
		{
			const FGameplayAbilitySpec* Spec = Primary(Source->GetLyraAbilitySystemComponent(), Probe->Index);
			if (!Spec || Spec->IsActive()) return Finish(Probe, TEXT("primary-did-not-end"));
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Ended index=%d hero=%s owner=%d client=%d active=0"), Probe->Index, Heroes[Probe->Index], Source->GetPlayerId(), Probe->bClient ? 1 : 0);
		}
		if (!Probe->bClient)
		{
			if (Probe->DamageEvents != Probe->ExpectedHits || Probe->HealEvents != Probe->ExpectedHits || !FMath::IsNearlyEqual(Probe->Damage, static_cast<double>(Probe->ExpectedDamage), 0.02) || !FMath::IsNearlyEqual(Probe->Healing, static_cast<double>(Damage * Probe->Bonus), 0.02)) return Finish(Probe, TEXT("damage-healing-events-mismatch"));
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitLeech: Result index=%d hero=%s source=%d victim=%d hits=%d heals=%d damage=%.4f healing=%.4f sourceAfter=%.4f victimAfter=%.4f"), Probe->Index, Heroes[Probe->Index], Probe->Source, Probe->Victim, Probe->DamageEvents, Probe->HealEvents, Probe->Damage, Probe->Healing, Health(Source), Health(Victim));
		}
		LogPeers(*Probe, Players, Local->GetPlayerId(), TEXT("AfterDamage"));
		Probe->bAfter = true;
		++Probe->Index; Probe->bBaseline = false; Probe->bActivated = false; Probe->bAfter = false; Probe->bPlanned = false;
		Probe->BaselineAt = 0; Probe->AfterAt = 0; Probe->StageStarted = Now; Probe->HealthKey.Reset();
	}
	if (!Probe->bClient && Probe->bPlanned && Now - Probe->StageStarted > 20) return Finish(Probe, TEXT("primary-damage-timeout"));
	return true;
}
void Start(bool bClient, bool bRemoteArgus = false)
{
	if (Active) return;
	Active = MakeShared<FProbe>(); Active->bClient = bClient; Active->bRemoteArgus = bRemoteArgus;
	const TSharedPtr<FProbe> Probe = Active;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float) { return Tick(Probe); }), 0.1f);
}
FAutoConsoleCommand Host(TEXT("bway.Test.AllKitLeechHost"), TEXT("Development mixed-roster primary activation and life-leech fixture; launch in hero-selection staging."), FConsoleCommandDelegate::CreateLambda([] { Start(false); }));
FAutoConsoleCommand HostRemoteArgus(TEXT("bway.Test.AllKitLeechHostRemoteArgus"), TEXT("Development mixed-roster life-leech fixture with a remote Argus owner; launch listen host in hero-selection staging."), FConsoleCommandDelegate::CreateLambda([] { Start(false, true); }));
FAutoConsoleCommand Client(TEXT("bway.Test.AllKitLeechClient"), TEXT("Development owning-client primary activation and all-peer life-leech observer."), FConsoleCommandDelegate::CreateLambda([] { Start(true); }));
}
#endif
