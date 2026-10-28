// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterWithAbilities.h"
#include "BwayGameplayTags.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "Camera/LyraCameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "BwayCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "Relic/BwayRelicRequestIndicatorComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayGameState.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameFramework/PlayerState.h"
#include "BwayPlayerState.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "Movement/BwayMovementFeelConfig.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Player/LyraPlayerState.h"
#include "Teams/LyraTeamDisplayAsset.h"
#include "Teams/LyraTeamSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Character/LyraHealthComponent.h"
#include "Messages/LyraVerbMessageHelpers.h"


ABwayCharacterWithAbilities::ABwayCharacterWithAbilities(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UBwayCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Lyra disables actor ticking; local slide camera presentation needs it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;

	BwayCharacterMovementComponent = Cast<UBwayCharacterMovementComponent>(GetCharacterMovement());
	BwayCharacterMovementComponent->SetIsReplicated(true);
	RelicRequestIndicatorComponent = CreateDefaultSubobject<UBwayRelicRequestIndicatorComponent>(TEXT("RelicRequestIndicatorComponent"));

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = bUseControllerRotationYaw = bUseControllerRotationRoll = false;

}

void ABwayCharacterWithAbilities::OnAbilitySystemInitialized()
{
	Super::OnAbilitySystemInitialized();
	UnbindCombatHealth();
	CombatContributors.Reset();
	LastDamagePlayerState.Reset();
	CombatRound = INDEX_NONE;
	bLastDamageWasLethal = false;
	bCombatDeathConsumed = false;
	if (HasAuthority())
	{
		if (const auto* ASC = GetLyraAbilitySystemComponent())
		{
			if (const auto* Health = ASC->GetSet<ULyraHealthSet>())
			{
				CombatHealthSet = Health;
				CombatHealthChangedHandle = Health->OnHealthChanged.AddUObject(this, &ThisClass::HandleCombatHealthChanged);
			}
		}
	}

	if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
	{
		MoveComp->BindAbilitySystem(GetAbilitySystemComponent());
	}
}

void ABwayCharacterWithAbilities::OnAbilitySystemUninitialized()
{
	StopPrimaryActionMontage(true, TEXT("ability-system-uninitialized"));
	UnbindCombatHealth();
	CombatContributors.Reset();
	LastDamagePlayerState.Reset();
	ClearRelicRequestForCurrentPawn();
	if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
	{
		MoveComp->UnbindAbilitySystem();
	}

	if (HasAuthority())
	{
		if (ABwayPlayerState* BwayPS = GetPlayerState<ABwayPlayerState>())
		{
			if (ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent())
			{
				BwayPS->GetHeroAbilityGrantedHandles().TakeFromAbilitySystem(ASC);
			}
		}
	}

	Super::OnAbilitySystemUninitialized();
}

void ABwayCharacterWithAbilities::ClearRelicRequestForCurrentPawn()
{
	ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent();
	if (!ASC || ASC->GetAvatarActor() != this)
	{
		return;
	}

	if (HasAuthority())
	{
		const FGameplayTag RequestTag = UBwayRelicRequestIndicatorComponent::ResolveRequestTag(this);
		if (RequestTag.IsValid())
		{
			// The PlayerState ASC survives this pawn. A new avatar must make its own request.
			ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(RequestTag));
		}
		if (BwayGameplayTags::State_Relic_PreparingCatch.GetTag().IsValid() &&
			ASC->GetTagCount(BwayGameplayTags::State_Relic_PreparingCatch) > 0)
		{
			ASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0, EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
	else if (BwayGameplayTags::State_Relic_PreparingCatch.GetTag().IsValid() &&
		ASC->GetTagCount(BwayGameplayTags::State_Relic_PreparingCatch) > 0)
	{
		ASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0, EGameplayTagReplicationState::None);
	}
}

void ABwayCharacterWithAbilities::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwayCharacterWithAbilities, ReplicatedHeroId);
}

