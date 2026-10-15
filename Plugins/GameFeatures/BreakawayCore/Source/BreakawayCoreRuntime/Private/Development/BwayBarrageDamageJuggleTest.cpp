// One bounded real-projectile damage/juggle observation; no shipping gameplay changes.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Abilities/LyraGameplayAbility_Jump.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayGameplayEffect_DamageSetByCaller.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "LyraGameplayTags.h"
#include "Player/LyraCheatManager.h"
#include "UObject/UnrealType.h"

namespace BwayBarrageDamageJuggleTest
{
enum class EPhase : uint8 { Setup, Converge, Jump, Channel, Land, Cleanup };
struct FEndpoint
{
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	TWeakObjectPtr<ABwayPlayerController> Controller;
	TWeakObjectPtr<ABwayPlayerState> State;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	TWeakObjectPtr<UBwayRoundManagementComponent> Round;
	FGameplayAbilitySpecHandle Handle;
	FTransform Original;
	FRotator View;
	int32 RoundNumber = INDEX_NONE;
	float Health = 0.f;
};
struct FHit
{
	TWeakObjectPtr<AActor> Projectile;
	float Before = 0.f, After = 0.f;
	bool bLaunch = false;
};
struct FShot
{
	TWeakObjectPtr<AActor> Projectile;
	float Attack = 0.f;
};
struct FRequest
{
	int32 SourceId = 0, VictimId = 0;
	EPhase Phase = EPhase::Setup;
	FEndpoint Owner, Authority, Victim;
	TWeakObjectPtr<ABwayCharacterWithAbilities> VictimProxy;
	TWeakObjectPtr<ABwayPlayerState> VictimProxyState;
	TWeakObjectPtr<ULyraAbilitySystemComponent> VictimProxyASC;
	TWeakObjectPtr<const ULyraHealthSet> VictimHealth;
	TWeakObjectPtr<UWorld> AuthorityWorld;
	TWeakObjectPtr<UClass> BarrageFamily, BulletFamily;
	FGameplayTag JumpTag, CooldownTag;
	FDelegateHandle HealthHandle, SpawnHandle, DestroyHandle;
	TMap<FString, FShot> Shots;
	TMap<FString, FHit> Hits;
	TSet<FString> Terminals;
	FVector Placement = FVector::ZeroVector;
	float Base = 0.f, Scaling = 0.f, Attack = 0.f, Armor = 0.f, Incoming = 0.f;
	float CooldownReduction = 0.f;
	float Interval = 0.f, Speed = 0.f, Life = 0.f, Offset = 0.f, Juggle = 0.f, Cooldown = 0.f;
	int32 Count = 0, AimTicks = 0, Launches = 0;
	double Start = 0., PhaseAt = 0., CastAt = 0., LastSample = 0., QuietAt = 0., RestoreAt = 0.;
	double DrainSeconds = 0.25, LastLaunchAt = 0., LandingQuietAt = 0.;
	float LastLaunchZ = 0.f;
	bool bPrepared = false, bPlaced = false, bJumpHeld = false, bJumpAttempted = false, bCast = false;
	bool bSawActive = false, bSawTrajectory = false, bLanded = false, bEvidenceEnded = false;
	bool bStopped = false, bCancelled = false, bHealed = false, bRestored = false, bTeleports = false;
	bool bCancelRequested = false;
	FString Failure;
};
TSharedPtr<FRequest> Pending;
FTSTicker::FDelegateHandle Ticker;
const TCHAR* EndpointName(int32 Index) { return Index == 0 ? TEXT("owner") : Index == 1 ? TEXT("authority") : TEXT("victim"); }
FEndpoint& Endpoint(FRequest& R, int32 Index) { return Index == 0 ? R.Owner : Index == 1 ? R.Authority : R.Victim; }

void Fail(FRequest& R, const TCHAR* Reason) { if (R.Failure.IsEmpty()) R.Failure = Reason; }
bool Live(const FEndpoint& E)
{
	const auto* P = E.Pawn.Get(); const auto* C = E.Controller.Get(); const auto* S = E.State.Get();
	const auto* A = E.ASC.Get(); const auto* Round = E.Round.Get();
	const auto* H = P ? ULyraHealthComponent::FindHealthComponent(P) : nullptr;
	const auto* GS = P && P->GetWorld() ? P->GetWorld()->GetGameState() : nullptr;
	return P && C && S && A && Round && H && !H->IsDeadOrDying() && H->GetHealth() > 0.f
		&& P->GetController() == C && C->GetPawn() == P && C->GetPlayerState<ABwayPlayerState>() == S
		&& P->GetPlayerState<ABwayPlayerState>() == S && S->GetPawn() == P && S->GetLyraAbilitySystemComponent() == A
		&& P->GetLyraAbilitySystemComponent() == A && A->GetAvatarActor() == P && A->GetOwnerActor() == S
		&& A->AbilityActorInfo.IsValid() && A->AbilityActorInfo->PlayerController.Get() == C
		&& GS && GS->FindComponentByClass<UBwayRoundManagementComponent>() == Round
		&& Round->GetCurrentRoundNumber() == E.RoundNumber && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
		&& Round->GetCurrentRoundState() == ERoundState::RoundActive;
}
bool AllLive(const FRequest& R)
{
	const auto* P = R.VictimProxy.Get(); const auto* S = R.VictimProxyState.Get(); const auto* A = R.VictimProxyASC.Get();
	return Live(R.Owner) && Live(R.Authority) && Live(R.Victim)
		&& R.Owner.Pawn->IsLocallyControlled() && R.Owner.Pawn->GetNetMode() == NM_Client
		&& !R.Authority.Pawn->IsLocallyControlled() && R.Authority.Pawn->HasAuthority()
		&& R.Victim.Pawn->HasAuthority() && R.Victim.Pawn->IsLocallyControlled()
		&& R.Authority.Pawn->GetWorld() == R.Victim.Pawn->GetWorld()
		&& P && S && A && P->GetWorld() == R.Owner.Pawn->GetWorld() && P->GetPlayerState<ABwayPlayerState>() == S
		&& S->GetPawn() == P && S->GetLyraAbilitySystemComponent() == A && P->GetLyraAbilitySystemComponent() == A
		&& A->GetAvatarActor() == P && A->GetOwnerActor() == S && S->GetPlayerId() == R.VictimId;
}
FGameplayAbilitySpec* Spec(const FEndpoint& E) { return E.ASC.IsValid() ? E.ASC->FindAbilitySpecFromHandle(E.Handle) : nullptr; }
bool Active(const FEndpoint& E) { const auto* S = Spec(E); return S && S->IsActive(); }
bool Ready(const FEndpoint& E) { const auto* S = Spec(E); return S && S->Ability && !S->IsActive() && S->Ability->CheckCooldown(S->Handle, E.ASC->AbilityActorInfo.Get()); }
bool BarrageSpecsValid(const FRequest& R)
{
	const auto* O=Spec(R.Owner); const auto* A=Spec(R.Authority);
	return R.BarrageFamily.IsValid() && O && A && O->Ability && A->Ability && O->Ability->IsA(R.BarrageFamily.Get())
		&& A->Ability->IsA(R.BarrageFamily.Get()) && O->Ability->GetClass()==A->Ability->GetClass();
}
float Health(const FEndpoint& E) { return E.ASC.IsValid() ? E.ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.f; }
bool Grounded(const ABwayCharacterWithAbilities* P)
{
	const auto* M = P ? P->GetCharacterMovement() : nullptr;
	return M && M->MovementMode == MOVE_Walking && M->CurrentFloor.HitResult.IsValidBlockingHit()
		&& M->CurrentFloor.IsWalkableFloor() && M->IsWalkable(M->CurrentFloor.HitResult)
		&& FMath::IsFinite(M->CurrentFloor.GetDistanceToFloor()) && M->CurrentFloor.GetDistanceToFloor() >= -0.5f
		&& M->CurrentFloor.GetDistanceToFloor() <= UCharacterMovementComponent::MAX_FLOOR_DIST + .01f;
}
bool Capture(FEndpoint& E, ABwayCharacterWithAbilities* P)
{
	if (!P) return false;
	E.Pawn = P; E.Controller = Cast<ABwayPlayerController>(P->GetController()); E.State = P->GetPlayerState<ABwayPlayerState>();
	E.ASC = E.State.IsValid() ? E.State->GetLyraAbilitySystemComponent() : nullptr;
	E.Round = P->GetWorld()->GetGameState() ? P->GetWorld()->GetGameState()->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
	E.RoundNumber = E.Round.IsValid() ? E.Round->GetCurrentRoundNumber() : INDEX_NONE;
	if (!Live(E)) return false;
	E.Original = P->GetActorTransform(); E.View = E.Controller->GetControlRotation(); E.Health = Health(E);
	return Grounded(P) && P->GetVelocity().Size() <= 1.f && !P->bIsCrouched && !P->bPressedJump
		&& FMath::IsNearlyEqual(E.Health, E.ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()), .01f);
}
bool FindUnique(FRequest& R, FEndpoint& E, const TCHAR* EndpointLabel, const UClass* Family, FGameplayTag Input = FGameplayTag())
{
	int32 Matches = 0, Bindings = 0;
	for (const FGameplayAbilitySpec& S : E.ASC->GetActivatableAbilities())
	{
		const bool Bound = !Input.IsValid() || S.GetDynamicSpecSourceTags().HasTagExact(Input);
		if (Input.IsValid() && Bound) ++Bindings;
		if (Bound && Family && S.Ability && S.Ability->IsA(Family)) { E.Handle = S.Handle; ++Matches; }
	}
	const auto* S = Spec(E);
	const bool CooldownReady=S && S->Ability && S->Ability->CheckCooldown(S->Handle,E.ASC->AbilityActorInfo.Get());
	// ServerTryActivateAbility latches InputPressed on a remotely activated spec.
	// Barrage is activated directly here; only the production jump input needs a
	// released binding before the fixture presses it. Never rewrite the spec bit.
	const bool InputReady=!Input.IsValid() || (S && !S->InputPressed);
	const TCHAR* Reason=!Family ? TEXT("family-unavailable") : Matches!=1 ? TEXT("family-spec-count")
		: Input.IsValid() && Bindings!=1 ? TEXT("input-binding-count") : !S || !S->Ability ? TEXT("spec-unavailable")
		: S->IsActive() ? TEXT("spec-active") : !InputReady ? TEXT("input-already-pressed")
		: !CooldownReady ? TEXT("cooldown-not-ready") : TEXT("ready");
	const bool Accepted=Matches==1 && (!Input.IsValid() || Bindings==1) && S && !S->IsActive() && InputReady && CooldownReady;
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: SpecAdmission source=%d victim=%d endpoint=%s pawn=%s asc=%s family=%s handle=%s matches=%d inputTag=%s bindings=%d inputReleaseRequired=%d inputPressed=%d active=%d cooldownReady=%d accepted=%d reason=%s guard=%d"),
		R.SourceId,R.VictimId,EndpointLabel,*GetPathNameSafe(E.Pawn.Get()),*GetPathNameSafe(E.ASC.Get()),*GetPathNameSafe(Family),*E.Handle.ToString(),Matches,*Input.ToString(),Bindings,Input.IsValid(),S ? int32(S->InputPressed) : -1,S ? int32(S->IsActive()) : -1,CooldownReady,Accepted,Reason,GAllowActorScriptExecutionInEditor);
	return Accepted;
}
bool ReadFloat(const UObject* O, const TCHAR* Name, float& Out)
{
	const auto* P = O ? FindFProperty<FFloatProperty>(O->GetClass(), Name) : nullptr;
	if (!P) return false;
	Out = P->GetPropertyValue_InContainer(O); return FMath::IsFinite(Out);
}
bool AttributesStable(const FRequest& R)
{
	const auto* S = R.Authority.ASC->GetSet<UBwayHeroAttributeSet>(); const auto* V = R.Victim.ASC->GetSet<UBwayHeroAttributeSet>();
	return S && V && FMath::IsNearlyEqual(S->GetAttackStrength(), R.Attack, .001f)
		&& FMath::IsNearlyEqual(V->GetArmor(), R.Armor, .001f) && FMath::IsNearlyEqual(V->GetIncomingDamageMultiplier(), R.Incoming, .001f);
}
void State(FRequest& R, const TCHAR* Phase)
{
	if (!AllLive(R)) return;
	FVector ViewLocation; FRotator View; R.Authority.Controller->GetPlayerViewPoint(ViewLocation, View);
	const FVector P = R.Victim.Pawn->GetActorLocation(), V = R.Victim.Pawn->GetVelocity();
	float OwnerRemaining=0.f,OwnerDuration=0.f,AuthorityRemaining=0.f,AuthorityDuration=0.f;
	if(const auto* S=Spec(R.Owner);S && S->Ability)S->Ability->GetCooldownTimeRemainingAndDuration(S->Handle,R.Owner.ASC->AbilityActorInfo.Get(),OwnerRemaining,OwnerDuration);
	if(const auto* S=Spec(R.Authority);S && S->Ability)S->Ability->GetCooldownTimeRemainingAndDuration(S->Handle,R.Authority.ASC->AbilityActorInfo.Get(),AuthorityRemaining,AuthorityDuration);
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageDamageJuggle: State source=%d victim=%d phase=%s elapsed=%.6f castElapsed=%.6f ownerActive=%d authorityActive=%d ownerTag=%d authorityTag=%d ownerCooldownReady=%d authorityCooldownReady=%d victimMode=%d victimX=%.6f victimY=%.6f victimZ=%.6f vx=%.6f vy=%.6f vz=%.6f victimHealth=%.6f ownerYaw=%.6f ownerPitch=%.6f authorityViewYaw=%.6f authorityViewPitch=%.6f aimTicks=%d shots=%d hits=%d launches=%d guard=%d authorityTime=%.6f ownerTime=%.6f ownerCooldownRemaining=%.6f ownerCooldownDuration=%.6f authorityCooldownRemaining=%.6f authorityCooldownDuration=%.6f"),
		R.SourceId, R.VictimId, Phase, FPlatformTime::Seconds()-R.Start, R.bCast ? FPlatformTime::Seconds()-R.CastAt : -1.,
		Active(R.Owner), Active(R.Authority), R.Owner.ASC->GetTagCount(TAG_Gameplay_MovementStopped), R.Authority.ASC->GetTagCount(TAG_Gameplay_MovementStopped),
		Ready(R.Owner), Ready(R.Authority), int32(R.Victim.Pawn->GetCharacterMovement()->MovementMode), P.X,P.Y,P.Z,V.X,V.Y,V.Z,Health(R.Victim),
		R.Owner.Controller->GetControlRotation().Yaw,R.Owner.Controller->GetControlRotation().Pitch,View.Yaw,View.Pitch,R.AimTicks,R.Shots.Num(),R.Hits.Num(),R.Launches,GAllowActorScriptExecutionInEditor,R.Authority.Pawn->GetWorld()->GetTimeSeconds(),R.Owner.Pawn->GetWorld()->GetTimeSeconds(),OwnerRemaining,OwnerDuration,AuthorityRemaining,AuthorityDuration);
}
bool JumpInput(FRequest& R, bool Press)
{
	if (GAllowActorScriptExecutionInEditor || !Live(R.Victim)) return false;
	const auto* S = Spec(R.Victim); int32 Bindings = 0;
	for (const auto& Candidate : R.Victim.ASC->GetActivatableAbilities()) if (Candidate.GetDynamicSpecSourceTags().HasTagExact(R.JumpTag)) ++Bindings;
	if (!S || !S->Ability || !S->Ability->IsA<ULyraGameplayAbility_Jump>() || !S->GetDynamicSpecSourceTags().HasTagExact(R.JumpTag) || Bindings != 1) return false;
	R.bJumpHeld = Press;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageDamageJuggle: JumpInput source=%d victim=%d pressed=%d pawn=%s asc=%s handle=%s guard=%d"),R.SourceId,R.VictimId,Press,*R.Victim.Pawn->GetPathName(),*R.Victim.ASC->GetPathName(),*R.Victim.Handle.ToString(),GAllowActorScriptExecutionInEditor);
	if (Press) R.Victim.ASC->AbilityInputTagPressed(R.JumpTag); else R.Victim.ASC->AbilityInputTagReleased(R.JumpTag);
	R.Victim.ASC->ProcessAbilityInput(0.f,false); return true;
}
void Bind(FRequest& R)
{
	const TWeakPtr<FRequest> Weak = Pending;
	R.HealthHandle = R.VictimHealth->OnHealthChanged.AddLambda([Weak](AActor* Instigator,AActor* Causer,const FGameplayEffectSpec* Effect,float Magnitude,float Before,float After)
	{
		const auto P = Weak.Pin(); if (!P || !AllLive(*P)) return;
		FRequest& Q = *P;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageDamageJuggle: Health source=%d victim=%d causer=%s before=%.6f after=%.6f delta=%.6f cleanup=%d guard=%d"),Q.SourceId,Q.VictimId,*GetPathNameSafe(Causer),Before,After,After-Before,Q.bEvidenceEnded,GAllowActorScriptExecutionInEditor);
		if (After >= Before || Q.bEvidenceEnded) return;
		const FString Key = GetPathNameSafe(Causer);
		const FShot* Shot = Q.Shots.Find(Key);
		const bool Exact = Q.bCast && Causer && Shot && Shot->Projectile.Get() == Causer && Effect && Effect->Def
			&& Effect->Def->IsA<UGE_Bway_Damage_SetByCaller>() && Effect->GetContext().GetEffectCauser() == Causer
			&& Effect->GetContext().GetOriginalInstigatorAbilitySystemComponent() == Q.Authority.ASC.Get()
			&& Effect->GetContext().GetOriginalInstigator() == Q.Authority.Pawn.Get() && Instigator == Q.Authority.Pawn.Get()
			&& Causer->GetOwner() == Q.Authority.Pawn.Get() && Causer->GetInstigator() == Q.Authority.Pawn.Get();
		if (!Exact || Q.Hits.Contains(Key)) { Fail(Q,TEXT("unpaired-or-duplicate-damage")); return; }
		const auto* TargetAttributes = Q.Victim.ASC->GetSet<UBwayHeroAttributeSet>();
		if (!TargetAttributes) { Fail(Q,TEXT("target-attributes-lost")); return; }
		const float Armor = TargetAttributes->GetArmor(), Incoming = TargetAttributes->GetIncomingDamageMultiplier();
		const float Raw = Q.Base+Q.Scaling*Shot->Attack, Final = FMath::Max(0.f,Raw*(1.f-Armor*.01f)*Incoming);
		const float SetByCaller = Effect->GetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage,false,-1.f);
		const FVector V = Q.Victim.Pawn->GetVelocity();
		FHit H; H.Projectile=Causer; H.Before=Before; H.After=After; Q.Hits.Add(Key,H);
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageDamageJuggle: Hit source=%d victim=%d index=%d projectile=%s effect=%s instigator=%s target=%s before=%.6f after=%.6f damage=%.6f base=%.6f scaling=%.6f attack=%.6f armor=%.6f incoming=%.6f raw=%.6f expected=%.6f setByCaller=%.6f attributesStable=%d mode=%d vx=%.6f vy=%.6f vz=%.6f z=%.6f guard=%d sourceASC=%s targetASC=%s effectCauser=%s causerOwner=%s causerInstigator=%s elapsed=%.6f worldTime=%.6f"),
			Q.SourceId,Q.VictimId,Q.Hits.Num(),*Key,*Effect->Def->GetClass()->GetPathName(),*GetPathNameSafe(Instigator),*Q.Victim.Pawn->GetPathName(),Before,After,Before-After,Q.Base,Q.Scaling,Shot->Attack,Armor,Incoming,Raw,Final,SetByCaller,AttributesStable(Q),int32(Q.Victim.Pawn->GetCharacterMovement()->MovementMode),V.X,V.Y,V.Z,Q.Victim.Pawn->GetActorLocation().Z,GAllowActorScriptExecutionInEditor,
			*GetPathNameSafe(Effect->GetContext().GetOriginalInstigatorAbilitySystemComponent()),*Q.Victim.ASC->GetPathName(),*GetPathNameSafe(Effect->GetContext().GetEffectCauser()),*GetPathNameSafe(Causer->GetOwner()),*GetPathNameSafe(Causer->GetInstigator()),FPlatformTime::Seconds()-Q.Start,Q.Authority.Pawn->GetWorld()->GetTimeSeconds());
		if (!AttributesStable(Q) || !FMath::IsNearlyEqual(Before-After,Final,.02f) || !FMath::IsNearlyEqual(SetByCaller,Final,.02f)) Fail(Q,TEXT("damage-value-mismatch"));
		if (!Q.Victim.Pawn->GetCharacterMovement()->IsFalling()) Fail(Q,TEXT("hit-not-airborne"));
	});
	R.SpawnHandle=R.Authority.Pawn->GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([Weak](AActor* Actor)
	{
		const auto P=Weak.Pin(); if (!P || !P->bCast || P->bEvidenceEnded || !AllLive(*P) || !Actor || !Actor->IsA(P->BulletFamily.Get())
			|| Actor->GetOwner()!=P->Authority.Pawn.Get() || Actor->GetInstigator()!=P->Authority.Pawn.Get()) return;
		const auto* SourceAttributes=P->Authority.ASC->GetSet<UBwayHeroAttributeSet>();
		if (!SourceAttributes) { Fail(*P,TEXT("source-attributes-lost")); return; }
		const FString Key=Actor->GetPathName(); FShot Shot; Shot.Projectile=Actor; Shot.Attack=SourceAttributes->GetAttackStrength(); P->Shots.Add(Key,Shot);
		FVector L; FRotator V; P->Authority.Controller->GetPlayerViewPoint(L,V);
		const FRotator Heading=Actor->GetActorRotation();
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageDamageJuggle: Shot source=%d victim=%d index=%d projectile=%s authorityPawn=%s headingYaw=%.6f headingPitch=%.6f authorityViewYaw=%.6f authorityViewPitch=%.6f guard=%d attack=%.6f elapsed=%.6f worldTime=%.6f"),P->SourceId,P->VictimId,P->Shots.Num(),*Key,*P->Authority.Pawn->GetPathName(),Heading.Yaw,Heading.Pitch,V.Yaw,V.Pitch,GAllowActorScriptExecutionInEditor,Shot.Attack,FPlatformTime::Seconds()-P->Start,Actor->GetWorld()->GetTimeSeconds());
	}));
	R.DestroyHandle=R.Authority.Pawn->GetWorld()->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateLambda([Weak](AActor* Actor)
	{
		const auto P=Weak.Pin(); if (!P || !Actor || !AllLive(*P)) return;
		const FString Key=Actor->GetPathName(); const FShot* Shot=P->Shots.Find(Key); if (!Shot || Shot->Projectile.Get()!=Actor) return;
		P->Terminals.Add(Key); FHit* H=P->Hits.Find(Key);
		const FVector Launch=P->Victim.Pawn->GetCharacterMovement()->PendingLaunchVelocity;
		const bool Exact=H && H->Projectile.Get()==Actor && Actor->GetWorld()==P->Authority.Pawn->GetWorld()
			&& Actor->GetOwner()==P->Authority.Pawn.Get() && Actor->GetInstigator()==P->Authority.Pawn.Get();
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageDamageJuggle: Terminal source=%d victim=%d projectile=%s paired=%d pendingX=%.6f pendingY=%.6f pendingZ=%.6f expectedZ=%.6f mode=%d victimZ=%.6f guard=%d elapsed=%.6f worldTime=%.6f"),P->SourceId,P->VictimId,*Key,Exact,Launch.X,Launch.Y,Launch.Z,P->Juggle,int32(P->Victim.Pawn->GetCharacterMovement()->MovementMode),P->Victim.Pawn->GetActorLocation().Z,GAllowActorScriptExecutionInEditor,FPlatformTime::Seconds()-P->Start,P->Authority.Pawn->GetWorld()->GetTimeSeconds());
		if (Exact)
		{
			if (H->bLaunch || !Launch.Equals(FVector(0,0,P->Juggle),.01)) Fail(*P,TEXT("queued-launch-mismatch"));
			else { H->bLaunch=true; ++P->Launches; P->LastLaunchAt=FPlatformTime::Seconds(); P->LastLaunchZ=P->Victim.Pawn->GetActorLocation().Z; }
		}
	}));
}

