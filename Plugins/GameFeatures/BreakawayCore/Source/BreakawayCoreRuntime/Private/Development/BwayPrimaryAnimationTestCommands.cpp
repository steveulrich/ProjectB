// Queue one synthetic primary activation outside the editor Python RPC guard.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Development/BwayCombatFixtureAbilities.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/UnrealType.h"

namespace BwayPrimaryAnimationTest
{
struct FRequest
{
	int32 PlayerId = INDEX_NONE;
	double Deadline = 0.0;
	const TCHAR* WaitReason = TEXT("client-owner-unavailable");
};

TOptional<FRequest> Pending;
FTSTicker::FDelegateHandle TickerHandle;

bool Finish(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Complete player=%d reason=%s cleanup=1"),
		Pending.IsSet() ? Pending->PlayerId : INDEX_NONE, Reason);
	Pending.Reset();
	TickerHandle.Reset();
	return false;
}

bool Tick(float)
{
	if (!Pending.IsSet()) return false;
	if (FPlatformTime::Seconds() >= Pending->Deadline)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: TIMEOUT player=%d reason=%s"), Pending->PlayerId, Pending->WaitReason);
		return Finish(TEXT("timeout"));
	}
	if (GAllowActorScriptExecutionInEditor)
	{
		Pending->WaitReason = TEXT("editor-script-guard");
		return true;
	}

	ABwayPlayerController* Controller = nullptr;
	int32 Matches = 0;
	if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!IsValid(World) || !World->IsGameWorld() || World->GetNetMode() != NM_Client) continue;
		for (TActorIterator<ABwayPlayerController> It(World); It; ++It)
		{
			ABwayPlayerController* Candidate = *It;
			if (!IsValid(Candidate)) continue;
			const ABwayPlayerState* State = Candidate->GetPlayerState<ABwayPlayerState>();
			if (Candidate->IsLocalController() && IsValid(State) && State->GetPlayerId() == Pending->PlayerId)
			{
				Controller = Candidate;
				++Matches;
			}
		}
	}
	if (Matches > 1)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Ambiguous owner player=%d matches=%d"), Pending->PlayerId, Matches);
		return Finish(TEXT("ambiguous-client-owner"));
	}
	if (!Controller)
	{
		Pending->WaitReason = TEXT("client-owner-unavailable");
		return true;
	}

	UWorld* World = Controller->GetWorld();
	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	const UBwayRoundManagementComponent* Round = GameState ? GameState->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
	ABwayCharacterWithAbilities* Pawn = Cast<ABwayCharacterWithAbilities>(Controller->GetPawn());
	ABwayPlayerState* State = Controller->GetPlayerState<ABwayPlayerState>();
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || !IsValid(Pawn)
		|| Pawn->GetController() != Controller || Pawn->GetPlayerState() != State
		|| !Health || Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)
	{
		Pending->WaitReason = TEXT("playing-live-owned-pawn-unavailable");
		return true;
	}
	ULyraAbilitySystemComponent* ASC = State->GetLyraAbilitySystemComponent();
	if (!ASC || Pawn->GetLyraAbilitySystemComponent() != ASC || ASC->GetAvatarActor() != Pawn
		|| ASC->GetOwnerActor() != State || !ASC->AbilityActorInfo.IsValid()
		|| ASC->AbilityActorInfo->PlayerController.Get() != Controller
		|| !ASC->AbilityActorInfo->IsLocallyControlled() || ASC->AbilityActorInfo->IsNetAuthority())
	{
		Pending->WaitReason = TEXT("asc-owner-avatar-disagreement");
		return true;
	}

	FGameplayAbilitySpecHandle Handle;
	FString AbilityName;
	int32 PrimaryCount = 0;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!BwayCombatFixture::IsPrimaryAbility(Spec.Ability)) continue;
		Handle = Spec.Handle;
		AbilityName = GetNameSafe(Spec.Ability);
		++PrimaryCount;
	}
	if (PrimaryCount > 1)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Ambiguous primary player=%d count=%d"), Pending->PlayerId, PrimaryCount);
		return Finish(TEXT("ambiguous-primary"));
	}
	if (!Handle.IsValid())
	{
		Pending->WaitReason = TEXT("primary-spec-unavailable");
		return true;
	}

	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Dispatch player=%d world=%s net=%d local=%d role=%d remoteRole=%d guard=%d avatar=%s ability=%s handle=%s"),
		Pending->PlayerId, *World->GetPathName(), int32(World->GetNetMode()), Controller->IsLocalController(),
		int32(Pawn->GetLocalRole()), int32(Pawn->GetRemoteRole()), GAllowActorScriptExecutionInEditor,
		*Pawn->GetPathName(), *AbilityName, *Handle.ToString());
	const TWeakObjectPtr<ABwayCharacterWithAbilities> WeakPawn(Pawn);
	const TWeakObjectPtr<ULyraAbilitySystemComponent> WeakASC(ASC);
	const bool Activated = ASC->TryActivateAbility(Handle, false);

	// Re-resolve after activation; synchronous ability callbacks may end or replace the avatar.
	Pawn = WeakPawn.Get();
	ASC = WeakASC.Get();
	const FGameplayAbilitySpec* AfterSpec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	UAnimInstance* Anim = Pawn && Pawn->GetMesh() ? Pawn->GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
	const FBoolProperty* Firing = Anim ? FindFProperty<FBoolProperty>(Anim->GetClass(), TEXT("GameplayTag_IsFiring")) : nullptr;
	const FDoubleProperty* SinceFire = Anim ? FindFProperty<FDoubleProperty>(Anim->GetClass(), TEXT("TimeSinceFiredWeapon")) : nullptr;
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Readback player=%d activated=%d active=%d avatarMatches=%d anim=%s montage=%s montagePosition=%.3f firing=%d sinceFire=%.3f scope=immediate-local-readback"),
		Pending->PlayerId, Activated, AfterSpec && AfterSpec->IsActive(), Pawn && ASC && ASC->GetAvatarActor() == Pawn,
		*GetNameSafe(Anim), *GetNameSafe(Montage), Montage ? Anim->Montage_GetPosition(Montage) : -1.0f,
		Firing ? int32(Firing->GetPropertyValue_InContainer(Anim)) : -1,
		SinceFire ? SinceFire->GetPropertyValue_InContainer(Anim) : -1.0f);
	return Finish(Activated ? TEXT("activation-accepted") : TEXT("activation-rejected"));
}

