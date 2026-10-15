// Bounded healthy-avatar reset observation; accepted owner-cancel fixture remains independent.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "GameState/BwayScoringComponent.h"
#include "GameplayEffect.h"
#include "Misc/DateTime.h"
#include "UObject/ObjectKey.h"
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

namespace BwayBarrageRoundResetTest
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
	int32 PlayerId=0, ShotCount=0, HealthChanges=0, LoadedCount=0;
	FEndpoint Owner, Authority;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UClass> Family, BulletFamily;
	FGameplayTag CooldownTag;
	FDelegateHandle SpawnDelegate;
	TArray<TWeakObjectPtr<AActor>> Projectiles;
	FString FirstProjectile, Failure, OwnerOldPath, AuthorityOldPath;
	FObjectKey OwnerOldKey, AuthorityOldKey;
	FEndpoint NewOwner, NewAuthority;
	TArray<FEndpoint> Roster;
	TArray<TWeakObjectPtr<ABwayCharacterWithAbilities>> NewRoster;
	TArray<FString> OldRosterPaths;
	TArray<int32> Teams;
	TWeakObjectPtr<UBwayScoringComponent> Scoring;
	TArray<int32> Scores;
	FActiveGameplayEffectHandle CooldownEffect;
	TWeakObjectPtr<const UGameplayEffect> CooldownDefinition;
	float CooldownExpiry=0.f, CooldownDuration=0.f;
	double ReplacementAt=0.;
	bool bReplacements=false, bResetCallInProgress=false;
	double Start=0., CastAt=0., FirstShotAt=0., CancelAt=0., LastSample=0., Transport=0.;
	float Interval=0.f, Life=0.f, Cooldown=0.f;
	bool bPrepared=false, bRequested=false, bAccepted=false, bDispatched=false, bEvidenceEnded=false, bAbort=false;
};
TSharedPtr<FRequest> Pending, FreshTicket;
bool bFreshRequested=false;
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
		&& A->GetActivationPolicy()==ELyraAbilityActivationPolicy::OnInputTriggered && Ready && E.ASC->GetTagCount(R.CooldownTag)==0;
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Policy player=%d endpoint=%s matches=%d handle=%s class=%s instancing=%d execution=%d security=%d activation=%d remoteProperty=%d respectsRemote=%d cooldownReady=%d cooldownTag=%d active=%d inputPressed=%d accepted=%d guard=%d"),
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
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Activation player=%d endpoint=%s instance=%s handle=%s key=%d keyBase=%d mode=%d canBeEndedByOther=%d canBeCanceled=%d respectsRemote=%d security=%d elapsed=%.6f worldTime=%.6f guard=%d"),
		R.PlayerId,Name(Index),*A->GetPathName(),*E.Handle.ToString(),E.Key.Current,E.Key.Base,int32(Info.ActivationMode),int32(Info.bCanBeEndedByOtherInstance),A->CanBeCanceled(),Remote?int32(Remote->GetPropertyValue_InContainer(A)):-1,int32(A->GetNetSecurityPolicy()),FPlatformTime::Seconds()-R.Start,E.Pawn->GetWorld()->GetTimeSeconds(),GAllowActorScriptExecutionInEditor);
	if(!E.Key.IsValidKey() || !A->CanBeCanceled() || A->GetAssetTags().HasTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Behavior.SurvivesDeath")))
		|| A->GetNetSecurityPolicy()!=EGameplayAbilityNetSecurityPolicy::ClientOrServer){Fail(R,TEXT("unsupported-active-cancellation-policy"));return false;}
	return true;
}