bool Setup(FRequest& R)
{
	ABwayCharacterWithAbilities *Owner=nullptr,*Authority=nullptr,*Victim=nullptr,*Proxy=nullptr;
	int32 Owners=0,Authorities=0,Victims=0,Proxies=0,Worlds=0;
	if (GEngine) for (const FWorldContext& C:GEngine->GetWorldContexts())
	{
		UWorld* W=C.World(); if (!W || !W->IsGameWorld() || (W->GetNetMode()!=NM_Client && W->GetNetMode()!=NM_ListenServer)) continue;
		++Worlds;
		for(TActorIterator<ABwayCharacterWithAbilities> It(W);It;++It)
		{
			auto* P=*It; auto* S=P->GetPlayerState<ABwayPlayerState>(); if(!S || S->GetPawn()!=P) continue;
			if(S->GetPlayerId()==R.SourceId) { if(W->GetNetMode()==NM_Client && P->IsLocallyControlled()){Owner=P;++Owners;} else if(W->GetNetMode()==NM_ListenServer){Authority=P;++Authorities;} }
			if(S->GetPlayerId()==R.VictimId && W->GetNetMode()==NM_ListenServer){Victim=P;++Victims;}
		}
	}
	if(Owners!=1 || Authorities!=1 || Victims!=1 || Worlds!=4 || !Victim->IsLocallyControlled()) { Fail(R,TEXT("unsupported-four-world-owner-victim-topology")); return false; }
	for(TActorIterator<ABwayCharacterWithAbilities> It(Owner->GetWorld());It;++It) if(auto* S=It->GetPlayerState<ABwayPlayerState>(); S && S->GetPlayerId()==R.VictimId && S->GetPawn()==*It){Proxy=*It;++Proxies;}
	if(Proxies!=1 || !Capture(R.Owner,Owner) || !Capture(R.Authority,Authority) || !Capture(R.Victim,Victim)){Fail(R,TEXT("endpoint-standing-full-health-precondition"));return false;}
	R.VictimProxy=Proxy; R.VictimProxyState=Proxy->GetPlayerState<ABwayPlayerState>(); R.VictimProxyASC=R.VictimProxyState->GetLyraAbilitySystemComponent();
	R.AuthorityWorld=Authority->GetWorld();
	const auto* GameState=Authority->GetWorld()->GetGameState<ABwayGameState>();
	if(!AllLive(R) || !GameState || GameState->AreOnSameTeam(Authority,Victim)
		|| R.Owner.RoundNumber!=R.Authority.RoundNumber || R.Authority.RoundNumber!=R.Victim.RoundNumber || R.Authority.Round!=R.Victim.Round){Fail(R,TEXT("enemy-ownership-precondition"));return false;}
	R.BarrageFamily=FindObject<UClass>(nullptr,TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage"));
	R.BulletFamily=FindObject<UClass>(nullptr,TEXT("/Script/HeroRawlinsRuntime.BwayRawlinsBulletProjectile"));
	R.JumpTag=FGameplayTag::RequestGameplayTag(TEXT("InputTag.Jump"),false); R.CooldownTag=FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Rawlins.BlazingBarrage"),false);
	if(!R.BulletFamily.IsValid()){Fail(R,TEXT("loaded-bullet-family-unavailable"));return false;}
	if(!R.BarrageFamily.IsValid()){Fail(R,TEXT("loaded-barrage-family-unavailable"));return false;}
	if(!R.JumpTag.IsValid()){Fail(R,TEXT("jump-input-tag-unavailable"));return false;}
	if(!R.CooldownTag.IsValid()){Fail(R,TEXT("barrage-cooldown-tag-unavailable"));return false;}
	if(!FindUnique(R,R.Owner,TEXT("owner"),R.BarrageFamily.Get())){Fail(R,TEXT("owner-barrage-spec-precondition"));return false;}
	if(!FindUnique(R,R.Authority,TEXT("authority"),R.BarrageFamily.Get())){Fail(R,TEXT("authority-barrage-spec-precondition"));return false;}
	if(!FindUnique(R,R.Victim,TEXT("victim"),ULyraGameplayAbility_Jump::StaticClass(),R.JumpTag)){Fail(R,TEXT("victim-jump-spec-precondition"));return false;}
	if(!Cast<ULyraCheatManager>(R.Victim.Controller->CheatManager)){Fail(R,TEXT("victim-heal-cheat-manager-unavailable"));return false;}
	if(!BarrageSpecsValid(R)){Fail(R,TEXT("owner-authority-ability-class-mismatch"));return false;}
	const auto* ConfigProp=FindFProperty<FSoftObjectProperty>(Spec(R.Authority)->Ability->GetClass(),TEXT("KitConfig"));
	const UObject* Config=ConfigProp ? ConfigProp->GetPropertyValue_InContainer(Spec(R.Authority)->Ability).Get() : nullptr;
	const auto* Count=Config ? FindFProperty<FIntProperty>(Config->GetClass(),TEXT("BarrageShotCount")) : nullptr;
	if(!Count || !ReadFloat(Config,TEXT("BarrageBaseDamagePerBullet"),R.Base) || !ReadFloat(Config,TEXT("BarrageDamageScaling"),R.Scaling)
		|| !ReadFloat(Config,TEXT("BarrageShotInterval"),R.Interval) || !ReadFloat(Config,TEXT("BarrageProjectileSpeed"),R.Speed)
		|| !ReadFloat(Config,TEXT("BarrageProjectileLifeSpan"),R.Life) || !ReadFloat(Config,TEXT("BarrageSpawnForwardOffset"),R.Offset)
		|| !ReadFloat(Config,TEXT("BarrageJuggleUpward"),R.Juggle) || !ReadFloat(Config,TEXT("BarrageCooldown"),R.Cooldown)){Fail(R,TEXT("loaded-kit-unavailable"));return false;}
	R.Count=Count->GetPropertyValue_InContainer(Config);
	const auto* S=R.Authority.ASC->GetSet<UBwayHeroAttributeSet>(); const auto* V=R.Victim.ASC->GetSet<UBwayHeroAttributeSet>(); R.VictimHealth=R.Victim.ASC->GetSet<ULyraHealthSet>();
	if(!S || !V || !R.VictimHealth.IsValid()){Fail(R,TEXT("missing-attributes"));return false;}
	R.Attack=S->GetAttackStrength();R.Armor=V->GetArmor();R.Incoming=V->GetIncomingDamageMultiplier();R.CooldownReduction=S->GetCooldownReduction();
	const float Expected=FMath::Max(0.f,(R.Base+R.Scaling*R.Attack)*(1.f-R.Armor*.01f)*R.Incoming);
	if(R.Count!=12 || R.Interval<=0.f || R.Interval>.2f || R.Juggle<=0.f || R.Life<=0.f || R.Life>2.f || R.Speed<=0.f || R.Offset<0.f || R.Offset>=180.f
		|| !FMath::IsNearlyZero(R.CooldownReduction,.001f) || !FMath::IsFinite(Expected) || Expected<=0.f || Expected*R.Count>=R.Victim.Health-10.f){Fail(R,TEXT("unsupported-tuning-or-nonlethal-budget"));return false;}
	for(TActorIterator<AActor> It(Authority->GetWorld(),R.BulletFamily.Get());It;++It) if(It->GetOwner()==Authority){Fail(R,TEXT("prior-owned-projectile"));return false;}
	R.bPrepared=true;
	for(int32 I=0;I<3;++I)
	{
		const auto& E=Endpoint(R,I); const FVector P=E.Original.GetLocation(); const FRotator Rot=E.Original.Rotator();
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: Endpoint source=%d victim=%d endpoint=%s pawn=%s controller=%s state=%s asc=%s round=%d handle=%s ability=%s net=%d role=%d local=%d health=%.6f x=%.6f y=%.6f z=%.6f pitch=%.6f yaw=%.6f roll=%.6f viewPitch=%.6f viewYaw=%.6f viewRoll=%.6f guard=%d"),R.SourceId,R.VictimId,EndpointName(I),*E.Pawn->GetPathName(),*E.Controller->GetPathName(),*E.State->GetPathName(),*E.ASC->GetPathName(),E.RoundNumber,*E.Handle.ToString(),*Spec(E)->Ability->GetClass()->GetPathName(),int32(E.Pawn->GetNetMode()),int32(E.Pawn->GetLocalRole()),E.Pawn->IsLocallyControlled(),E.Health,P.X,P.Y,P.Z,Rot.Pitch,Rot.Yaw,Rot.Roll,E.View.Pitch,E.View.Yaw,E.View.Roll,GAllowActorScriptExecutionInEditor);
	}
	int32 Peer=0;
	for(const FWorldContext& C:GEngine->GetWorldContexts()) if(UWorld* W=C.World(); W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer))
	{
		const UNetDriver* D=W->GetNetDriver(); if(!D){Fail(R,TEXT("missing-net-driver"));return false;}
		const auto& N=D->PacketSimulationSettings;
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: Network source=%d victim=%d peer=%d world=%s driver=%s lag=%d lagMin=%d lagMax=%d incomingMin=%d incomingMax=%d loss=%d incomingLoss=%d jitter=%d variance=%d"),R.SourceId,R.VictimId,Peer++,*W->GetPathName(),*D->GetPathName(),N.PktLag,N.PktLagMin,N.PktLagMax,N.PktIncomingLagMin,N.PktIncomingLagMax,N.PktLoss,N.PktIncomingLoss,N.PktJitter,N.PktLagVariance);
		if(W==Owner->GetWorld() || W==Authority->GetWorld()) R.DrainSeconds+=(FMath::Max3(double(N.PktLag+N.PktLagVariance),double(N.PktLagMin),double(N.PktLagMax))+FMath::Max(0.,double(N.PktJitter))+FMath::Max3(0.,double(N.PktIncomingLagMin),double(N.PktIncomingLagMax)))/1000.;
	}
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: Plan source=%d victim=%d config=%s count=%d interval=%.6f speed=%.6f life=%.6f offset=%.6f juggle=%.6f cooldown=%.6f base=%.6f scaling=%.6f attack=%.6f armor=%.6f incoming=%.6f expectedPerHit=%.6f expectedTotal=%.6f aimPeer=%s guard=%d projectileClass=%s cooldownReduction=%.6f"),R.SourceId,R.VictimId,*GetPathNameSafe(Config),R.Count,R.Interval,R.Speed,R.Life,R.Offset,R.Juggle,R.Cooldown,R.Base,R.Scaling,R.Attack,R.Armor,R.Incoming,Expected,Expected*R.Count,*Proxy->GetPathName(),GAllowActorScriptExecutionInEditor,*R.BulletFamily->GetPathName(),R.CooldownReduction);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BarrageJuggleLane),false);Params.AddIgnoredActor(Authority);Params.AddIgnoredActor(Victim);
	FHitResult Floor; const float Half=Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
	UWorld* W=Authority->GetWorld();
	const auto* Move=Victim->GetCharacterMovement(); const double Gravity=-Move->GetGravityZ();
	const double Clearance=Gravity>0. ? FMath::Square(double(Move->JumpZVelocity))/(2.*Gravity)+R.Count*R.Interval*R.Juggle+FMath::Square(double(R.Juggle))/(2.*Gravity)+100. : -1.;
	if(!FMath::IsFinite(Clearance) || Clearance<=0. || Clearance>1500.){Fail(R,TEXT("unsupported-juggle-clearance"));return false;}
	const FCollisionShape Capsule=FCollisionShape::MakeCapsule(Radius,Half);
	const float YawOffsets[]={0.f,45.f,-45.f,80.f,-80.f};
	FVector Candidate=FVector::ZeroVector; int32 Selected=INDEX_NONE;
	// Search only nearby lanes within the declared owner aim scope. Every candidate
	// retains collision with unrelated pawns; no fixture mutation precedes selection.
	for(int32 Index=0;Index<UE_ARRAY_COUNT(YawOffsets);++Index)
	{
		const FRotator Direction(0.,Authority->GetActorRotation().Yaw+YawOffsets[Index],0.);
		Candidate=Authority->GetActorLocation()+Direction.Vector()*240.f;
		const double YawDelta=FMath::Abs(FMath::FindDeltaAngleDegrees(R.Owner.View.Yaw,Direction.Yaw));
		FHitResult Block; const TCHAR* Reason=TEXT("clear");
		if(YawDelta>90.) Reason=TEXT("outside-owner-aim-scope");
		else if(!W->LineTraceSingleByChannel(Floor,Candidate+FVector(0,0,200),Candidate-FVector(0,0,400),ECC_Visibility,Params)
			|| !Floor.IsValidBlockingHit() || Floor.ImpactNormal.Z<.99f){Reason=TEXT("flat-floor-unavailable");Block=Floor;}
		else
		{
			Candidate.Z=Floor.ImpactPoint.Z+Half+(UCharacterMovementComponent::MIN_FLOOR_DIST+UCharacterMovementComponent::MAX_FLOOR_DIST)*.5f;
			if(W->SweepSingleByChannel(Block,Candidate,Candidate+FVector(0,0,Clearance),FQuat::Identity,ECC_Pawn,Capsule,Params)) Reason=TEXT("overhead-blocked");
			else if(W->LineTraceSingleByChannel(Block,Authority->GetActorLocation(),Candidate,ECC_Visibility,Params)) Reason=TEXT("lane-blocked");
			else Selected=Index;
		}
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: LaneCandidate source=%d victim=%d index=%d yawOffset=%.6f yawDelta=%.6f x=%.6f y=%.6f z=%.6f accepted=%d reason=%s actor=%s component=%s blocking=%d penetrating=%d hitX=%.6f hitY=%.6f hitZ=%.6f normalZ=%.6f distance=%.6f guard=%d"),
			R.SourceId,R.VictimId,Index,YawOffsets[Index],YawDelta,Candidate.X,Candidate.Y,Candidate.Z,Selected==Index,Reason,*GetPathNameSafe(Block.GetActor()),*GetPathNameSafe(Block.GetComponent()),Block.bBlockingHit,Block.bStartPenetrating,Block.ImpactPoint.X,Block.ImpactPoint.Y,Block.ImpactPoint.Z,Block.ImpactNormal.Z,Block.Distance,GAllowActorScriptExecutionInEditor);
		if(Selected!=INDEX_NONE)break;
	}
	if(Selected==INDEX_NONE){Fail(R,TEXT("no-clear-lane-in-owner-aim-scope"));return false;}
	Bind(R); R.Placement=Candidate; R.bPlaced=true;
	if(!Victim->TeleportTo(Candidate,Victim->GetActorRotation(),false,false)){Fail(R,TEXT("authority-placement-rejected"));return false;}
	Victim->ForceNetUpdate();
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: Placement source=%d victim=%d x=%.6f y=%.6f z=%.6f floorNormalZ=%.6f clearance=%.6f jumpZ=%.6f gravity=%.6f clientTransformWrites=0 guard=%d laneIndex=%d yawOffset=%.6f"),R.SourceId,R.VictimId,Candidate.X,Candidate.Y,Candidate.Z,Floor.ImpactNormal.Z,Clearance,Move->JumpZVelocity,Gravity,GAllowActorScriptExecutionInEditor,Selected,YawOffsets[Selected]);
	R.Phase=EPhase::Converge; R.PhaseAt=FPlatformTime::Seconds();return true;
}

