// Development-only fixtures for the production buildable placement and replication path.
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "Abilities/BwayGameplayAbility_PlaceBuildable.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "BwayGameplayTags.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "TimerManager.h"

namespace BwayBuildableTest
{

void RunAbility(UWorld* World, bool bConfirm);

UWorld* FindPIEWorld(bool bAuthority)
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType == EWorldType::PIE && World && World->IsGameWorld() &&
			(bAuthority ? World->GetNetMode() != NM_Client : World->GetNetMode() == NM_Client))
		{
			return World;
		}
	}

	return nullptr;
}

ABwayCharacterWithAbilities* FindLocalCharacter(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
	{
		ABwayCharacterWithAbilities* Character = *It;
		APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
		if (Character && PC && PC->IsLocalController() && Character->GetPlayerState())
		{
			return Character;
		}
	}

	return nullptr;
}

ABwayPlayerState* FindPlayerStateById(UWorld* World, int32 PlayerId)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ABwayPlayerState> It(World); It; ++It)
	{
		ABwayPlayerState* PlayerState = *It;
		if (PlayerState && PlayerState->GetPlayerId() == PlayerId)
		{
			return PlayerState;
		}
	}

	return nullptr;
}

APlayerController* FindPlayerControllerById(UWorld* World, int32 PlayerId)
{
	if (!World)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPlayerState<ABwayPlayerState>() && PC->GetPlayerState<ABwayPlayerState>()->GetPlayerId() == PlayerId)
		{
			return PC;
		}
	}

	return nullptr;
}

int32 CountBuildables(UWorld* World)
{
	int32 Count = 0;
	if (!World)
	{
		return Count;
	}

	for (TActorIterator<ABuildableActor> It(World); It; ++It)
	{
		if (*It && !(*It)->IsActorBeingDestroyed())
		{
			++Count;
		}
	}

	return Count;
}

void LogPeerSamples(UWorld* AuthorityWorld, int32 PlayerId, const TCHAR* FixtureName)
{
	if (!AuthorityWorld)
	{
		return;
	}

	TArray<UWorld*> Worlds;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Context.WorldType == EWorldType::PIE && Candidate && !Worlds.Contains(Candidate))
			{
				Worlds.Add(Candidate);
			}
		}
	}
	if (!Worlds.Contains(AuthorityWorld))
	{
		Worlds.Add(AuthorityWorld);
	}

	int32 WorldsWithBuildable = 0;
	for (UWorld* World : Worlds)
	{
		const bool bAuthority = World == AuthorityWorld;
		const int32 TotalBuildables = CountBuildables(World);
		if (TotalBuildables > 0)
		{
			++WorldsWithBuildable;
		}

		ABwayPlayerState* PlayerState = FindPlayerStateById(World, PlayerId);
		const bool bHasPlaced = PlayerState && PlayerState->HasPlacedBuildableThisRound();
		UE_LOG(LogTemp, Display,
			TEXT("BwayBuildableTest: %s PeerSample world=%s role=%s playerId=%d totalBuildables=%d hasPlaced=%d"),
			FixtureName,
			*GetNameSafe(World),
			bAuthority ? TEXT("Authority") : TEXT("Client"),
			PlayerId,
			TotalBuildables,
			bHasPlaced ? 1 : 0);
	}

	UE_LOG(LogTemp, Display,
		TEXT("BwayBuildableTest: %s PeerConvergence worlds=%d worldsWithBuildable=%d playerId=%d pass=%d"),
		FixtureName,
		Worlds.Num(),
		WorldsWithBuildable,
		PlayerId,
		(Worlds.Num() >= 2 && WorldsWithBuildable == Worlds.Num()) ? 1 : 0);
}

