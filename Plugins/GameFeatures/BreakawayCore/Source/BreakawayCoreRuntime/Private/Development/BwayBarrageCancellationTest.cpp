// Observes the owner's production GAS cancellation route; no shipping gameplay changes.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/UnrealType.h"

namespace BwayBarrageCancellationTest
{
struct FEndpoint
{
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	TWeakObjectPtr<ABwayPlayerController> Controller;
	TWeakObjectPtr<ABwayPlayerState> State;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	TWeakObjectPtr<UBwayRoundManagementComponent> Round;
	TWeakObjectPtr<UClass> AbilityClass;
	TWeakObjectPtr<ULyraGameplayAbility> Instance;
	FGameplayAbilitySpecHandle Handle;
	FPredictionKey Key;
	FDelegateHandle EndDelegate, HealthDelegate;
	FVector Position=FVector::ZeroVector;
	float Health=0.f;
	int32 RoundNumber=INDEX_NONE, Ends=0;
	double EndAt=0.;
	bool bCleanupCancel=false;
};
struct FRequest
{
	int32 PlayerId=0, ShotCount=0, HealthChanges=0, PredicateMatches=0, LoadedCount=0;
	FEndpoint Owner, Authority;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UClass> Family, BulletFamily;
	FGameplayTag CooldownTag;
	FDelegateHandle SpawnDelegate;
	TArray<TWeakObjectPtr<AActor>> Projectiles;
	FString FirstProjectile, Failure;
	double Start=0., CastAt=0., FirstShotAt=0., CancelAt=0., LastSample=0., Transport=0.;
	float Interval=0.f, Life=0.f, Cooldown=0.f;
	bool bPrepared=false, bRequested=false, bAccepted=false, bDispatched=false, bEvidenceEnded=false, bAbort=false;
};
TSharedPtr<FRequest> Pending;
FTSTicker::FDelegateHandle Ticker;
FEndpoint& Endpoint(FRequest& R,int32 Index){return Index==0?R.Owner:R.Authority;}
const TCHAR* Name(int32 Index){return Index==0?TEXT("owner"):TEXT("authority");}
void Fail(FRequest& R,const TCHAR* Reason){if(R.Failure.IsEmpty())R.Failure=Reason;}
FGameplayAbilitySpec* Spec(const FEndpoint& E){return E.ASC.IsValid()?E.ASC->FindAbilitySpecFromHandle(E.Handle):nullptr;}
bool Active(const FEndpoint& E){const auto* S=Spec(E);return S && S->IsActive();}
float Health(const FEndpoint& E){return E.ASC.IsValid()?E.ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()):-1.f;}
bool Live(const FEndpoint& E)
{
	const auto* P=E.Pawn.Get();const auto* C=E.Controller.Get();const auto* S=E.State.Get();const auto* A=E.ASC.Get();const auto* Round=E.Round.Get();
	const auto* H=P?ULyraHealthComponent::FindHealthComponent(P):nullptr;
	const auto* GS=P && P->GetWorld()?P->GetWorld()->GetGameState():nullptr;
	return P && C && S && A && Round && H && !H->IsDeadOrDying() && H->GetHealth()>0.f
		&& P->GetController()==C && C->GetPawn()==P && P->GetPlayerState<ABwayPlayerState>()==S && C->GetPlayerState<ABwayPlayerState>()==S
		&& S->GetPawn()==P && S->GetLyraAbilitySystemComponent()==A && P->GetLyraAbilitySystemComponent()==A
		&& A->GetOwnerActor()==S && A->GetAvatarActor()==P && A->AbilityActorInfo.IsValid() && A->AbilityActorInfo->PlayerController.Get()==C
		&& GS && GS->FindComponentByClass<UBwayRoundManagementComponent>()==Round && Round->GetCurrentRoundNumber()==E.RoundNumber
		&& Round->GetCurrentMatchPhase()==EBwayMatchPhase::Playing && Round->GetCurrentRoundState()==ERoundState::RoundActive;
}
bool AllLive(const FRequest& R)
{
	return Live(R.Owner) && Live(R.Authority) && R.Owner.Pawn->IsLocallyControlled() && R.Owner.Pawn->GetNetMode()==NM_Client
		&& R.Authority.Pawn->HasAuthority() && !R.Authority.Pawn->IsLocallyControlled() && R.Authority.Pawn->GetWorld()==R.World.Get();
}
bool SameSpec(const FEndpoint& E)
{
	const auto* S=Spec(E);return S && S->Ability && S->Ability->GetClass()==E.AbilityClass.Get();
}
bool SameActivation(const FEndpoint& E)
{
	const auto* S=Spec(E);const auto* A=E.Instance.Get();
	return SameSpec(E) && A && S->GetPrimaryInstance()==A && A->GetCurrentAbilitySpecHandle()==E.Handle
		&& A->GetCurrentActivationInfo().GetActivationPredictionKey()==E.Key;
}
bool ReadFloat(const UObject* O,const TCHAR* Field,float& Value)
{
	const auto* P=O?FindFProperty<FFloatProperty>(O->GetClass(),Field):nullptr;
	if(!P)return false;Value=P->GetPropertyValue_InContainer(O);return FMath::IsFinite(Value);
}
bool Capture(FEndpoint& E,ABwayCharacterWithAbilities* Pawn)
{
	E.Pawn=Pawn;E.Controller=Cast<ABwayPlayerController>(Pawn->GetController());E.State=Pawn->GetPlayerState<ABwayPlayerState>();
	E.ASC=E.State.IsValid()?E.State->GetLyraAbilitySystemComponent():nullptr;
	E.Round=Pawn->GetWorld()->GetGameState()?Pawn->GetWorld()->GetGameState()->FindComponentByClass<UBwayRoundManagementComponent>():nullptr;
	E.RoundNumber=E.Round.IsValid()?E.Round->GetCurrentRoundNumber():INDEX_NONE;
	if(!Live(E))return false;
	E.Position=Pawn->GetActorLocation();E.Health=Health(E);
	return FMath::IsNearlyEqual(E.Health,E.ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()),.01f)
		&& Pawn->GetVelocity().Size()<=1. && Pawn->GetPendingMovementInputVector().IsNearlyZero(.01) && E.Controller->RotationInput.IsNearlyZero(.01)
		&& !E.ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped);
}
bool Policy(FRequest& R,FEndpoint& E,int32 Index)
{
	int32 Matches=0;
	for(const auto& S:E.ASC->GetActivatableAbilities())if(S.Ability && S.Ability->IsA(R.Family.Get())){E.Handle=S.Handle;++Matches;}
	const auto* S=Spec(E);const auto* A=S?Cast<ULyraGameplayAbility>(S->Ability):nullptr;
	const auto* Remote=A?FindFProperty<FBoolProperty>(A->GetClass(),TEXT("bServerRespectsRemoteAbilityCancellation")):nullptr;
	const bool Respects=Remote && Remote->GetPropertyValue_InContainer(A);
	const bool Ready=A && !S->IsActive() && A->CheckCooldown(E.Handle,E.ASC->AbilityActorInfo.Get());
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const bool Supported=Matches==1 && A && A->GetInstancingPolicy()==EGameplayAbilityInstancingPolicy::InstancedPerActor
		&& A->GetNetExecutionPolicy()==EGameplayAbilityNetExecutionPolicy::LocalPredicted && A->GetNetSecurityPolicy()==EGameplayAbilityNetSecurityPolicy::ClientOrServer
		&& A->GetActivationPolicy()==ELyraAbilityActivationPolicy::OnInputTriggered && Respects && Ready && E.ASC->GetTagCount(R.CooldownTag)==0;
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Policy player=%d endpoint=%s matches=%d handle=%s class=%s instancing=%d execution=%d security=%d activation=%d remoteProperty=%d respectsRemote=%d cooldownReady=%d cooldownTag=%d active=%d inputPressed=%d accepted=%d guard=%d"),
		R.PlayerId,Name(Index),Matches,*E.Handle.ToString(),*GetPathNameSafe(A?A->GetClass():nullptr),A?int32(A->GetInstancingPolicy()):-1,A?int32(A->GetNetExecutionPolicy()):-1,A?int32(A->GetNetSecurityPolicy()):-1,A?int32(A->GetActivationPolicy()):-1,Remote!=nullptr,Respects,Ready,E.ASC->GetTagCount(R.CooldownTag),S?int32(S->IsActive()):-1,S?int32(S->InputPressed):-1,Supported,GAllowActorScriptExecutionInEditor);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if(Supported)E.AbilityClass=A->GetClass();return Supported;
}
bool RememberActivation(FRequest& R,FEndpoint& E,int32 Index)
{
	if(E.Instance.IsValid())return SameActivation(E);
	const auto* S=Spec(E);auto* A=S?Cast<ULyraGameplayAbility>(S->GetPrimaryInstance()):nullptr;
	if(!SameSpec(E) || !A || !S->IsActive())return false;
	E.Instance=A;E.Key=A->GetCurrentActivationInfo().GetActivationPredictionKey();
	const auto Info=A->GetCurrentActivationInfo();const auto* Remote=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("bServerRespectsRemoteAbilityCancellation"));
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Activation player=%d endpoint=%s instance=%s handle=%s key=%d keyBase=%d mode=%d canBeEndedByOther=%d canBeCanceled=%d respectsRemote=%d security=%d elapsed=%.6f worldTime=%.6f guard=%d"),
		R.PlayerId,Name(Index),*A->GetPathName(),*E.Handle.ToString(),E.Key.Current,E.Key.Base,int32(Info.ActivationMode),int32(Info.bCanBeEndedByOtherInstance),A->CanBeCanceled(),Remote?int32(Remote->GetPropertyValue_InContainer(A)):-1,int32(A->GetNetSecurityPolicy()),FPlatformTime::Seconds()-R.Start,E.Pawn->GetWorld()->GetTimeSeconds(),GAllowActorScriptExecutionInEditor);
	if(!E.Key.IsValidKey() || !Info.bCanBeEndedByOtherInstance || !A->CanBeCanceled() || !Remote || !Remote->GetPropertyValue_InContainer(A)
		|| A->GetNetSecurityPolicy()!=EGameplayAbilityNetSecurityPolicy::ClientOrServer){Fail(R,TEXT("unsupported-active-cancellation-policy"));return false;}
	return true;
}
void State(FRequest& R,const TCHAR* Phase)
{
	if(!AllLive(R))return;
	for(int32 I=0;I<2;++I)
	{
		const auto& E=Endpoint(R,I);const auto* S=Spec(E);float Remaining=0.f,Duration=0.f;
		if(S && S->Ability)S->Ability->GetCooldownTimeRemainingAndDuration(E.Handle,E.ASC->AbilityActorInfo.Get(),Remaining,Duration);
		const FVector P=E.Pawn->GetActorLocation();
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: State player=%d endpoint=%s phase=%s elapsed=%.6f worldTime=%.6f active=%d movementTag=%d cooldownTag=%d cooldownRemaining=%.6f cooldownDuration=%.6f health=%.6f x=%.6f y=%.6f z=%.6f speed=%.6f shots=%d ends=%d sameActivation=%d guard=%d"),
			R.PlayerId,Name(I),Phase,FPlatformTime::Seconds()-R.Start,E.Pawn->GetWorld()->GetTimeSeconds(),Active(E),E.ASC->GetTagCount(TAG_Gameplay_MovementStopped),E.ASC->GetTagCount(R.CooldownTag),Remaining,Duration,Health(E),P.X,P.Y,P.Z,E.Pawn->GetVelocity().Size(),R.ShotCount,E.Ends,SameActivation(E),GAllowActorScriptExecutionInEditor);
	}
}
void Bind(FRequest& R)
{
	const TWeakPtr<FRequest> Weak=Pending;
	for(int32 I=0;I<2;++I)
	{
		auto& E=Endpoint(R,I);
		E.EndDelegate=E.ASC->OnAbilityEnded.AddLambda([Weak,I](const FAbilityEndedData& Data)
		{
			const auto P=Weak.Pin();if(!P)return;auto& Q=*P;auto& End=Endpoint(Q,I);if(Data.AbilitySpecHandle!=End.Handle)return;
			auto* A=Data.AbilityThatEnded.Get();const auto Key=A?A->GetCurrentActivationInfo().GetActivationPredictionKey():FPredictionKey();
			const bool Exact=AllLive(Q) && SameActivation(End) && A==End.Instance.Get() && Key==End.Key;
			++End.Ends;End.EndAt=FPlatformTime::Seconds();
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: AbilityEnded player=%d endpoint=%s pawn=%s asc=%s instance=%s handle=%s key=%d keyBase=%d expectedKey=%d exact=%d cancelled=%d activeAfter=%d movementTag=%d cooldownTag=%d health=%.6f shots=%d elapsed=%.6f worldTime=%.6f cleanup=%d guard=%d"),
				Q.PlayerId,Name(I),*GetPathNameSafe(End.Pawn.Get()),*GetPathNameSafe(End.ASC.Get()),*GetPathNameSafe(A),*Data.AbilitySpecHandle.ToString(),Key.Current,Key.Base,End.Key.Current,Exact,Data.bWasCancelled,Active(End),End.ASC.IsValid()?End.ASC->GetTagCount(TAG_Gameplay_MovementStopped):-1,End.ASC.IsValid()?End.ASC->GetTagCount(Q.CooldownTag):-1,Health(End),Q.ShotCount,End.EndAt-Q.Start,End.Pawn.IsValid()?End.Pawn->GetWorld()->GetTimeSeconds():-1.,Q.bEvidenceEnded,GAllowActorScriptExecutionInEditor);
			if(!Q.bEvidenceEnded && (!Exact || !Data.bWasCancelled || !Q.bDispatched || End.Ends!=1))Fail(Q,TEXT("unexpected-ability-end"));
		});
		E.HealthDelegate=E.ASC->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute()).AddLambda([Weak,I](const FOnAttributeChangeData& Data)
		{
			const auto P=Weak.Pin();if(!P)return;
			// Replication may broadcast a same-value attribute refresh. Retain the
			// notification, but only a real delta or invalid value changes this gate.
			const bool Changed=!FMath::IsFinite(Data.OldValue) || !FMath::IsFinite(Data.NewValue) || Data.OldValue!=Data.NewValue;
			if(Changed)++P->HealthChanges;
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: HealthChange player=%d endpoint=%s before=%.6f after=%.6f changed=%d elapsed=%.6f cleanup=%d guard=%d"),P->PlayerId,Name(I),Data.OldValue,Data.NewValue,Changed,FPlatformTime::Seconds()-P->Start,P->bEvidenceEnded,GAllowActorScriptExecutionInEditor);
			if(Changed && !P->bEvidenceEnded)Fail(*P,TEXT("health-changed-during-cancellation"));
		});
	}
	R.SpawnDelegate=R.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([Weak](AActor* Actor)
	{
		const auto P=Weak.Pin();if(!P || !P->bRequested || !Actor || !Actor->IsA(P->BulletFamily.Get()) || Actor->GetWorld()!=P->World.Get()
			|| Actor->GetOwner()!=P->Authority.Pawn.Get() || Actor->GetInstigator()!=P->Authority.Pawn.Get())return;
		const double Now=FPlatformTime::Seconds();++P->ShotCount;P->Projectiles.Add(Actor);
		if(P->ShotCount==1){P->FirstShotAt=Now;P->FirstProjectile=Actor->GetPathName();}
		const bool Identity=AllLive(*P) && RememberActivation(*P,P->Authority,1);
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Shot player=%d index=%d projectile=%s owner=%s instance=%s handle=%s key=%d identity=%d afterAuthorityEnd=%d elapsed=%.6f worldTime=%.6f cleanup=%d guard=%d"),
			P->PlayerId,P->ShotCount,*Actor->GetPathName(),*GetPathNameSafe(Actor->GetOwner()),*GetPathNameSafe(P->Authority.Instance.Get()),*P->Authority.Handle.ToString(),P->Authority.Key.Current,Identity,P->Authority.Ends>0,Now-P->Start,Actor->GetWorld()->GetTimeSeconds(),P->bEvidenceEnded,GAllowActorScriptExecutionInEditor);
		if(!P->bEvidenceEnded && (!Identity || P->Authority.Ends>0))Fail(*P,TEXT("shot-after-end-or-identity-change"));
	}));
}
bool Setup(FRequest& R)
{
	ABwayCharacterWithAbilities *Owner=nullptr,*Authority=nullptr;int32 Owners=0,Authorities=0,Worlds=0;
	if(GEngine)for(const FWorldContext& Context:GEngine->GetWorldContexts())
	{
		UWorld* W=Context.World();if(!W || !W->IsGameWorld() || (W->GetNetMode()!=NM_Client && W->GetNetMode()!=NM_ListenServer))continue;++Worlds;
		for(TActorIterator<ABwayCharacterWithAbilities> It(W);It;++It)if(auto* PS=It->GetPlayerState<ABwayPlayerState>();PS && PS->GetPlayerId()==R.PlayerId && PS->GetPawn()==*It)
		{
			if(W->GetNetMode()==NM_Client && It->IsLocallyControlled()){Owner=*It;++Owners;}
			else if(W->GetNetMode()==NM_ListenServer && !It->IsLocallyControlled()){Authority=*It;++Authorities;}
		}
	}
	if(Worlds!=4 || Owners!=1 || Authorities!=1){Fail(R,TEXT("unsupported-four-world-owner-topology"));return false;}
	R.World=Authority->GetWorld();if(!Capture(R.Owner,Owner) || !Capture(R.Authority,Authority) || !AllLive(R) || R.Owner.RoundNumber!=R.Authority.RoundNumber){Fail(R,TEXT("healthy-idle-owner-chain-required"));return false;}
	R.Family=FindObject<UClass>(nullptr,TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage"));
	R.BulletFamily=FindObject<UClass>(nullptr,TEXT("/Script/HeroRawlinsRuntime.BwayRawlinsBulletProjectile"));
	R.CooldownTag=FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Rawlins.BlazingBarrage"),false);
	if(!R.Family.IsValid() || !R.BulletFamily.IsValid() || !R.CooldownTag.IsValid()){Fail(R,TEXT("loaded-family-or-tag-unavailable"));return false;}
	if(!Policy(R,R.Owner,0) || !Policy(R,R.Authority,1) || R.Owner.AbilityClass!=R.Authority.AbilityClass){Fail(R,TEXT("unsupported-policy-or-granted-spec"));return false;}
	const auto* A=Spec(R.Authority)->Ability.Get();const auto* ConfigProp=FindFProperty<FSoftObjectProperty>(A->GetClass(),TEXT("KitConfig"));
	const UObject* Config=ConfigProp?ConfigProp->GetPropertyValue_InContainer(A).Get():nullptr;const auto* Count=Config?FindFProperty<FIntProperty>(Config->GetClass(),TEXT("BarrageShotCount")):nullptr;
	const auto* Stats=R.Authority.ASC->GetSet<UBwayHeroAttributeSet>();
	if(!Count || !Stats || !FMath::IsNearlyZero(Stats->GetCooldownReduction(),.001f) || !ReadFloat(Config,TEXT("BarrageShotInterval"),R.Interval)
		|| !ReadFloat(Config,TEXT("BarrageProjectileLifeSpan"),R.Life) || !ReadFloat(Config,TEXT("BarrageCooldown"),R.Cooldown)){Fail(R,TEXT("loaded-cancellation-tuning-unavailable"));return false;}
	R.LoadedCount=Count->GetPropertyValue_InContainer(Config);
	if(R.LoadedCount!=12 || R.Interval<.05f || R.Interval>.2f || R.Life<=0.f || R.Life>2.f || R.Cooldown<20.f){Fail(R,TEXT("unsupported-cancellation-timing"));return false;}
	for(TActorIterator<AActor> It(R.World.Get(),R.BulletFamily.Get());It;++It)if(It->GetOwner()==Authority){Fail(R,TEXT("prior-owned-projectile"));return false;}
	int32 Peer=0,ExpectedLag=INDEX_NONE;
	for(const FWorldContext& Context:GEngine->GetWorldContexts())if(UWorld* W=Context.World();W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer))
	{
		const UNetDriver* D=W->GetNetDriver();if(!D){Fail(R,TEXT("missing-driver"));return false;}const auto& N=D->PacketSimulationSettings;
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Network player=%d peer=%d world=%s driver=%s lag=%d lagMin=%d lagMax=%d incomingMin=%d incomingMax=%d loss=%d incomingLoss=%d jitter=%d variance=%d"),R.PlayerId,Peer++,*W->GetPathName(),*D->GetPathName(),N.PktLag,N.PktLagMin,N.PktLagMax,N.PktIncomingLagMin,N.PktIncomingLagMax,N.PktLoss,N.PktIncomingLoss,N.PktJitter,N.PktLagVariance);
		if(ExpectedLag==INDEX_NONE)ExpectedLag=N.PktLag;
		if((N.PktLag!=0 && N.PktLag!=100) || N.PktLag!=ExpectedLag || N.PktLagMin || N.PktLagMax || N.PktIncomingLagMin || N.PktIncomingLagMax || N.PktLoss || N.PktIncomingLoss || N.PktJitter || N.PktLagVariance){Fail(R,TEXT("unsupported-network-settings"));return false;}
	}
	R.Transport=2.*ExpectedLag/1000.;R.bPrepared=true;
	for(int32 I=0;I<2;++I)
	{
		const auto& E=Endpoint(R,I);
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Endpoint player=%d endpoint=%s pawn=%s controller=%s state=%s asc=%s round=%d handle=%s class=%s net=%d role=%d local=%d health=%.6f x=%.6f y=%.6f z=%.6f guard=%d"),R.PlayerId,Name(I),*E.Pawn->GetPathName(),*E.Controller->GetPathName(),*E.State->GetPathName(),*E.ASC->GetPathName(),E.RoundNumber,*E.Handle.ToString(),*E.AbilityClass->GetPathName(),int32(E.Pawn->GetNetMode()),int32(E.Pawn->GetLocalRole()),E.Pawn->IsLocallyControlled(),E.Health,E.Position.X,E.Position.Y,E.Position.Z,GAllowActorScriptExecutionInEditor);
	}
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Plan player=%d config=%s count=%d interval=%.6f life=%.6f cooldown=%.6f cooldownReduction=%.6f transport=%.6f projectileClass=%s timeout=5.0 guard=%d"),R.PlayerId,*GetPathNameSafe(Config),R.LoadedCount,R.Interval,R.Life,R.Cooldown,Stats->GetCooldownReduction(),R.Transport,*R.BulletFamily->GetPathName(),GAllowActorScriptExecutionInEditor);
	Bind(R);R.bRequested=true;R.CastAt=FPlatformTime::Seconds();R.bAccepted=R.Owner.ASC->TryActivateAbility(R.Owner.Handle,true);
	const bool OwnerActivation=AllLive(R) && RememberActivation(R,R.Owner,0);
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Dispatch player=%d accepted=%d ownerActivation=%d owner=%s authority=%s ownerHandle=%s authorityHandle=%s elapsed=%.6f guard=%d"),R.PlayerId,R.bAccepted,OwnerActivation,*GetPathNameSafe(R.Owner.Pawn.Get()),*GetPathNameSafe(R.Authority.Pawn.Get()),*R.Owner.Handle.ToString(),*R.Authority.Handle.ToString(),R.CastAt-R.Start,GAllowActorScriptExecutionInEditor);
	if(!R.bAccepted || !OwnerActivation){Fail(R,TEXT("owner-activation-not-accepted"));return false;}return true;
}
void EvidenceEnd(FRequest& R)
{
	if(R.bEvidenceEnded)return;State(R,TEXT("evidence-end"));R.bEvidenceEnded=true;
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: EvidenceEnd player=%d reason=%s elapsed=%.6f shots=%d ownerEnds=%d authorityEnds=%d healthChanges=%d guard=%d"),R.PlayerId,R.Failure.IsEmpty()?TEXT("observed"):*R.Failure,FPlatformTime::Seconds()-R.Start,R.ShotCount,R.Owner.Ends,R.Authority.Ends,R.HealthChanges,GAllowActorScriptExecutionInEditor);
}
bool Finish(FRequest& R,bool Hard)
{
	EvidenceEnd(R);
	// Failure cleanup must not follow an InstancedPerActor object into a newer cast.
	for(int32 I=0;I<2;++I)
	{
		auto& E=Endpoint(R,I);
		if(!GAllowActorScriptExecutionInEditor && Live(E) && SameActivation(E) && Active(E) && !E.bCleanupCancel)
		{
			E.bCleanupCancel=true;const auto Instance=E.Instance;const auto Handle=E.Handle;const auto Key=E.Key;
			E.ASC->CancelAbilitiesByFunc([Instance,Handle,Key](const ULyraGameplayAbility* A,FGameplayAbilitySpecHandle H){return A==Instance.Get() && H==Handle && A->GetCurrentActivationInfo().GetActivationPredictionKey()==Key;},true);
		}
	}
	int32 Removed=0;
	for(int32 I=0;I<2;++I)
	{
		auto& E=Endpoint(R,I);if(auto* ASC=E.ASC.Get())
		{
			if(E.EndDelegate.IsValid()){ASC->OnAbilityEnded.Remove(E.EndDelegate);++Removed;}
			if(E.HealthDelegate.IsValid()){ASC->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute()).Remove(E.HealthDelegate);++Removed;}
		}
	}
	if(auto* W=R.World.Get();W && R.SpawnDelegate.IsValid()){W->RemoveOnActorSpawnedHandler(R.SpawnDelegate);++Removed;}
	const bool Known=R.bPrepared && AllLive(R) && SameSpec(R.Owner) && SameSpec(R.Authority);
	const bool Clean=!R.bRequested || (Known && SameActivation(R.Owner) && SameActivation(R.Authority) && !Active(R.Owner) && !Active(R.Authority)
		&& !R.Owner.ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped) && !R.Authority.ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped));
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Complete player=%d reason=%s cleanup=%d stateKnown=%d ownerFallback=%d authorityFallback=%d delegatesRemoved=%d shots=%d predicateMatches=%d ownerEnds=%d authorityEnds=%d healthChanges=%d hardDeadline=%d inputWrites=0 transformWrites=0 guard=%d outcome=observed"),R.PlayerId,R.Failure.IsEmpty()?TEXT("observed"):*R.Failure,Clean,Known,R.Owner.bCleanupCancel,R.Authority.bCleanupCancel,Removed,R.ShotCount,R.PredicateMatches,R.Owner.Ends,R.Authority.Ends,R.HealthChanges,Hard,GAllowActorScriptExecutionInEditor);
	Pending.Reset();Ticker.Reset();return false;
}
bool Tick(float Delta)
{
	(void)Delta;const auto Keep=Pending;if(!Keep)return false;auto& R=*Keep;const double Now=FPlatformTime::Seconds();
	if(Now-R.Start>=5.){Fail(R,TEXT("hard-timeout"));return Finish(R,true);}
	if(GAllowActorScriptExecutionInEditor)return true;
	if(R.bAbort){Fail(R,TEXT("observer-abort"));return Finish(R,false);}
	if(!R.bRequested){if(!Setup(R))return Finish(R,false);return true;}
	if(!AllLive(R) || !SameSpec(R.Owner) || !SameSpec(R.Authority)){Fail(R,TEXT("original-ownership-or-spec-invalidated"));return Finish(R,false);}
	if(!FMath::IsNearlyEqual(Health(R.Owner),R.Owner.Health,.01f) || !FMath::IsNearlyEqual(Health(R.Authority),R.Authority.Health,.01f)){Fail(R,TEXT("original-full-health-changed"));return Finish(R,false);}
	if(!SameActivation(R.Owner) || (R.Authority.Instance.IsValid() && !SameActivation(R.Authority))){Fail(R,TEXT("activation-identity-changed"));return Finish(R,false);}
	if(!R.Failure.IsEmpty())return Finish(R,false);
	if(Now-R.LastSample>=.05){State(R,R.bDispatched?TEXT("after-cancel"):TEXT("await-first-shot"));R.LastSample=Now;}
	if(!R.bDispatched)
	{
		if(R.ShotCount==0){if(Now-R.CastAt>1.){Fail(R,TEXT("authority-first-shot-timeout"));return Finish(R,false);}return true;}
		if(R.ShotCount!=1 || Now-R.FirstShotAt>=R.Interval || !Active(R.Owner) || !Active(R.Authority) || !RememberActivation(R,R.Authority,1)
			|| R.Owner.Key!=R.Authority.Key){Fail(R,TEXT("first-shot-cancellation-setup-missed"));return Finish(R,false);}
		if(!R.Owner.Instance->CanBeCanceled() || !R.Authority.Instance->CanBeCanceled()){Fail(R,TEXT("active-instance-no-longer-cancellable"));return Finish(R,false);}
		R.bDispatched=true;R.CancelAt=Now;
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: CancelDispatch player=%d owner=%s authority=%s ownerASC=%s authorityASC=%s ownerInstance=%s authorityInstance=%s ownerHandle=%s authorityHandle=%s ownerKey=%d authorityKey=%d firstProjectile=%s shots=%d firstShotElapsed=%.6f elapsed=%.6f age=%.6f interval=%.6f ownerHealth=%.6f authorityHealth=%.6f ownerActive=%d authorityActive=%d replicate=1 guard=%d"),R.PlayerId,*R.Owner.Pawn->GetPathName(),*R.Authority.Pawn->GetPathName(),*R.Owner.ASC->GetPathName(),*R.Authority.ASC->GetPathName(),*R.Owner.Instance->GetPathName(),*R.Authority.Instance->GetPathName(),*R.Owner.Handle.ToString(),*R.Authority.Handle.ToString(),R.Owner.Key.Current,R.Authority.Key.Current,*R.FirstProjectile,R.ShotCount,R.FirstShotAt-R.Start,Now-R.Start,Now-R.FirstShotAt,R.Interval,Health(R.Owner),Health(R.Authority),Active(R.Owner),Active(R.Authority),GAllowActorScriptExecutionInEditor);
		const auto Instance=R.Owner.Instance;const auto Handle=R.Owner.Handle;const auto Key=R.Owner.Key;
		R.Owner.ASC->CancelAbilitiesByFunc([&R,Instance,Handle,Key](const ULyraGameplayAbility* A,FGameplayAbilitySpecHandle H)
		{
			const bool Match=A==Instance.Get() && H==Handle && A->GetCurrentActivationInfo().GetActivationPredictionKey()==Key;if(Match)++R.PredicateMatches;return Match;
		},true);
		// The cancellation callback is synchronous locally; resolve all weak state again.
		if(!AllLive(R) || R.PredicateMatches!=1 || !SameActivation(R.Owner) || !SameActivation(R.Authority)){Fail(R,TEXT("cancel-predicate-or-lifetime-mismatch"));return Finish(R,false);}
		State(R,TEXT("cancel-return"));return true;
	}
	if(Now-R.CancelAt>R.Transport+.5 && (Active(R.Owner) || Active(R.Authority) || R.Owner.Ends!=1 || R.Authority.Ends!=1)){Fail(R,TEXT("replicated-cancel-end-timeout"));return Finish(R,false);}
	const double Observation=R.LoadedCount*R.Interval+R.Life+R.Transport+.25;
	if(Now-R.CastAt>=Observation)
	{
		bool LiveProjectiles=false;for(const auto& A:R.Projectiles)if(A.IsValid() && !A->IsActorBeingDestroyed())LiveProjectiles=true;
		if(Active(R.Owner) || Active(R.Authority) || R.Owner.Ends!=1 || R.Authority.Ends!=1 || R.ShotCount<1 || R.ShotCount>=R.LoadedCount || LiveProjectiles || R.HealthChanges){Fail(R,TEXT("incomplete-cancellation-observation"));}
		if(R.Owner.ASC->GetTagCount(R.CooldownTag)==0 || R.Authority.ASC->GetTagCount(R.CooldownTag)==0)Fail(R,TEXT("committed-cooldown-not-retained"));
		State(R,TEXT("settled"));return Finish(R,false);
	}
	return true;
}
bool Parse(const TArray<FString>& Args,int32& Id)
{
	if(Args.Num()!=1 || Args[0].IsEmpty())return false;Id=0;for(TCHAR C:Args[0]){const int32 D=int32(C)-int32(TEXT('0'));if(D<0 || D>9 || Id>(MAX_int32-D)/10)return false;Id=Id*10+D;}return true;
}
FAutoConsoleCommand StartCommand(TEXT("bway.Test.BarrageOwnerCancel"),TEXT("Observe exact remote-owner replicated GAS cancellation: <player-id>."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
	int32 Id=0;if(!Parse(Args,Id) || Pending){UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Rejected reason=invalid-id-or-pending"));return;}
	Pending=MakeShared<FRequest>();Pending->PlayerId=Id;Pending->Start=FPlatformTime::Seconds();Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick),0.f);
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageOwnerCancel: Queued player=%d timeout=5.0 scope=remote-owner-gas-cancellation"),Id);
}));
FAutoConsoleCommand AbortCommand(TEXT("bway.Test.BarrageOwnerCancelAbort"),TEXT("Request captured cancellation fixture cleanup: <player-id>."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
	int32 Id=0;if(Pending && Parse(Args,Id) && Pending->PlayerId==Id)Pending->bAbort=true;
}));
}
#endif