void ABwayCharacterWithAbilities::OnDeathStarted(AActor* OwningActor)
{
	StopPrimaryActionMontage(true, TEXT("death"));
	StopSlideCameraShake(true);
	Super::OnDeathStarted(OwningActor);
	ClearRelicRequestForCurrentPawn();
	if (HasAuthority())
	{
		if (ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>())
		{
			if (UBwayRoundManagementComponent* RoundMgmt = GS->FindComponentByClass<UBwayRoundManagementComponent>())
			{
				RoundMgmt->OnPlayerDied(GetController(), nullptr);
			}

			if (UBwayRelicManagerComponent* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>())
			{
				RelicMgr->DropRelicIfCarriedBy(this);
			}
		}
	}
}

void ABwayCharacterWithAbilities::UnbindCombatHealth()
{
	if (const auto* Health = CombatHealthSet.Get())
	{
		Health->OnHealthChanged.Remove(CombatHealthChangedHandle);
	}
	CombatHealthChangedHandle.Reset();
	CombatHealthSet.Reset();
}

void ABwayCharacterWithAbilities::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPrimaryActionMontage(true, TEXT("end-play"));
	StopSlideCameraShake(true);
	UnbindCombatHealth();
	Super::EndPlay(EndPlayReason);
}

bool ABwayCharacterWithAbilities::PlayPrimaryActionMontage(UAnimMontage* Montage, float PlayRate)
{
	const auto Reject = [this, Montage](const TCHAR* Reason)
	{
#if !UE_BUILD_SHIPPING
		const APlayerState* State = GetPlayerState();
		UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAction: Rejected player=%d net=%d montage=%s reason=%s pawn=%s"),
			State ? State->GetPlayerId() : INDEX_NONE, int32(GetNetMode()), *GetNameSafe(Montage), Reason, *GetPathName());
#else
		UE_UNUSED(Reason);
#endif
		return false;
	};
	if (bStartingPrimaryActionMontage) return Reject(TEXT("reentrant-playback"));
	TGuardValue<bool> StartingGuard(bStartingPrimaryActionMontage, true);

	const auto IsLiveAvatar = [this]()
	{
		const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(this);
		const ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent();
		return IsValid(this) && GetWorld() && GetWorld()->IsGameWorld() && GetNetMode() != NM_DedicatedServer
			&& Health && !Health->IsDeadOrDying() && FMath::IsFinite(Health->GetHealth()) && Health->GetHealth() > 0.0f
			&& ASC && ASC->GetAvatarActor() == this;
	};
	if (!IsLiveAvatar()) return Reject(TEXT("invalid-or-dead-avatar"));
	if (!IsValid(Montage) || !FMath::IsFinite(PlayRate) || PlayRate <= 0.0f
		|| !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0.0f
		|| !FMath::IsFinite(Montage->GetPlayLength()) || Montage->GetPlayLength() <= 0.0f
		|| Montage->SlotAnimTracks.IsEmpty() || !Montage->bEnableAutoBlendOut)
	{
		return Reject(TEXT("invalid-montage-or-rate"));
	}
	if (Montage->HasRootMotion() || Montage->bEnableRootMotionTranslation || Montage->bEnableRootMotionRotation)
	{
		return Reject(TEXT("root-motion-not-cosmetic"));
	}
	USkeletalMeshComponent* ActionMesh = GetMesh();
	if (ActionMesh && ActionMesh->VisibilityBasedAnimTickOption > EVisibilityBasedAnimTickOption::AlwaysTickPose)
	{
		ActionMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	}
	UAnimInstance* Anim = ActionMesh ? ActionMesh->GetAnimInstance() : nullptr;
	if (!IsValid(Anim) || ActionMesh->IsSimulatingPhysics()) return Reject(TEXT("animation-unavailable"));

	const FName Group = Montage->GetGroupName();
	const auto HasForeignMontageInGroup = [this, Group](const UAnimInstance* CandidateAnim)
	{
		for (const FAnimMontageInstance* Instance : CandidateAnim->MontageInstances)
		{
			if (!Instance || !Instance->IsActive() || !Instance->Montage || Instance->Montage->GetGroupName() != Group) continue;
			const bool bOwnedInstance = CandidateAnim == PrimaryActionAnimInstance.Get()
				&& Instance->Montage == PrimaryActionMontage.Get() && Instance->GetInstanceID() == PrimaryActionMontageInstanceId;
			if (!bOwnedInstance) return true;
		}
		return false;
	};
	if (HasForeignMontageInGroup(Anim)) return Reject(TEXT("montage-group-in-use"));

	const TWeakObjectPtr<UAnimInstance> WeakAnim(Anim);
	StopPrimaryActionMontage(false, TEXT("replaced"));
	// Stopping a montage can invoke arbitrary blend-out callbacks, including avatar teardown.
	if (!IsLiveAvatar() || !WeakAnim.IsValid() || !GetMesh() || GetMesh()->GetAnimInstance() != WeakAnim.Get())
	{
		return Reject(TEXT("avatar-changed-during-stop"));
	}
	Anim = WeakAnim.Get();
	if (HasForeignMontageInGroup(Anim)) return Reject(TEXT("montage-group-changed-during-stop"));
	const float Duration = Anim->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::Duration, 0.0f, false);
	if (Duration <= 0.0f || !WeakAnim.IsValid()) return Reject(TEXT("montage-play-failed"));
	FAnimMontageInstance* Instance = WeakAnim->GetActiveInstanceForMontage(Montage);
	if (!Instance) return Reject(TEXT("montage-ended-during-start"));
	PrimaryActionAnimInstance = WeakAnim;
	PrimaryActionMontage = Montage;
	PrimaryActionMontageInstanceId = Instance->GetInstanceID();
	if (!IsLiveAvatar() || !GetMesh() || GetMesh()->GetAnimInstance() != WeakAnim.Get())
	{
		StopPrimaryActionMontage(true, TEXT("avatar-changed-during-start"));
		return Reject(TEXT("avatar-changed-during-start"));
	}