void Spawn(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Spawn requires a game world"));
		return;
	}
	// The editor console is usually bound to the local PIE client. Exercise the
	// real predicted ability there so GAS sends the confirm to the authority.
	if (World->GetNetMode() == NM_Client)
	{
		RunAbility(World, true);
		return;
	}

	ABwayCharacterWithAbilities* Character = FindLocalCharacter(World);
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	UBwayBuildableDataAsset* BuildableData = PC
		? UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(World, PC)
		: nullptr;
	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	ABwayPlayerState* PlayerState = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (!Character || !PC || !BuildableData || !BuildableData->BuildableActorClass || !GameState || !PlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Spawn could not resolve local character, player state, game state, or buildable data"));
		if (World->GetNetMode() != NM_Client)
		{
			RunAbility(FindPIEWorld(false), true);
		}
		return;
	}

	const int32 PlayerId = PlayerState->GetPlayerId();
	const int32 CountBefore = UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(World, PC);
	const bool bPlacedBefore = PlayerState->HasPlacedBuildableThisRound();
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector SpawnLocation = Character->GetActorLocation() + Forward * 600.0f + FVector(0.0f, 0.0f, 20.0f);
	const FTransform SpawnTransform(Character->GetActorRotation(), SpawnLocation);
	ABuildableActor* Buildable = UBwayBuildablePlacementLibrary::SpawnBuildableForPlayer(World, PC, BuildableData, SpawnTransform);
	const int32 CountAfter = UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(World, PC);

	UE_LOG(LogTemp, Display,
		TEXT("BwayBuildableTest: SpawnResult playerId=%d class=%s spawned=%d countBefore=%d countAfter=%d hasPlacedBefore=%d hasPlacedAfter=%d mesh=%s replicated=%d netLoad=%d onlyOwner=%d ownerRelevancy=%d dormancy=%d"),
		PlayerId,
		*GetNameSafe(BuildableData->BuildableActorClass),
		Buildable ? 1 : 0,
		CountBefore,
		CountAfter,
		bPlacedBefore ? 1 : 0,
		PlayerState->HasPlacedBuildableThisRound() ? 1 : 0,
		Buildable && Buildable->GetMesh() ? *GetNameSafe(Buildable->GetMesh()->GetSkeletalMeshAsset()) : TEXT("(null)"),
		Buildable && Buildable->GetIsReplicated() ? 1 : 0,
		Buildable && Buildable->GetIsReplicated() && Buildable->bNetLoadOnClient ? 1 : 0,
		Buildable && Buildable->bOnlyRelevantToOwner ? 1 : 0,
		Buildable && Buildable->bNetUseOwnerRelevancy ? 1 : 0,
		Buildable ? static_cast<int32>(Buildable->NetDormancy) : -1);

	FText FailureText;
	const bool bCanPlaceAgain = UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable(World, PC, BuildableData, FailureText);
	UE_LOG(LogTemp, Display, TEXT("BwayBuildableTest: QuotaResult playerId=%d canPlaceAgain=%d reason=%s"),
		PlayerId, bCanPlaceAgain ? 1 : 0, *FailureText.ToString());

	TWeakObjectPtr<UWorld> WeakWorld(World);
	for (const float Delay : {0.5f, 2.0f})
	{
		FTimerHandle SampleTimer;
		World->GetTimerManager().SetTimer(SampleTimer, FTimerDelegate::CreateLambda([WeakWorld, PlayerId]()
		{
			if (UWorld* SampleWorld = WeakWorld.Get())
			{
				LogPeerSamples(SampleWorld, PlayerId, TEXT("Spawn"));
			}
		}), Delay, false);
	}
}

void ResetRound(UWorld* World)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: ResetRound requires an authority game world"));
		return;
	}

	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	UBwayRoundManagementComponent* RoundManagement = GameState ? GameState->RoundManagementComponent : nullptr;
	ABwayCharacterWithAbilities* Character = FindLocalCharacter(World);
	ABwayPlayerState* PlayerState = Character ? Character->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (!RoundManagement || !PlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: ResetRound could not resolve round management or player state"));
		return;
	}

	const int32 PlayerId = PlayerState->GetPlayerId();
	const int32 CountBefore = CountBuildables(World);
	RoundManagement->ResetRoundState();
	// ResetRoundState preserves persistent actors. The production round-start path
	// then resets the per-round quota through each PlayerState's public tracking API.
	if (GameState)
	{
		for (APlayerState* RawPlayerState : GameState->PlayerArray)
		{
			if (ABwayPlayerState* BwayPlayerState = Cast<ABwayPlayerState>(RawPlayerState))
			{
				BwayPlayerState->BeginRoundStatTracking();
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("BwayBuildableTest: ResetRoundResult playerId=%d countBefore=%d countAfter=%d hasPlacedAfter=%d"),
		PlayerId, CountBefore, CountBuildables(World), PlayerState->HasPlacedBuildableThisRound() ? 1 : 0);

	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld, PlayerId]()
	{
		if (UWorld* SampleWorld = WeakWorld.Get())
		{
			LogPeerSamples(SampleWorld, PlayerId, TEXT("ResetRound"));
		}
	}));
}

