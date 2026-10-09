// Development-only GAS/CMC lifecycle proof. This is synthetic input, not physical-input or trajectory parity evidence.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Abilities/LyraGameplayAbility_Jump.h"
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayAbility_Slide.h"
#include "BwayPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayCueManager.h"
#include "HAL/IConsoleManager.h"

namespace BwaySlideLifecycleAudit
{
#if DO_ENABLE_NET_TEST
struct FRun;
TSharedPtr<FRun> ActiveRun;

struct FPeer
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	TWeakObjectPtr<UNetDriver> Driver;
	FPacketSimulationSettings PacketSettings;
	FDelegateHandle Activated, Ended;
	float AuthoredMinSlideSpeed = 0.f;
	int32 Activations = 0, Ends = 0, SlideEntries = 0;
};

struct FRun : TSharedFromThis<FRun>
{
	FTSTicker::FDelegateHandle Ticker;
	TWeakObjectPtr<UGameplayCueManager> CueManager;
	FDelegateHandle CueHandle;
	TArray<FPeer> Peers;
	FGameplayAbilitySpecHandle SlideHandle, JumpHandle;
	FGameplayTag SlideInput, JumpInput, SlidingTag;
	FTransform OriginalTransform;
	FVector OriginalVelocity = FVector::ZeroVector;
	EMovementMode OriginalMode = MOVE_Walking;
	uint8 OriginalCustomMode = 0;
	FVector Direction = FVector::ZeroVector;
	int32 OwnerIndex = INDEX_NONE, PlayerId = INDEX_NONE, Case = 0, Stage = 0, PassedCases = 0;
	double Started = FPlatformTime::Seconds(), Since = Started, SettledSince = 0, HeldSince = 0;
	bool bSetup = false, bSlidePressed = false, bJumpPressed = false, bImmediateJump = false, bStopping = false;
	FString LastSetupReason, LastSetupDetail;
	int32 SetupDiagnosticCount = 0;

	bool SetupWait(const TCHAR* Reason, const FString& Detail)
	{
		// Values are refreshed for the final timeout, but changing speed alone must not flood the log.
		if (LastSetupReason != Reason && SetupDiagnosticCount < 8)
		{
			UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: SetupWait reason=%s detail=%s"), Reason, *Detail);
			++SetupDiagnosticCount;
		}
		LastSetupReason = Reason; LastSetupDetail = Detail;
		return false;
	}