#if !UE_BUILD_SHIPPING
	const APlayerState* State = GetPlayerState();
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAction: Played player=%d net=%d local=%d montage=%s group=%s instance=%d rate=%.3f duration=%.3f pawn=%s"),
		State ? State->GetPlayerId() : INDEX_NONE, int32(GetNetMode()), IsLocallyControlled(),
		*Montage->GetPathName(), *Group.ToString(), PrimaryActionMontageInstanceId, PlayRate, Duration, *GetPathName());
#endif
	return true;
}

void ABwayCharacterWithAbilities::StopPrimaryActionMontage(bool bImmediately, const TCHAR* Reason)
{
	TGuardValue<bool> StartingGuard(bStartingPrimaryActionMontage, true);
	UAnimInstance* Anim = PrimaryActionAnimInstance.Get();
	UAnimMontage* Montage = PrimaryActionMontage.Get();
	const int32 InstanceId = PrimaryActionMontageInstanceId;
	PrimaryActionAnimInstance.Reset();
	PrimaryActionMontage.Reset();
	PrimaryActionMontageInstanceId = INDEX_NONE;
	FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(InstanceId) : nullptr;
	if (!Instance || !Montage || Instance->Montage != Montage) return;
#if !UE_BUILD_SHIPPING
	const APlayerState* State = GetPlayerState();
	UE_LOG(LogTemp, Display, TEXT("BwayPrimaryAction: Stopped player=%d net=%d montage=%s instance=%d immediate=%d reason=%s pawn=%s"),
		State ? State->GetPlayerId() : INDEX_NONE, int32(GetNetMode()), *Montage->GetPathName(), InstanceId, bImmediately, Reason, *GetPathName());
#else
	UE_UNUSED(Reason);
#endif
	FAlphaBlend BlendOut = Montage->BlendOut;
	if (bImmediately) BlendOut.SetBlendTime(0.0f);
	// Address the owned instance ID, so a newer use of the same montage is never stopped.
	Instance->Stop(BlendOut);
}

