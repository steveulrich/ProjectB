// Opt-in, four-world PIE evidence for the remaining persistent-actor policy boundary.
// No gameplay tuning, grant, cooldown, actor lifetime or replication setting is changed.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayHeroProjectileBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "LyraGameplayTags.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/ObjectKey.h"
#include "UObject/UnrealType.h"

namespace BwayAllKitPersistentPIE
{
// Class-name selection avoids making BreakawayCore depend on its hero feature modules.
const TCHAR* const AbilityFamilies[] = { TEXT("BwayGameplayAbility_AlonaBlessing"), TEXT("BwayGameplayAbility_KorrynCircleOfSpite"), TEXT("BwayGameplayAbility_KorrynPrimary") };
const TCHAR* const ActorFamilies[] = { TEXT("BwayAlonaBlessingZone"), TEXT("BwayKorrynSpiteZone"), TEXT("BwayKorrynPrimaryProjectile") };
bool Family(const UClass* Class, const TCHAR* Name)
{
	for (; Class; Class = Class->GetSuperClass()) if (Class->GetFName() == FName(Name)) return true;
	return false;
}
FString Identity(const UObject* Object)
{
	FObjectKey Key(Object); TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); Writer << Key;
	return BytesToHex(Bytes.GetData(), Bytes.Num());
}
bool Alive(const AActor* A) { return IsValid(A) && !A->IsActorBeingDestroyed(); }
ABwayCharacterWithAbilities* Pawn(ABwayPlayerState* P) { return P ? Cast<ABwayCharacterWithAbilities>(P->GetPawn()) : nullptr; }
ULyraAbilitySystemComponent* ASC(ABwayPlayerState* P) { return P ? P->GetLyraAbilitySystemComponent() : nullptr; }
float Value(UAbilitySystemComponent* A, const FGameplayAttribute& Attribute) { return A ? A->GetNumericAttribute(Attribute) : -1.f; }
float Health(ABwayPlayerState* P) { return Value(ASC(P), ULyraHealthSet::GetHealthAttribute()); }
uint64 Guid(const AActor* A)
{
	const UNetDriver* D = A && A->GetWorld() ? A->GetWorld()->GetNetDriver() : nullptr;
	return D && D->GuidCache ? D->GuidCache->GetNetGUID(A).ObjectId : 0;
}
FGameplayAbilitySpec* Spec(ULyraAbilitySystemComponent* A, int32 Kind)
{
	FGameplayAbilitySpec* Found = nullptr;
	if (A) for (auto& S : A->GetActivatableAbilities()) if (S.Ability && Family(S.Ability->GetClass(), AbilityFamilies[Kind]))
	{ if (Found) return nullptr; Found = &S; }
	return Found;
}
float ConfigFloat(const FGameplayAbilitySpec* S, const TCHAR* Name)
{
	const auto* P = S ? FindFProperty<FSoftObjectProperty>(S->Ability->GetClass(), TEXT("KitConfig")) : nullptr;
	const UObject* C = P ? P->GetPropertyValue_InContainer(S->Ability.Get()).LoadSynchronous() : nullptr;
	const auto* F = C ? FindFProperty<FFloatProperty>(C->GetClass(), Name) : nullptr;
	return F ? F->GetPropertyValue_InContainer(C) : -1.f;
}
bool Clear(ULyraAbilitySystemComponent* A)
{
	return A && !A->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity) && !A->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)
		&& !A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal) && !A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced)
		&& !A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed) && !A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_ArmorShred);
}
struct FPerson
{
	int32 Id = INDEX_NONE, Team = INDEX_NONE;
	TWeakObjectPtr<ABwayPlayerState> PS;
	TWeakObjectPtr<ULyraAbilitySystemComponent> Component;
	TWeakObjectPtr<ABwayCharacterWithAbilities> OldPawn;
	TWeakObjectPtr<AController> Controller;
	FString OldPath, OldKey, NewKey;
	FTransform Transform;
	FVector Velocity, Pending, Desired;
	EMovementMode Mode = MOVE_None;
	uint8 Custom = 0;
	float InitialHealth = 0, HealthBase = 0, LeechBase = 0, Slow = 0, Amp = 0, Armor = 0;
	bool bHealthInput = false, bLeechInput = false;
	FDelegateHandle Applied;
};
struct FPeer { TWeakObjectPtr<UWorld> World; TArray<FPerson> People; TWeakObjectPtr<AActor> Floor; };
struct FControl { TWeakObjectPtr<APlayerController> PC; FRotator Aim; bool Move = false, Look = false; };
struct FEndpoint
{
	int32 Peer = INDEX_NONE, Activations = 0, Ends = 0;
	FGameplayAbilitySpecHandle Handle;
	FPredictionKey Prediction;
	TWeakObjectPtr<UGameplayAbility> Instance;
	FDelegateHandle Activated, Ended;
};
struct FAction
{
	int32 Kind = INDEX_NONE, Source = INDEX_NONE, Target = INDEX_NONE, Owner = INDEX_NONE;
	FEndpoint Endpoints[2]; // authority, actual remote autonomous owner
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UClass> ActorClass;
	TArray<TWeakObjectPtr<AActor>> Replicas;
	TArray<FString> ReplicaKeys;
	FString ActorPath, ActorKey;
	uint64 NetGuid = 0;
	int32 Spawns = 0, Applied = 0, PostDeathHeals = 0, AfterTeardownHeals = 0, DamageEffects = 0, DestroyEvents = 0;
	float Duration = 0, Offset = 0, Radius = 0, HealTick = 0, InitialHeal = 0, Slow = 0, Amp = 0;
	float RawDamage = 0, ExpectedDamage = 0, Shred = 0, TargetBefore = 0, TargetAfter = 0;
	float SpawnAt = 0, SpawnDelta = 0, MaxWorldDelta = 0, Deadline = 0, DestroyedAt = 0, LastAliveAt = 0;
	float LastHealAt = -1, LastHealHealth = 0;
	int32 SourceDamageBefore = 0;
	bool bDispatched = false, bCaptured = false, bReplicated = false, bPeersCaptured = false, bPreExpiry = false, bEffectObserved = false;
	TArray<FActiveGameplayEffectHandle> TargetEffects;
};
struct FRequest
{
	FString Token, Failure;
	int32 Case = INDEX_NONE, RequestedSource = INDEX_NONE, AlonaId = INDEX_NONE, KorrynId = INDEX_NONE, Authority = INDEX_NONE, Stage = 0, CastIndex = 0, DeadId = INDEX_NONE, RoundNumber = 0;
	TArray<FPeer> Peers;
	TArray<FControl> Controls;
	TArray<FAction> Actions;
	TWeakObjectPtr<UBwayRoundManagementComponent> Round;
	FDelegateHandle Spawned, Destroyed;
	FTSTicker::FDelegateHandle Ticker;
	double Started = 0, StageAt = 0, StableAt = 0, CleanupAt = 0;
	float InterventionAt = 0;
	bool bIntervened = false, bEntryPlaced = false, bReplacements = false, bCleaning = false, bAbort = false, bCleanupInitial = false;
};
TSharedPtr<FRequest> Active;
void Fail(FRequest& R, const TCHAR* Why) { if (R.Failure.IsEmpty()) { R.Failure = Why; UE_LOG(LogTemp, Error, TEXT("BwayAllKitPersistent: Failure token=%s stage=%d reason=%s"), *R.Token, R.Stage, Why); } }
void Stage(FRequest& R, int32 Number) { R.Stage = Number; R.StageAt = FPlatformTime::Seconds(); R.StableAt = 0; }
FPerson* Person(FPeer& P, int32 Id) { return P.People.FindByPredicate([Id](const auto& V) { return V.Id == Id; }); }
FPerson& ServerPerson(FRequest& R, int32 Id) { return *Person(R.Peers[R.Authority], Id); }
float Time(FRequest& R) { return R.Peers[R.Authority].World->GetTimeSeconds(); }
bool Live(const FPerson& P)
{
	auto* C = Pawn(P.PS.Get()); auto* A = P.Component.Get();
	return P.PS.IsValid() && P.PS->GetTeamId() == P.Team && Alive(C) && A && ASC(P.PS.Get()) == A && A->GetOwnerActor() == P.PS.Get()
		&& A->GetAvatarActor() == C && C->GetLyraAbilitySystemComponent() == A && Health(P.PS.Get()) > 0
		&& (!P.Controller.IsValid() || (C->GetController() == P.Controller.Get() && P.Controller->GetPawn() == C && P.Controller->PlayerState == P.PS.Get()));
}
void Hold(FRequest& R, APlayerController* PC)
{
	if (!PC || R.Controls.ContainsByPredicate([PC](const auto& C) { return C.PC.Get() == PC; })) return;
	auto& C = R.Controls.AddDefaulted_GetRef(); C.PC = PC; C.Aim = PC->GetControlRotation(); C.Move = !PC->IsMoveInputIgnored(); C.Look = !PC->IsLookInputIgnored();
	if (C.Move) PC->SetIgnoreMoveInput(true); if (C.Look) PC->SetIgnoreLookInput(true);
}
bool Floor(FRequest& R, FPeer& P)
{
	FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient; Params.bDeferConstruction = true;
	Params.Name = FName(*FString::Printf(TEXT("BwayPersistentFloor_%s"), *R.Token)); Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
	auto* A = P.World->SpawnActor<AActor>(Params); if (!A) return false; P.Floor = A; A->SetNetAddressable();
	auto* B = NewObject<UBoxComponent>(A, TEXT("PersistentFloorCollision")); B->SetNetAddressable(); A->SetRootComponent(B);
	B->SetCollisionObjectType(ECC_WorldStatic); B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); B->SetCollisionResponseToAllChannels(ECR_Block);
	B->SetBoxExtent(FVector(12000, 12000, 50)); B->RegisterComponent(); const FTransform T(FRotator::ZeroRotator, FVector(0, 0, 9950)); A->SetActorTransform(T); A->FinishSpawning(T);
	return A->GetActorTransform().Equals(T, .01f);
}
void Place(FRequest& R, int32 Id, FVector XY, float Yaw = 0)
{
	for (auto& Peer : R.Peers) if (auto* P = Person(Peer, Id)) if (auto* C = Pawn(P->PS.Get()))
	{
		P->Desired = FVector(XY.X, XY.Y, 10000 + C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		C->SetActorLocationAndRotation(P->Desired + FVector(0, 0, 2), FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
		auto* M = C->GetCharacterMovement(); M->StopMovementImmediately(); M->PendingLaunchVelocity = FVector::ZeroVector; M->SetMovementMode(MOVE_Walking);
		if (auto* PC = Cast<APlayerController>(C->GetController())) { Hold(R, PC); PC->SetControlRotation(FRotator(0, Yaw, 0)); }
		C->UpdateOverlaps(); C->ForceNetUpdate();
	}
}
bool Placed(FRequest& R)
{
	for (const auto& Peer : R.Peers) for (const auto& P : Peer.People)
		if (!Live(P) || FVector::Dist(Pawn(P.PS.Get())->GetActorLocation(), P.Desired) > 8 || Pawn(P.PS.Get())->GetVelocity().Size() > 5) return false;
	return true;
}
void SetHealthInput(FRequest& R, int32 Id, float V)
{
	auto& P = ServerPerson(R, Id); P.bHealthInput = true; P.Component->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), V); P.PS->ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: FixtureInput token=%s player=%d attribute=Health beforeBase=%.6f value=%.6f"), *R.Token, Id, P.HealthBase, V);
}
void Bind(FRequest& R)
{
	const TWeakPtr<FRequest> Weak = Active;
	for (int32 J = 0; J < R.Actions.Num(); ++J) for (int32 E = 0; E < 2; ++E)
	{
		auto& Action = R.Actions[J]; auto& Endpoint = Action.Endpoints[E]; auto* A = Person(R.Peers[Endpoint.Peer], Action.Source)->Component.Get();
		Endpoint.Activated = A->AbilityActivatedCallbacks.AddLambda([Weak, J, E](UGameplayAbility* Ability)
		{
			auto P = Weak.Pin(); if (!P || P->bCleaning || !Ability) return; auto& A = P->Actions[J]; auto& EP = A.Endpoints[E];
			if (Ability->GetCurrentAbilitySpecHandle() != EP.Handle) return;
			EP.Instance = Ability; EP.Prediction = Ability->GetCurrentActivationInfo().GetActivationPredictionKey(); ++EP.Activations;
			if (!A.bDispatched || EP.Activations != 1 || Ability->GetAvatarActorFromActorInfo() != Person(P->Peers[EP.Peer], A.Source)->OldPawn.Get()) Fail(*P, TEXT("activation-count-or-avatar"));
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Activation token=%s kind=%d peer=%d source=%d handle=%s prediction=%d pawn=%s key=%s"), *P->Token, A.Kind, EP.Peer, A.Source, *EP.Handle.ToString(), EP.Prediction.Current, *GetPathNameSafe(Ability->GetAvatarActorFromActorInfo()), *Identity(Ability->GetAvatarActorFromActorInfo()));
		});
		Endpoint.Ended = A->OnAbilityEnded.AddLambda([Weak, J, E](const FAbilityEndedData& Data)
		{
			auto P = Weak.Pin(); if (!P || P->bCleaning) return; auto& A = P->Actions[J]; auto& EP = A.Endpoints[E]; if (Data.AbilitySpecHandle != EP.Handle) return;
			++EP.Ends; auto* Ability = Data.AbilityThatEnded.Get(); const bool Exact = Ability && Ability == EP.Instance.Get() && Ability->GetCurrentActivationInfo().GetActivationPredictionKey() == EP.Prediction;
			if (!Exact || EP.Ends != 1 || Data.bWasCancelled) Fail(*P, TEXT("cast-did-not-end-naturally"));
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: End token=%s kind=%d peer=%d exact=%d count=%d cancelled=%d"), *P->Token, A.Kind, EP.Peer, Exact, EP.Ends, Data.bWasCancelled);
		});
	}
	R.Spawned = R.Peers[R.Authority].World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([Weak](AActor* Actor)
	{
		auto P = Weak.Pin(); if (!P || P->bCleaning || !Actor) return;
		for (auto& A : P->Actions) if (Family(Actor->GetClass(), ActorFamilies[A.Kind]))
		{
			if (!A.bDispatched || P->bIntervened || Actor->GetOwner() != ServerPerson(*P, A.Source).OldPawn.Get() || ++A.Spawns != 1) { Fail(*P, TEXT("unrelated-or-late-persistent-spawn")); return; }
			A.Actor = Actor; A.ActorClass = Actor->GetClass(); A.ActorPath = Actor->GetPathName(); A.ActorKey = Identity(Actor); A.SpawnAt = Time(*P); A.SpawnDelta = Actor->GetWorld()->GetDeltaSeconds(); A.MaxWorldDelta = A.SpawnDelta;
		}
	}));
	R.Destroyed = R.Peers[R.Authority].World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateLambda([Weak](AActor* Actor)
	{
		auto P = Weak.Pin(); if (!P || P->bCleaning) return;
		for (auto& A : P->Actions) if (A.Actor.Get() == Actor) { ++A.DestroyEvents; A.DestroyedAt = Time(*P); UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Destroyed token=%s kind=%d actor=%s key=%s worldTime=%.6f deadline=%.6f intervened=%d"), *P->Token, A.Kind, *A.ActorPath, *A.ActorKey, A.DestroyedAt, A.Deadline, P->bIntervened); }
	}));
	for (auto& PersonValue : R.Peers[R.Authority].People)
	{
		const int32 Id = PersonValue.Id;
		PersonValue.Applied = PersonValue.Component->OnGameplayEffectAppliedDelegateToSelf.AddLambda([Weak, Id](UAbilitySystemComponent*, const FGameplayEffectSpec& Effect, FActiveGameplayEffectHandle Handle)
		{
			auto P = Weak.Pin(); if (!P || P->bCleaning) return;
			const float Heal = Effect.GetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Heal, false, 0.f);
			const float Damage = Effect.GetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, false, 0.f);
			if (P->bIntervened && Id == P->DeadId && Heal > 0 && Identity(Pawn(ServerPerson(*P, Id).PS.Get())) == ServerPerson(*P, Id).OldKey) Fail(*P, TEXT("healing-effect-applied-to-dead-source"));
			for (auto& A : P->Actions) if (A.Actor.IsValid() && A.Actor.Get() == Effect.GetContext().GetSourceObject() && Id == A.Target)
			{
				const auto& C = Effect.GetContext(); const auto& Source = ServerPerson(*P, A.Source);
				const bool Context = C.GetOriginalInstigator() == Source.PS.Get() && C.GetOriginalInstigatorAbilitySystemComponent() == Source.Component.Get() && C.GetEffectCauser() == A.Actor.Get();
				if (!Context) Fail(*P, TEXT("effect-lost-original-player-attribution")); ++A.Applied;
				if (A.Kind == 0 && Heal > 0)
				{
					if (P->bIntervened && P->Case == 0) { ++A.PostDeathHeals; if (!Alive(Source.OldPawn.Get())) ++A.AfterTeardownHeals; }
					const float At = Time(*P); if (A.bCaptured && At >= A.Deadline + .001f) Fail(*P, TEXT("blessing-healed-at-or-after-original-expiry"));
					if (A.LastHealAt >= 0 && (!FMath::IsNearlyEqual(Heal, A.HealTick, .01f) || At - A.LastHealAt < .9f)) Fail(*P, TEXT("blessing-tick-magnitude-or-duplicate"));
					A.LastHealAt = At; A.LastHealHealth = Health(ServerPerson(*P, Id).PS.Get());
				}
				if (A.Kind != 0 && Handle.IsValid()) A.TargetEffects.AddUnique(Handle);
				if (A.Kind == 2 && Damage > 0)
				{
					++A.DamageEffects; A.TargetAfter = Health(ServerPerson(*P, Id).PS.Get());
					if (P->Case != 2 || !P->bIntervened || Health(Source.PS.Get()) != 0 || !FMath::IsNearlyEqual(Damage, A.ExpectedDamage, .01f)
						|| !FMath::IsNearlyEqual(Value(Source.Component.Get(), UBwayHeroAttributeSet::GetLifeLeechAttribute()), .1f, .001f)) Fail(*P, TEXT("projectile-hit-not-exactly-post-death-with-positive-leech"));
				}
				UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Effect token=%s kind=%d target=%d source=%d context=%d sourcePS=%s sourceASC=%s causer=%s sourceObject=%s heal=%.6f damage=%.6f health=%.6f sourceHealth=%.6f sourceLeech=%.6f oldSourceAlive=%d worldTime=%.6f handle=%s"), *P->Token, A.Kind, Id, A.Source, Context, *GetPathNameSafe(C.GetOriginalInstigator()), *GetPathNameSafe(C.GetOriginalInstigatorAbilitySystemComponent()), *GetPathNameSafe(C.GetEffectCauser()), *GetPathNameSafe(C.GetSourceObject()), Heal, Damage, Health(ServerPerson(*P, Id).PS.Get()), Health(Source.PS.Get()), Value(Source.Component.Get(), UBwayHeroAttributeSet::GetLifeLeechAttribute()), Alive(Source.OldPawn.Get()), Time(*P), *Handle.ToString());
			}
		});
	}
}
bool Setup(FRequest& R)
{
	if (!GEngine) return false;
	for (const FWorldContext& C : GEngine->GetWorldContexts())
	{
		auto* W = C.World(); if (!W || W->WorldType != EWorldType::PIE || (W->GetNetMode() != NM_Client && W->GetNetMode() != NM_ListenServer)) continue;
		auto* GS = W->GetGameState<ABwayGameState>(); auto* Round = GS ? GS->GetRoundManagement() : nullptr;
		if (!W->GetMapName().Contains(TEXT("L_BW_Dorado")) || !GS || GS->PlayerArray.Num() != 4 || !Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundState() != ERoundState::RoundActive) return false;
		const int32 I = R.Peers.AddDefaulted(); auto& Peer = R.Peers[I]; Peer.World = W;
		if (W->GetNetMode() == NM_ListenServer) { if (R.Authority != INDEX_NONE || !W->GetAuthGameMode()) return false; R.Authority = I; R.Round = Round; R.RoundNumber = Round->GetCurrentRoundNumber(); }
		int32 LocalControllers = 0; for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It) if (It->IsValid() && It->Get()->IsLocalController()) ++LocalControllers;
		if (LocalControllers != 1) return false;
		const auto* D = W->GetNetDriver(); if (!D) return false; const auto& N = D->PacketSimulationSettings;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Network token=%s peer=%d world=%s mode=%d localControllers=%d lag=%d lagMin=%d lagMax=%d incomingMin=%d incomingMax=%d loss=%d incomingLoss=%d jitter=%d variance=%d"), *R.Token, I, *W->GetPathName(), int32(W->GetNetMode()), LocalControllers, N.PktLag, N.PktLagMin, N.PktLagMax, N.PktIncomingLagMin, N.PktIncomingLagMax, N.PktLoss, N.PktIncomingLoss, N.PktJitter, N.PktLagVariance);
		if (N.PktLag || N.PktLagMin || N.PktLagMax || N.PktIncomingLagMin || N.PktIncomingLagMax || N.PktLoss || N.PktIncomingLoss || N.PktJitter || N.PktLagVariance) return false;
		for (APlayerState* State : GS->PlayerArray)
		{
			auto* PS = Cast<ABwayPlayerState>(State); auto* A = ASC(PS); auto* PawnValue = Pawn(PS);
			if (!PS || PS->IsABot() || !Alive(PawnValue) || !A || !Clear(A) || !A->GetSet<UBwayHeroAttributeSet>()) return false;
			auto& P = Peer.People.AddDefaulted_GetRef(); P.Id = PS->GetPlayerId(); P.Team = PS->GetTeamId(); P.PS = PS; P.Component = A; P.OldPawn = PawnValue; P.Controller = PawnValue->GetController();
			P.OldPath = PawnValue->GetPathName(); P.OldKey = Identity(PawnValue); P.Transform = PawnValue->GetActorTransform(); P.InitialHealth = Health(PS);
			P.HealthBase = A->GetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute()); P.LeechBase = A->GetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute());
			P.Slow = Value(A, UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()); P.Amp = Value(A, UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute()); P.Armor = Value(A, UBwayHeroAttributeSet::GetArmorAttribute());
			auto* M = PawnValue->GetCharacterMovement(); P.Mode = M->MovementMode; P.Custom = M->CustomMovementMode; P.Velocity = M->Velocity; P.Pending = M->PendingLaunchVelocity;
			if (!Live(P) || P.Mode != MOVE_Walking || !P.Velocity.IsNearlyZero(1) || !P.Pending.IsNearlyZero(.01f) || !FMath::IsNearlyEqual(P.Slow, 1.f) || !FMath::IsNearlyEqual(P.Amp, 1.f)) return false;
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Roster token=%s peer=%d player=%d team=%d pawn=%s key=%s ps=%s asc=%s avatar=%s health=%.6f healthBase=%.6f leechBase=%.6f armor=%.6f slow=%.6f amp=%.6f"), *R.Token, I, P.Id, P.Team, *P.OldPath, *P.OldKey, *PS->GetPathName(), *A->GetPathName(), *GetPathNameSafe(A->GetAvatarActor()), P.InitialHealth, P.HealthBase, P.LeechBase, P.Armor, P.Slow, P.Amp);
		}
		Peer.People.Sort([](const auto& A, const auto& B) { return A.Id < B.Id; });
		for (TActorIterator<AActor> It(W); It; ++It) if (Alive(*It)) for (const TCHAR* Name : ActorFamilies) if (Family(It->GetClass(), Name)) return false;
	}
	if (R.Peers.Num() != 4 || R.Authority == INDEX_NONE) return false;
	for (auto& P : R.Peers[R.Authority].People)
	{
		if (Spec(P.Component.Get(), 0)) { if (R.AlonaId != INDEX_NONE) return false; R.AlonaId = P.Id; }
		if (Spec(P.Component.Get(), 1)) { if (R.KorrynId != INDEX_NONE) return false; R.KorrynId = P.Id; }
	}
	if (R.AlonaId == INDEX_NONE || R.KorrynId == INDEX_NONE || R.AlonaId == R.KorrynId || R.RequestedSource != (R.Case == 0 ? R.AlonaId : R.KorrynId)) return false;
	for (auto& P : R.Peers)
	{
		int32 Team0 = 0, Team1 = 0;
		for (auto& V : P.People)
		{
			const auto* S = Person(R.Peers[R.Authority], V.Id); if (!S || S->Team != V.Team) return false;
			Team0 += V.Team == 0; Team1 += V.Team == 1;
		}
		if (Team0 != 2 || Team1 != 2 || !Person(P, R.AlonaId) || !Person(P, R.KorrynId)) return false;
	}
	for (int32 Kind = 0; Kind < 3; ++Kind) if (R.Case == 3 || R.Case == Kind)
	{
		auto& A = R.Actions.AddDefaulted_GetRef(); A.Kind = Kind; A.Source = Kind == 0 ? R.AlonaId : R.KorrynId;
		auto& Source = ServerPerson(R, A.Source); auto* S = Spec(Source.Component.Get(), Kind); if (!S) return false;
		for (const auto& P : R.Peers[R.Authority].People) if (P.Id != A.Source && (Kind == 0 ? P.Team == Source.Team : P.Team != Source.Team)) { A.Target = P.Id; break; }
		if (A.Target == INDEX_NONE) return false;
		for (int32 I = 0; I < R.Peers.Num(); ++I) if (I != R.Authority && Pawn(Person(R.Peers[I], A.Source)->PS.Get())->IsLocallyControlled()) { if (A.Owner != INDEX_NONE) return false; A.Owner = I; }
		if (A.Owner == INDEX_NONE) return false;
		A.Endpoints[0].Peer = R.Authority; A.Endpoints[1].Peer = A.Owner;
		for (auto& E : A.Endpoints)
		{
			auto* Component = Person(R.Peers[E.Peer], A.Source)->Component.Get(); auto* Granted = Spec(Component, Kind);
			if (!Granted || Granted->Ability->GetClass() != S->Ability->GetClass() || Granted->IsActive() || !Component->AbilityActorInfo.IsValid() || !Granted->Ability->CanActivateAbility(Granted->Handle, Component->AbilityActorInfo.Get())) return false;
			E.Handle = Granted->Handle;
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Endpoint token=%s kind=%d peer=%d source=%d target=%d class=%s handle=%s pawn=%s asc=%s"), *R.Token, Kind, E.Peer, A.Source, A.Target, *Granted->Ability->GetClass()->GetPathName(), *E.Handle.ToString(), *Person(R.Peers[E.Peer], A.Source)->OldPath, *GetPathNameSafe(Component));
		}
		A.Duration = ConfigFloat(S, Kind == 0 ? TEXT("BlessingZoneDuration") : Kind == 1 ? TEXT("CircleDuration") : TEXT("PrimaryProjectileLifeSpan"));
		if (A.Duration < 3 || A.Duration > 10) return false;
		if (Kind < 2)
		{
			A.Offset = ConfigFloat(S, Kind == 0 ? TEXT("BlessingForwardPlacementDistance") : TEXT("CircleSpawnForwardOffset")); A.Radius = ConfigFloat(S, Kind == 0 ? TEXT("BlessingZoneRadius") : TEXT("CircleRadius"));
			if (A.Offset <= 0 || A.Offset > 1000 || A.Radius < 300 || A.Radius > 1500) return false;
			A.HealTick = ConfigFloat(S, TEXT("BlessingHealPerSecond")); A.InitialHeal = ConfigFloat(S, TEXT("BlessingInitialHeal"));
			A.Slow = ConfigFloat(S, TEXT("CircleSlowMultiplier")); A.Amp = ConfigFloat(S, TEXT("CircleIncomingDamageMultiplier"));
			if (Kind == 0 && (A.HealTick <= 0 || A.InitialHeal <= 0)) return false;
			if (Kind == 1 && (A.Slow <= 0 || A.Slow >= 1 || A.Amp <= 1)) return false;
		}
		else
		{
			A.Offset = ConfigFloat(S, TEXT("PrimaryProjectileSpeed")); if (A.Offset < 500 || A.Offset > 6000) return false;
			A.RawDamage = ConfigFloat(S, TEXT("PrimaryBaseDamage")) + ConfigFloat(S, TEXT("PrimaryDamageScaling")) * Value(Source.Component.Get(), UBwayHeroAttributeSet::GetAttackStrengthAttribute());
			A.Shred = ConfigFloat(S, TEXT("ArmorShredPerHit")); if (A.Shred >= 0 || A.Shred < -20) return false;
			auto& Target = ServerPerson(R, A.Target); A.ExpectedDamage = FMath::Max(0.f, A.RawDamage * (1 - Target.Armor * .01f) * Target.Amp);
			if (A.ExpectedDamage <= 0 || A.ExpectedDamage >= Health(Target.PS.Get()) - 1 || (R.Case == 2 && Pawn(Target.PS.Get())->GetCombatContributionCountForAudit() != 0)) return false;
			A.SourceDamageBefore = Source.PS->GetDamageDealt();
			if (R.Case == 2) Target.bHealthInput = true; // Restore this request's actual collision health loss after proof, preserving combat stats.
		}
		A.Replicas.SetNum(4); A.ReplicaKeys.SetNum(4);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Plan token=%s case=%d kind=%d source=%d target=%d owner=%d duration=%.6f offset=%.6f radius=%.6f tick=%.6f initial=%.6f slow=%.6f amp=%.6f rawDamage=%.6f damage=%.6f shred=%.6f"), *R.Token, R.Case, Kind, A.Source, A.Target, A.Owner, A.Duration, A.Offset, A.Radius, A.HealTick, A.InitialHeal, A.Slow, A.Amp, A.RawDamage, A.ExpectedDamage, A.Shred);
	}
	for (auto& P : R.Peers) { if (!Floor(R, P)) return false; for (const auto& V : P.People) Hold(R, Cast<APlayerController>(Pawn(V.PS.Get())->GetController())); }
	Bind(R);
	for (int32 I = 0; I < R.Peers[R.Authority].People.Num(); ++I) Place(R, R.Peers[R.Authority].People[I].Id, FVector(-2000, 2000 + I * 200, 0));
	for (auto& A : R.Actions) if (A.Kind < 2) Place(R, A.Source, FVector(-A.Offset, R.Case == 3 ? (A.Kind == 0 ? -200 : 200) : 0, 0));
	if (R.Case == 0) Place(R, R.Actions[0].Target, FVector(0, 0, 0));
	if (R.Case == 2)
	{
		auto& A = R.Actions[0]; Place(R, A.Source, FVector::ZeroVector); Place(R, A.Target, FVector(A.Offset * 1.5f, 0, 0));
		auto& P = ServerPerson(R, A.Source); P.bLeechInput = true; P.Component->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), .1f); P.PS->ForceNetUpdate();
		if (!FMath::IsNearlyEqual(Value(P.Component.Get(), UBwayHeroAttributeSet::GetLifeLeechAttribute()), .1f, .001f)) return false;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: FixtureInput token=%s player=%d attribute=LifeLeech beforeBase=%.6f value=0.100000"), *R.Token, P.Id, P.LeechBase);
	}
	if (R.Case == 0 || R.Case == 3)
	{
		auto& A = R.Actions[0]; const float Max = Value(ServerPerson(R, A.Target).Component.Get(), ULyraHealthSet::GetMaxHealthAttribute());
		const float Deficit = A.InitialHeal + A.HealTick * (A.Duration + 1); if (Max <= Deficit + 1) return false; SetHealthInput(R, A.Target, Max - Deficit);
	}
	for (auto& A : R.Actions) A.TargetBefore = Health(ServerPerson(R, A.Target).PS.Get());
	Stage(R, 1); return true;
}
bool EndpointsDone(const FAction& A)
{
	return A.Endpoints[0].Activations == 1 && A.Endpoints[1].Activations == 1 && A.Endpoints[0].Ends == 1 && A.Endpoints[1].Ends == 1
		&& A.Endpoints[0].Prediction.Current > 0 && A.Endpoints[0].Prediction == A.Endpoints[1].Prediction;
}
bool CaptureActor(FRequest& R, FAction& A)
{
	if (A.bCaptured) return true;
	if (!Alive(A.Actor.Get())) return false;
	auto* Actor = A.Actor.Get(); auto& Source = ServerPerson(R, A.Source);
	// An activation RPC can spawn before this frame advances World.TimeSeconds;
	// TimerManager starts pending lifespan timers later in that world tick. Capture
	// only after a world advance, preserving that first observed timer deadline.
	A.MaxWorldDelta = FMath::Max(A.MaxWorldDelta, Actor->GetWorld()->GetDeltaSeconds());
	if (Time(R) <= A.SpawnAt) return false;
	const float Remaining = Actor->GetLifeSpan(); A.Deadline = Time(R) + Remaining;
	const float StartSkew = A.Deadline - (A.SpawnAt + A.Duration);
	A.bReplicated = Actor->GetIsReplicated(); A.NetGuid = Guid(Actor);
	if (!A.bReplicated) { Fail(R, TEXT("authored-persistent-actor-not-replicated")); return false; }
	if (Remaining <= 0 || Remaining > A.Duration + .003f || StartSkew < -.003f || StartSkew > A.MaxWorldDelta + .003f || Actor->GetInstigator() != Source.OldPawn.Get()
		|| Actor->GetOwner() != (A.Kind == 0 ? static_cast<AActor*>(Source.PS.Get()) : static_cast<AActor*>(Source.OldPawn.Get())))
	{ Fail(R, TEXT("actor-authored-lifetime-or-ownership")); return false; }
	if (!A.NetGuid) return false;
	A.bCaptured = true; A.LastAliveAt = Time(R);
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Actor token=%s kind=%d source=%d path=%s key=%s class=%s guid=%llu replicated=%d owner=%s instigator=%s spawnAt=%.6f captureTime=%.6f remaining=%.6f worldDelta=%.6f maxWorldDelta=%.6f startSkew=%.6f deadline=%.6f duration=%.6f"),
		*R.Token, A.Kind, A.Source, *A.ActorPath, *A.ActorKey, *Actor->GetClass()->GetPathName(), static_cast<unsigned long long>(A.NetGuid), A.bReplicated, *GetPathNameSafe(Actor->GetOwner()), *GetPathNameSafe(Actor->GetInstigator()), A.SpawnAt, Time(R), Remaining, Actor->GetWorld()->GetDeltaSeconds(), A.MaxWorldDelta, StartSkew, A.Deadline, A.Duration);
	return true;
}
bool PeerActors(FRequest& R, FAction& A, const TCHAR* Phase, bool bLog)
{
	if (!A.bCaptured) return false;
	for (int32 I = 0; I < R.Peers.Num(); ++I)
	{
		AActor* Found = nullptr;
		for (TActorIterator<AActor> It(R.Peers[I].World.Get()); It; ++It) if (Alive(*It) && It->GetClass() == A.ActorClass.Get() && Guid(*It) == A.NetGuid)
		{ if (Found) { Fail(R, TEXT("duplicate-guid-actor")); return false; } Found = *It; }
		if (!Found) return false;
		if (A.Replicas[I].IsValid() && A.Replicas[I].Get() != Found) { Fail(R, TEXT("actor-replaced-during-original-lifetime")); return false; }
		A.Replicas[I] = Found; A.ReplicaKeys[I] = Identity(Found);
		if (bLog) UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: PeerActor token=%s phase=%s kind=%d peer=%d guid=%llu key=%s actor=%s owner=%s source=%d replicated=%d"), *R.Token, Phase, A.Kind, I, static_cast<unsigned long long>(Guid(Found)), *Identity(Found), *Found->GetPathName(), *GetPathNameSafe(Found->GetOwner()), A.Source, A.bReplicated);
	}
	return true;
}
bool CircleEffects(FRequest& R, FAction& A, bool bPresent, bool bLog);
bool ObserveActors(FRequest& R)
{
	for (auto& A : R.Actions)
	{
		if (A.Spawns) A.MaxWorldDelta = FMath::Max(A.MaxWorldDelta, R.Peers[R.Authority].World->GetDeltaSeconds());
		if (!A.bCaptured) continue;
		if (Alive(A.Actor.Get()))
		{
			A.LastAliveAt = Time(R);
			if (!FMath::IsNearlyEqual(Time(R) + A.Actor->GetLifeSpan(), A.Deadline, .03f)) { Fail(R, TEXT("original-expiry-was-restarted-or-changed")); return false; }
		}
		if (R.bIntervened && R.Case < 2)
		{
			if (A.DestroyEvents && A.DestroyedAt < A.Deadline - .03f) { Fail(R, TEXT("zone-removed-before-original-expiry")); return false; }
			const float Remaining = A.Deadline - Time(R);
			if (A.Kind == 1 && A.bEffectObserved && Remaining > .03f)
				for (auto H : A.TargetEffects) if (!ServerPerson(R, A.Target).Component->GetActiveGameplayEffect(H)) { Fail(R, TEXT("circle-effect-removed-before-original-expiry")); return false; }
			if (!A.bPreExpiry && Remaining <= .30f && Remaining > .02f && PeerActors(R, A, TEXT("pre-expiry"), false) && (A.Kind != 1 || CircleEffects(R, A, true, false)))
			{
				A.bPreExpiry = true; PeerActors(R, A, TEXT("pre-expiry"), true);
				UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: PreExpiry token=%s kind=%d worldTime=%.6f deadline=%.6f remaining=%.6f effects=%d"), *R.Token, A.Kind, Time(R), A.Deadline, Remaining, A.Kind == 1 ? A.TargetEffects.Num() : 0);
			}
		}
	}
	return true;
}
bool CircleEffects(FRequest& R, FAction& A, bool bPresent, bool bLog)
{
	auto& T = ServerPerson(R, A.Target); const float Slow = bPresent ? A.Slow : T.Slow; const float Amp = bPresent ? A.Amp : T.Amp;
	if (A.TargetEffects.Num() != 2) return false;
	for (auto Handle : A.TargetEffects)
	{
		const auto* Effect = T.Component->GetActiveGameplayEffect(Handle);
		if (bPresent != (Effect != nullptr)) return false;
		if (Effect && (Effect->Spec.GetContext().GetOriginalInstigator() != ServerPerson(R, A.Source).PS.Get() || Effect->Spec.GetContext().GetSourceObject() != A.Actor.Get())) return false;
		if (bLog) UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: CircleHandle token=%s target=%d asc=%s handle=%s present=%d"), *R.Token, A.Target, *T.Component->GetPathName(), *Handle.ToString(), bPresent);
	}
	for (int32 I = 0; I < R.Peers.Num(); ++I)
	{
		auto* P = Person(R.Peers[I], A.Target); auto* C = P->Component.Get();
		if (!FMath::IsNearlyEqual(Value(C, UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()), Slow, .001f)
			|| !FMath::IsNearlyEqual(Value(C, UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute()), Amp, .001f)
			|| C->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed) != bPresent) return false;
		if (bLog) UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: CircleState token=%s peer=%d target=%d asc=%s present=%d handles=%d slow=%.6f amp=%.6f"), *R.Token, I, A.Target, *GetPathNameSafe(C), bPresent, A.TargetEffects.Num(), Slow, Amp);
	}
	return true;
}
bool HealthSettled(FRequest& R, int32 Id, float Expected)
{
	for (auto& P : R.Peers) if (!FMath::IsNearlyEqual(Health(Person(P, Id)->PS.Get()), Expected, .05f)) return false;
	return true;
}
bool ShredSettled(FRequest& R, FAction& A, bool bPresent, bool bLog)
{
	for (int32 I = 0; I < R.Peers.Num(); ++I)
	{
		auto& P = *Person(R.Peers[I], A.Target); const float Armor = Value(P.Component.Get(), UBwayHeroAttributeSet::GetArmorAttribute());
		const bool Tag = P.Component->HasMatchingGameplayTag(BwayGameplayTags::State_Status_ArmorShred);
		if (!FMath::IsNearlyEqual(Armor, P.Armor + (bPresent ? A.Shred : 0), .001f) || Tag != bPresent) return false;
		if (bLog) UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: ShredState token=%s peer=%d target=%d present=%d armor=%.6f baseline=%.6f delta=%.6f tag=%d"), *R.Token, I, A.Target, bPresent, Armor, P.Armor, A.Shred, Tag);
	}
	return true;
}
bool Replacements(FRequest& R, bool bLog)
{
	for (int32 I = 0; I < R.Peers.Num(); ++I) for (auto& P : R.Peers[I].People)
	{
		const bool Changed = R.Case == 3 || P.Id == R.DeadId; auto* C = Pawn(P.PS.Get());
		if (!Live(P) || (Changed ? Identity(C) == P.OldKey : Identity(C) != P.OldKey) || (Changed && Alive(P.OldPawn.Get()))) return false;
		if (!P.NewKey.IsEmpty() && P.NewKey != Identity(C)) { Fail(R, TEXT("unexpected-third-pawn")); return false; }
		if (bLog)
		{
			P.NewKey = Identity(C);
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Replacement token=%s peer=%d player=%d changed=%d oldPawn=%s oldKey=%s pawn=%s key=%s ps=%s asc=%s avatar=%s health=%.6f"), *R.Token, I, P.Id, Changed, *P.OldPath, *P.OldKey, *C->GetPathName(), *P.NewKey, *P.PS->GetPathName(), *P.Component->GetPathName(), *GetPathNameSafe(P.Component->GetAvatarActor()), Health(P.PS.Get()));
		}
	}
	return true;
}
bool Gone(FRequest& R, bool bLog)
{
	for (const auto& A : R.Actions)
	{
		if (Alive(A.Actor.Get()) || A.DestroyEvents != 1) return false;
		for (int32 I = 0; I < R.Peers.Num(); ++I)
		{
			if (Alive(A.Replicas[I].Get())) return false;
			for (TActorIterator<AActor> It(R.Peers[I].World.Get()); It; ++It) if (Alive(*It) && It->GetClass() == A.ActorClass.Get() && A.NetGuid && Guid(*It) == A.NetGuid) return false;
			if (bLog) UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: PeerGone token=%s kind=%d peer=%d guid=%llu priorKey=%s alive=0 replicated=%d"), *R.Token, A.Kind, I, static_cast<unsigned long long>(A.NetGuid), A.ReplicaKeys[I].IsEmpty() ? TEXT("none") : *A.ReplicaKeys[I], A.bReplicated);
		}
	}
	return true;
}
void Dispatch(FRequest& R, int32 Index)
{
	auto& A = R.Actions[Index]; A.bDispatched = true;
	auto* Component = Person(R.Peers[A.Owner], A.Source)->Component.Get();
	const bool Accepted = Component->TryActivateAbility(A.Endpoints[1].Handle);
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Dispatch token=%s kind=%d source=%d peer=%d accepted=%d guard=%d worldTime=%.6f"), *R.Token, A.Kind, A.Source, A.Owner, Accepted, GAllowActorScriptExecutionInEditor, Time(R));
	if (!Accepted) Fail(R, TEXT("owner-activation-rejected"));
}
void Death(FRequest& R)
{
	auto& A = R.Actions[0]; auto& P = ServerPerson(R, A.Source); auto* C = P.OldPawn.Get(); auto* H = ULyraHealthComponent::FindHealthComponent(C);
	if (!H || !Alive(C) || H->IsDeadOrDying() || !Alive(A.Actor.Get()) || !A.bPeersCaptured) { Fail(R, TEXT("death-precondition")); return; }
	R.DeadId = A.Source; R.bIntervened = true; R.InterventionAt = Time(R);
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Intervention token=%s trigger=death source=%d pawn=%s key=%s actor=%s worldTime=%.6f deadline=%.6f"), *R.Token, A.Source, *P.OldPath, *P.OldKey, *A.ActorPath, R.InterventionAt, A.Deadline);
	H->DamageSelfDestruct();
	const bool Dead = H->IsDeadOrDying() && Health(P.PS.Get()) == 0;
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Death token=%s source=%d pawn=%s key=%s dead=%d health=%.6f actorAlive=%d remaining=%.6f"), *R.Token, A.Source, *P.OldPath, *P.OldKey, Dead, Health(P.PS.Get()), Alive(A.Actor.Get()), A.Actor.IsValid() ? A.Actor->GetLifeSpan() : -1.f);
	if (!Dead || !Alive(A.Actor.Get()) || !FMath::IsNearlyEqual(Time(R) + A.Actor->GetLifeSpan(), A.Deadline, .03f)) Fail(R, TEXT("death-destroyed-or-retimed-persistent-actor"));
	Stage(R, 3);
}
bool Finish(FRequest& R)
{
	if (!R.bCleaning)
	{
		R.bCleaning = true; bool Restored = true;
		// Stop observers first. Failure cleanup cannot turn a failed policy assertion into PASS.
		for (auto& A : R.Actions) for (auto& E : A.Endpoints) if (R.Peers.IsValidIndex(E.Peer)) if (auto* P = Person(R.Peers[E.Peer], A.Source)) if (auto* C = P->Component.Get())
		{
			if (E.Activated.IsValid()) C->AbilityActivatedCallbacks.Remove(E.Activated);
			if (E.Ended.IsValid()) C->OnAbilityEnded.Remove(E.Ended);
			if (const auto* S = C->FindAbilitySpecFromHandle(E.Handle); S && S->IsActive()) C->CancelAbilityHandle(E.Handle);
		}
		if (R.Peers.IsValidIndex(R.Authority))
		{
			auto& Server = R.Peers[R.Authority];
			if (Server.World.IsValid()) { if (R.Spawned.IsValid()) Server.World->RemoveOnActorSpawnedHandler(R.Spawned); if (R.Destroyed.IsValid()) Server.World->RemoveOnActorDestroyedHandler(R.Destroyed); }
			for (auto& P : Server.People) if (auto* C = P.Component.Get())
			{
				if (P.Applied.IsValid()) C->OnGameplayEffectAppliedDelegateToSelf.Remove(P.Applied);
				if (P.bLeechInput) C->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), P.LeechBase);
				if (P.bHealthInput && Live(P)) C->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), P.HealthBase);
				if (P.bHealthInput && !Live(P)) Restored = false;
				if (P.bLeechInput || P.bHealthInput) UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: RestoreInput token=%s player=%d healthChanged=%d healthBase=%.6f expectedHealthBase=%.6f leechChanged=%d leechBase=%.6f expectedLeechBase=%.6f"), *R.Token, P.Id, P.bHealthInput, C->GetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute()), P.HealthBase, P.bLeechInput, C->GetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute()), P.LeechBase);
				if (P.PS.IsValid()) P.PS->ForceNetUpdate(); else Restored = false;
			}
		}
		// Only actors/effects actually created by this request are eligible for failure cleanup.
		for (auto& A : R.Actions)
		{
			if (Alive(A.Actor.Get())) { if (R.Failure.IsEmpty()) Fail(R, TEXT("live-actor-at-success-cleanup")); A.Actor->Destroy(); }
			if (!R.Failure.IsEmpty() && R.Peers.IsValidIndex(R.Authority)) if (auto* P = Person(R.Peers[R.Authority], A.Target)) if (P->Component.IsValid())
				for (auto H : A.TargetEffects) P->Component->RemoveActiveGameplayEffect(H);
		}
		for (auto& Peer : R.Peers) for (auto& P : Peer.People) if (auto* C = Pawn(P.PS.Get()); Alive(C))
		{
			auto* M = C->GetCharacterMovement(); C->SetActorTransform(P.Transform, false, nullptr, ETeleportType::TeleportPhysics);
			M->StopMovementImmediately(); M->SetMovementMode(P.Mode, P.Custom); M->Velocity = P.Velocity; M->PendingLaunchVelocity = P.Pending; C->ForceNetUpdate();
		}
		for (auto& C : R.Controls) if (C.PC.IsValid()) { C.PC->SetControlRotation(C.Aim); if (C.Move) C.PC->SetIgnoreMoveInput(false); if (C.Look) C.PC->SetIgnoreLookInput(false); }
		for (auto& P : R.Peers) if (Alive(P.Floor.Get())) Restored &= P.Floor->Destroy();
		R.bCleanupInitial = Restored; R.CleanupAt = FPlatformTime::Seconds(); return true;
	}
	bool Settled = R.bCleanupInitial;
	for (const auto& Peer : R.Peers)
	{
		Settled &= !Alive(Peer.Floor.Get());
		for (const auto& P : Peer.People)
		{
			auto* C = Pawn(P.PS.Get()); if (!Live(P)) { Settled = false; continue; } auto* M = C->GetCharacterMovement();
			Settled &= C->GetActorLocation().Equals(P.Transform.GetLocation(), 5) && M->Velocity.Equals(P.Velocity, 5) && M->MovementMode == P.Mode && M->CustomMovementMode == P.Custom;
			const auto* S = R.Peers.IsValidIndex(R.Authority) ? Person(R.Peers[R.Authority], P.Id) : nullptr;
			if (S && S->bLeechInput) Settled &= FMath::IsNearlyEqual(P.Component->GetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute()), S->LeechBase, .001f);
			if (S && S->bHealthInput) Settled &= FMath::IsNearlyEqual(P.Component->GetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute()), S->HealthBase, .05f);
		}
	}
	for (const auto& C : R.Controls) if (C.PC.IsValid()) Settled &= C.PC->IsMoveInputIgnored() == !C.Move && C.PC->IsLookInputIgnored() == !C.Look && C.PC->GetControlRotation().Equals(C.Aim, .1f);
	for (const auto& A : R.Actions) { Settled &= !Alive(A.Actor.Get()); for (const auto& P : A.Replicas) Settled &= !Alive(P.Get()); }
	const double Age = FPlatformTime::Seconds() - R.CleanupAt; if (Age < .5 || (!Settled && Age < 4)) return true;
	for (int32 I = 0; I < R.Peers.Num(); ++I) UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Cleanup token=%s peer=%d restored=%d players=%d floorAlive=%d"), *R.Token, I, Settled, R.Peers[I].People.Num(), Alive(R.Peers[I].Floor.Get()));
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Complete token=%s case=%d pass=%d cleanup=%d cooldownWrites=0 statsRestored=0 scope=same-process-PIE lag=0 reason=%s"), *R.Token, R.Case, R.Failure.IsEmpty() && Settled, Settled, R.Failure.IsEmpty() ? TEXT("observed") : *R.Failure);
	Active.Reset(); return false;
}
bool Tick(float)
{
	const auto Keep = Active; if (!Keep) return false; FRequest& R = *Keep; const double Now = FPlatformTime::Seconds();
	if (GAllowActorScriptExecutionInEditor) return true;
	if (R.bCleaning) return Finish(R);
	if (R.bAbort) Fail(R, TEXT("aborted"));
	if (Now - R.Started > 65) Fail(R, TEXT("request-timeout"));
	if (!R.Failure.IsEmpty()) return Finish(R);
	if (R.Stage == 0) { if (!Setup(R)) Fail(R, TEXT("four-world-roster-config-or-ready-precondition")); return true; }
	for (const auto& P : R.Peers) if (!P.World.IsValid()) { Fail(R, TEXT("world-ended")); return true; }
	if (!R.Round.IsValid() || R.Round->GetCurrentRoundNumber() != R.RoundNumber || R.Round->GetCurrentRoundState() != ERoundState::RoundActive || R.Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) { Fail(R, TEXT("unexpected-round-transition")); return true; }
	if (!ObserveActors(R)) return true;
	if (R.Stage == 1)
	{
		if (!Placed(R)) { R.StableAt = 0; if (Now - R.StageAt > 8) Fail(R, TEXT("initial-placement-settlement")); return true; }
		if (!R.StableAt) R.StableAt = Now; if (Now - R.StableAt < .35) return true;
		Dispatch(R, 0); Stage(R, 2); return true;
	}
	if (R.Stage == 2)
	{
		auto& A = R.Actions[R.CastIndex];
		if (!EndpointsDone(A) || !CaptureActor(R, A) || !PeerActors(R, A, TEXT("initial"), false))
		{ if (Now - R.StageAt > 1.0) Fail(R, TEXT("cast-or-actor-peer-observation-timeout")); return true; }
		if (!A.bPeersCaptured) { A.bPeersCaptured = true; PeerActors(R, A, TEXT("initial"), true); }
		if (R.Case < 3)
		{
			if (A.Kind == 0 && (A.Applied != 1 || !FMath::IsNearlyEqual(Health(ServerPerson(R, A.Target).PS.Get()), A.TargetBefore + A.InitialHeal, .05f))) { Fail(R, TEXT("blessing-initial-heal-precondition")); return true; }
			if (A.Kind == 1 && (A.Applied != 0 || ServerPerson(R, A.Target).Component->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed))) { Fail(R, TEXT("circle-target-entered-before-caster-death")); return true; }
			if (A.Kind == 2 && (Time(R) - A.SpawnAt > .6f || A.DamageEffects != 0 || !FMath::IsNearlyEqual(Health(ServerPerson(R, A.Target).PS.Get()), A.TargetBefore, .01f))) { Fail(R, TEXT("projectile-death-before-collision-window")); return true; }
			Death(R); return true;
		}
		if (R.CastIndex == 0) { R.CastIndex = 1; Dispatch(R, 1); Stage(R, 2); return true; }
		if (R.CastIndex == 1)
		{
			// Targets enter the overlapping real zones only after both casts complete.
			Place(R, R.Actions[0].Target, FVector(0, 0, 0));
			if (R.Actions[1].Target != R.Actions[0].Target) Place(R, R.Actions[1].Target, FVector(150, 0, 0));
			Stage(R, 5); return true;
		}
		if (!CircleEffects(R, R.Actions[1], true, true)) { Fail(R, TEXT("reset-lost-live-circle-effect-precondition")); return true; }
		for (auto& LiveAction : R.Actions) if (!Alive(LiveAction.Actor.Get()) || LiveAction.Deadline - Time(R) < .3f) { Fail(R, TEXT("reset-must-precede-original-expiry")); return true; }
		R.bIntervened = true; R.InterventionAt = Time(R);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Intervention token=%s trigger=reset source=%d worldTime=%.6f liveActors=3"), *R.Token, R.KorrynId, R.InterventionAt);
		R.Round->ResetRoundState();
		bool Removed = true; for (const auto& X : R.Actions) Removed &= !Alive(X.Actor.Get()) && X.DestroyEvents == 1;
		for (auto H : R.Actions[1].TargetEffects) Removed &= ServerPerson(R, R.Actions[1].Target).Component->GetActiveGameplayEffect(H) == nullptr;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Reset token=%s immediateActorsRemoved=%d exactEffectsRemoved=%d worldTime=%.6f"), *R.Token, Removed, Removed, Time(R));
		if (!Removed) Fail(R, TEXT("reset-did-not-remove-live-actors-and-exact-effects")); Stage(R, 4); return true;
	}
	if (R.Stage == 3)
	{
		auto& A = R.Actions[0];
		if (R.Case == 1 && !R.bEntryPlaced)
		{
			const auto& Source = ServerPerson(R, A.Source);
			if (Alive(Source.OldPawn.Get()) || (Source.Component->GetAvatarActor() && Identity(Source.Component->GetAvatarActor()) == Source.OldKey)) return true;
			if (A.Deadline - Time(R) < 1) { Fail(R, TEXT("caster-teardown-too-late-for-circle-entry")); return true; }
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: CasterGone token=%s source=%d oldPawn=%s oldKey=%s ps=%s asc=%s worldTime=%.6f"), *R.Token, A.Source, *Source.OldPath, *Source.OldKey, *Source.PS->GetPathName(), *Source.Component->GetPathName(), Time(R));
			Place(R, A.Target, FVector::ZeroVector); R.bEntryPlaced = true;
		}
		if (R.Case == 1 && R.bEntryPlaced && !A.bEffectObserved && CircleEffects(R, A, true, false) && PeerActors(R, A, TEXT("after-death"), false)) { A.bEffectObserved = true; CircleEffects(R, A, true, true); PeerActors(R, A, TEXT("after-death"), true); }
		if (R.Case == 0 && !A.bEffectObserved && A.PostDeathHeals >= 1 && HealthSettled(R, A.Target, A.LastHealHealth) && PeerActors(R, A, TEXT("after-death"), false))
		{
			A.bEffectObserved = true; PeerActors(R, A, TEXT("after-death"), true);
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: ContinuedHeal token=%s target=%d count=%d health=%.6f lastHeal=%.6f worldTime=%.6f"), *R.Token, A.Target, A.PostDeathHeals, A.LastHealHealth, A.LastHealAt, Time(R));
		}
		if (R.Case == 2 && A.DamageEffects > 0 && !A.bEffectObserved)
		{
			auto& T = ServerPerson(R, A.Target); auto& S = ServerPerson(R, A.Source);
			if (A.DamageEffects != 1 || !FMath::IsNearlyEqual(A.TargetAfter, A.TargetBefore - A.ExpectedDamage, .05f) || Pawn(T.PS.Get())->GetCombatContributionCountForAudit() != 1
				|| S.PS->GetDamageDealt() - A.SourceDamageBefore != FMath::RoundToInt(A.ExpectedDamage) || Health(S.PS.Get()) != 0
				|| !FMath::IsNearlyEqual(Value(S.Component.Get(), UBwayHeroAttributeSet::GetLifeLeechAttribute()), .1f, .001f)) { Fail(R, TEXT("post-mortem-hit-contribution-or-no-leech")); return true; }
			if (!HealthSettled(R, A.Target, A.TargetAfter) || !HealthSettled(R, A.Source, 0) || !ShredSettled(R, A, true, false)) return true;
			A.bEffectObserved = true;
			ShredSettled(R, A, true, true);
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: PostMortemHit token=%s source=%d target=%d sourcePS=%s sourceASC=%s damageEffects=%d contributions=%d damageStatDelta=%d expectedDamage=%.6f before=%.6f after=%.6f sourceHealth=%.6f leech=%.6f worldTime=%.6f"), *R.Token, A.Source, A.Target, *S.PS->GetPathName(), *S.Component->GetPathName(), A.DamageEffects, Pawn(T.PS.Get())->GetCombatContributionCountForAudit(), S.PS->GetDamageDealt() - A.SourceDamageBefore, A.ExpectedDamage, A.TargetBefore, A.TargetAfter, Health(S.PS.Get()), Value(S.Component.Get(), UBwayHeroAttributeSet::GetLifeLeechAttribute()), Time(R));
		}
		if (R.Case < 2 && Time(R) >= A.Deadline + .25f)
		{
			if (!A.bPreExpiry || !A.bEffectObserved || A.DestroyEvents != 1 || !FMath::IsNearlyEqual(A.DestroyedAt, A.Deadline, .10f)) { Fail(R, TEXT("original-expiry-not-observed")); return true; }
			if (R.Case == 0 && A.PostDeathHeals < 2) { Fail(R, TEXT("insufficient-post-death-blessing-ticks")); return true; }
			if (R.Case == 0)
			{
				const int32 ExpectedTicks = FMath::FloorToInt(A.Duration - .001f);
				const float ExpectedHealth = A.TargetBefore + A.InitialHeal + ExpectedTicks * A.HealTick;
				if (A.Applied != 1 + ExpectedTicks || A.PostDeathHeals != ExpectedTicks || A.AfterTeardownHeals < 1 || !FMath::IsNearlyEqual(A.LastHealHealth, ExpectedHealth, .05f)
					|| !FMath::IsNearlyEqual(A.LastHealAt, A.Deadline - A.Duration + ExpectedTicks, .15f)) { Fail(R, TEXT("blessing-full-original-window-heal-count-or-health")); return true; }
				UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: HealSummary token=%s target=%d effects=%d postDeath=%d afterTeardown=%d expectedTicks=%d before=%.6f after=%.6f expectedHealth=%.6f lastHeal=%.6f deadline=%.6f"), *R.Token, A.Target, A.Applied, A.PostDeathHeals, A.AfterTeardownHeals, ExpectedTicks, A.TargetBefore, A.LastHealHealth, ExpectedHealth, A.LastHealAt, A.Deadline);
			}
			Stage(R, 4); return true;
		}
		if (R.Case == 2 && A.bEffectObserved && Time(R) >= A.SpawnAt + A.Duration + .25f) { Stage(R, 4); return true; }
		if (Time(R) > A.Deadline + 2) Fail(R, TEXT("persistent-policy-outcome-timeout"));
		return true;
	}
	if (R.Stage == 5)
	{
		auto& B = R.Actions[0]; auto& C = R.Actions[1];
		if (B.Applied < 1 || !CircleEffects(R, C, true, false) || !HealthSettled(R, B.Target, B.LastHealHealth))
		{ if (Now - R.StageAt > 1.5) Fail(R, TEXT("shared-live-zone-effects-precondition")); return true; }
		B.bEffectObserved = C.bEffectObserved = true;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: LiveZones token=%s healEffects=%d healHealth=%.6f circleHandles=%d worldTime=%.6f"), *R.Token, B.Applied, B.LastHealHealth, C.TargetEffects.Num(), Time(R));
		// Fire away from all targets without changing the projectile, speed, collision or lifespan.
		Place(R, R.KorrynId, FVector(-2000, -2000, 0), 180); Stage(R, 6); return true;
	}
	if (R.Stage == 6)
	{
		if (!Placed(R)) { if (Now - R.StageAt > 1) Fail(R, TEXT("reset-primary-placement")); return true; }
		R.CastIndex = 2; Dispatch(R, 2); Stage(R, 2); return true;
	}
	if (R.Stage == 4)
	{
		bool Settled = Gone(R, false) && Replacements(R, false);
		for (auto& A : R.Actions)
		{
			if (A.Kind == 1) Settled &= CircleEffects(R, A, false, false);
			if (A.Kind == 0 && R.Case != 3) Settled &= HealthSettled(R, A.Target, A.LastHealHealth);
			if (A.Kind == 2 && R.Case == 2)
			{
				Settled &= A.DamageEffects == 1 && HealthSettled(R, A.Target, A.TargetAfter);
				Settled &= ShredSettled(R, A, false, false);
			}
		}
		if (!Settled) { R.StableAt = 0; if (Now - R.StageAt > 30) Fail(R, TEXT("replacement-or-final-peer-settlement")); return true; }
		if (!R.StableAt) R.StableAt = Now; if (Now - R.StableAt < 1.1) return true;
		Gone(R, true); Replacements(R, true); for (auto& A : R.Actions) { if (A.Kind == 1) CircleEffects(R, A, false, true); if (A.Kind == 2 && R.Case == 2) ShredSettled(R, A, false, true); }
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Settled token=%s peers=4 span=%.6f worldTime=%.6f"), *R.Token, Now - R.StableAt, Time(R));
		return Finish(R);
	}
	Fail(R, TEXT("invalid-stage")); return true;
}
FAutoConsoleCommand Command(TEXT("bway.Test.AllKitPersistentPIE"), TEXT("Disposable zero-lag four-world PIE: <fresh-token> <case0..3> <remote-source-id>. 0 Blessing death/expiry; 1 Circle death/expiry; 2 Korryn post-mortem projectile; 3 shared reset (Korryn source; remote Alona discovered)."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		int32 Case = INDEX_NONE, Source = INDEX_NONE; bool TokenOkay = Args.Num() == 3 && !Args[0].IsEmpty() && Args[0].Len() <= 48;
		if (TokenOkay) for (TCHAR C : Args[0]) TokenOkay &= FChar::IsAlnum(C) || C == TCHAR('_') || C == TCHAR('-');
		if (Active || !TokenOkay || !LexTryParseString(Case, *Args[1]) || Case < 0 || Case > 3 || !LexTryParseString(Source, *Args[2]) || Source < 0)
		{ UE_LOG(LogTemp, Error, TEXT("BwayAllKitPersistent: Rejected reason=arguments-or-existing-request")); return; }
		Active = MakeShared<FRequest>(); Active->Token = Args[0]; Active->Case = Case; Active->RequestedSource = Source; Active->Started = FPlatformTime::Seconds();
		Active->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.f);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitPersistent: Queued token=%s case=%d source=%d scope=same-process-PIE lag=0 timeout=65"), *Active->Token, Case, Source);
	}));
FAutoConsoleCommand Abort(TEXT("bway.Test.AllKitPersistentPIEAbort"), TEXT("Abort only this matching request: <token>."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { if (Active && Args.Num() == 1 && Active->Token == Args[0]) Active->bAbort = true; }));
}
#endif