// During expected UnPossess/Destroy, the original pawn chain is deliberately absent.
// Only persistent identities and the captured activation may authorize observations/cleanup.
bool Persistent(const FEndpoint& E)
{
	const auto* C=E.Controller.Get();const auto* S=E.State.Get();const auto* A=E.ASC.Get();const auto* Round=E.Round.Get();
	const auto* W=C?C->GetWorld():nullptr;const auto* GS=W?W->GetGameState():nullptr;
	return C && S && A && Round && C->GetPlayerState<ABwayPlayerState>()==S && S->GetLyraAbilitySystemComponent()==A && A->GetOwnerActor()==S
		&& S->GetWorld()==W && A->GetWorld()==W && GS && GS->FindComponentByClass<UBwayRoundManagementComponent>()==Round
		&& Round->GetCurrentRoundNumber()==E.RoundNumber && Round->GetCurrentMatchPhase()==EBwayMatchPhase::Playing && Round->GetCurrentRoundState()==ERoundState::RoundActive;
}
FEndpoint& NewEndpoint(FRequest& R,int32 I){return I==0?R.NewOwner:R.NewAuthority;}
bool CaptureRoster(FRequest& R)
{
	auto* GS=R.World->GetGameState<ABwayGameState>();R.Scoring=GS?GS->FindComponentByClass<UBwayScoringComponent>():nullptr;
	if(!R.Scoring.IsValid())return false;R.Scores=R.Scoring->GetAllScores();
	TSet<int32> Ids;
	for(FConstPlayerControllerIterator It=R.World->GetPlayerControllerIterator();It;++It)
	{
		auto* C=Cast<ABwayPlayerController>(It->Get());auto* P=C?Cast<ABwayCharacterWithAbilities>(C->GetPawn()):nullptr;FEndpoint E;
		if(!P || !Capture(E,P) || E.RoundNumber!=R.Authority.RoundNumber || Ids.Contains(E.State->GetPlayerId()))return false;
		Ids.Add(E.State->GetPlayerId());R.OldRosterPaths.Add(P->GetPathName());R.Teams.Add(E.State->GetTeamId());R.Roster.Add(E);R.NewRoster.Add(nullptr);
	}
	return R.Roster.Num()==4 && Ids.Contains(R.PlayerId);
}
void LogRoster(FRequest& R,const TCHAR* Phase)
{
	for(int32 I=0;I<R.Roster.Num();++I)
	{
		const auto& E=R.Roster[I];APawn* P=E.Controller.IsValid()?E.Controller->GetPawn():nullptr;auto* A=E.ASC.Get();auto* H=ULyraHealthComponent::FindHealthComponent(P);
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Roster player=%d member=%d phase=%s oldPawn=%s pawn=%s controller=%s state=%s asc=%s avatar=%s oldAlive=%d persistent=%d team=%d round=%d matchPhase=%d roundState=%d health=%.6f maxHealth=%.6f elapsed=%.6f guard=%d"),
			R.PlayerId,E.State.IsValid()?E.State->GetPlayerId():-1,Phase,*R.OldRosterPaths[I],*GetPathNameSafe(P),*GetPathNameSafe(E.Controller.Get()),*GetPathNameSafe(E.State.Get()),*GetPathNameSafe(A),*GetPathNameSafe(A?A->GetAvatarActor():nullptr),E.Pawn.IsValid() && !E.Pawn->IsActorBeingDestroyed(),Persistent(E),E.State.IsValid()?E.State->GetTeamId():-1,E.Round.IsValid()?E.Round->GetCurrentRoundNumber():-1,E.Round.IsValid()?int32(E.Round->GetCurrentMatchPhase()):-1,E.Round.IsValid()?int32(E.Round->GetCurrentRoundState()):-1,H?H->GetHealth():-1.f,H?H->GetMaxHealth():-1.f,FPlatformTime::Seconds()-R.Start,GAllowActorScriptExecutionInEditor);
	}
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Scores player=%d phase=%s team0=%d team1=%d elapsed=%.6f"),R.PlayerId,Phase,R.Scoring.IsValid()?R.Scoring->GetTeamScore(0):-1,R.Scoring.IsValid()?R.Scoring->GetTeamScore(1):-1,FPlatformTime::Seconds()-R.Start);
}
bool RosterPersistent(FRequest& R)
{
	if(!R.World.IsValid() || R.Roster.Num()!=4 || !R.Scoring.IsValid() || R.Scoring->GetAllScores()!=R.Scores)return false;
	int32 Controllers=0;for(FConstPlayerControllerIterator It=R.World->GetPlayerControllerIterator();It;++It)++Controllers;
	if(Controllers!=4)return false;
	for(int32 I=0;I<R.Roster.Num();++I)if(!Persistent(R.Roster[I]) || R.Roster[I].State->GetTeamId()!=R.Teams[I])return false;
	return Persistent(R.Owner) && Persistent(R.Authority);
}
TArray<FActiveGameplayEffectHandle> CooldownEffects(const FRequest& R,const FEndpoint& E)
{
	FGameplayTagContainer Tags;Tags.AddTag(R.CooldownTag);
	return E.ASC.IsValid()?E.ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Tags)):TArray<FActiveGameplayEffectHandle>();
}
bool RetainedCooldown(FRequest& R)
{
	if(!Persistent(R.Authority) || !R.CooldownEffect.IsValid())return false;
	const auto Effects=CooldownEffects(R,R.Authority);const auto* GE=R.Authority.ASC->GetActiveGameplayEffect(R.CooldownEffect);
	return Effects.Num()==1 && Effects[0]==R.CooldownEffect && GE && GE->Spec.Def==R.CooldownDefinition.Get()
		&& FMath::IsNearlyEqual(GE->GetDuration(),R.CooldownDuration,.001f) && FMath::IsNearlyEqual(GE->GetEndTime(),R.CooldownExpiry,.001f)
		&& GE->GetTimeRemaining(R.World->GetTimeSeconds())>0.f;
}
void Effects(FRequest& R,const FEndpoint& E,int32 I,const TCHAR* Phase)
{
	if(!E.ASC.IsValid() || !E.ASC->GetWorld())return;const float Now=E.ASC->GetWorld()->GetTimeSeconds();const auto Handles=CooldownEffects(R,E);
	for(const auto Handle:Handles)if(const auto* GE=E.ASC->GetActiveGameplayEffect(Handle))
	{
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: CooldownEffect player=%d endpoint=%s phase=%s asc=%s handle=%s definition=%s count=%d duration=%.6f remaining=%.6f start=%.6f serverStart=%.6f expiry=%.6f worldTime=%.6f elapsed=%.6f capturedAuthority=%d"),
			R.PlayerId,Name(I),Phase,*E.ASC->GetPathName(),*Handle.ToString(),*GetPathNameSafe(GE->Spec.Def.Get()),Handles.Num(),GE->GetDuration(),GE->GetTimeRemaining(Now),GE->StartWorldTime,GE->StartServerWorldTime,GE->GetEndTime(),Now,FPlatformTime::Seconds()-R.Start,I==1 && Handle==R.CooldownEffect);
	}
}
void State(FRequest& R,const TCHAR* Phase)
{
	for(int32 I=0;I<2;++I)
	{
		const auto& E=Endpoint(R,I);const auto& N=NewEndpoint(R,I);const auto* Old=Spec(E);const auto* New=Spec(N);
		const auto* A=E.ASC.Get();APawn* P=E.Controller.IsValid()?E.Controller->GetPawn():nullptr;
		const bool Ready=New && New->Ability && A && A->AbilityActorInfo.IsValid() && New->Ability->CheckCooldown(N.Handle,A->AbilityActorInfo.Get());
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: State player=%d endpoint=%s phase=%s oldPawn=%s pawn=%s avatar=%s oldHandle=%s newHandle=%s oldSpec=%d oldActive=%d newActive=%d movementTag=%d cooldownTag=%d cooldownReady=%d health=%.6f persistent=%d ends=%d shots=%d elapsed=%.6f worldTime=%.6f guard=%d"),
			R.PlayerId,Name(I),Phase,I==0?*R.OwnerOldPath:*R.AuthorityOldPath,*GetPathNameSafe(P),*GetPathNameSafe(A?A->GetAvatarActor():nullptr),*E.Handle.ToString(),*N.Handle.ToString(),Old!=nullptr,Old?int32(Old->IsActive()):0,New?int32(New->IsActive()):-1,A?A->GetTagCount(TAG_Gameplay_MovementStopped):-1,A?A->GetTagCount(R.CooldownTag):-1,Ready,Health(E),Persistent(E),E.Ends,R.ShotCount,FPlatformTime::Seconds()-R.Start,A && A->GetWorld()?A->GetWorld()->GetTimeSeconds():-1.,GAllowActorScriptExecutionInEditor);
		Effects(R,E,I,Phase);
	}
}
bool Replacements(FRequest& R)
{
	if(!RosterPersistent(R)){Fail(R,TEXT("persistent-roster-round-team-or-score-changed"));return false;}
	bool All=true;
	for(int32 I=0;I<R.Roster.Num();++I)
	{
		const auto& E=R.Roster[I];auto* P=Cast<ABwayCharacterWithAbilities>(E.Controller->GetPawn());
		if(!R.NewRoster[I].IsExplicitlyNull() && (!R.NewRoster[I].IsValid() || (P && R.NewRoster[I].Get()!=P))){Fail(R,TEXT("third-roster-pawn-lifetime"));return false;}
		if(!P || P==E.Pawn.Get() || P->IsActorBeingDestroyed()){All=false;continue;}
		FEndpoint N;N.Pawn=P;N.Controller=E.Controller;N.State=E.State;N.ASC=E.ASC;N.Round=E.Round;N.RoundNumber=E.RoundNumber;
		if(!Live(N) || (E.Pawn.IsValid() && !E.Pawn->IsActorBeingDestroyed())){All=false;continue;}R.NewRoster[I]=P;
	}
	for(int32 I=0;I<2;++I)
	{
		auto& E=Endpoint(R,I);auto& N=NewEndpoint(R,I);auto* P=Cast<ABwayCharacterWithAbilities>(E.Controller->GetPawn());
		if(!N.Pawn.IsExplicitlyNull() && (!N.Pawn.IsValid() || (P && N.Pawn.Get()!=P))){Fail(R,TEXT("third-endpoint-pawn-lifetime"));return false;}
		if(!P || P==E.Pawn.Get() || P->IsActorBeingDestroyed()){All=false;continue;}
		N.Pawn=P;N.Controller=E.Controller;N.State=E.State;N.ASC=E.ASC;N.Round=E.Round;N.RoundNumber=E.RoundNumber;N.AbilityClass=E.AbilityClass;
		if(!Live(N)){All=false;continue;}
		const FGameplayAbilitySpec* New=nullptr;int32 Count=0;for(auto& S:E.ASC->GetActivatableAbilities())if(S.Ability && S.Ability->IsA(R.Family.Get())){New=&S;++Count;}
		if(Count>1){Fail(R,TEXT("ambiguous-replacement-spec"));return false;}
		if(!New || New->Handle==E.Handle || Spec(E)){All=false;continue;}
		if(N.Handle.IsValid() && N.Handle!=New->Handle){Fail(R,TEXT("third-endpoint-spec-lifetime"));return false;}
		N.Handle=New->Handle;
		if(New->Ability->GetClass()!=E.AbilityClass.Get() || New->IsActive()){Fail(R,TEXT("replacement-class-or-active-state-changed"));return false;}
		if(E.ASC->GetTagCount(TAG_Gameplay_MovementStopped)!=0 || E.ASC->GetTagCount(R.CooldownTag)!=1){All=false;continue;}
		if(New->Ability->CheckCooldown(N.Handle,E.ASC->AbilityActorInfo.Get())){Fail(R,TEXT("replacement-refunded-cooldown"));return false;}
		if(!FMath::IsNearlyEqual(Health(E),E.Health,.01f)){Fail(R,TEXT("replacement-health-changed"));return false;}
	}
	return All;
}
void Bind(FRequest& R)
{
	const TWeakPtr<FRequest> Weak=Pending;
	for(int32 I=0;I<2;++I)
	{
		auto& E=Endpoint(R,I);
		E.EndDelegate=E.ASC->OnAbilityEnded.AddLambda([Weak,I](const FAbilityEndedData& Data)
		{
			const auto P=Weak.Pin();if(!P)return;auto& Q=*P;auto& E=Endpoint(Q,I);if(Data.AbilitySpecHandle!=E.Handle)return;
			auto* A=Data.AbilityThatEnded.Get();const auto Key=A?A->GetCurrentActivationInfo().GetActivationPredictionKey():FPredictionKey();
			const bool Exact=Persistent(E) && A==E.Instance.Get() && Key==E.Key && A && A->GetCurrentAbilitySpecHandle()==E.Handle;
			++E.Ends;E.EndAt=FPlatformTime::Seconds();
			const bool Detached=E.ASC.IsValid() && FObjectKey(E.ASC->GetAvatarActor())!=(I==0?Q.OwnerOldKey:Q.AuthorityOldKey);
			const bool InWindow=Q.bDispatched && E.EndAt>=Q.CancelAt && (I==1?Q.bResetCallInProgress:E.EndAt-Q.CancelAt<=Q.Transport+.75);
			const bool Inactive=!Active(E) && E.ASC.IsValid() && E.ASC->GetTagCount(TAG_Gameplay_MovementStopped)==0;
			// ClearAbility -> OnRemoveAbility ends active grants with cancelled=false.
			// Admit that route only after this reset detached the captured old avatar;
			// later replacement checks still require the old spec gone/new handle.
			const bool EndRoute=InWindow && (Data.bWasCancelled || Detached);
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: AbilityEnded player=%d endpoint=%s oldPawn=%s asc=%s avatar=%s instance=%s handle=%s key=%d keyBase=%d expectedKey=%d exact=%d cancelled=%d activeAfter=%d movementTag=%d cooldownTag=%d health=%.6f shots=%d elapsed=%.6f perfSeconds=%.6f worldTime=%.6f expectedTeardown=%d avatarDetached=%d withinResetWindow=%d resetCallInProgress=%d endRoute=%d cleanup=%d guard=%d"),
				Q.PlayerId,Name(I),I==0?*Q.OwnerOldPath:*Q.AuthorityOldPath,*GetPathNameSafe(E.ASC.Get()),*GetPathNameSafe(E.ASC.IsValid()?E.ASC->GetAvatarActor():nullptr),*GetPathNameSafe(A),*Data.AbilitySpecHandle.ToString(),Key.Current,Key.Base,E.Key.Current,Exact,Data.bWasCancelled,Active(E),E.ASC.IsValid()?E.ASC->GetTagCount(TAG_Gameplay_MovementStopped):-1,E.ASC.IsValid()?E.ASC->GetTagCount(Q.CooldownTag):-1,Health(E),Q.ShotCount,E.EndAt-Q.Start,E.EndAt,E.ASC.IsValid() && E.ASC->GetWorld()?E.ASC->GetWorld()->GetTimeSeconds():-1.,Q.bDispatched,Detached,InWindow,Q.bResetCallInProgress,EndRoute,Q.bEvidenceEnded,GAllowActorScriptExecutionInEditor);
			if(!Q.bEvidenceEnded && (!Exact || !EndRoute || !Inactive || E.Ends!=1 || !FMath::IsNearlyEqual(Health(E),E.Health,.01f)))Fail(Q,TEXT("unexpected-or-unhealthy-old-ability-end"));
		});
		E.HealthDelegate=E.ASC->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute()).AddLambda([Weak,I](const FOnAttributeChangeData& D)
		{
			const auto P=Weak.Pin();if(!P)return;const bool Changed=!FMath::IsFinite(D.OldValue) || !FMath::IsFinite(D.NewValue) || D.OldValue!=D.NewValue;
			if(Changed)++P->HealthChanges;
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: HealthChange player=%d endpoint=%s before=%.6f after=%.6f changed=%d elapsed=%.6f expectedTeardown=%d cleanup=%d guard=%d"),P->PlayerId,Name(I),D.OldValue,D.NewValue,Changed,FPlatformTime::Seconds()-P->Start,P->bDispatched,P->bEvidenceEnded,GAllowActorScriptExecutionInEditor);
			if(Changed && !P->bEvidenceEnded)Fail(*P,TEXT("health-changed-during-reset"));
		});
	}
	R.SpawnDelegate=R.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([Weak](AActor* A)
	{
		const auto P=Weak.Pin();if(!P || !P->bRequested || !A || !A->IsA(P->BulletFamily.Get()) || A->GetWorld()!=P->World.Get() || FObjectKey(A->GetOwner())!=P->AuthorityOldKey)return;
		const double Now=FPlatformTime::Seconds();++P->ShotCount;P->Projectiles.Add(A);if(P->ShotCount==1){P->FirstShotAt=Now;P->FirstProjectile=A->GetPathName();}
		const bool Identity=!P->bDispatched && AllLive(*P) && FObjectKey(A->GetInstigator())==P->AuthorityOldKey && RememberActivation(*P,P->Authority,1);
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Shot player=%d index=%d projectile=%s owner=%s instigator=%s instance=%s handle=%s key=%d identity=%d afterReset=%d afterAuthorityEnd=%d elapsed=%.6f perfSeconds=%.6f worldTime=%.6f cleanup=%d guard=%d"),P->PlayerId,P->ShotCount,*A->GetPathName(),*GetPathNameSafe(A->GetOwner()),*GetPathNameSafe(A->GetInstigator()),*GetPathNameSafe(P->Authority.Instance.Get()),*P->Authority.Handle.ToString(),P->Authority.Key.Current,Identity,P->bDispatched,P->Authority.Ends>0,Now-P->Start,Now,A->GetWorld()->GetTimeSeconds(),P->bEvidenceEnded,GAllowActorScriptExecutionInEditor);
		if(!P->bEvidenceEnded && !Identity)Fail(*P,TEXT("old-shot-after-reset-or-identity-change"));
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
		|| !ReadFloat(Config,TEXT("BarrageProjectileLifeSpan"),R.Life) || !ReadFloat(Config,TEXT("BarrageCooldown"),R.Cooldown)){Fail(R,TEXT("loaded-reset-tuning-unavailable"));return false;}
	R.LoadedCount=Count->GetPropertyValue_InContainer(Config);
	if(R.LoadedCount!=12 || R.Interval<.05f || R.Interval>.2f || R.Life<=0.f || R.Life>2.f || R.Cooldown<20.f){Fail(R,TEXT("unsupported-reset-timing"));return false;}
	for(TActorIterator<AActor> It(R.World.Get(),R.BulletFamily.Get());It;++It)if(It->GetOwner()==Authority){Fail(R,TEXT("prior-owned-projectile"));return false;}
	int32 Peer=0,ExpectedLag=INDEX_NONE;
	for(const FWorldContext& Context:GEngine->GetWorldContexts())if(UWorld* W=Context.World();W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer))
	{
		const UNetDriver* D=W->GetNetDriver();if(!D){Fail(R,TEXT("missing-driver"));return false;}const auto& N=D->PacketSimulationSettings;
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Network player=%d peer=%d world=%s driver=%s lag=%d lagMin=%d lagMax=%d incomingMin=%d incomingMax=%d loss=%d incomingLoss=%d jitter=%d variance=%d"),R.PlayerId,Peer++,*W->GetPathName(),*D->GetPathName(),N.PktLag,N.PktLagMin,N.PktLagMax,N.PktIncomingLagMin,N.PktIncomingLagMax,N.PktLoss,N.PktIncomingLoss,N.PktJitter,N.PktLagVariance);
		if(ExpectedLag==INDEX_NONE)ExpectedLag=N.PktLag;
		if((N.PktLag!=0 && N.PktLag!=100) || N.PktLag!=ExpectedLag || N.PktLagMin || N.PktLagMax || N.PktIncomingLagMin || N.PktIncomingLagMax || N.PktLoss || N.PktIncomingLoss || N.PktJitter || N.PktLagVariance){Fail(R,TEXT("unsupported-network-settings"));return false;}
	}
	R.Transport=2.*ExpectedLag/1000.;
	if(!CaptureRoster(R)){Fail(R,TEXT("four-healthy-authority-roster-required"));return false;}
	R.OwnerOldPath=Owner->GetPathName();R.AuthorityOldPath=Authority->GetPathName();R.OwnerOldKey=FObjectKey(Owner);R.AuthorityOldKey=FObjectKey(Authority);R.bPrepared=true;LogRoster(R,TEXT("before"));
	for(int32 I=0;I<2;++I)
	{
		const auto& E=Endpoint(R,I);
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Endpoint player=%d endpoint=%s pawn=%s controller=%s state=%s asc=%s round=%d handle=%s class=%s net=%d role=%d local=%d health=%.6f x=%.6f y=%.6f z=%.6f guard=%d"),R.PlayerId,Name(I),*E.Pawn->GetPathName(),*E.Controller->GetPathName(),*E.State->GetPathName(),*E.ASC->GetPathName(),E.RoundNumber,*E.Handle.ToString(),*E.AbilityClass->GetPathName(),int32(E.Pawn->GetNetMode()),int32(E.Pawn->GetLocalRole()),E.Pawn->IsLocallyControlled(),E.Health,E.Position.X,E.Position.Y,E.Position.Z,GAllowActorScriptExecutionInEditor);
	}
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Plan player=%d config=%s count=%d interval=%.6f life=%.6f cooldown=%.6f cooldownReduction=%.6f transport=%.6f projectileClass=%s timeout=5.0 guard=%d"),R.PlayerId,*GetPathNameSafe(Config),R.LoadedCount,R.Interval,R.Life,R.Cooldown,Stats->GetCooldownReduction(),R.Transport,*R.BulletFamily->GetPathName(),GAllowActorScriptExecutionInEditor);
	Bind(R);R.bRequested=true;R.CastAt=FPlatformTime::Seconds();R.bAccepted=R.Owner.ASC->TryActivateAbility(R.Owner.Handle,true);
	const bool OwnerActivation=AllLive(R) && RememberActivation(R,R.Owner,0);
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Dispatch player=%d accepted=%d ownerActivation=%d owner=%s authority=%s ownerHandle=%s authorityHandle=%s elapsed=%.6f guard=%d"),R.PlayerId,R.bAccepted,OwnerActivation,*GetPathNameSafe(R.Owner.Pawn.Get()),*GetPathNameSafe(R.Authority.Pawn.Get()),*R.Owner.Handle.ToString(),*R.Authority.Handle.ToString(),R.CastAt-R.Start,GAllowActorScriptExecutionInEditor);
	if(!R.bAccepted || !OwnerActivation){Fail(R,TEXT("owner-activation-not-accepted"));return false;}return true;
}