void ABwayCharacterWithAbilities::HandleCombatHealthChanged(AActor* DamageInstigator, AActor* Causer,
	const FGameplayEffectSpec* Spec, float Magnitude, float OldHealth, float NewHealth)
{
	// Observe the actual health loss before OnOutOfHealth starts the death ability. Healing,
	// blocked damage and fractional UI rounding cannot create or hide a contribution.
	if (!HasAuthority() || bCombatDeathConsumed || !FMath::IsFinite(OldHealth)
		|| !FMath::IsFinite(NewHealth) || OldHealth <= NewHealth || OldHealth <= 0.0f)
	{
		return;
	}
	auto* GS = GetWorld()->GetGameState<ABwayGameState>();
	auto* Round = GS ? GS->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
	auto* Victim = GetPlayerState<ABwayPlayerState>();
	if (!Round || !Victim || Victim->GetPawn() != this || !GS->PlayerArray.Contains(Victim)
		|| Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing
		|| Round->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}
	const int32 VictimTeam = GS->GetPlayerTeam(Victim);
	if (CombatRound != Round->GetCurrentRoundNumber() || CombatVictimTeam != VictimTeam)
	{
		CombatContributors.Reset();
	}
	CombatRound = Round->GetCurrentRoundNumber();
	CombatVictimTeam = VictimTeam;

	// The ASC owner survives the source pawn. This also covers a projectile or periodic
	// effect that kills after its source pawn dies or is replaced.
	ABwayPlayerState* Source = nullptr;
	if (Spec)
	{
		if (const auto* SourceASC = Spec->GetEffectContext().GetOriginalInstigatorAbilitySystemComponent())
		{
			Source = Cast<ABwayPlayerState>(SourceASC->GetOwnerActor());
		}
	}
	if (!Source) Source = Cast<ABwayPlayerState>(ULyraVerbMessageHelpers::GetPlayerStateFromObject(DamageInstigator));
	if (!Source && Causer) Source = Cast<ABwayPlayerState>(ULyraVerbMessageHelpers::GetPlayerStateFromObject(Causer->GetInstigator()));
	const int32 SourceTeam = Source ? GS->GetPlayerTeam(Source) : INDEX_NONE;
	const bool bEnemy = Source && Source != Victim && GS->PlayerArray.Contains(Source)
		&& SourceTeam != INDEX_NONE && VictimTeam != INDEX_NONE && SourceTeam != VictimTeam;
	LastDamagePlayerState = bEnemy ? Source : nullptr;
	bLastDamageWasLethal = NewHealth <= 0.0f;
	if (bEnemy)
	{
		CombatContributors.FindOrAdd(Source) = {GetWorld()->GetTimeSeconds(), SourceTeam};
	}
}

bool ABwayCharacterWithAbilities::ConsumeCombatCredit(ABwayPlayerState*& OutKiller,
	TArray<ABwayPlayerState*>& OutAssisters)
{
	OutKiller = nullptr;
	OutAssisters.Reset();
	const auto* Health = ULyraHealthComponent::FindHealthComponent(this);
	if (!HasAuthority() || bCombatDeathConsumed || !Health || !Health->IsDeadOrDying() || Health->GetHealth() > 0.0f)
	{
		return false;
	}
	// Set this before callbacks can reenter the round component.
	bCombatDeathConsumed = true;
	auto* GS = GetWorld()->GetGameState<ABwayGameState>();
	auto* Round = GS ? GS->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
	auto* Victim = GetPlayerState<ABwayPlayerState>();
	const bool bSameLife = Round && Victim && Victim->GetPawn() == this
		&& CombatRound == Round->GetCurrentRoundNumber() && CombatVictimTeam == GS->GetPlayerTeam(Victim);
	if (bSameLife && bLastDamageWasLethal)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const auto Eligible = [&](ABwayPlayerState* Player, const FCombatContribution& Contribution)
		{
			return Player && Player != Victim && GS->PlayerArray.Contains(Player)
				&& Contribution.Team != INDEX_NONE && Contribution.Team != CombatVictimTeam
				&& GS->GetPlayerTeam(Player) == Contribution.Team;
		};
		auto* Killer = LastDamagePlayerState.Get();
		const auto* KillingHit = CombatContributors.Find(Killer);
		if (KillingHit && Eligible(Killer, *KillingHit))
		{
			OutKiller = Killer;
			for (const auto& Pair : CombatContributors)
			{
				auto* Player = Pair.Key.Get();
				const double Age = Now - Pair.Value.LastDamageTime;
				if (Player != Killer && Eligible(Player, Pair.Value) && Age >= 0.0
					&& Age <= FMath::Max(0.0f, Round->AssistCreditWindowSeconds))
				{
					OutAssisters.Add(Player);
				}
			}
		}
	}
	CombatContributors.Reset();
	LastDamagePlayerState.Reset();
	#if !UE_BUILD_SHIPPING
	ConsumedAssisterCountForAudit = OutAssisters.Num();
	#endif
	return true;
}