void RunAbility(UWorld* World, bool bConfirm)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Ability fixture requires a game world"));
		return;
	}

	ABwayCharacterWithAbilities* Character = FindLocalCharacter(World);
	ULyraAbilitySystemComponent* ASC = Character ? Character->GetLyraAbilitySystemComponent() : nullptr;
	ABwayPlayerState* PlayerState = Character ? Character->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (!Character || !ASC || !PlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Ability fixture could not resolve local character ASC or player state"));
		return;
	}

	FGameplayAbilitySpecHandle PlacementHandle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA(UBwayGameplayAbility_PlaceBuildable::StaticClass()))
		{
			PlacementHandle = Spec.Handle;
			break;
		}
	}
	if (!PlacementHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Ability fixture found no granted placement ability"));
		return;
	}

	const FGameplayAbilitySpec* ExistingSpec = ASC->FindAbilitySpecFromHandle(PlacementHandle);
	const bool bReuseActiveForCancel = !bConfirm && ExistingSpec && ExistingSpec->IsActive();
	const bool bActivated = bReuseActiveForCancel || ASC->TryActivateAbility(PlacementHandle, false);
	UE_LOG(LogTemp, Display, TEXT("BwayBuildableTest: AbilityStart mode=%s activated=%d reusedActive=%d"),
		bConfirm ? TEXT("confirm") : TEXT("cancel"), bActivated ? 1 : 0, bReuseActiveForCancel ? 1 : 0);
	if (!bActivated)
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld(World);
	TWeakObjectPtr<ULyraAbilitySystemComponent> WeakASC(ASC);
	const int32 PlayerId = PlayerState->GetPlayerId();
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld, WeakASC, PlacementHandle, bConfirm, PlayerId]()
	{
		if (UWorld* CallbackWorld = WeakWorld.Get())
		{
			if (ULyraAbilitySystemComponent* CallbackASC = WeakASC.Get())
			{
				if (bConfirm)
				{
					CallbackASC->LocalInputConfirm();
				}
				else
				{
					CallbackASC->LocalInputCancel();
				}

				const FGameplayTag PlacementState = BwayGameplayTags::State_BuildablePlacement;
				const FGameplayAbilitySpec* Spec = CallbackASC->FindAbilitySpecFromHandle(PlacementHandle);
				UE_LOG(LogTemp, Display,
					TEXT("BwayBuildableTest: AbilityResult mode=%s active=%d placementTag=%d hasPlaced=%d"),
					bConfirm ? TEXT("confirm") : TEXT("cancel"),
					Spec && Spec->IsActive() ? 1 : 0,
					CallbackASC->HasMatchingGameplayTag(PlacementState) ? 1 : 0,
					FindPlayerStateById(CallbackWorld, PlayerId) && FindPlayerStateById(CallbackWorld, PlayerId)->HasPlacedBuildableThisRound() ? 1 : 0);
			}
		}
	}));
}

void Confirm(UWorld* World)
{
	RunAbility(World, true);
}

void Cancel(UWorld* World)
{
	RunAbility(World, false);
}

FGameplayAbilityTargetDataHandle MakeMalformedTargetData(const FString& Kind)
{
	FGameplayAbilityTargetDataHandle Data;
	if (Kind.Equals(TEXT("wrong"), ESearchCase::IgnoreCase))
	{
		Data.Add(new FGameplayAbilityTargetData_LocationInfo());
	}
	else if (Kind.Equals(TEXT("multi"), ESearchCase::IgnoreCase))
	{
		Data.Add(new FGameplayAbilityTargetData_SingleTargetHit());
		Data.Add(new FGameplayAbilityTargetData_SingleTargetHit());
	}
	return Data;
}

FGameplayAbilityTargetDataHandle MakeBlockingTargetData(const ABwayCharacterWithAbilities* Character)
{
    const ULyraAbilitySystemComponent* ASC = Character ? Character->GetLyraAbilitySystemComponent() : nullptr;
    if (ASC) for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        const auto* Ability = Cast<UBwayGameplayAbility_PlaceBuildable>(Spec.GetPrimaryInstance());
        FHitResult Hit;
        if (Ability && Spec.IsActive() && Ability->FindDevelopmentGroundTarget(Hit))
            return FGameplayAbilityTargetDataHandle(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
    }
    UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Fixture found no real valid ground target"));
    return FGameplayAbilityTargetDataHandle();
}