void EvidenceEnd(FRequest& R)
{
	if(R.bEvidenceEnded)return;State(R,TEXT("evidence-end"));LogRoster(R,TEXT("evidence-end"));R.bEvidenceEnded=true;const double Now=FPlatformTime::Seconds();
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: EvidenceEnd player=%d reason=%s elapsed=%.6f perfSeconds=%.6f shots=%d ownerEnds=%d authorityEnds=%d replacements=%d healthChanges=%d guard=%d"),R.PlayerId,R.Failure.IsEmpty()?TEXT("observed"):*R.Failure,Now-R.Start,Now,R.ShotCount,R.Owner.Ends,R.Authority.Ends,R.bReplacements,R.HealthChanges,GAllowActorScriptExecutionInEditor);
}
FTSTicker::FDelegateHandle FreshExpiry, FreshTicker;
void ClearTicket(const TCHAR* Reason);
bool Finish(FRequest& R,bool Hard)
{
	EvidenceEnd(R);
	// Cleanup can only cancel the captured old activation. Never follow the persistent
	// ASC to a replacement or a reused per-actor instance with a different key.
	for(int32 I=0;I<2;++I)
	{
		auto& E=Endpoint(R,I);
		if(!GAllowActorScriptExecutionInEditor && Persistent(E) && SameActivation(E) && Active(E) && !E.bCleanupCancel)
		{
			E.bCleanupCancel=true;const auto Instance=E.Instance;const auto Handle=E.Handle;const auto Key=E.Key;
			E.ASC->CancelAbilitiesByFunc([Instance,Handle,Key](const ULyraGameplayAbility* A,FGameplayAbilitySpecHandle H){return A==Instance.Get() && H==Handle && A->GetCurrentActivationInfo().GetActivationPredictionKey()==Key;},true);
		}
	}
	int32 Removed=0;
	for(int32 I=0;I<2;++I)
	{
		auto& E=Endpoint(R,I);if(auto* A=E.ASC.Get())
		{
			if(E.EndDelegate.IsValid()){A->OnAbilityEnded.Remove(E.EndDelegate);E.EndDelegate.Reset();++Removed;}
			if(E.HealthDelegate.IsValid()){A->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute()).Remove(E.HealthDelegate);E.HealthDelegate.Reset();++Removed;}
		}
	}
	if(auto* W=R.World.Get();W && R.SpawnDelegate.IsValid()){W->RemoveOnActorSpawnedHandler(R.SpawnDelegate);R.SpawnDelegate.Reset();++Removed;}
	const bool Known=R.bPrepared && RosterPersistent(R);
	const bool Clean=!R.bRequested || (Known && !Active(R.Owner) && !Active(R.Authority)
		&& (!R.bDispatched || (R.bReplacements && Live(R.NewOwner) && Live(R.NewAuthority) && SameSpec(R.NewOwner) && SameSpec(R.NewAuthority)
			&& !Spec(R.Owner) && !Spec(R.Authority) && !Active(R.NewOwner) && !Active(R.NewAuthority)))
		&& R.Owner.ASC->GetTagCount(TAG_Gameplay_MovementStopped)==0 && R.Authority.ASC->GetTagCount(TAG_Gameplay_MovementStopped)==0);
	const bool Ticket=Clean && !Hard && R.Failure.IsEmpty() && R.bReplacements && Removed==5 && !R.Owner.bCleanupCancel && !R.Authority.bCleanupCancel;
	if(Ticket)
	{
		FreshTicket=Pending;
		FreshExpiry=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			if(!FreshTicket || FPlatformTime::Seconds()-FreshTicket->Start>40.){ClearTicket(TEXT("expired"));return false;}return true;
		}),.25f);
	}
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Complete player=%d reason=%s cleanup=%d stateKnown=%d ownerFallback=%d authorityFallback=%d delegatesRemoved=%d shots=%d ownerEnds=%d authorityEnds=%d healthChanges=%d hardDeadline=%d freshTicket=%d inputWrites=0 transformWrites=0 elapsed=%.6f perfSeconds=%.6f guard=%d outcome=observed"),R.PlayerId,R.Failure.IsEmpty()?TEXT("observed"):*R.Failure,Clean,Known,R.Owner.bCleanupCancel,R.Authority.bCleanupCancel,Removed,R.ShotCount,R.Owner.Ends,R.Authority.Ends,R.HealthChanges,Hard,Ticket,FPlatformTime::Seconds()-R.Start,FPlatformTime::Seconds(),GAllowActorScriptExecutionInEditor);
	Pending.Reset();Ticker.Reset();return false;
}
bool Tick(float Delta)
{
	(void)Delta;const auto Keep=Pending;if(!Keep)return false;auto& R=*Keep;const double Now=FPlatformTime::Seconds();
	if(Now-R.Start>=5.){Fail(R,TEXT("hard-timeout"));return Finish(R,true);}
	if(GAllowActorScriptExecutionInEditor)return true;
	if(R.bAbort){Fail(R,TEXT("observer-abort"));return Finish(R,false);}
	if(!R.bRequested){if(!Setup(R))return Finish(R,false);return true;}
	if(!R.Failure.IsEmpty())return Finish(R,false);
	if(!R.bDispatched)
	{
		if(!AllLive(R) || !RosterPersistent(R) || !SameSpec(R.Owner) || !SameSpec(R.Authority) || !SameActivation(R.Owner)
			|| (R.Authority.Instance.IsValid() && !SameActivation(R.Authority))){Fail(R,TEXT("original-ownership-or-activation-invalidated"));return Finish(R,false);}
		if(!FMath::IsNearlyEqual(Health(R.Owner),R.Owner.Health,.01f) || !FMath::IsNearlyEqual(Health(R.Authority),R.Authority.Health,.01f)){Fail(R,TEXT("original-full-health-changed"));return Finish(R,false);}
		if(Now-R.LastSample>=.05){State(R,TEXT("await-first-shot"));R.LastSample=Now;}
		if(R.ShotCount==0){if(Now-R.CastAt>1.){Fail(R,TEXT("authority-first-shot-timeout"));return Finish(R,false);}return true;}
		if(R.ShotCount!=1 || Now-R.FirstShotAt>=R.Interval || !Active(R.Owner) || !Active(R.Authority) || !RememberActivation(R,R.Authority,1)
			|| R.Owner.Key!=R.Authority.Key){Fail(R,TEXT("first-shot-reset-setup-missed"));return Finish(R,false);}
		for(const auto& E:R.Roster)if(!Live(E) || !FMath::IsNearlyEqual(Health(E),E.Health,.01f))
		{Fail(R,TEXT("original-healthy-roster-changed-before-reset"));return Finish(R,false);}
		const auto EffectsNow=CooldownEffects(R,R.Authority);const auto* GE=EffectsNow.Num()==1?R.Authority.ASC->GetActiveGameplayEffect(EffectsNow[0]):nullptr;
		if(!GE || !FMath::IsNearlyEqual(GE->GetDuration(),R.Cooldown,.01f)){Fail(R,TEXT("unique-committed-authority-cooldown-required"));return Finish(R,false);}
		R.CooldownEffect=EffectsNow[0];R.CooldownDefinition=GE->Spec.Def.Get();R.CooldownExpiry=GE->GetEndTime();R.CooldownDuration=GE->GetDuration();
		Effects(R,R.Authority,1,TEXT("pre-reset"));
		// Set before entering production reset: UnPossess invalidates the old pawn chain
		// and synchronous end callbacks must already know teardown is intentional.
		R.bDispatched=true;R.CancelAt=Now;
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: ResetDispatch player=%d owner=%s authority=%s ownerASC=%s authorityASC=%s ownerInstance=%s authorityInstance=%s ownerHandle=%s authorityHandle=%s ownerKey=%d authorityKey=%d firstProjectile=%s shots=%d firstShotElapsed=%.6f elapsed=%.6f perfSeconds=%.6f age=%.6f interval=%.6f ownerHealth=%.6f authorityHealth=%.6f ownerActive=%d authorityActive=%d roster=%d round=%d expectedTeardown=1 guard=%d"),R.PlayerId,*R.OwnerOldPath,*R.AuthorityOldPath,*R.Owner.ASC->GetPathName(),*R.Authority.ASC->GetPathName(),*R.Owner.Instance->GetPathName(),*R.Authority.Instance->GetPathName(),*R.Owner.Handle.ToString(),*R.Authority.Handle.ToString(),R.Owner.Key.Current,R.Authority.Key.Current,*R.FirstProjectile,R.ShotCount,R.FirstShotAt-R.Start,Now-R.Start,Now,Now-R.FirstShotAt,R.Interval,Health(R.Owner),Health(R.Authority),Active(R.Owner),Active(R.Authority),R.Roster.Num(),R.Authority.RoundNumber,GAllowActorScriptExecutionInEditor);
		R.bResetCallInProgress=true;
		R.Authority.Round->ResetRoundState();
		R.bResetCallInProgress=false;
		// No old pawn/ability/spec raw pointers are retained across the reset callbacks.
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: ResetReturn player=%d elapsed=%.6f perfSeconds=%.6f authorityEnds=%d ownerEnds=%d shots=%d guard=%d"),R.PlayerId,FPlatformTime::Seconds()-R.Start,FPlatformTime::Seconds(),R.Authority.Ends,R.Owner.Ends,R.ShotCount,GAllowActorScriptExecutionInEditor);
		State(R,TEXT("reset-return"));LogRoster(R,TEXT("reset-return"));return true;
	}
	if(!RosterPersistent(R) || !RetainedCooldown(R)){Fail(R,TEXT("persistent-roster-or-committed-cooldown-changed"));return Finish(R,false);}
	const bool Ready=Replacements(R);
	if(!R.Failure.IsEmpty())return Finish(R,false);
	if(Ready && !R.bReplacements)
	{
		R.bReplacements=true;R.ReplacementAt=Now;
		for(int32 I=0;I<2;++I)
		{
			const auto& E=Endpoint(R,I);const auto& N=NewEndpoint(R,I);
			UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Replacement player=%d endpoint=%s oldPawn=%s newPawn=%s controller=%s state=%s asc=%s avatar=%s oldHandle=%s newHandle=%s class=%s oldSpec=%d active=%d movementTag=%d cooldownTag=%d cooldownReady=%d health=%.6f elapsed=%.6f perfSeconds=%.6f guard=%d"),R.PlayerId,Name(I),I==0?*R.OwnerOldPath:*R.AuthorityOldPath,*N.Pawn->GetPathName(),*N.Controller->GetPathName(),*N.State->GetPathName(),*N.ASC->GetPathName(),*GetPathNameSafe(N.ASC->GetAvatarActor()),*E.Handle.ToString(),*N.Handle.ToString(),*N.AbilityClass->GetPathName(),Spec(E)!=nullptr,Active(N),N.ASC->GetTagCount(TAG_Gameplay_MovementStopped),N.ASC->GetTagCount(R.CooldownTag),Spec(N)->Ability->CheckCooldown(N.Handle,N.ASC->AbilityActorInfo.Get()),Health(N),Now-R.Start,Now,GAllowActorScriptExecutionInEditor);
		}
		LogRoster(R,TEXT("replacement-ready"));
	}
	if(Now-R.LastSample>=.05){State(R,Ready?TEXT("replacement-settled"):TEXT("await-replacements"));R.LastSample=Now;}
	if(Now-R.CancelAt>R.Transport+.75 && (Active(R.Owner) || Active(R.Authority) || R.Owner.Ends!=1 || R.Authority.Ends!=1)){Fail(R,TEXT("old-activation-teardown-timeout"));return Finish(R,false);}
	const double Observation=R.LoadedCount*R.Interval+R.Life+R.Transport+.25;
	if(Now-R.CastAt>=Observation && Ready && R.bReplacements && Now-R.ReplacementAt>=R.Transport+.2)
	{
		bool Bullets=false;for(const auto& A:R.Projectiles)if(A.IsValid() && !A->IsActorBeingDestroyed())Bullets=true;
		if(!Ready || !R.bReplacements || Now-R.ReplacementAt<R.Transport+.2 || R.Owner.Ends!=1 || R.Authority.Ends!=1 || R.ShotCount!=1 || Bullets || R.HealthChanges){Fail(R,TEXT("incomplete-healthy-reset-observation"));}
		return Finish(R,false);
	}
	return true;
}
bool Parse(const TArray<FString>& Args,int32& Id)
{
	if(Args.Num()!=1 || Args[0].IsEmpty())return false;Id=0;for(TCHAR C:Args[0]){const int32 D=int32(C)-int32(TEXT('0'));if(D<0 || D>9 || Id>(MAX_int32-D)/10)return false;Id=Id*10+D;}return true;
}
void ClearTicket(const TCHAR* Reason)
{
	const int32 Id=FreshTicket?FreshTicket->PlayerId:-1;
	const bool HadExpiry=FreshExpiry.IsValid(),HadFresh=FreshTicker.IsValid();
	if(HadExpiry){FTSTicker::GetCoreTicker().RemoveTicker(FreshExpiry);FreshExpiry.Reset();}
	if(HadFresh){FTSTicker::GetCoreTicker().RemoveTicker(FreshTicker);FreshTicker.Reset();}
	FreshTicket.Reset();bFreshRequested=false;
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: TicketCleared player=%d reason=%s expiryRemoved=%d freshRemoved=%d remainingCallbacks=0 guard=%d"),Id,Reason,HadExpiry,HadFresh,GAllowActorScriptExecutionInEditor);
}
bool TickFresh(float)
{
	if(!FreshTicket){ClearTicket(TEXT("missing"));return false;}const auto Keep=FreshTicket;auto& R=*Keep;
	if(FPlatformTime::Seconds()-R.Start>40.){UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: FreshRejected player=%d reason=ticket-expired"),R.PlayerId);ClearTicket(TEXT("rejected-or-expired"));return false;}
	if(GAllowActorScriptExecutionInEditor)return true;
	bool Ready=RosterPersistent(R) && R.bReplacements;
	for(int32 I=0;I<2;++I)
	{
		const auto& E=Endpoint(R,I);const auto& N=NewEndpoint(R,I);const auto* S=Spec(N);int32 Count=0;
		if(N.ASC.IsValid())for(const auto& Item:N.ASC->GetActivatableAbilities())if(Item.Ability && Item.Ability->IsA(R.Family.Get()))++Count;
		const bool EReady=Live(N) && SameSpec(N) && !Spec(E) && Count==1 && S && !S->IsActive() && N.ASC->GetTagCount(TAG_Gameplay_MovementStopped)==0
			&& N.ASC->GetTagCount(R.CooldownTag)==0 && CooldownEffects(R,N).IsEmpty() && S->Ability->CheckCooldown(N.Handle,N.ASC->AbilityActorInfo.Get());
		Ready &= EReady;
		UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: FreshAdmission player=%d endpoint=%s pawn=%s controller=%s state=%s asc=%s avatar=%s handle=%s class=%s unique=%d ready=%d active=%d cooldownTag=%d effectCount=%d cooldownReady=%d elapsed=%.6f perfSeconds=%.6f guard=%d"),R.PlayerId,Name(I),*GetPathNameSafe(N.Pawn.Get()),*GetPathNameSafe(N.Controller.Get()),*GetPathNameSafe(N.State.Get()),*GetPathNameSafe(N.ASC.Get()),*GetPathNameSafe(N.ASC.IsValid()?N.ASC->GetAvatarActor():nullptr),*N.Handle.ToString(),*GetPathNameSafe(N.AbilityClass.Get()),Count,EReady,S?int32(S->IsActive()):-1,N.ASC.IsValid()?N.ASC->GetTagCount(R.CooldownTag):-1,CooldownEffects(R,N).Num(),S && S->Ability && N.ASC.IsValid() && N.ASC->AbilityActorInfo.IsValid()?int32(S->Ability->CheckCooldown(N.Handle,N.ASC->AbilityActorInfo.Get())):-1,FPlatformTime::Seconds()-R.Start,FPlatformTime::Seconds(),GAllowActorScriptExecutionInEditor);
	}
	for(int32 I=0;I<R.Roster.Num();++I)
	{
		FEndpoint N=R.Roster[I];N.Pawn=R.NewRoster[I];Ready &= Live(N);
	}
	IConsoleObject* Object=IConsoleManager::Get().FindConsoleObject(TEXT("bway.Test.BarrageAnimation"));
	IConsoleCommand* Command=Object?Object->AsCommand():nullptr;
	if(!Ready || !Command){UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: FreshRejected player=%d reason=replacement-or-real-cooldown-not-ready"),R.PlayerId);ClearTicket(TEXT("rejected-or-expired"));return false;}
	const int32 Id=R.PlayerId;UWorld* W=R.World.Get();const double Now=FPlatformTime::Seconds();ClearTicket(TEXT("consumed"));
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: FreshDispatch player=%d command=bway.Test.BarrageAnimation elapsed=%.6f perfSeconds=%.6f guard=%d"),Id,Now-R.Start,Now,GAllowActorScriptExecutionInEditor);
	Command->Execute(TArray<FString>{FString::FromInt(Id)},W,*GLog);return false;
}
FAutoConsoleCommand StartCommand(TEXT("bway.Test.BarrageRoundReset"),TEXT("Observe healthy first-shot production round reset: <remote-player-id>."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
	int32 Id=0;if(!Parse(Args,Id) || Pending || bFreshRequested || FreshTicket){UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Rejected reason=invalid-id-or-pending-ticket"));return;}
	Pending=MakeShared<FRequest>();Pending->PlayerId=Id;Pending->Start=FPlatformTime::Seconds();
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: ClockAnchor player=%d phase=initial monotonicSeconds=%.9f elapsed=0.000000 utcTicks=%lld guard=%d"),Id,Pending->Start,FDateTime::UtcNow().GetTicks(),GAllowActorScriptExecutionInEditor);
	Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick),0.f);
	UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: Queued player=%d timeout=5.0 scope=healthy-round-avatar-reset"),Id);
}));
FAutoConsoleCommand AbortCommand(TEXT("bway.Test.BarrageRoundResetAbort"),TEXT("Abort exact reset request and discard its fresh-cast ticket: <player-id>."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
	int32 Id=0;if(!Parse(Args,Id))return;if(Pending && Pending->PlayerId==Id)Pending->bAbort=true;if(FreshTicket && FreshTicket->PlayerId==Id)ClearTicket(TEXT("observer-abort"));
}));
FAutoConsoleCommand FreshCommand(TEXT("bway.Test.BarrageRoundResetFresh"),TEXT("Once real cooldown expires, validate captured replacement and queue the normal Barrage command: <player-id>."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
	int32 Id=0;if(!Parse(Args,Id) || Pending || !FreshTicket || FreshTicket->PlayerId!=Id || bFreshRequested){UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: FreshRejected reason=invalid-id-or-ticket"));return;}
	const double Now=FPlatformTime::Seconds();UE_LOG(LogTemp,Display,TEXT("BwayBarrageRoundReset: ClockAnchor player=%d phase=fresh monotonicSeconds=%.9f elapsed=%.6f utcTicks=%lld guard=%d"),Id,Now,Now-FreshTicket->Start,FDateTime::UtcNow().GetTicks(),GAllowActorScriptExecutionInEditor);
	bFreshRequested=true;FreshTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickFresh),0.f);
}));
}
#endif