void ABwayCharacterWithAbilities::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsLocallyControlled())
	{
		CurrentSlideFOVOffset = 0.f;
		StopSlideCameraShake(true);
		return;
	}

	if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
	{
		ULyraCameraComponent* CameraComp = ULyraCameraComponent::FindCameraComponent(this);
		if (CameraComp)
		{
			// --- FOV Scaling ---
			const float TargetFOVOffset = MoveComp->IsSliding() ? (MoveComp->GetSlideIntensity() * MoveComp->SlideFOVOffsetMax) : 0.0f;
			CurrentSlideFOVOffset = FMath::FInterpTo(CurrentSlideFOVOffset, TargetFOVOffset, DeltaSeconds, MoveComp->SlideFOVInterpSpeed);

			if (FMath::Abs(CurrentSlideFOVOffset) > KINDA_SMALL_NUMBER)
			{
				CameraComp->AddFieldOfViewOffset(CurrentSlideFOVOffset);
			}

		}
	}
	UpdateSlideCameraShake();
}

void ABwayCharacterWithAbilities::StopSlideCameraShake(bool bImmediately)
{
	if (UCameraShakeBase* Shake = ActiveSlideCameraShake.Get())
	{
		if (APlayerCameraManager* Manager = SlideCameraShakeManager.Get())
		{
			Manager->StopCameraShake(Shake, bImmediately);
			if (!bImmediately)
			{
				// Keep ownership through the authored blend-out, so teardown can
				// still remove this exact instance and rapid re-entry cannot stack it.
				bSlideCameraShakeStopping = true;
				return;
			}
		}
	}
	ActiveSlideCameraShake = nullptr;
	SlideCameraShakeManager.Reset();
	SlideCameraShakeController.Reset();
	bSlideCameraShakeStopping = false;
}

void ABwayCharacterWithAbilities::UpdateSlideCameraShake()
{
	const UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement();
	APlayerController* PC = IsLocallyControlled() ? Cast<APlayerController>(GetController()) : nullptr;
	APlayerCameraManager* Manager = PC ? PC->PlayerCameraManager.Get() : nullptr;
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(this);
	const bool bAlive = !Health || !Health->IsDeadOrDying();
	// Resolve the current local-world policy while starting or owning a shake;
	// this also honors a setting change during its authored blend-out.
	const UBwayCombatReadabilityConfig* Readability = Manager && (ActiveSlideCameraShake || (MoveComp && MoveComp->IsSliding()))
		? UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(this) : nullptr;
	const bool bReduceShake = Readability && Readability->bReduceCameraShake;
	const bool bWantsShake = bAlive && !bReduceShake && MoveComp && MoveComp->IsSliding() && MoveComp->SlideCameraShakeClass && Manager;
	if (UCameraShakeBase* Shake = ActiveSlideCameraShake.Get())
	{
		const bool bOwnerOrClassChanged = SlideCameraShakeController.Get() != PC
			|| SlideCameraShakeManager.Get() != Manager || !MoveComp
			|| Shake->GetClass() != MoveComp->SlideCameraShakeClass.Get();
		if (!bAlive || bReduceShake || bOwnerOrClassChanged || (bSlideCameraShakeStopping && (bWantsShake || Shake->IsFinished())))
		{
			StopSlideCameraShake(true);
		}
		else if (!bWantsShake && !bSlideCameraShakeStopping)
		{
			StopSlideCameraShake(false);
		}
	}
	if (bWantsShake && !ActiveSlideCameraShake)
	{
		FAddCameraShakeParams Params;
		Params.Scale = MoveComp->GetSlideIntensity();
		// Custom-initialized instances are excluded from the engine's expired
		// shake pool. Otherwise the same UObject (including its weak serial) can
		// be reused for an unrelated effect before we release our handle.
		Params.Initializer.BindLambda([](UCameraShakeBase*) {});
		ActiveSlideCameraShake = Manager->StartCameraShake(MoveComp->SlideCameraShakeClass, Params);
		// Retain a finished finite shake until slide exit: GC must not cause a
		// one-shot authored effect to restart while the same slide is held.
		SlideCameraShakeManager = Manager;
		SlideCameraShakeController = PC;
		bSlideCameraShakeStopping = false;
	}
}