void RunMalformedTargetData(UWorld* World, const FString& Kind)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Malformed target-data fixture requires a game world"));
		return;
	}

	ABwayCharacterWithAbilities* Character = FindLocalCharacter(World);
	ULyraAbilitySystemComponent* ASC = Character ? Character->GetLyraAbilitySystemComponent() : nullptr;
	if (!Character || !ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Malformed target-data fixture could not resolve client character ASC"));
		return;
	}

	FGameplayAbilitySpecHandle PlacementHandle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA(UBwayGameplayAbility_PlaceBuildable::StaticClass()))
		{
			PlacementHandle = Spec.Handle;
			break;
		}
	}
	FGameplayAbilitySpec* Spec = PlacementHandle.IsValid() ? ASC->FindAbilitySpecFromHandle(PlacementHandle) : nullptr;
	if (!Spec || !ASC->TryActivateAbility(PlacementHandle, false))
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Malformed target-data fixture could not activate placement ability kind=%s"), *Kind);
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld(World);
	TWeakObjectPtr<ULyraAbilitySystemComponent> WeakASC(ASC);
	const int32 PlayerId = Character->GetPlayerState<ABwayPlayerState>() ? Character->GetPlayerState<ABwayPlayerState>()->GetPlayerId() : INDEX_NONE;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld, WeakASC, PlacementHandle, Kind, PlayerId]()
	{
		if (UWorld* CallbackWorld = WeakWorld.Get())
		{
			if (ULyraAbilitySystemComponent* CallbackASC = WeakASC.Get())
			{
				if (FGameplayAbilitySpec* CallbackSpec = CallbackASC->FindAbilitySpecFromHandle(PlacementHandle))
				{
					if (UBwayGameplayAbility_PlaceBuildable* Ability = Cast<UBwayGameplayAbility_PlaceBuildable>(CallbackSpec->GetPrimaryInstance()))
					{
						Ability->RunDevelopmentTargetDataProbe(MakeMalformedTargetData(Kind));
					}
				}

				const FGameplayTag PlacementState = BwayGameplayTags::State_BuildablePlacement;
				const FGameplayAbilitySpec* ResultSpec = CallbackASC->FindAbilitySpecFromHandle(PlacementHandle);
				UE_LOG(LogTemp, Display,
					TEXT("BwayBuildableTest: MalformedResult kind=%s active=%d placementTag=%d hasPlaced=%d"),
					*Kind,
					ResultSpec && ResultSpec->IsActive() ? 1 : 0,
					CallbackASC->HasMatchingGameplayTag(PlacementState) ? 1 : 0,
					FindPlayerStateById(CallbackWorld, PlayerId) && FindPlayerStateById(CallbackWorld, PlayerId)->HasPlacedBuildableThisRound() ? 1 : 0);
			}
		}
	}));
}

void SpawnFromConsole()
{
	Spawn(FindPIEWorld(true));
}

void ResetRoundFromConsole()
{
	ResetRound(FindPIEWorld(true));
}

void ConfirmFromConsole()
{
	Confirm(FindPIEWorld(false));
}

void CancelFromConsole()
{
	Cancel(FindPIEWorld(false));
}

void MalformedEmptyFromConsole()
{
	RunMalformedTargetData(FindPIEWorld(false), TEXT("empty"));
}

void MalformedWrongFromConsole()
{
	RunMalformedTargetData(FindPIEWorld(false), TEXT("wrong"));
}

void MalformedMultiFromConsole()
{
	RunMalformedTargetData(FindPIEWorld(false), TEXT("multi"));
}