	const TCHAR* CaseName() const { return Case == 0 ? TEXT("AuthoredLowSpeed") : Case == 1 ? TEXT("LowSpeed") : Case == 2 ? TEXT("Release") : TEXT("Jump"); }
	bool AuthoredHoldPolicy() const { return Peers.IsValidIndex(OwnerIndex) && Peers[OwnerIndex].AuthoredMinSlideSpeed == 0.f; }
	UBwayCharacterMovementComponent* Movement(int32 Index) const
	{
		return Peers.IsValidIndex(Index) && Peers[Index].Pawn.IsValid() ? Peers[Index].Pawn->GetBwayCharacterMovement() : nullptr;
	}
	bool SpecActive(int32 Index) const
	{
		const auto* Spec = Peers[Index].ASC.IsValid() ? Peers[Index].ASC->FindAbilitySpecFromHandle(SlideHandle) : nullptr;
		return Spec && Spec->IsActive();
	}
	int32 RestoreThresholds()
	{
		int32 Restored = 0;
		for (int32 Index = 0; Index < Peers.Num(); ++Index)
		{
			if (auto* Move = Movement(Index))
			{
				Move->MinSlideSpeed = Peers[Index].AuthoredMinSlideSpeed;
				if (Move->MinSlideSpeed == Peers[Index].AuthoredMinSlideSpeed) ++Restored;
			}
		}
		return Restored;
	}
	void Input(const FGameplayTag& Tag, bool bPressed)
	{
		if (Peers.IsValidIndex(OwnerIndex) && Peers[OwnerIndex].ASC.IsValid())
		{
			auto* ASC = Peers[OwnerIndex].ASC.Get();
			if (bPressed) ASC->AbilityInputTagPressed(Tag); else ASC->AbilityInputTagReleased(Tag);
			ASC->ProcessAbilityInput(0.f, false);
		}
	}
	void Sample(const TCHAR* Label) const
	{
		for (int32 Index = 0; Index < Peers.Num(); ++Index)
		{
			const auto* Move = Movement(Index);
			UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: Sample case=%s label=%s peer=%d owner=%d id=%d mode=%d custom=%d sliding=%d active=%d tag=%d speed=%.2f slideJump=%d activated=%d ended=%d slideEntries=%d minSlide=%.2f authoredMinSlide=%.2f"),
				CaseName(), Label, Index, Index == OwnerIndex, PlayerId, Move ? int32(Move->MovementMode) : -1,
				Move ? int32(Move->CustomMovementMode) : -1, Move && Move->IsSliding(), SpecActive(Index),
				Peers[Index].ASC.IsValid() && Peers[Index].ASC->HasMatchingGameplayTag(SlidingTag), Move ? Move->Velocity.Size2D() : -1.f,
				Move && Move->IsSlideJumping(), Peers[Index].Activations, Peers[Index].Ends, Peers[Index].SlideEntries,
				Move ? Move->MinSlideSpeed : -1.f, Peers[Index].AuthoredMinSlideSpeed);
		}
	}
	bool Stop(bool bPassed, const TCHAR* Category, const TCHAR* Reason)
	{
		bStopping = true;
		if (!bSetup)
			UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: SetupFinal reason=%s detail=%s diagnostics=%d"), *LastSetupReason, *LastSetupDetail, SetupDiagnosticCount);
		if (CueManager.IsValid()) CueManager->OnGameplayCueRouted().Remove(CueHandle);
		CueManager.Reset();
		const int32 RestoredThresholds = RestoreThresholds();
		// Only this fixture's input and granted abilities are touched. Delegates are removed before cancellation.
		for (auto& Peer : Peers)
		{
			if (Peer.ASC.IsValid())
			{
				Peer.ASC->AbilityActivatedCallbacks.Remove(Peer.Activated);
				Peer.ASC->AbilityEndedCallbacks.Remove(Peer.Ended);
			}
		}
		if (bSlidePressed) Input(SlideInput, false);
		if (bJumpPressed) Input(JumpInput, false);
		for (const int32 Index : {OwnerIndex, 0})
		{
			if (!bSetup || !Peers.IsValidIndex(Index)) continue;
			if (auto* ASC = Peers[Index].ASC.Get())
			{
				ASC->CancelAbilityHandle(SlideHandle);
				ASC->CancelAbilityHandle(JumpHandle);
			}
			if (auto* Pawn = Peers[Index].Pawn.Get())
			{
				Pawn->ConsumeMovementInputVector();
				Pawn->StopJumping();
			}
		}
		bool bTransformRestored = !bSetup;
		if (bSetup && Peers[0].Pawn.IsValid())
		{
			auto* Pawn = Peers[0].Pawn.Get();
			// Baseline requires idle grounded pawns. Restore authority only; normal movement replication corrects peers.
			Pawn->GetBwayCharacterMovement()->StopMovementImmediately();
			Pawn->GetBwayCharacterMovement()->SetMovementMode(OriginalMode, OriginalCustomMode);
			const bool bTeleported = Pawn->TeleportTo(OriginalTransform.GetLocation(), OriginalTransform.Rotator(), false, false);
			Pawn->GetBwayCharacterMovement()->Velocity = OriginalVelocity;
			// TeleportTo may succeed at an adjusted collision-free location. Its return
			// value alone cannot prove restoration of the fixture's original transform.
			const FTransform RestoredTransform = Pawn->GetActorTransform();
			const double LocationError = FVector::Distance(RestoredTransform.GetLocation(), OriginalTransform.GetLocation());
			const double RotationErrorDegrees = FMath::RadiansToDegrees(RestoredTransform.GetRotation().AngularDistance(OriginalTransform.GetRotation()));
			const double ScaleError = (RestoredTransform.GetScale3D() - OriginalTransform.GetScale3D()).GetAbsMax();
			bTransformRestored = bTeleported && LocationError <= 0.1 && RotationErrorDegrees <= 0.01 && ScaleError <= 0.0001;
			UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: TransformRestore passed=%d teleported=%d locationErrorCm=%.6f rotationErrorDegrees=%.6f scaleError=%.6f locationToleranceCm=0.1 rotationToleranceDegrees=0.01 scaleTolerance=0.0001"),
				bTransformRestored, bTeleported, LocationError, RotationErrorDegrees, ScaleError);
			Pawn->ForceNetUpdate();
		}
		int32 RestoredDrivers = 0;
		for (auto& Peer : Peers)
		{
			if (Peer.Driver.IsValid()) { Peer.Driver->SetPacketSimulationSettings(Peer.PacketSettings); ++RestoredDrivers; }
		}
		UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: COMPLETE passed=%d category=%s reason=%s cases=%d peers=%d restoredDrivers=%d restoredThresholds=%d transformRestored=%d physicalInput=0 trajectoryParity=0"),
			bPassed && bTransformRestored && RestoredDrivers == Peers.Num() && RestoredThresholds == Peers.Num(),
			Category, Reason, PassedCases, Peers.Num(), RestoredDrivers, RestoredThresholds, bTransformRestored);
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
		Peers.Reset();
		ActiveRun.Reset();
		return false;
	}

	bool Choose()
	{
		TArray<UWorld*> Worlds;
		UWorld* Authority = nullptr;
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			auto* World = Context.World();
			if (Context.WorldType != EWorldType::PIE || !World || !World->IsGameWorld()) continue;
			if (World->GetNetMode() == NM_ListenServer)
			{
				if (Authority) return SetupWait(TEXT("multiple-listen-worlds"), FString::Printf(TEXT("first=%s second=%s"), *Authority->GetPathName(), *World->GetPathName()));
				Authority = World;
			}
			else if (World->GetNetMode() == NM_Client) Worlds.Add(World);
		}
		if (!Authority || Worlds.Num() != 3)
			return SetupWait(TEXT("topology"), FString::Printf(TEXT("listen=%d clients=%d"), Authority != nullptr, Worlds.Num()));
		Worlds.Insert(Authority, 0);
		const auto* GS = Authority->GetGameState<ABwayGameState>();
		if (!GS || !GS->GetRoundManagement() || GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
			return SetupWait(TEXT("authority-not-playing"), FString::Printf(TEXT("world=%s gameState=%d roundManager=%d phase=%d"), *Authority->GetPathName(),
				GS != nullptr, GS && GS->GetRoundManagement(), GS && GS->GetRoundManagement() ? int32(GS->GetRoundManagement()->GetCurrentMatchPhase()) : -1));
		APlayerController* PC = Worlds[1]->GetFirstPlayerController();
		auto* OwnerPawn = PC && PC->IsLocalController() ? Cast<ABwayCharacterWithAbilities>(PC->GetPawn()) : nullptr;
		auto* PS = OwnerPawn ? OwnerPawn->GetPlayerState<ABwayPlayerState>() : nullptr;
		auto* ASC = OwnerPawn ? OwnerPawn->GetLyraAbilitySystemComponent() : nullptr;
		if (!PS || !ASC || !OwnerPawn->GetBwayCharacterMovement())
			return SetupWait(TEXT("owner-chain"), FString::Printf(TEXT("world=%s controller=%s local=%d pawn=%s playerState=%s asc=%s movement=%d"),
				*Worlds[1]->GetPathName(), *GetNameSafe(PC), PC && PC->IsLocalController(), *GetNameSafe(OwnerPawn), *GetNameSafe(PS), *GetNameSafe(ASC), OwnerPawn && OwnerPawn->GetBwayCharacterMovement()));
		SlideInput = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Slide"));
		JumpInput = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Jump"));
		SlidingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sliding"));
		SlideHandle = FGameplayAbilitySpecHandle(); JumpHandle = FGameplayAbilitySpecHandle();
		int32 SlideBindings = 0, JumpBindings = 0;
		FString Bindings;
		for (const auto& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(SlideInput))
			{
				++SlideBindings;
				if (SlideBindings <= 3) Bindings += FString::Printf(TEXT(" slide[%d]={class=%s family=%d active=%d pressed=%d}"), SlideBindings,
					*GetPathNameSafe(Spec.Ability ? Spec.Ability->GetClass() : nullptr), Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_Slide>(), Spec.IsActive(), Spec.InputPressed);
				if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_Slide>() && !Spec.IsActive() && !Spec.InputPressed) SlideHandle = Spec.Handle;
			}
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(JumpInput))
			{
				++JumpBindings;
				if (JumpBindings <= 3) Bindings += FString::Printf(TEXT(" jump[%d]={class=%s family=%d active=%d pressed=%d}"), JumpBindings,
					*GetPathNameSafe(Spec.Ability ? Spec.Ability->GetClass() : nullptr), Spec.Ability && Spec.Ability->IsA<ULyraGameplayAbility_Jump>(), Spec.IsActive(), Spec.InputPressed);
				if (Spec.Ability && Spec.Ability->IsA<ULyraGameplayAbility_Jump>() && !Spec.IsActive() && !Spec.InputPressed) JumpHandle = Spec.Handle;
			}
		}
		if (SlideBindings != 1 || JumpBindings != 1 || !SlideHandle.IsValid() || !JumpHandle.IsValid())
			return SetupWait(TEXT("owner-input-bindings"), FString::Printf(TEXT("id=%d slideBindings=%d jumpBindings=%d slideHandle=%d jumpHandle=%d%s"),
				PS->GetPlayerId(), SlideBindings, JumpBindings, SlideHandle.IsValid(), JumpHandle.IsValid(), *Bindings));
		PlayerId = PS->GetPlayerId();
		TArray<FPeer> Found;
		FString PeerDetails;
		const TCHAR* PeerFailure = nullptr;
		int32 PeerIndex = 0;
		for (auto* World : Worlds)
		{
			auto* PeerState = World->GetGameState<ABwayGameState>();
			ABwayCharacterWithAbilities* Pawn = nullptr;
			if (PeerState) for (APlayerState* Player : PeerState->PlayerArray)
				if (Player && Player->GetPlayerId() == PlayerId) Pawn = Cast<ABwayCharacterWithAbilities>(Player->GetPawn());
			auto* Move = Pawn ? Pawn->GetBwayCharacterMovement() : nullptr;
			auto* PeerASC = Pawn ? Pawn->GetLyraAbilitySystemComponent() : nullptr;
			PeerDetails += FString::Printf(TEXT(" peer[%d]={world=%s id=%d pawn=%s asc=%d driver=%d mode=%d custom=%d speed=%.2f minSlide=%.2f crouched=%d jumpPressed=%d tag=%d}"),
				PeerIndex++, *World->GetPathName(), PlayerId, *GetNameSafe(Pawn), PeerASC != nullptr, World->GetNetDriver() != nullptr,
				Move ? int32(Move->MovementMode) : -1, Move ? int32(Move->CustomMovementMode) : -1, Move ? Move->Velocity.Size() : -1.f,
				Move ? Move->MinSlideSpeed : -1.f, Pawn && Pawn->bIsCrouched, Pawn && Pawn->bPressedJump, PeerASC && PeerASC->HasMatchingGameplayTag(SlidingTag));
			if (!Pawn || !Move || !PeerASC || !World->GetNetDriver())
			{
				if (!PeerFailure) PeerFailure = TEXT("peer-chain");
				continue;
			}
			if (!PeerFailure && (!FMath::IsFinite(Move->MinSlideSpeed) || Move->MinSlideSpeed < 0.f)) PeerFailure = TEXT("min-slide-speed-invalid");
			if (!PeerFailure && (!Move->IsMovingOnGround() || Move->Velocity.Size() > 10.f || Pawn->bIsCrouched || Pawn->bPressedJump || PeerASC->HasMatchingGameplayTag(SlidingTag)))
				PeerFailure = TEXT("peer-not-idle-grounded");
			FPeer Peer;
			Peer.World = World; Peer.Pawn = Pawn; Peer.ASC = PeerASC; Peer.Driver = World->GetNetDriver();
			Peer.PacketSettings = World->GetNetDriver()->PacketSimulationSettings;
			Peer.AuthoredMinSlideSpeed = Move->MinSlideSpeed;
			Found.Add(Peer);
		}
		if (PeerFailure) return SetupWait(PeerFailure, Bindings + PeerDetails);
		if ((Found[0].AuthoredMinSlideSpeed == 0.f) != (Found[1].AuthoredMinSlideSpeed == 0.f))
			return SetupWait(TEXT("authored-slide-policy-mismatch"), PeerDetails);
		const auto* AuthoritySlide = Found[0].ASC->FindAbilitySpecFromHandle(SlideHandle);
		const auto* AuthorityJump = Found[0].ASC->FindAbilitySpecFromHandle(JumpHandle);
		// ServerTryActivateAbility latches InputPressed. Lyra forwards release only while the
		// owner's spec is active, so automatic mode exit can leave this inactive server bit set.
		// The owner input baseline above is authoritative for held input; require idle server
		// abilities here, and still prove actual entry/end separately in every test case.
		if (!AuthoritySlide || AuthoritySlide->IsActive() || !AuthorityJump || AuthorityJump->IsActive())
			return SetupWait(TEXT("authority-specs"), FString::Printf(TEXT("id=%d slideExists=%d slideActive=%d slidePressed=%d jumpExists=%d jumpActive=%d jumpPressed=%d%s"),
				PlayerId, AuthoritySlide != nullptr, AuthoritySlide && AuthoritySlide->IsActive(), AuthoritySlide && AuthoritySlide->InputPressed,
				AuthorityJump != nullptr, AuthorityJump && AuthorityJump->IsActive(), AuthorityJump && AuthorityJump->InputPressed, *Bindings));
		CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
		if (!CueManager.IsValid()) return SetupWait(TEXT("cue-manager-unavailable"), TEXT("Cannot observe production slide entry"));
		UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: SetupReady detail=%s%s"), *Bindings, *PeerDetails);
		Peers = MoveTemp(Found); OwnerIndex = 1; OriginalTransform = Peers[0].Pawn->GetActorTransform(); bSetup = true;
		OriginalVelocity = Movement(0)->Velocity; OriginalMode = Movement(0)->MovementMode; OriginalCustomMode = Movement(0)->CustomMovementMode;
		const TWeakPtr<FRun> Weak = AsShared();
		CueHandle = CueManager->OnGameplayCueRouted().AddLambda([Weak](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event,
			const FGameplayCueParameters& Params, EGameplayCueExecutionOptions)
		{
			const auto Run = Weak.Pin();
			if (!Run || Run->bStopping || !Target || Tag != BwayGameplayCueTags::MovementSlide || Event != EGameplayCueEvent::Executed || Params.Instigator.Get() != Target) return;
			for (const int32 Index : {0, Run->OwnerIndex})
			{
				if (Run->Peers[Index].Pawn.Get() != Target) continue;
				// The production slide emits this cue after SetMovementMode. Check mode here rather than relying on PreActivate's GAS callback.
				const auto* Move = Run->Movement(Index);
				if (Move && Move->IsSliding()) ++Run->Peers[Index].SlideEntries;
				UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: SlideEntry case=%s peer=%d id=%d sliding=%d entries=%d"),
					Run->CaseName(), Index, Run->PlayerId, Move && Move->IsSliding(), Run->Peers[Index].SlideEntries);
			}
		});
		for (const int32 Index : {0, OwnerIndex})
		{
			Peers[Index].Activated = Peers[Index].ASC->AbilityActivatedCallbacks.AddLambda([Weak, Index](UGameplayAbility* Ability)
			{
				const auto Run = Weak.Pin();
				if (Run && !Run->bStopping && Ability && Ability->GetCurrentAbilitySpecHandle() == Run->SlideHandle) ++Run->Peers[Index].Activations;
			});
			Peers[Index].Ended = Peers[Index].ASC->AbilityEndedCallbacks.AddLambda([Weak, Index](UGameplayAbility* Ability)
			{
				const auto Run = Weak.Pin();
				if (Run && !Run->bStopping && Ability && Ability->GetCurrentAbilitySpecHandle() == Run->SlideHandle) ++Run->Peers[Index].Ends;
			});
			GEngine->Exec(Peers[Index].World.Get(), TEXT("Net PktLag=100"));
		}
		UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: Setup peers=4 listen=1 clients=3 ownerPeer=%d id=%d authority=%s owner=%s lagAuthority=%d lagOwner=%d"),
			OwnerIndex, PlayerId, *Authority->GetPathName(), *Worlds[OwnerIndex]->GetPathName(), Peers[0].Driver->PacketSimulationSettings.PktLag, Peers[OwnerIndex].Driver->PacketSimulationSettings.PktLag);
		return true;
	}

	bool ChooseRunway()
	{
		auto* Pawn = Peers[0].Pawn.Get();
		auto* World = Peers[0].World.Get();
		const FVector Origin = Pawn->GetActorLocation();
		const auto* Capsule = Pawn->GetCapsuleComponent();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BwaySlideAuditRunway), false, Pawn);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector Candidate = FVector::ForwardVector.RotateAngleAxis(45.f * Index, FVector::UpVector);
			bool bClear = true;
			for (int32 Distance = 0; Distance <= 1400 && bClear; Distance += 200)
			{
				const FVector Point = Origin + Candidate * Distance;
				FHitResult Floor;
				bClear = World->LineTraceSingleByChannel(Floor, Point, Point - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 30.f), ECC_Visibility, Params)
					&& Floor.ImpactNormal.Z > 0.995f && FMath::Abs(Floor.ImpactPoint.Z - (Origin.Z - Capsule->GetScaledCapsuleHalfHeight())) < 15.f;
			}
			FHitResult Obstacle;
			if (bClear && !World->SweepSingleByChannel(Obstacle, Origin + FVector(0, 0, 5.f), Origin + Candidate * 1400.f + FVector(0, 0, 5.f),
				FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() - 5.f), Params))
			{
				Direction = Candidate;
				UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: Runway case=%s direction=%s length=1400"), CaseName(), *Direction.ToCompactString());
				return true;
			}
		}
		return false;
	}

	bool Tick(float DeltaTime)
	{
		(void)DeltaTime;
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 45.0) return Stop(false, bSetup ? TEXT("gameplay") : TEXT("setup"), TEXT("overall-timeout"));
		if (!bSetup)
		{
			if (!Choose()) return true;
			if (Peers[0].Driver->PacketSimulationSettings.PktLag != 100 || Peers[OwnerIndex].Driver->PacketSimulationSettings.PktLag != 100)
				return Stop(false, TEXT("setup"), TEXT("packet-lag-not-applied"));
			Since = Now;
		}
		for (const auto& Peer : Peers)
		{
			const auto* GS = Peer.World.IsValid() ? Peer.World->GetGameState<ABwayGameState>() : nullptr;
			if (!Peer.Pawn.IsValid() || !Peer.ASC.IsValid() || !Peer.Driver.IsValid() || !GS || !GS->GetRoundManagement() ||
				GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
				return Stop(false, TEXT("setup"), TEXT("world-pawn-or-playing-phase-changed"));
		}
		auto* OwnerMove = Movement(OwnerIndex);
		auto* ServerMove = Movement(0);
		if (Stage == 0)
		{
			if (!OwnerMove->IsMovingOnGround() || !ServerMove->IsMovingOnGround() || OwnerMove->Velocity.Size() > 10.f || ServerMove->Velocity.Size() > 10.f)
			{
				if (Now - Since > 6.0) return Stop(false, TEXT("setup"), TEXT("idle-grounded-baseline-timeout"));
				return true;
			}
			for (auto& Peer : Peers) { Peer.Activations = 0; Peer.Ends = 0; Peer.SlideEntries = 0; }
			// Stationary cases use the actual authored threshold first, then a scoped
			// positive override to cover the optional automatic-exit lifecycle.
			if (Case < 2 && (OwnerMove->Velocity.Size2D() > 1.f || ServerMove->Velocity.Size2D() > 1.f))
			{
				if (Now - Since > 6.0) return Stop(false, TEXT("setup"), TEXT("stationary-baseline-timeout"));
				return true;
			}
			if (RestoreThresholds() != Peers.Num()) return Stop(false, TEXT("cleanup"), TEXT("threshold-restore-failed"));
			if (Case == 1) for (int32 Index = 0; Index < Peers.Num(); ++Index) Movement(Index)->MinSlideSpeed = 100.f;
			bImmediateJump = false; SettledSince = 0; HeldSince = 0;
			if (Case >= 2 && !ChooseRunway()) return Stop(false, TEXT("setup"), TEXT("no-flat-clear-runway"));
			UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: CasePolicy case=%s authoredHold=%d authorityMinSlide=%.2f ownerMinSlide=%.2f temporaryOverride=%d"),
				CaseName(), AuthoredHoldPolicy(), ServerMove->MinSlideSpeed, OwnerMove->MinSlideSpeed, Case == 1);
			Stage = Case < 2 ? 2 : 1; Since = Now;
		}
		if (Stage == 1)
		{
			if (!OwnerMove->IsMovingOnGround() || !ServerMove->IsMovingOnGround()) return Stop(false, TEXT("setup"), TEXT("run-up-left-ground"));
			const float RequiredSpeed = FMath::Max(300.f, FMath::Max(OwnerMove->MinSlideSpeed, ServerMove->MinSlideSpeed) * 2.f);
			if (OwnerMove->Velocity.Size2D() >= RequiredSpeed && ServerMove->Velocity.Size2D() >= RequiredSpeed)
			{
				Stage = 2;
			}
			else
			{
				if (Now - Since > 3.0) return Stop(false, TEXT("setup"), TEXT("run-up-speed-timeout"));
				Peers[OwnerIndex].Pawn->AddMovementInput(Direction, 1.f);
				return true;
			}
		}
		if (Stage == 2)
		{
			bSlidePressed = true;
			Input(SlideInput, true);
			Sample(TEXT("activated-owner"));
			if (!OwnerMove->IsSliding() || !SpecActive(OwnerIndex)) return Stop(false, TEXT("gameplay"), TEXT("owner-slide-did-not-activate"));
			if (Case == 2)
			{
				Input(SlideInput, false); bSlidePressed = false;
				Sample(TEXT("released-owner"));
				if (OwnerMove->IsSliding() || SpecActive(OwnerIndex) || Peers[OwnerIndex].ASC->HasMatchingGameplayTag(SlidingTag))
					return Stop(false, TEXT("gameplay"), TEXT("release-did-not-end-owner-slide"));
			}
			else if (Case == 3)
			{
				bJumpPressed = true; Input(JumpInput, true);
			}
			Stage = 3; Since = Now;
			return true;
		}
		if (Stage == 3)
		{
			if (Case == 0 && AuthoredHoldPolicy() && bSlidePressed)
			{
				const auto* OwnerSpec = Peers[OwnerIndex].ASC->FindAbilitySpecFromHandle(SlideHandle);
				if (!OwnerSpec || !OwnerSpec->InputPressed || !SpecActive(OwnerIndex) || !OwnerMove->IsSliding()
					|| Peers[OwnerIndex].ASC->GetTagCount(SlidingTag) != 1 || Peers[OwnerIndex].Ends != 0 || Peers[0].Ends != 0)
					return Stop(false, TEXT("gameplay"), TEXT("authored-zero-ended-while-held"));
				const bool bHeldStationary = ServerMove->IsSliding() && SpecActive(0) && Peers[0].ASC->GetTagCount(SlidingTag) == 1
					&& Peers[0].Activations == 1 && Peers[0].SlideEntries == 1
					&& Peers[OwnerIndex].Activations == 1 && Peers[OwnerIndex].SlideEntries == 1
					&& OwnerMove->Velocity.Size2D() <= 1.f && ServerMove->Velocity.Size2D() <= 1.f;
				if (bHeldStationary)
				{
					if (HeldSince == 0) HeldSince = Now;
					if (Now - HeldSince >= 0.5)
					{
						Sample(TEXT("held-stationary"));
						UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: HeldStationaryPASS case=%s seconds=%.3f maxPlanarSpeed=1 authorityActivated=1 authorityEnded=0 authoritySlideEntry=1 ownerActivated=1 ownerEnded=0 ownerSlideEntry=1"), CaseName(), Now - HeldSince);
						Input(SlideInput, false); bSlidePressed = false;
						Sample(TEXT("released-owner"));
						if (OwnerMove->IsSliding() || SpecActive(OwnerIndex) || Peers[OwnerIndex].ASC->HasMatchingGameplayTag(SlidingTag))
							return Stop(false, TEXT("gameplay"), TEXT("authored-zero-release-did-not-end-owner"));
						Since = Now;
					}
				}
				else if (HeldSince != 0) return Stop(false, TEXT("gameplay"), TEXT("authored-zero-held-state-interrupted"));
				if (bSlidePressed)
				{
					if (Now - Since > 3.0) { Sample(TEXT("held-timeout")); return Stop(false, TEXT("gameplay"), TEXT("authored-zero-held-state-timeout")); }
					return true;
				}
			}
			if (Case == 3 && !bImmediateJump)
			{
				// Jump input sets bPressedJump; the next CMC tick must preserve Falling when slide ends.
				if (OwnerMove->IsFalling())
				{
					Sample(TEXT("immediate-jump-owner"));
					bImmediateJump = OwnerMove->IsSlideJumping() && !SpecActive(OwnerIndex) && !Peers[OwnerIndex].ASC->HasMatchingGameplayTag(SlidingTag);
					if (!bImmediateJump) return Stop(false, TEXT("gameplay"), TEXT("jump-destination-or-slide-cleanup"));
					Input(JumpInput, false); bJumpPressed = false;
				}
				else if (Now - Since > 0.5) return Stop(false, TEXT("gameplay"), TEXT("jump-did-not-enter-falling"));
			}
			bool bClean = (Case != 3 || bImmediateJump) && Peers[0].Activations == 1 && Peers[0].Ends == 1 && Peers[0].SlideEntries == 1 &&
				Peers[OwnerIndex].Activations == 1 && Peers[OwnerIndex].Ends == 1 && Peers[OwnerIndex].SlideEntries == 1;
			for (int32 Index = 0; Index < Peers.Num(); ++Index)
			{
				const auto* Move = Movement(Index);
				const bool bExpectedMode = Case == 3 ? !Move->IsSliding() : Move->MovementMode == MOVE_Walking && Move->CustomMovementMode == 0;
				bClean &= bExpectedMode && !SpecActive(Index) && !Peers[Index].ASC->HasMatchingGameplayTag(SlidingTag);
			}
			if (bClean)
			{
				if (SettledSince == 0) SettledSince = Now;
				if (Now - SettledSince >= 0.5)
				{
					Sample(TEXT("settled"));
					UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: CasePASS case=%s peers=4 authorityActivated=1 authorityEnded=1 authoritySlideEntry=1 ownerActivated=1 ownerEnded=1 ownerSlideEntry=1 lagMs=100"), CaseName());
					++PassedCases;
					if (bSlidePressed) { Input(SlideInput, false); bSlidePressed = false; }
					if (RestoreThresholds() != Peers.Num()) return Stop(false, TEXT("cleanup"), TEXT("threshold-restore-failed"));
					if (++Case == 4) return Stop(true, TEXT("lifecycle"), TEXT("all-cases-passed"));
					Stage = 0; Since = Now;
				}
			}
			else SettledSince = 0;
			if (Stage == 3 && Now - Since > 5.0) { Sample(TEXT("timeout")); return Stop(false, TEXT("gameplay"), TEXT("peer-lifecycle-convergence-timeout")); }
		}
		return true;
	}
};
#endif

void Start()
{
#if DO_ENABLE_NET_TEST
	if (ActiveRun) { UE_LOG(LogTemp, Warning, TEXT("BwaySlideLifecycle: SetupBlocked reason=already-running")); return; }
	if (!GEngine) return;
	ActiveRun = MakeShared<FRun>();
	const TWeakPtr<FRun> Weak = ActiveRun;
	ActiveRun->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float DeltaTime)
	{
		const auto Run = Weak.Pin();
		return Run && Run->Tick(DeltaTime);
	}));
	UE_LOG(LogTemp, Display, TEXT("BwaySlideLifecycle: START expectedPeers=4 timeoutSeconds=45 syntheticInput=1 physicalInput=0"));
#else
	UE_LOG(LogTemp, Warning, TEXT("BwaySlideLifecycle: SetupBlocked reason=network-emulation-disabled"));
#endif
}

FAutoConsoleCommand Command(TEXT("bway.Test.SlideLifecycleRemote"),
	TEXT("Development: four-world listen PIE slide authored stationary/optional low-speed/release/jump lifecycle under 100ms lag. Restores thresholds, authority transform and packet settings."),
	FConsoleCommandDelegate::CreateStatic(&Start));
}
#endif