FCollisionQueryParams ABwayCharacterWithAbilities::GetIgnoreCharacterParams() const
{
	FCollisionQueryParams Params;

	TArray<AActor*> CharacterChildren;
	GetAllChildActors(CharacterChildren);
	Params.AddIgnoredActors(CharacterChildren);
	Params.AddIgnoredActor(this);

	return Params;
}

// Optional method to update character appearance based on team
void ABwayCharacterWithAbilities::UpdateAppearanceForTeam()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>();
	if (!TeamSubsystem)
	{
		return;
	}

	bool bIsPartOfTeam = false;
	int32 TeamId = INDEX_NONE;
	TeamSubsystem->FindTeamFromActor(this, bIsPartOfTeam, TeamId);
	if (!bIsPartOfTeam || TeamId == INDEX_NONE)
	{
		if (const ALyraPlayerState* LPS = GetPlayerState<ALyraPlayerState>())
		{
			TeamId = LPS->GetTeamId();
		}
	}

	if (TeamId == INDEX_NONE)
	{
		return;
	}

	// Prefer Lyra team display assets (Green=1 / Blue=2 for B_BW_TeamSetup_TwoTeams)
	// instead of hardcoded Blue/Red Breakaway indices that fought the nameplate colors.
	if (ULyraTeamDisplayAsset* DisplayAsset = TeamSubsystem->GetTeamDisplayAsset(TeamId, INDEX_NONE))
	{
		DisplayAsset->ApplyToMeshComponent(MeshComp);
	}
}

void ABwayCharacterWithAbilities::PossessedBy(AController* NewController)
{
	StopSlideCameraShake(true);
	Super::PossessedBy(NewController);
	BindTeamAppearanceListener();
	UpdateAppearanceForTeam();
}

void ABwayCharacterWithAbilities::UnPossessed()
{
	StopSlideCameraShake(true);
	ClearRelicRequestForCurrentPawn();
	Super::UnPossessed();
}

void ABwayCharacterWithAbilities::OnRep_Controller()
{
	Super::OnRep_Controller();
	if (!IsLocallyControlled() || SlideCameraShakeController.Get() != GetController())
	{
		StopSlideCameraShake(true);
	}
}

void ABwayCharacterWithAbilities::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindTeamAppearanceListener();
	UpdateAppearanceForTeam();
}

void ABwayCharacterWithAbilities::BindTeamAppearanceListener()
{
	if (bBoundTeamAppearanceListener)
	{
		return;
	}

	GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::HandleTeamAppearanceChanged);
	bBoundTeamAppearanceListener = true;
}

void ABwayCharacterWithAbilities::HandleTeamAppearanceChanged(UObject* /*ObjectChangingTeam*/, int32 /*OldTeamID*/, int32 /*NewTeamID*/)
{
	UpdateAppearanceForTeam();
}