void RunReplicatedQuotaReject(UWorld* ClientWorld)
{
	if (!ClientWorld || !ClientWorld->IsGameWorld() || ClientWorld->GetNetMode() != NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated quota-reject fixture requires a client game world"));
		return;
	}

	UWorld* AuthorityWorld = FindPIEWorld(true);
	const ABwayGameState* AuthorityGameState = AuthorityWorld ? AuthorityWorld->GetGameState<ABwayGameState>() : nullptr;
	if (!AuthorityGameState || !AuthorityGameState->RoundManagementComponent || AuthorityGameState->RoundManagementComponent->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated quota-reject fixture requires the Playing phase"));
		return;
	}
	ABwayCharacterWithAbilities* Character = FindLocalCharacter(ClientWorld);
	ULyraAbilitySystemComponent* ASC = Character ? Character->GetLyraAbilitySystemComponent() : nullptr;
	ABwayPlayerState* ClientPlayerState = Character ? Character->GetPlayerState<ABwayPlayerState>() : nullptr;
	ABwayPlayerState* AuthorityPlayerState = ClientPlayerState && AuthorityWorld
		? FindPlayerStateById(AuthorityWorld, ClientPlayerState->GetPlayerId())
		: nullptr;
	APlayerController* AuthorityPC = AuthorityPlayerState && AuthorityWorld
		? FindPlayerControllerById(AuthorityWorld, AuthorityPlayerState->GetPlayerId())
		: nullptr;
	if (!AuthorityWorld || !Character || !ASC || !ClientPlayerState || !AuthorityPlayerState || !AuthorityPC)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Replicated quota-reject fixture could not resolve client/authority ownership"));
		return;
	}
	if (AuthorityPlayerState->HasPlacedBuildableThisRound())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated quota-reject fixture requires an unused authority quota"));
		return;
	}

	FGameplayAbilitySpecHandle PlacementHandle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA(UBwayGameplayAbility_PlaceBuildable::StaticClass()))
		{
			PlacementHandle = Spec.Handle;
			break;
		}
	}
	if (!PlacementHandle.IsValid() || !ASC->TryActivateAbility(PlacementHandle, false))
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Replicated quota-reject fixture could not activate placement ability"));
		return;
	}

	FGameplayAbilitySpec* ClientSpec = ASC->FindAbilitySpecFromHandle(PlacementHandle);
	UGameplayAbility* Ability = ClientSpec ? ClientSpec->GetPrimaryInstance() : nullptr;
	if (!Ability)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Replicated quota-reject fixture has no active ability instance"));
		ASC->LocalInputCancel();
		return;
	}

	const int32 PlayerId = ClientPlayerState->GetPlayerId();
	const int32 CountBefore = UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(AuthorityWorld, AuthorityPC);
	const FGameplayAbilityActivationInfo ActivationInfo = Ability->GetCurrentActivationInfo();
	const FGameplayAbilityTargetDataHandle TargetData = MakeBlockingTargetData(Character);

	TWeakObjectPtr<UWorld> WeakAuthorityWorld(AuthorityWorld);
	TWeakObjectPtr<ABwayPlayerState> WeakAuthorityPlayerState(AuthorityPlayerState);
	TWeakObjectPtr<APlayerController> WeakAuthorityPC(AuthorityPC);
	TWeakObjectPtr<UWorld> WeakClientWorld(ClientWorld);
	TWeakObjectPtr<ULyraAbilitySystemComponent> WeakClientASC(ASC);
	const FPredictionKey CurrentPredictionKey = ASC->ScopedPredictionKey;
	FTimerHandle QuotaTimer;
	AuthorityWorld->GetTimerManager().SetTimer(QuotaTimer, FTimerDelegate::CreateLambda([WeakAuthorityPlayerState]()
	{
		if (ABwayPlayerState* PlayerState = WeakAuthorityPlayerState.Get())
		{
			// Let the server receive the predicted activation before making the
			// target-data request fail its authoritative quota check.
			PlayerState->MarkBuildablePlacedThisRound();
		}
	}), 1.0f, false);

	FTimerHandle SubmitTimer;
	ClientWorld->GetTimerManager().SetTimer(SubmitTimer, FTimerDelegate::CreateLambda([WeakClientWorld, WeakClientASC, PlacementHandle, ActivationInfo, TargetData, PlayerId, CountBefore, CurrentPredictionKey]()
	{
		if (UWorld* SubmitWorld = WeakClientWorld.Get())
		{
			if (ULyraAbilitySystemComponent* SubmitASC = WeakClientASC.Get())
			{
				if (GEngine)
				{
					GEngine->Exec(SubmitWorld, TEXT("Net PktLag=100"));
				}
				SubmitASC->CallServerSetReplicatedTargetData(
					PlacementHandle,
					ActivationInfo.GetActivationPredictionKey(),
					TargetData,
					FGameplayTag(),
					CurrentPredictionKey);
				UE_LOG(LogTemp, Display,
					TEXT("BwayBuildableTest: ReplicatedRejectSubmit playerId=%d countBefore=%d lagMs=100"),
					PlayerId,
					CountBefore);
				if (GEngine)
				{
					GEngine->Exec(SubmitWorld, TEXT("Net PktLag=0"));
				}
			}
		}
	}), 1.5f, false);

	FTimerHandle ResultTimer;
	AuthorityWorld->GetTimerManager().SetTimer(ResultTimer, FTimerDelegate::CreateLambda([WeakAuthorityWorld, WeakAuthorityPlayerState, WeakAuthorityPC, PlacementHandle, PlayerId, CountBefore]()
	{
		if (UWorld* DelayedWorld = WeakAuthorityWorld.Get())
		{
			ABwayPlayerState* PlayerState = WeakAuthorityPlayerState.Get();
			APlayerController* PC = WeakAuthorityPC.Get();
			const int32 CountAfter = UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(DelayedWorld, PC);
			const bool bHasPlaced = PlayerState && PlayerState->HasPlacedBuildableThisRound();
			bool bAuthorityAbilityActive = false;
			if (ABwayCharacterWithAbilities* AuthorityCharacter = PlayerState ? Cast<ABwayCharacterWithAbilities>(PlayerState->GetPawn()) : nullptr)
			{
				if (ULyraAbilitySystemComponent* AuthorityASC = AuthorityCharacter->GetLyraAbilitySystemComponent())
				{
					if (const FGameplayAbilitySpec* AuthoritySpec = AuthorityASC->FindAbilitySpecFromHandle(PlacementHandle))
					{
						bAuthorityAbilityActive = AuthoritySpec->IsActive();
					}
				}
			}
			UE_LOG(LogTemp, Display,
				TEXT("BwayBuildableTest: ReplicatedRejectResult playerId=%d countBefore=%d countAfter=%d spawned=%d hasPlacedAfter=%d authorityAbilityActive=%d"),
				PlayerId,
				CountBefore,
				CountAfter,
				CountAfter > CountBefore ? 1 : 0,
				bHasPlaced ? 1 : 0,
				bAuthorityAbilityActive ? 1 : 0);
			if (PlayerState)
			{
				PlayerState->BeginRoundStatTracking();
			}
		}
	}), 4.0f, false);
}