void Start(const TArray<FString>& Args)
{
	int32 PlayerId = 0;
	bool ValidId = Args.Num() == 1 && !Args[0].IsEmpty();
	if (ValidId) for (TCHAR Character : Args[0])
	{
		const int32 Digit = int32(Character) - int32(TEXT('0'));
		if (Digit < 0 || Digit > 9 || PlayerId > (MAX_int32 - Digit) / 10)
		{
			ValidId = false;
			break;
		}
		PlayerId = PlayerId * 10 + Digit;
	}
	if (!ValidId)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPrimaryAnimation: Usage bway.Test.PrimaryAnimation <nonnegative-int32-player-id>"));
		return;
	}
	if (Pending.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPrimaryAnimation: Request rejected; player=%d already pending"), Pending->PlayerId);
		return;
	}
	FRequest Request;
	Request.PlayerId = PlayerId;
	Request.Deadline = FPlatformTime::Seconds() + 5.0;
	Pending = Request;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.05f);
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAnimation: Queued player=%d timeout=5.0"), PlayerId);
}

void Cancel()
{
	if (!Pending.IsSet()) return;
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Finish(TEXT("cancelled"));
}

FAutoConsoleCommand Command(TEXT("bway.Test.PrimaryAnimation"),
	TEXT("Development only: queue one primary activation for an exact local NM_Client player ID; no aim or fixture changes."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&Start));
FAutoConsoleCommand CancelCommand(TEXT("bway.Test.PrimaryAnimationCancel"),
	TEXT("Cancel the pending development primary-animation activation."), FConsoleCommandDelegate::CreateStatic(&Cancel));
}
#endif