void ABwayCharacterWithAbilities::TryPickupOverlappingRelic()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	// Use the character's capsule or another relevant component for overlap checks
	GetCapsuleComponent()->GetOverlappingActors(OverlappingActors, ARelicActor::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		ARelicActor* Relic = Cast<ARelicActor>(OverlappingActor);
		if (Relic && Relic->CanBePickedUpBy(this))
		{
			APlayerState* MyPlayerState = GetPlayerState();
			if (MyPlayerState)
			{
				ULyraAbilitySystemComponent* PlayerStateASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MyPlayerState));
				const URelicSettings* Settings = Relic->GetRelicSettings(); // Assuming a getter for RelicSettings
				const FGameplayTag PickupEventTag = (Settings && Settings->PickupEventTag.IsValid())
					? Settings->PickupEventTag
					: FGameplayTag::RequestGameplayTag(FName("Event.Interaction.PickupRelic"), /*ErrorIfNotFound*/ false);

				if (PlayerStateASC && PickupEventTag.IsValid())
				{
					FGameplayEventData Payload;
					Payload.EventTag = PickupEventTag;
					Payload.Instigator = this;
					Payload.Target = Relic; // Target is the relic

					PlayerStateASC->HandleGameplayEvent(Payload.EventTag, &Payload);
					UE_LOG(LogTemp, Log, TEXT("Server: Sent PickupRelic event from Character overlap check to PlayerState ASC of %s for Relic %s"), *GetNameSafe(MyPlayerState), *GetNameSafe(Relic));

					// Found a valid relic and sent the event, stop checking
					return;
				}
			}
		}
	}
}

void ABwayCharacterWithAbilities::InitializeHeroData(const UBwayHeroDataAsset* HeroData)
{
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: BEGIN for character %s"), *GetName());
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	
	check(HeroData);

	UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: HeroData = %s (DisplayName: %s)"), 
		*GetNameSafe(HeroData), *HeroData->DisplayName.ToString());

	HeroDataAsset = HeroData;

	// Set the replicated hero ID so clients can apply visuals via OnRep
	if (HasAuthority())
	{
		ReplicatedHeroId = HeroData->GetPrimaryAssetId();
	}

	// Apply visuals on the server (clients will apply via OnRep_ReplicatedHeroId)
	ApplyHeroVisuals(HeroData);

	// Abilities are server-only
	if (HasAuthority())
	{
		ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent();
		if (!ASC)
		{
			UE_LOG(LogTemp, Error, TEXT("InitializeHeroData: LyraAbilitySystemComponent is null for %s."), *GetName());
			UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: END (FAILED - No ASC)"));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: ASC = %s"), *GetNameSafe(ASC));

		// Ability Sets
		if (!HeroData->AbilitySets.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: Processing %d AbilitySets"), HeroData->AbilitySets.Num());

			ABwayPlayerState* BwayPS = GetPlayerState<ABwayPlayerState>();
			if (!BwayPS)
			{
				UE_LOG(LogTemp, Error, TEXT("InitializeHeroData: BwayPlayerState is null for %s — cannot track hero ability grants."), *GetName());
				UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: END (FAILED - No PlayerState)"));
				return;
			}

			FLyraAbilitySet_GrantedHandles& HeroAbilityGrantedHandles = BwayPS->GetHeroAbilityGrantedHandles();

			// Clear any previously granted hero abilities on the persistent ASC (survives pawn respawn).
			HeroAbilityGrantedHandles.TakeFromAbilitySystem(ASC);
			
			for (int32 i = 0; i < HeroData->AbilitySets.Num(); ++i)
			{
				const ULyraAbilitySet* Set = HeroData->AbilitySets[i];
				if (Set)
				{
					UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: Granting AbilitySet[%d] = %s"), i, *GetNameSafe(Set));

					// Grant the ability set (tracked on PlayerState for cleanup)
					Set->GiveToAbilitySystem(ASC, &HeroAbilityGrantedHandles);
					
					UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: AbilitySet[%d] granted successfully"), i);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: AbilitySet[%d] is NULL!"), i);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: HeroData has NO AbilitySets configured!"));
		}

		// Log summary
		TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();
		UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: SUMMARY - Total activatable abilities on ASC: %d"), ActivatableAbilities.Num());
		
		for (const FGameplayAbilitySpec& Spec : ActivatableAbilities)
		{
			if (Spec.Ability)
			{
				FGameplayTagContainer DynamicTags = Spec.GetDynamicSpecSourceTags();
				FString TagsStr = DynamicTags.ToStringSimple();
				UE_LOG(LogTemp, Log, TEXT("  - %s [InputTags: %s]"), 
					*Spec.Ability->GetClass()->GetName(), 
					TagsStr.IsEmpty() ? TEXT("None") : *TagsStr);
			}
		}
	}

	UBwayHeroStatsLibrary::ApplyHeroStatsFromDataAsset(this, HeroData);

	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: END for character %s"), *GetName());
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