void ReplicatedQuotaRejectFromConsole()
{
	UWorld* ClientWorld = FindPIEWorld(false);
	if (!ClientWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated quota-reject fixture could not resolve a client world"));
		return;
	}

	TWeakObjectPtr<UWorld> WeakClientWorld(ClientWorld);
	TSharedRef<int32> Attempts = MakeShared<int32>(0);
	TSharedRef<FTimerHandle> PollHandle = MakeShared<FTimerHandle>();
	ClientWorld->GetTimerManager().SetTimer(*PollHandle, FTimerDelegate::CreateLambda([WeakClientWorld, Attempts, PollHandle]()
	{
		UWorld* PollWorld = WeakClientWorld.Get();
		if (!PollWorld)
		{
			return;
		}
		++(*Attempts);
		const ABwayGameState* GameState = PollWorld->GetGameState<ABwayGameState>();
		ABwayCharacterWithAbilities* Character = FindLocalCharacter(PollWorld);
		ABwayPlayerState* ClientPlayerState = Character ? Character->GetPlayerState<ABwayPlayerState>() : nullptr;
		UWorld* AuthorityWorld = FindPIEWorld(true);
		const bool bOwnershipReady = Character && Character->GetLyraAbilitySystemComponent() && ClientPlayerState && AuthorityWorld
			&& FindPlayerStateById(AuthorityWorld, ClientPlayerState->GetPlayerId())
			&& FindPlayerControllerById(AuthorityWorld, ClientPlayerState->GetPlayerId());
		if (GameState && GameState->RoundManagementComponent && GameState->RoundManagementComponent->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && bOwnershipReady)
		{
			PollWorld->GetTimerManager().ClearTimer(*PollHandle);
			RunReplicatedQuotaReject(PollWorld);
		}
		else if (*Attempts >= 120)
		{
			PollWorld->GetTimerManager().ClearTimer(*PollHandle);
			UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated quota-reject fixture timed out waiting for Playing phase"));
		}
	}), 0.1f, true);
}