bool Aim(FRequest& R,float Delta)
{
	if(!AllLive(R) || GAllowActorScriptExecutionInEditor) return false;
	const FVector Target=R.VictimProxy->GetActorLocation(); const FRotator Desired=(Target-R.Owner.Pawn->GetActorLocation()).Rotation();
	if(FMath::Abs(Desired.Pitch)>85.f || FMath::Abs(FMath::FindDeltaAngleDegrees(R.Owner.View.Yaw,Desired.Yaw))>90.f) return false;
	const FRotator Next=FMath::RInterpConstantTo(R.Owner.Controller->GetControlRotation(),Desired,FMath::Clamp(Delta,0.f,.05f),180.f);
	R.Owner.Controller->SetControlRotation(Next); ++R.AimTicks;
	return true;
}
bool Finish(FRequest& R,bool Cleanup,bool Hard)
{
	int32 Removed=0;
	if(R.VictimHealth.IsValid() && R.HealthHandle.IsValid()){R.VictimHealth->OnHealthChanged.Remove(R.HealthHandle);++Removed;}
	if(UWorld* W=R.AuthorityWorld.Get())
	{
		if(R.SpawnHandle.IsValid()){W->RemoveOnActorSpawnedHandler(R.SpawnHandle);++Removed;}
		if(R.DestroyHandle.IsValid()){W->RemoveOnActorDestroyedHandler(R.DestroyHandle);++Removed;}
	}
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: Complete source=%d victim=%d reason=%s cleanup=%d hardDeadline=%d delegatesRemoved=%d shots=%d hits=%d launches=%d trajectory=%d landed=%d guard=%d outcome=observed"),R.SourceId,R.VictimId,R.Failure.IsEmpty()?TEXT("observed"):*R.Failure,Cleanup,Hard,Removed,R.Shots.Num(),R.Hits.Num(),R.Launches,R.bSawTrajectory,R.bLanded,GAllowActorScriptExecutionInEditor);
	Pending.Reset();Ticker.Reset();return false;
}
void BeginCleanup(FRequest& R,const TCHAR* Reason=nullptr)
{
	if(Reason) Fail(R,Reason);
	if(R.Phase!=EPhase::Cleanup){R.Phase=EPhase::Cleanup;R.PhaseAt=FPlatformTime::Seconds();}
	if(!R.bEvidenceEnded)
	{
		State(R,TEXT("evidence-end"));R.bEvidenceEnded=true;
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: EvidenceEnd source=%d victim=%d reason=%s shots=%d hits=%d launches=%d trajectory=%d landed=%d guard=%d"),R.SourceId,R.VictimId,R.Failure.IsEmpty()?TEXT("observed-landing"):*R.Failure,R.Shots.Num(),R.Hits.Num(),R.Launches,R.bSawTrajectory,R.bLanded,GAllowActorScriptExecutionInEditor);
	}
}
bool Cleanup(FRequest& R,bool Hard)
{
	if(!R.bPrepared) return Finish(R,true,Hard);
	if(!AllLive(R) || GAllowActorScriptExecutionInEditor) return Hard ? Finish(R,false,true) : true;
	if(!R.bStopped)
	{
		if(R.bJumpHeld && !JumpInput(R,false)) {Fail(R,TEXT("jump-release-failed"));return Hard?Finish(R,false,true):true;}
		for(int32 I=0;I<3;++I)
		{
			auto& E=Endpoint(R,I);
			if((I<2 ? R.bCast : R.bJumpAttempted) && Active(E)){E.ASC->CancelAbilityHandle(E.Handle);R.bCancelled=true;}
			if(!AllLive(R)) return Hard?Finish(R,false,true):true;
			E.Pawn->ConsumeMovementInputVector();E.Pawn->GetCharacterMovement()->StopMovementImmediately();E.Controller->RotationInput=FRotator::ZeroRotator;
		}
		R.bStopped=true;R.PhaseAt=FPlatformTime::Seconds();
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: DrainStart source=%d victim=%d minimumSeconds=%.6f guard=%d"),R.SourceId,R.VictimId,R.DrainSeconds,GAllowActorScriptExecutionInEditor);
	}
	bool LiveShots=false;for(const auto& Entry:R.Shots) if(Entry.Value.Projectile.IsValid() && !Entry.Value.Projectile->IsActorBeingDestroyed()) LiveShots=true;
	const auto* Jump=Spec(R.Victim);
	const bool Quiet=!LiveShots && !Active(R.Owner) && !Active(R.Authority) && Jump && !Jump->IsActive() && !Jump->InputPressed && !R.bJumpHeld
		&& Grounded(R.Owner.Pawn.Get()) && Grounded(R.Authority.Pawn.Get()) && Grounded(R.Victim.Pawn.Get())
		&& R.Owner.Pawn->GetVelocity().Size()<=1. && R.Authority.Pawn->GetVelocity().Size()<=1. && R.Victim.Pawn->GetVelocity().Size()<=1.
		&& R.Owner.Pawn->GetPendingMovementInputVector().IsNearlyZero(.01) && R.Authority.Pawn->GetPendingMovementInputVector().IsNearlyZero(.01) && R.Victim.Pawn->GetPendingMovementInputVector().IsNearlyZero(.01)
		&& R.Owner.Controller->RotationInput.IsNearlyZero(.01) && R.Authority.Controller->RotationInput.IsNearlyZero(.01) && R.Victim.Controller->RotationInput.IsNearlyZero(.01)
		&& FVector::Dist(R.Owner.Pawn->GetActorLocation(),R.Authority.Pawn->GetActorLocation())<=2. && FVector::Dist(R.VictimProxy->GetActorLocation(),R.Victim.Pawn->GetActorLocation())<=3.;
	const double Now=FPlatformTime::Seconds();if(!Quiet)R.QuietAt=0.;else if(R.QuietAt==0.)R.QuietAt=Now;
	if(!R.bRestored && Quiet && Now-R.PhaseAt>=R.DrainSeconds && Now-R.QuietAt>=.1)
	{
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: DrainReady source=%d victim=%d elapsed=%.6f quietSeconds=%.6f liveShots=0 guard=%d"),R.SourceId,R.VictimId,Now-R.PhaseAt,Now-R.QuietAt,GAllowActorScriptExecutionInEditor);
		if(!R.bHealed)
		{
			R.bHealed=true;const float Before=Health(R.Victim),Amount=R.Victim.Health-Before;
			if(Amount>0.f)
			{
				ULyraCheatManager* Cheat=Cast<ULyraCheatManager>(R.Victim.Controller->CheatManager);
				if(!Cheat){Fail(R,TEXT("production-heal-unavailable"));return Hard?Finish(R,false,true):true;}
				Cheat->HealSelf(Amount);
			}
			if(!AllLive(R)) return Hard?Finish(R,false,true):true;
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: RestoreHealth source=%d victim=%d before=%.6f requested=%.6f after=%.6f expected=%.6f guard=%d"),R.SourceId,R.VictimId,Before,Amount,Health(R.Victim),R.Victim.Health,GAllowActorScriptExecutionInEditor);
		}
		R.Owner.Controller->SetControlRotation(R.Owner.View);R.Authority.Controller->SetControlRotation(R.Authority.View);R.Victim.Controller->SetControlRotation(R.Victim.View);
		const bool SourceRestored=R.Authority.Pawn->TeleportTo(R.Authority.Original.GetLocation(),R.Authority.Original.Rotator(),false,false);
		const bool VictimRestored=R.Victim.Pawn->TeleportTo(R.Victim.Original.GetLocation(),R.Victim.Original.Rotator(),false,false);
		R.bTeleports=SourceRestored && VictimRestored;R.bRestored=true;R.RestoreAt=Now;
		R.Authority.Pawn->GetCharacterMovement()->ForceClientAdjustment();R.Authority.Pawn->ForceNetUpdate();R.Victim.Pawn->ForceNetUpdate();
	}
	bool Restored=R.bRestored && R.bTeleports && Quiet && BarrageSpecsValid(R) && Now-R.RestoreAt>=.5;
	for(int32 I=0;I<3 && Restored;++I)
	{
		const auto& E=Endpoint(R,I);
		Restored=FVector::Dist(E.Pawn->GetActorLocation(),E.Original.GetLocation())<=(I==0?3.:1.)
			&& FMath::RadiansToDegrees(E.Pawn->GetActorQuat().AngularDistance(E.Original.GetRotation()))<=1.
			&& (E.Controller->GetControlRotation()-E.View).GetNormalized().GetManhattanDistance(FRotator::ZeroRotator)<=1.
			&& FMath::IsNearlyEqual(Health(E),E.Health,.01f) && !E.ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped);
	}
	Restored=Restored && FMath::IsNearlyEqual(R.VictimProxyASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()),R.Victim.Health,.01f);
	if(Restored || Hard)
	{
		State(R,TEXT("cleanup"));
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: Cleanup source=%d victim=%d restored=%d teleports=%d clientTransformWrites=0 ownerError=%.6f authorityError=%.6f victimError=%.6f cancelled=%d jumpHeld=%d hardDeadline=%d guard=%d"),R.SourceId,R.VictimId,Restored,R.bTeleports,FVector::Dist(R.Owner.Pawn->GetActorLocation(),R.Owner.Original.GetLocation()),FVector::Dist(R.Authority.Pawn->GetActorLocation(),R.Authority.Original.GetLocation()),FVector::Dist(R.Victim.Pawn->GetActorLocation(),R.Victim.Original.GetLocation()),R.bCancelled,R.bJumpHeld,Hard,GAllowActorScriptExecutionInEditor);
		return Finish(R,Restored,Hard);
	}
	return true;
}
bool Tick(float Delta)
{
	const auto Keep=Pending;if(!Keep) return false;FRequest& R=*Keep;const double Now=FPlatformTime::Seconds();const bool Hard=Now-R.Start>=12.;
	if(Hard){BeginCleanup(R,R.Failure.IsEmpty()?TEXT("hard-timeout"):nullptr);return Cleanup(R,true);}
	if(GAllowActorScriptExecutionInEditor) return true;
	if(R.bCancelRequested) BeginCleanup(R,TEXT("observer-cancel-request"));
	if(R.Phase==EPhase::Setup){if(!Setup(R)){BeginCleanup(R);return Cleanup(R,false);}return true;}
	if(R.Phase==EPhase::Cleanup)return Cleanup(R,false);
	if(!AllLive(R)){BeginCleanup(R,TEXT("original-lifetime-invalidated"));return Cleanup(R,false);}
	if(!BarrageSpecsValid(R)){BeginCleanup(R,TEXT("original-barrage-spec-invalidated"));return Cleanup(R,false);}
	if(!R.Failure.IsEmpty() || Now-R.Start>=9.){BeginCleanup(R,R.Failure.IsEmpty()?TEXT("observation-timeout"):nullptr);return Cleanup(R,false);}
	if(!AttributesStable(R)){BeginCleanup(R,TEXT("combat-attributes-changed"));return Cleanup(R,false);}
	if(Now-R.LastSample>=.05){State(R,R.Phase==EPhase::Converge?TEXT("converge"):R.Phase==EPhase::Jump?TEXT("jump"):R.Phase==EPhase::Channel?TEXT("channel"):TEXT("landing"));R.LastSample=Now;}
	if(R.Phase==EPhase::Converge)
	{
		const bool PlacementSettled=Grounded(R.Victim.Pawn.Get()) && Grounded(R.VictimProxy.Get()) && FVector::Dist(R.Victim.Pawn->GetActorLocation(),R.Placement)<=1.
			&& FVector::Dist(R.VictimProxy->GetActorLocation(),R.Victim.Pawn->GetActorLocation())<=3. && FVector::Dist(R.Owner.Pawn->GetActorLocation(),R.Authority.Pawn->GetActorLocation())<=3.;
		if(PlacementSettled && !Aim(R,Delta)){BeginCleanup(R,TEXT("aim-scope-invalid-before-jump"));return true;}
		const FRotator OwnerDesired=(R.VictimProxy->GetActorLocation()-R.Owner.Pawn->GetActorLocation()).Rotation();
		const FRotator AuthorityDesired=(R.Victim.Pawn->GetActorLocation()-R.Authority.Pawn->GetActorLocation()).Rotation();
		FVector ViewLocation; FRotator View;R.Authority.Controller->GetPlayerViewPoint(ViewLocation,View);
		const FRotator OwnerError=(R.Owner.Controller->GetControlRotation()-OwnerDesired).GetNormalized();
		const FRotator AuthorityError=(View-AuthorityDesired).GetNormalized();
		const bool Settled=PlacementSettled && FMath::Abs(OwnerError.Yaw)<=1. && FMath::Abs(OwnerError.Pitch)<=1.
			&& FMath::Abs(AuthorityError.Yaw)<=2. && FMath::Abs(AuthorityError.Pitch)<=2.;
		if(!Settled) R.QuietAt=0.;else if(R.QuietAt==0.)R.QuietAt=Now;
		if(Settled && Now-R.QuietAt>=.2)
		{
			if(!Ready(R.Owner)||!Ready(R.Authority)){BeginCleanup(R,TEXT("cooldown-not-ready-at-jump"));return true;}
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: AimReady source=%d victim=%d aimPeer=%s ownerDesiredYaw=%.6f ownerDesiredPitch=%.6f ownerYaw=%.6f ownerPitch=%.6f authorityDesiredYaw=%.6f authorityDesiredPitch=%.6f authorityViewYaw=%.6f authorityViewPitch=%.6f ownerYawError=%.6f ownerPitchError=%.6f authorityYawError=%.6f authorityPitchError=%.6f quietSeconds=%.6f minimumSeconds=0.2 aimTicks=%d guard=%d"),R.SourceId,R.VictimId,*R.VictimProxy->GetPathName(),OwnerDesired.Yaw,OwnerDesired.Pitch,R.Owner.Controller->GetControlRotation().Yaw,R.Owner.Controller->GetControlRotation().Pitch,AuthorityDesired.Yaw,AuthorityDesired.Pitch,View.Yaw,View.Pitch,OwnerError.Yaw,OwnerError.Pitch,AuthorityError.Yaw,AuthorityError.Pitch,Now-R.QuietAt,R.AimTicks,GAllowActorScriptExecutionInEditor);
			R.bJumpAttempted=true;if(!JumpInput(R,true)){BeginCleanup(R,TEXT("jump-input-rejected"));return true;}
			R.Phase=EPhase::Jump;R.PhaseAt=Now;R.QuietAt=0.;State(R,TEXT("jump-start"));
		}
		else if(Now-R.PhaseAt>3.)BeginCleanup(R,TEXT("placement-or-aim-convergence-timeout"));
		return true;
	}
	if(R.Phase==EPhase::Jump || R.Phase==EPhase::Channel) if(!Aim(R,Delta)){BeginCleanup(R,TEXT("aim-scope-invalid"));return true;}
	if(R.Phase==EPhase::Jump)
	{
		const auto* M=R.Victim.Pawn->GetCharacterMovement();const FVector V=R.Victim.Pawn->GetVelocity();
		if(R.bJumpHeld && M->IsFalling() && V.Z>0.f) if(!JumpInput(R,false)){BeginCleanup(R,TEXT("jump-release-rejected"));return true;}
		const double Flight=FMath::Max(0.,FVector::Dist(R.Authority.Pawn->GetActorLocation(),R.Victim.Pawn->GetActorLocation())-R.Offset)/R.Speed;
		const double Delay=FMath::Max(0.,R.DrainSeconds-.25)+Flight+.05;
		const double Height=R.Victim.Pawn->GetActorLocation().Z-R.Placement.Z;
		const double RequiredHeight=FMath::Max(0.,-double(V.Z))*Delay+.5*FMath::Max(0.,-double(M->GetGravityZ()))*Delay*Delay+20.;
		if(!R.bJumpHeld && M->IsFalling() && V.Z<0.f && Height>RequiredHeight)
		{
			if(!Ready(R.Owner)||!Ready(R.Authority)){BeginCleanup(R,TEXT("cooldown-not-ready-at-cast"));return true;}
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: AirborneGate source=%d victim=%d height=%.6f requiredHeight=%.6f transportAndFlight=%.6f projectileFlight=%.6f vz=%.6f guard=%d"),R.SourceId,R.VictimId,Height,RequiredHeight,Delay,Flight,V.Z,GAllowActorScriptExecutionInEditor);
			State(R,TEXT("descending-baseline"));R.bCast=true;R.CastAt=Now;R.Phase=EPhase::Channel;R.PhaseAt=Now;
			const bool Accepted=R.Owner.ASC->TryActivateAbility(R.Owner.Handle,true);
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: Dispatch source=%d victim=%d accepted=%d owner=%s authority=%s ownerHandle=%s authorityHandle=%s guard=%d"),R.SourceId,R.VictimId,Accepted,*GetPathNameSafe(R.Owner.Pawn.Get()),*GetPathNameSafe(R.Authority.Pawn.Get()),*R.Owner.Handle.ToString(),*R.Authority.Handle.ToString(),GAllowActorScriptExecutionInEditor);
			if(!Accepted)BeginCleanup(R,TEXT("barrage-activation-rejected"));return true;
		}
		if(Now-R.PhaseAt>3.)BeginCleanup(R,TEXT("natural-jump-descending-timeout"));
		return true;
	}
	if(R.LastLaunchAt>0. && Now>R.LastLaunchAt && R.Victim.Pawn->GetCharacterMovement()->IsFalling()
		&& R.Victim.Pawn->GetVelocity().Z>0.f && R.Victim.Pawn->GetActorLocation().Z>R.LastLaunchZ+.5f)R.bSawTrajectory=true;
	if(R.Phase==EPhase::Channel)
	{
		R.bSawActive|=Active(R.Authority);
		if(R.bSawActive && !Active(R.Owner) && !Active(R.Authority)) {R.Phase=EPhase::Land;R.PhaseAt=Now;State(R,TEXT("channel-end"));}
		else if(Now-R.CastAt>R.Count*R.Interval+R.DrainSeconds+1.)BeginCleanup(R,TEXT("channel-end-timeout"));
	}
	if(R.Phase==EPhase::Land)
	{
		const bool Landed=Now-R.CastAt>R.Count*R.Interval+R.Life && Grounded(R.Victim.Pawn.Get()) && R.Victim.Pawn->GetVelocity().Size()<=1.;
		if(!Landed) R.LandingQuietAt=0.;
		else
		{
			if(R.LandingQuietAt==0.){R.LandingQuietAt=Now;R.bLanded=true;State(R,TEXT("landed"));}
			// Leave an unmodified network/sample window so proxies can independently
			// observe natural landing before StopMovementImmediately or teleport.
			if(Now-R.LandingQuietAt>=R.DrainSeconds)
			{
				State(R,TEXT("landed-ready"));
				UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: LandingReady source=%d victim=%d quietSeconds=%.6f minimumSeconds=%.6f guard=%d"),R.SourceId,R.VictimId,Now-R.LandingQuietAt,R.DrainSeconds,GAllowActorScriptExecutionInEditor);
				if(R.Shots.Num()!=R.Count || R.Hits.Num()!=R.Count || R.Launches!=R.Count)Fail(R,TEXT("incomplete-shot-hit-launch-count"));
				if(!R.bSawTrajectory)Fail(R,TEXT("missing-post-launch-trajectory"));
				BeginCleanup(R);return Cleanup(R,false);
			}
		}
	}
	return true;
}
bool Parse(const TArray<FString>& Args,int32& Source,int32& Victim)
{
	if(Args.Num()!=2)return false;int32 Values[2]={0,0};
	for(int32 I=0;I<2;++I){if(Args[I].IsEmpty())return false;for(TCHAR C:Args[I]){const int32 D=int32(C)-int32(TEXT('0'));if(D<0||D>9||Values[I]>(MAX_int32-D)/10)return false;Values[I]=Values[I]*10+D;}}
	Source=Values[0];Victim=Values[1];return Source!=Victim;
}
void Start(const TArray<FString>& Args)
{
	int32 Source=0,Victim=0;
	if(!Parse(Args,Source,Victim)||Pending){UE_LOG(LogTemp,Warning,TEXT("BwayBarrageDamageJuggle: Rejected reason=invalid-ids-or-request-pending"));return;}
	Pending=MakeShared<FRequest>();Pending->SourceId=Source;Pending->VictimId=Victim;Pending->Start=FPlatformTime::Seconds();Pending->PhaseAt=Pending->Start;
	Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick),0.f);
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageDamageJuggle: Queued source=%d victim=%d timeout=12.0 scope=remote-source-listen-host-victim"),Source,Victim);
}
FAutoConsoleCommand StartCommand(TEXT("bway.Test.BarrageDamageJuggle"),TEXT("One real Barrage against a naturally jumping listen-host enemy: <remote-source-id> <host-victim-id>."),FConsoleCommandWithArgsDelegate::CreateStatic(&Start));
FAutoConsoleCommand CancelCommand(TEXT("bway.Test.BarrageDamageJuggleCancel"),TEXT("Request bounded cleanup for matching <source-id> <victim-id>."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
	int32 Source=0,Victim=0;if(Pending && Parse(Args,Source,Victim) && Pending->SourceId==Source && Pending->VictimId==Victim)Pending->bCancelRequested=true;
}));
}
#endif