void ABwayCharacterWithAbilities::ApplyHeroVisuals(const UBwayHeroDataAsset* HeroData)
{
	if (!HeroData)
	{
		return;
	}

	USkeletalMeshComponent* MainMesh = GetMesh();

	TArray<USkeletalMeshComponent*> MeshComponents;
	GetComponents<USkeletalMeshComponent>(MeshComponents);
	for (USkeletalMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp || MeshComp == MainMesh)
		{
			continue;
		}

		MeshComp->SetHiddenInGame(true);
		MeshComp->SetVisibility(false, true);
	}

	if (MainMesh)
	{
		if (MainMesh->VisibilityBasedAnimTickOption > EVisibilityBasedAnimTickOption::AlwaysTickPose)
		{
			MainMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
		}

		if (HeroData->HeroMesh)
		{
			UE_LOG(LogTemp, Log, TEXT("ApplyHeroVisuals: Setting HeroMesh = %s on %s"), *GetNameSafe(HeroData->HeroMesh), *GetName());
			MainMesh->SetHiddenInGame(false);
			MainMesh->SetVisibility(true);
			MainMesh->SetSkeletalMesh(HeroData->HeroMesh);
			MainMesh->SetRelativeTransform(HeroData->HeroMeshRelativeTransform);
			// Lyra ignores its hidden driver mesh when spawning character VFX. A
			// hero-owned visible body must participate in death and other mesh effects.
			MainMesh->ComponentTags.Remove(FName(TEXT("ignore")));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyHeroVisuals: HeroMesh is null for %s — keeping experience pawn mesh"), *GetNameSafe(HeroData));
		}

		if (HeroData->AnimationClass)
		{
			UE_LOG(LogTemp, Log, TEXT("ApplyHeroVisuals: Setting AnimInstanceClass = %s"), *GetNameSafe(HeroData->AnimationClass.Get()));
			MainMesh->SetAnimInstanceClass(HeroData->AnimationClass);
			MainMesh->InitAnim(true);
			if (HeroData->AnimationLayerClass)
			{
				MainMesh->LinkAnimClassLayers(HeroData->AnimationLayerClass);
			}
		}
		else if (HeroData->HeroMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyHeroVisuals: AnimationClass is null for %s — mesh may T-pose"), *GetNameSafe(HeroData));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyHeroVisuals: MeshComponent is NULL on %s!"), *GetName());
	}

	UpdateAppearanceForTeam();

	if (!HeroData->MovementFeelConfig.IsNull())
	{
		if (const UBwayMovementFeelConfig* FeelConfig = HeroData->MovementFeelConfig.LoadSynchronous())
		{
			if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
			{
				MoveComp->ApplyMovementFeelConfig(FeelConfig);
			}
		}
	}
}

void ABwayCharacterWithAbilities::OnRep_ReplicatedHeroId()
{
	if (!ReplicatedHeroId.IsValid())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("OnRep_ReplicatedHeroId: Applying hero visuals for %s on %s"), 
		*ReplicatedHeroId.ToString(), *GetName());

	// Load hero data from registry and apply visuals
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(ReplicatedHeroId);
	if (HeroData)
	{
		HeroDataAsset = HeroData;
		ApplyHeroVisuals(HeroData);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRep_ReplicatedHeroId: Could not load hero data for %s"), *ReplicatedHeroId.ToString());
	}
}