void RunReplicatedValidConfirm(UWorld* ClientWorld)
{
	if (!ClientWorld || !ClientWorld->IsGameWorld() || ClientWorld->GetNetMode() != NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated valid-confirm fixture requires a client game world"));
		return;
	}

	UWorld* AuthorityWorld = FindPIEWorld(true);
	const ABwayGameState* AuthorityGameState = AuthorityWorld ? AuthorityWorld->GetGameState<ABwayGameState>() : nullptr;
	if (!AuthorityGameState || !AuthorityGameState->RoundManagementComponent || AuthorityGameState->RoundManagementComponent->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated valid-confirm fixture requires the Playing phase"));
		return;
	}

	ABwayCharacterWithAbilities* Character = FindLocalCharacter(ClientWorld);
	ULyraAbilitySystemComponent* ASC = Character ? Character->GetLyraAbilitySystemComponent() : nullptr;
	ABwayPlayerState* ClientPlayerState = Character ? Character->GetPlayerState<ABwayPlayerState>() : nullptr;
	ABwayPlayerState* AuthorityPlayerState = ClientPlayerState && AuthorityWorld
		? FindPlayerStateById(AuthorityWorld, ClientPlayerState->GetPlayerId())
		: nullptr;
	APlayerController* AuthorityPC = AuthorityPlayerState && AuthorityWorld
		? FindPlayerControllerById(AuthorityWorld, AuthorityPlayerState->GetPlayerId())
		: nullptr;
	if (!AuthorityWorld || !Character || !ASC || !ClientPlayerState || !AuthorityPlayerState || !AuthorityPC)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Replicated valid-confirm fixture could not resolve client/authority ownership"));
		return;
	}

	const int32 PlayerId = ClientPlayerState->GetPlayerId();
	const int32 CountBefore = UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(AuthorityWorld, AuthorityPC);
	if (AuthorityPlayerState->HasPlacedBuildableThisRound() || CountBefore != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated valid-confirm fixture requires an unused quota and zero owned buildables"));
		return;
	}

	FGameplayAbilitySpecHandle PlacementHandle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA(UBwayGameplayAbility_PlaceBuildable::StaticClass()))
		{
			PlacementHandle = Spec.Handle;
			break;
		}
	}
	if (!PlacementHandle.IsValid() || !ASC->TryActivateAbility(PlacementHandle, false))
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Replicated valid-confirm fixture could not activate placement ability"));
		return;
	}

	FGameplayAbilitySpec* ClientSpec = ASC->FindAbilitySpecFromHandle(PlacementHandle);
	UGameplayAbility* Ability = ClientSpec ? ClientSpec->GetPrimaryInstance() : nullptr;
	if (!Ability)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayBuildableTest: Replicated valid-confirm fixture has no active ability instance"));
		ASC->LocalInputCancel();
		return;
	}

	const FGameplayAbilityActivationInfo ActivationInfo = Ability->GetCurrentActivationInfo();
	const FGameplayAbilityTargetDataHandle TargetData = MakeBlockingTargetData(Character);
	const FPredictionKey CurrentPredictionKey = ASC->ScopedPredictionKey;
	TWeakObjectPtr<UWorld> WeakAuthorityWorld(AuthorityWorld);
	TWeakObjectPtr<ABwayPlayerState> WeakAuthorityPlayerState(AuthorityPlayerState);
	TWeakObjectPtr<APlayerController> WeakAuthorityPC(AuthorityPC);
	TWeakObjectPtr<UWorld> WeakClientWorld(ClientWorld);
	TWeakObjectPtr<ULyraAbilitySystemComponent> WeakClientASC(ASC);

	FTimerHandle SubmitTimer;
	ClientWorld->GetTimerManager().SetTimer(SubmitTimer, FTimerDelegate::CreateLambda([WeakClientWorld, WeakClientASC, PlacementHandle, ActivationInfo, TargetData, PlayerId, CountBefore, CurrentPredictionKey]()
	{
		if (UWorld* SubmitWorld = WeakClientWorld.Get())
		{
			if (ULyraAbilitySystemComponent* SubmitASC = WeakClientASC.Get())
			{
				SubmitASC->CallServerSetReplicatedTargetData(
					PlacementHandle,
					ActivationInfo.GetActivationPredictionKey(),
					TargetData,
					FGameplayTag(),
					CurrentPredictionKey);
				UE_LOG(LogTemp, Display,
					TEXT("BwayBuildableTest: ReplicatedConfirmSubmit playerId=%d countBefore=%d targetBlocking=%d"),
					PlayerId,
					CountBefore,
					TargetData.IsValid(0) ? 1 : 0);
			}
		}
	}), 1.0f, false);

	FTimerHandle ResultTimer;
	AuthorityWorld->GetTimerManager().SetTimer(ResultTimer, FTimerDelegate::CreateLambda([WeakAuthorityWorld, WeakAuthorityPlayerState, WeakAuthorityPC, PlacementHandle, PlayerId, CountBefore]()
	{
		if (UWorld* ResultWorld = WeakAuthorityWorld.Get())
		{
			ABwayPlayerState* PlayerState = WeakAuthorityPlayerState.Get();
			APlayerController* PC = WeakAuthorityPC.Get();
			const int32 CountAfter = UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(ResultWorld, PC);
			const bool bHasPlaced = PlayerState && PlayerState->HasPlacedBuildableThisRound();
			bool bAuthorityAbilityActive = false;
			if (ABwayCharacterWithAbilities* AuthorityCharacter = PlayerState ? Cast<ABwayCharacterWithAbilities>(PlayerState->GetPawn()) : nullptr)
			{
				if (ULyraAbilitySystemComponent* AuthorityASC = AuthorityCharacter->GetLyraAbilitySystemComponent())
				{
					if (const FGameplayAbilitySpec* AuthoritySpec = AuthorityASC->FindAbilitySpecFromHandle(PlacementHandle))
					{
						bAuthorityAbilityActive = AuthoritySpec->IsActive();
					}
				}
			}
			UE_LOG(LogTemp, Display,
				TEXT("BwayBuildableTest: ReplicatedConfirmResult playerId=%d countBefore=%d countAfter=%d spawned=%d hasPlacedAfter=%d authorityAbilityActive=%d"),
				PlayerId,
				CountBefore,
				CountAfter,
				CountAfter > CountBefore ? 1 : 0,
				bHasPlaced ? 1 : 0,
				bAuthorityAbilityActive ? 1 : 0);

			LogPeerSamples(ResultWorld, PlayerId, TEXT("ReplicatedConfirm"));
			FTimerHandle CleanupTimer;
			ResultWorld->GetTimerManager().SetTimer(CleanupTimer, FTimerDelegate::CreateLambda([WeakAuthorityWorld, WeakAuthorityPlayerState, WeakAuthorityPC, PlayerId]()
			{
				if (UWorld* CleanupWorld = WeakAuthorityWorld.Get())
				{
					if (APlayerController* CleanupPC = WeakAuthorityPC.Get())
					{
						for (TActorIterator<ABuildableActor> It(CleanupWorld); It; ++It)
						{
							if (ABuildableActor* Buildable = *It; Buildable && Buildable->GetOwner() == CleanupPC)
							{
								Buildable->Destroy();
							}
						}
					}
					if (ABwayPlayerState* CleanupPlayerState = WeakAuthorityPlayerState.Get())
					{
						CleanupPlayerState->BeginRoundStatTracking();
					}
					UE_LOG(LogTemp, Display,
						TEXT("BwayBuildableTest: ReplicatedConfirmCleanup playerId=%d countAfterCleanup=%d"),
						PlayerId,
						WeakAuthorityPC.Get() ? UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(CleanupWorld, WeakAuthorityPC.Get()) : -1);
				}
			}), 0.5f, false);
		}
	}), 4.0f, false);
}

void ReplicatedValidConfirmFromConsole()
{
	UWorld* ClientWorld = FindPIEWorld(false);
	if (!ClientWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated valid-confirm fixture could not resolve a client world"));
		return;
	}

	TWeakObjectPtr<UWorld> WeakClientWorld(ClientWorld);
	TSharedRef<int32> Attempts = MakeShared<int32>(0);
	TSharedRef<FTimerHandle> PollHandle = MakeShared<FTimerHandle>();
	ClientWorld->GetTimerManager().SetTimer(*PollHandle, FTimerDelegate::CreateLambda([WeakClientWorld, Attempts, PollHandle]()
	{
		UWorld* PollWorld = WeakClientWorld.Get();
		if (!PollWorld)
		{
			return;
		}
		++(*Attempts);
		const ABwayGameState* GameState = PollWorld->GetGameState<ABwayGameState>();
		ABwayCharacterWithAbilities* Character = FindLocalCharacter(PollWorld);
		ABwayPlayerState* ClientPlayerState = Character ? Character->GetPlayerState<ABwayPlayerState>() : nullptr;
		UWorld* AuthorityWorld = FindPIEWorld(true);
		const bool bOwnershipReady = Character && Character->GetLyraAbilitySystemComponent() && ClientPlayerState && AuthorityWorld
			&& FindPlayerStateById(AuthorityWorld, ClientPlayerState->GetPlayerId())
			&& FindPlayerControllerById(AuthorityWorld, ClientPlayerState->GetPlayerId());
		if (GameState && GameState->RoundManagementComponent && GameState->RoundManagementComponent->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && bOwnershipReady)
		{
			PollWorld->GetTimerManager().ClearTimer(*PollHandle);
			RunReplicatedValidConfirm(PollWorld);
		}
		else if (*Attempts >= 120)
		{
			PollWorld->GetTimerManager().ClearTimer(*PollHandle);
			UE_LOG(LogTemp, Warning, TEXT("BwayBuildableTest: Replicated valid-confirm fixture timed out waiting for Playing phase"));
		}
	}), 0.1f, true);
}

FAutoConsoleCommand SpawnCommand(
	TEXT("bway.Test.BuildableSpawn"),
	TEXT("Development authority fixture: spawn the selected player's production buildable, verify quota, and sample PIE peers."),
	FConsoleCommandDelegate::CreateStatic(&SpawnFromConsole));

FAutoConsoleCommand ResetRoundCommand(
	TEXT("bway.Test.BuildableResetRound"),
	TEXT("Development authority fixture: call the production round reset and sample buildable persistence/quota reset."),
	FConsoleCommandDelegate::CreateStatic(&ResetRoundFromConsole));

FAutoConsoleCommand ConfirmCommand(
	TEXT("bway.Test.BuildableConfirm"),
	TEXT("Development fixture: activate the production placement ability and issue LocalInputConfirm on the next tick."),
	FConsoleCommandDelegate::CreateStatic(&ConfirmFromConsole));

FAutoConsoleCommand CancelCommand(
	TEXT("bway.Test.BuildableCancel"),
	TEXT("Development fixture: activate the production placement ability and issue LocalInputCancel on the next tick."),
	FConsoleCommandDelegate::CreateStatic(&CancelFromConsole));

FAutoConsoleCommand MalformedEmptyCommand(
	TEXT("bway.Test.BuildableMalformedEmpty"),
	TEXT("Development fixture: submit empty target data to the production placement ability."),
	FConsoleCommandDelegate::CreateStatic(&MalformedEmptyFromConsole));

FAutoConsoleCommand MalformedWrongCommand(
	TEXT("bway.Test.BuildableMalformedWrong"),
	TEXT("Development fixture: submit wrong-type target data to the production placement ability."),
	FConsoleCommandDelegate::CreateStatic(&MalformedWrongFromConsole));

FAutoConsoleCommand MalformedMultiCommand(
	TEXT("bway.Test.BuildableMalformedMulti"),
	TEXT("Development fixture: submit multi-entry target data to the production placement ability."),
	FConsoleCommandDelegate::CreateStatic(&MalformedMultiFromConsole));

FAutoConsoleCommand ReplicatedQuotaRejectCommand(
	TEXT("bway.Test.BuildableReplicatedQuotaReject"),
	TEXT("Development fixture: submit a valid target through GAS target-data replication while the server quota rejects it under 100 ms lag."),
	FConsoleCommandDelegate::CreateStatic(&ReplicatedQuotaRejectFromConsole));

FAutoConsoleCommand ReplicatedValidConfirmCommand(
	TEXT("bway.Test.BuildableReplicatedValidConfirm"),
	TEXT("Development fixture: submit a valid target through GAS target-data replication and verify authoritative spawn and peer convergence."),
	FConsoleCommandDelegate::CreateStatic(&ReplicatedValidConfirmFromConsole));

} // namespace BwayBuildableTest

#endif
