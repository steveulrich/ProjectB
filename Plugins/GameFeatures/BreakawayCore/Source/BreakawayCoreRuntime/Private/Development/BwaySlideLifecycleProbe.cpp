#include "Development/BwaySlideLifecycleProbe.h"
#include "Development/BwaySlideReviewCourse.h"

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
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayCueManager.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwaySlideLifecycleProbe)

bool FBwaySlideLifecycleFrame::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	Ar << Serial << Phase << Case << Peer << Source << Passed << Trajectory << SupportDiagnostic << GroundAirDiagnostic << ReviewRegression << Token << AuthoredMinSpeed << ReviewCourseRadius << Position << Rotation << Direction << RestoreScale << RestoreVelocity << RestoreMode << RestoreCustom;
	bOutSuccess = !Ar.IsError();
	return true;
}

#if !UE_BUILD_SHIPPING && DO_ENABLE_NET_TEST
namespace BwaySlidePackaged
{
enum EPhase { Baseline = 1, Relocated, Armed, RunUp, Execute, ReleaseHeld, Settled, Stop, Restored, Complete };
constexpr double SupportDiagnosticSeconds = 1.0;
// This is an observation deadline only. It is intentionally not an acceptance
// threshold and it does not widen the normal 15 cm trajectory support rule.
constexpr double GroundAirDiagnosticSeconds = 4.0;
const TCHAR* CaseName(int32 Case, bool bTrajectory = false, bool bReview = false)
{
	if (bReview) return Case == 0 ? TEXT("HeldJumpReview") : TEXT("ConcaveContactReview");
	if (bTrajectory) return Case == 0 ? TEXT("FlatRelease") : Case == 1 ? TEXT("FlatJump") : Case == 2 ? TEXT("RampRouteRelease") : TEXT("RampRouteJump");
	return Case == 0 ? TEXT("AuthoredLowSpeed") : Case == 1 ? TEXT("LowSpeed") : Case == 2 ? TEXT("Release") : TEXT("Jump");
}
ABwayCharacterWithAbilities* FindPawn(UWorld* World, int32 Id)
{
	const auto* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	if (GS) for (APlayerState* PS : GS->PlayerArray)
		if (PS && PS->GetPlayerId() == Id) return Cast<ABwayCharacterWithAbilities>(PS->GetPawn());
	return nullptr;
}
bool Playing(UWorld* World)
{
	const auto* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	return GS && GS->GetRoundManagement() && GS->GetRoundManagement()->GetCurrentMatchPhase() == EBwayMatchPhase::Playing;
}
bool Active(ULyraAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle Handle)
{
	const auto* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	return Spec && Spec->IsActive();
}
bool Runway(ABwayCharacterWithAbilities* Pawn, FVector& Direction)
{
	const FVector Origin = Pawn->GetActorLocation();
	const auto* Capsule = Pawn->GetCapsuleComponent();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SlidePackagedRunway), false, Pawn);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FVector Candidate = FVector::ForwardVector.RotateAngleAxis(45.f * Index, FVector::UpVector);
		bool bClear = true;
		for (int32 Distance = 0; Distance <= 1400 && bClear; Distance += 200)
		{
			const FVector Point = Origin + Candidate * Distance;
			FHitResult Floor;
			bClear = Pawn->GetWorld()->LineTraceSingleByChannel(Floor, Point, Point - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 30.f), ECC_Visibility, Query)
				&& Floor.ImpactNormal.Z > 0.995f && FMath::Abs(Floor.ImpactPoint.Z - (Origin.Z - Capsule->GetScaledCapsuleHalfHeight())) < 15.f;
		}
		FHitResult Obstacle;
		if (bClear && !Pawn->GetWorld()->SweepSingleByChannel(Obstacle, Origin + FVector(0, 0, 5), Origin + Candidate * 1400.f + FVector(0, 0, 5), FQuat::Identity,
			ECC_Pawn, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() - 5.f), Query))
		{ Direction = Candidate; return true; }
	}
	return false;
}
FString JsonVector(const FVector& V)
{
	return FString::Printf(TEXT("[%.5f,%.5f,%.5f]"), V.X, V.Y, V.Z);
}
// Diagnostic queries describe observed geometry only. In particular, a plane
// extrapolated from one mesh hit is not proof that the capsule touches that mesh.
FString ContactHitJson(const FHitResult& Hit)
{
	return FString::Printf(TEXT("{\"blocking\":%d,\"validBlocking\":%d,\"componentValid\":%d,\"startPenetrating\":%d,\"time\":%.7f,\"distance\":%.6f,\"location\":%s,\"impactPoint\":%s,\"normal\":%s,\"impactNormal\":%s,\"traceStart\":%s,\"traceEnd\":%s,\"actor\":\"%s\",\"component\":\"%s\",\"faceIndex\":%d}"),
		Hit.bBlockingHit, Hit.IsValidBlockingHit(), Hit.GetComponent() != nullptr, Hit.bStartPenetrating, Hit.Time, Hit.Distance,
		*JsonVector(Hit.Location), *JsonVector(Hit.ImpactPoint), *JsonVector(Hit.Normal), *JsonVector(Hit.ImpactNormal),
		*JsonVector(Hit.TraceStart), *JsonVector(Hit.TraceEnd), *GetPathNameSafe(Hit.GetActor()).ReplaceCharWithEscapedChar(),
		*GetPathNameSafe(Hit.GetComponent()).ReplaceCharWithEscapedChar(), Hit.FaceIndex);
}
FString ContactPlaneJson(const FVector& Center, const UCapsuleComponent* Capsule, const FHitResult& Hit)
{
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const bool bValid = Hit.IsValidBlockingHit() && Hit.GetComponent() && !Center.ContainsNaN() && !Hit.ImpactPoint.ContainsNaN()
		&& !Normal.ContainsNaN() && !Normal.IsNearlyZero();
	const double Support = Capsule->GetScaledCapsuleRadius() +
		(Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius()) * FMath::Abs(FVector::DotProduct(Normal, Capsule->GetUpVector()));
	const double Offset = FVector::DotProduct(Normal, Hit.ImpactPoint);
	const double Clearance = FVector::DotProduct(Normal, Center) - Offset - Support;
	return FString::Printf(TEXT("{\"valid\":%d,\"kind\":\"local-hit-plane-extrapolation\",\"meshEdgeValidityVerified\":0,\"normal\":%s,\"offset\":%s,\"capsuleSupport\":%s,\"capsulePlaneClearance\":%s}"),
		bValid, *JsonVector(bValid ? Normal : FVector::ZeroVector),
		*(bValid ? FString::Printf(TEXT("%.7f"), Offset) : TEXT("null")),
		*(bValid ? FString::Printf(TEXT("%.7f"), Support) : TEXT("null")),
		*(bValid ? FString::Printf(TEXT("%.7f"), Clearance) : TEXT("null")));
}
FString ContactCapsuleQueryJson(const UCapsuleComponent* Capsule, const FCollisionQueryParams& Query, const FCollisionResponseParams& Responses)
{
	FString Channels;
	// ECC_MAX includes a deprecated sentinel outside the response storage.
	for (int32 Channel = 0; Channel < UE_ARRAY_COUNT(Responses.CollisionResponse.EnumArray); ++Channel)
	{
		if (Channel) Channels += TEXT(",");
		Channels += FString::FromInt(int32(Responses.CollisionResponse.GetResponse(ECollisionChannel(Channel))));
	}
	const FQuat Rotation = Capsule->GetComponentQuat();
	return FString::Printf(TEXT("{\"radius\":%.6f,\"halfHeight\":%.6f,\"axis\":%s,\"rotationQuat\":[%.7f,%.7f,%.7f,%.7f],\"channel\":%d,\"profile\":\"%s\",\"responses\":[%s],\"queryCollision\":%d,\"traceComplex\":%d,\"ignoredActorCount\":%d,\"ignoredComponentCount\":%d,\"ignoreMask\":%u}"),
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), *JsonVector(Capsule->GetUpVector()),
		Rotation.X, Rotation.Y, Rotation.Z, Rotation.W, int32(Capsule->GetCollisionObjectType()),
		*Capsule->GetCollisionProfileName().ToString().ReplaceCharWithEscapedChar(), *Channels, Capsule->IsQueryCollisionEnabled(), Query.bTraceComplex,
		Query.GetIgnoredActors().Num(), Query.GetIgnoredComponents().Num(), uint32(Query.IgnoreMask));
}
FString ContactFloorJson(const FVector& Center, const UCapsuleComponent* Capsule, const FFindFloorResult& Floor)
{
	return FString::Printf(TEXT("{\"blocking\":%d,\"walkable\":%d,\"lineTrace\":%d,\"distance\":%.6f,\"sweepDistance\":%.6f,\"lineDistance\":%.6f,\"hit\":%s,\"plane\":%s}"),
		Floor.bBlockingHit, Floor.IsWalkableFloor(), Floor.bLineTrace, Floor.GetDistanceToFloor(), Floor.FloorDist, Floor.LineDist,
		*ContactHitJson(Floor.HitResult), *ContactPlaneJson(Center, Capsule, Floor.HitResult));
}
FString ContactBoundsJson(const UBwayCharacterMovementComponent* Move)
{
	const float HeightAdjust = Move->IsMovingOnGround() ? UCharacterMovementComponent::MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -UCharacterMovementComponent::MAX_FLOOR_DIST;
	return FString::Printf(TEXT("{\"maxSpeed\":%.6f,\"gravityZ\":%.6f,\"maxStepHeight\":%.6f,\"movingOnGround\":%d,\"floorQueryBaseDistance\":%.6f,\"floorQueryModel\":\"FindFloor-base-before-perch-not-cached-query-history\",\"minWalkingFloorDistance\":%.6f,\"maxWalkingFloorDistance\":%.6f}"),
		Move->GetMaxSpeed(), Move->GetGravityZ(), Move->MaxStepHeight, Move->IsMovingOnGround(),
		FMath::Max(UCharacterMovementComponent::MAX_FLOOR_DIST, Move->MaxStepHeight + HeightAdjust),
		UCharacterMovementComponent::MIN_FLOOR_DIST, UCharacterMovementComponent::MAX_FLOOR_DIST);
}
FString SlideTuning(const UBwayCharacterMovementComponent* M)
{
	return FString::Printf(TEXT("BaseSlideSpeed=%.6f;SlideMaxSpeedFactor=%.6f;SlideSlopeAccelerationFactor=%.6f;MinSlopeAngleForAccel=%.6f;SlideBaseFrictionFactor=%.6f;SlideWallHitFriction=%.6f;SlideFrictionPower=%.6f;MinSlideSpeed=%.6f;GravityScale=%.6f;SlideGravityScale=%.6f;SlideSteerSpeed=%.6f;SlideJumpMomentumBoost=%.6f;SlideJumpMinHorizontalSpeed=%.6f;SlideJumpMaxHorizontalSpeed=%.6f;SlideJumpAirControl=%.6f;SlideJumpGravityScale=%.6f;SlideJumpLandedVelocityFactor=%.6f;JumpZVelocity=%.6f;PreserveSlideJumpUntilLanding=%d;SlideWalkableFloorAngle=%.6f;SlideCharacterRotationSpeed=%.6f;LootMaxSpeedFactorMultiplier=%.6f;LootSlopeAccelFactorMultiplier=%.6f;LootFrictionMultiplier=%.6f;MaxAcceleration=%.6f;MaxWalkSpeed=%.6f;AirControl=%.6f;GroundFriction=%.6f;BrakingDecelerationWalking=%.6f;BrakingFrictionFactor=%.6f;MaxStepHeight=%.6f;ServerCorrectionBypass=%d;ClientCorrectionBypass=%d;AcceptClientPosition=%d;GlobalClientAuthority=%d"),
		M->BaseSlideSpeed, M->SlideMaxSpeedFactor, M->SlideSlopeAccelerationFactor, M->MinSlopeAngleForAccel, M->SlideBaseFrictionFactor,
		M->SlideWallHitFriction, M->SlideFrictionPower, M->MinSlideSpeed, M->GravityScale, M->SlideGravityScale, M->SlideSteerSpeed,
		M->SlideJumpMomentumBoost, M->SlideJumpMinHorizontalSpeed, M->SlideJumpMaxHorizontalSpeed, M->SlideJumpAirControl,
		M->SlideJumpGravityScale, M->SlideJumpLandedVelocityFactor, M->JumpZVelocity, M->bPreserveSlideJumpUntilLanding,
		M->SlideWalkableFloorAngle, M->SlideCharacterRotationSpeed, M->LootMaxSpeedFactorMultiplier, M->LootSlopeAccelFactorMultiplier,
		M->LootFrictionMultiplier, M->MaxAcceleration, M->MaxWalkSpeed, M->AirControl, M->GroundFriction,
		M->BrakingDecelerationWalking, M->BrakingFrictionFactor, M->MaxStepHeight, M->bIgnoreClientMovementErrorChecksAndCorrection,
		M->bClientIgnoreMovementCorrections, M->bServerAcceptClientAuthoritativePosition, GetDefault<AGameNetworkManager>()->ClientAuthorativePosition);
}
// Exclude sub-centimeter contact tolerance, while detecting penetration into any
// blocking wall, ceiling, pawn, or floor with the capsule's actual collision rules.
bool CapsulePenetrating(const ABwayCharacterWithAbilities* Pawn)
{
	const auto* Capsule = Pawn->GetCapsuleComponent();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SlideTrajectoryOverlap), false, Pawn);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	return Pawn->GetWorld()->OverlapBlockingTestByChannel(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(FMath::Max(0.1f, Capsule->GetScaledCapsuleRadius() - 0.5f),
			FMath::Max(0.1f, Capsule->GetScaledCapsuleHalfHeight() - 0.5f)), Query, Responses);
}
bool HasTrajectorySlideSupport(const FFindFloorResult& Floor)
{
	// Custom slide deliberately uses a separate authored walkable angle. Its
	// production physics follows blocking floor contact, even where Walking is
	// disallowed. Preserve that distinction and require actual near-floor support.
	const float Distance = Floor.GetDistanceToFloor();
	return Floor.bBlockingHit && Floor.HitResult.IsValidBlockingHit() && Floor.HitResult.GetComponent()
		&& FMath::IsFinite(Distance) && Distance >= -0.5f && Distance <= 15.f && Floor.HitResult.ImpactNormal.Z >= 0.80f;
}
const TCHAR* GeometryPhase(int32 Case, const FFindFloorResult& Floor)
{
	if (!HasTrajectorySlideSupport(Floor)) return TEXT("unsupported");
	if (Case < 2) return TEXT("flat");
	const FVector& N = Floor.HitResult.ImpactNormal;
	const double Y = Floor.HitResult.ImpactPoint.Y;
	if (N.Z < 0.94 && N.Z > 0.80) return Y < -2950 ? TEXT("ramp1") : TEXT("ramp2");
	if (N.Z >= 0.99) return Y < -3150 ? TEXT("upper") : Y < -2730 ? TEXT("middle") : TEXT("lower");
	return TEXT("transition");
}
}
using namespace BwaySlidePackaged;

struct FBwaySlideLifecycleProbeState
{
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	TWeakObjectPtr<UNetDriver> Driver;
	TWeakObjectPtr<UGameplayCueManager> CueManager;
	FPacketSimulationSettings PacketSettings;
	FDelegateHandle Activated, Ended, Cue;
	FGameplayAbilitySpecHandle Slide, Jump;
	FGameplayTag SlideInput, JumpInput, SlidingTag;
	TArray<TWeakObjectPtr<ABwaySlideLifecycleProbe>> Relays;
	TArray<bool> Acks;
	FTransform OriginalTransform;
	FRotator OriginalPeerRotation = FRotator::ZeroRotator, OriginalControlRotation = FRotator::ZeroRotator;
	TWeakObjectPtr<AController> OriginalController;
	FString OriginalControllerPath;
	bool bPeerOrientationSaved = false, bHadController = false, bPeerOrientationAttempted = false, bPeerOrientationRestored = false;
	double OrientationReadySince = 0;
	FVector OriginalVelocity = FVector::ZeroVector;
	EMovementMode OriginalMode = MOVE_Walking;
	uint8 OriginalCustom = 0;
	float ConfiguredMinSpeed = 0.f;
	double Started = FPlatformTime::Seconds(), Published = Started, PhaseStarted = Started, StableSince = 0, FinalSince = 0;
	double EntryAt = 0;
	int32 SeenSerial = 0, Activations = 0, Ends = 0, Entries = 0, CompletedCases = 0;
	bool bCoordinator = false, bBound = false, bOwner = false, bPacketSaved = false, bAckSent = false, bFailed = false;
	bool bSlidePressed = false, bJumpPressed = false, bExecuted = false, bJumpProof = false, bCleaned = false;
	bool bOriginalSaved = false, bAuthorityRestored = false, bFinalConfirmed = false;
	bool bCleanupOkay = true;
	FString FailureReason, SetupReason;
	FString AuthoredTuning;
	FVector TrajectoryEntryPosition = FVector::ZeroVector;
	double SlideSeconds = 0, FirstSlideTimestamp = -1, LastSlideTimestamp = -1, LastMoveTimestamp = -1, LastProxySample = 0;
	double SupportRejectedAt = 0;
	int32 GeometryMask = 0, TrajectorySamples = 0, OriginalCorrections = 0, EventSequence = 0, ApproachInputFrames = 0;
	bool bTerminalIssued = false, bAirborne = false, bLanded = false, bMovementBound = false, bCorrectionsSaved = false;
	FString GroundAirExpectedComponent;
	TSet<FString> GroundAirRouteComponents;
	struct FIsolatedBystander
	{
		TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
		FTransform Transform;
		FVector Velocity = FVector::ZeroVector;
		EMovementMode Mode = MOVE_Walking;
		uint8 Custom = 0;
	};
	TArray<FIsolatedBystander> IsolatedBystanders;
	double GroundAirStartedAt = 0, GroundAirLastGroundTimestamp = -1, GroundAirReturnAt = 0;
	FVector GroundAirLastGroundPosition = FVector::ZeroVector;
	double GroundAirGroundTravel = 0;
	int32 GroundAirCoverageMask = 0, GroundAirGroundSamples = 0, GroundAirAirSamples = 0, GroundAirSlideJumpAirSamples = 0, GroundAirInvalidSamples = 0;
	bool bGroundAirSawAir = false, bGroundAirSawReturn = false, bGroundAirGroundContiguous = false, bGroundAirObservationEnded = false;
	TArray<TWeakObjectPtr<AActor>> ReviewCourse;
	FDelegateHandle ReviewObserver;
	float OriginalJumpHold = 0.f;
	bool bReviewHoldSaved = false;
	int32 ReviewBoosts = 0, ReviewHeldCalls = 0, ReviewConcaveSteps = 0, ReviewDecisiveSteps = 0;
	double ReviewHeldSeconds = 0, ReviewFirstJumpTime = 0;
	float ReviewFirstHeldTimestamp = -1.f, ReviewLastHeldTimestamp = -1.f;
	double ReviewRelocationStartedAt = 0;
	float ReviewLaunchSpeed = 0.f;
	FString ReviewError;
};
#else
struct FBwaySlideLifecycleProbeState {};
#endif

ABwaySlideLifecycleProbe::ABwaySlideLifecycleProbe()
{
	bReplicates = true; bOnlyRelevantToOwner = true; bNetLoadOnClient = false;
	SetReplicateMovement(false); SetNetUpdateFrequency(20.f);
	PrimaryActorTick.bCanEverTick = !UE_BUILD_SHIPPING;
	PrimaryActorTick.TickInterval = 0.f;
}
void ABwaySlideLifecycleProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwaySlideLifecycleProbe, Frame);
}
void ABwaySlideLifecycleProbe::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING && DO_ENABLE_NET_TEST
	State = MakeShared<FBwaySlideLifecycleProbeState>();
#endif
}
void ABwaySlideLifecycleProbe::ServerReport_Implementation(int32 Serial, int32 Case, int32 Phase, bool bPassed, const FString& Reason)
{
#if !UE_BUILD_SHIPPING && DO_ENABLE_NET_TEST
	if (auto* Root = Coordinator.Get()) Root->ReceiveReport(this, Serial, Case, Phase, bPassed, Reason.Left(96));
#endif
}
void ABwaySlideLifecycleProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING && DO_ENABLE_NET_TEST
	if (!State || Frame.Serial == 0 || (HasAuthority() && !State->bCoordinator)) return;
	const double Now = FPlatformTime::Seconds();
	if (Now - State->Started > 180.0 && Frame.Phase < Stop) Fail(TEXT("setup"), TEXT("overall-timeout"));
	if (Frame.Trajectory && Frame.Phase >= RunUp && Frame.Phase <= Settled)
		TrajectoryRecord(TEXT("Frame"), FString::Printf(TEXT("\"frameId\":%llu,\"frameDt\":%.7f"), GFrameCounter, GetWorld()->GetDeltaSeconds()));
	Observe();
	if (State->bCoordinator) AuthorityTick();
#endif
}
void ABwaySlideLifecycleProbe::EndPlay(const EEndPlayReason::Type Reason)
{
#if !UE_BUILD_SHIPPING && DO_ENABLE_NET_TEST
	if (State && (!HasAuthority() || State->bCoordinator))
	{
		CleanupLocal();
		if (State->bCoordinator) RestoreAuthority();
		if (Frame.Trajectory && !State->bPeerOrientationRestored && State->Pawn.IsValid()) RestorePeerOrientation();
		if (Frame.ReviewRegression) BwaySlideReviewCourse::Destroy(State->ReviewCourse);
		UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Teardown peer=%d player=%d confirmed=%d passed=%d"), Frame.Peer, Frame.Source, State->bFinalConfirmed, Frame.Passed);
		if (!HasAuthority() && State->bFinalConfirmed && Frame.Passed && Reason == EEndPlayReason::Destroyed
			&& (!Frame.Trajectory || GetWorld()->WorldType == EWorldType::Game)) FPlatformMisc::RequestExit(false);
	}
	State.Reset();
#endif
	Super::EndPlay(Reason);
}

#if !UE_BUILD_SHIPPING && DO_ENABLE_NET_TEST
bool ABwaySlideLifecycleProbe::StartInWorld(UWorld* World, bool bTrajectory, const FString& Token, bool bSupportDiagnostic, bool bGroundAirDiagnostic, bool bReviewRegression)
{
	if (bReviewRegression && (!bTrajectory || bSupportDiagnostic || bGroundAirDiagnostic)) return false;
	if ((bSupportDiagnostic || bGroundAirDiagnostic) && (!bTrajectory || !World || World->WorldType != EWorldType::PIE)) return false;
	if (bSupportDiagnostic && bGroundAirDiagnostic) return false;
	if (!World || World->GetNetMode() != NM_ListenServer || !World->GetMapName().Contains(TEXT("L_BW_Dorado")) || !Playing(World)) return false;
	for (TActorIterator<ABwaySlideLifecycleProbe> It(World); It; ++It) return true;
	const auto* GS = World->GetGameState<ABwayGameState>();
	if (!GS || GS->PlayerArray.Num() != 4) return false;
	TArray<APlayerController*> Controllers;
	APlayerController* Host = nullptr;
	for (APlayerState* PS : GS->PlayerArray)
	{
		auto* Player = Cast<ABwayPlayerState>(PS);
		auto* PC = Player ? Player->GetPlayerController() : nullptr;
		auto* Pawn = Player ? Cast<ABwayCharacterWithAbilities>(Player->GetPawn()) : nullptr;
		if (!Player || Player->IsABot() || !PC || !Pawn || !Pawn->GetLyraAbilitySystemComponent() || !Pawn->GetBwayCharacterMovement()) return false;
		if (PC->IsLocalController()) { if (Host) return false; Host = PC; }
		else { if (!PC->GetNetConnection()) return false; Controllers.Add(PC); }
	}
	if (!Host || Controllers.Num() != 3) return false;
	Controllers.Sort([](const APlayerController& A, const APlayerController& B) { return A.PlayerState->GetPlayerId() < B.PlayerState->GetPlayerId(); });
	Controllers.Insert(Host, 0);
	FActorSpawnParameters Parameters;
	Parameters.Owner = Host;
	auto* Root = World->SpawnActor<ABwaySlideLifecycleProbe>(Parameters);
	if (!Root) return false;
	Root->Coordinator = Root; Root->State->bCoordinator = true;
	Root->Frame.Trajectory = bTrajectory; Root->Frame.Token = Token;
	Root->Frame.SupportDiagnostic = bSupportDiagnostic; Root->Frame.GroundAirDiagnostic = bGroundAirDiagnostic;
	Root->Frame.ReviewRegression = bReviewRegression;
	Root->Frame.Source = Controllers[1]->PlayerState->GetPlayerId();
	Root->Frame.AuthoredMinSpeed = FindPawn(World, Root->Frame.Source)->GetBwayCharacterMovement()->MinSlideSpeed;
	Root->Frame.ReviewCourseRadius = bReviewRegression ? FindPawn(World, Root->Frame.Source)->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	Root->State->Relays.Add(Root);
	for (int32 Index = 1; Index < 4; ++Index)
	{
		Parameters.Owner = Controllers[Index];
		auto* Relay = World->SpawnActor<ABwaySlideLifecycleProbe>(Parameters);
		if (!Relay) { Root->Fail(TEXT("setup"), TEXT("relay-spawn-failed")); return true; }
		Relay->Coordinator = Root; Relay->Frame.Peer = Index;
		Root->State->Relays.Add(Relay);
	}
	Root->Publish(Baseline);
	UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: START player=%d peers=4 sourcePeer=1 timeoutSeconds=180 physicalInput=0 trajectoryParity=0"), Root->Frame.Source);
	if (bSupportDiagnostic) UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: SupportDiagnosticScope token=%s acceptance=0 finalPassed=0 continuationSeconds=%.1f"), *Token, SupportDiagnosticSeconds);
	if (bGroundAirDiagnostic) UE_LOG(LogTemp, Display, TEXT("BwaySlideGroundAirDiagnostic: Scope token=%s diagnosticOnly=1 acceptance=0 finalPassed=0 continuationSeconds=%.1f"), *Token, GroundAirDiagnosticSeconds);
	return true;
}

void ABwaySlideLifecycleProbe::Publish(int32 Phase)
{
	Frame.Phase = Phase; ++Frame.Serial; Frame.Passed = !State->bFailed && !Frame.SupportDiagnostic && !Frame.GroundAirDiagnostic;
	State->Acks.Init(false, 4); State->Published = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < State->Relays.Num(); ++Index)
		if (auto* Relay = State->Relays[Index].Get()) { Relay->Frame = Frame; Relay->Frame.Peer = Index; Relay->ForceNetUpdate(); }
	UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Phase serial=%d case=%d name=%s phase=%d player=%d"), Frame.Serial, Frame.Case, CaseName(Frame.Case, Frame.Trajectory, Frame.ReviewRegression), Frame.Phase, Frame.Source);
}

bool ABwaySlideLifecycleProbe::BindSource()
{
	if (State->bFailed || State->bCleaned) return false;
	auto* GS = GetWorld()->GetGameState<ABwayGameState>();
	auto* PC = Cast<APlayerController>(GetOwner());
	auto* Pawn = FindPawn(GetWorld(), Frame.Source);
	auto* ASC = Pawn ? Pawn->GetLyraAbilitySystemComponent() : nullptr;
	auto* Move = Pawn ? Pawn->GetBwayCharacterMovement() : nullptr;
	if (!GS || GS->PlayerArray.Num() != 4 || !PC || !PC->IsLocalController() || !Pawn || !ASC || !Move || ASC->GetAvatarActor() != Pawn || !GetWorld()->GetNetDriver() || !Playing(GetWorld()))
	{
		State->SetupReason = FString::Printf(TEXT("owner-chain players=%d localPC=%d pawn=%d asc=%d movement=%d avatarMatches=%d playing=%d"),
			GS ? GS->PlayerArray.Num() : -1, PC && PC->IsLocalController(), Pawn != nullptr, ASC != nullptr, Move != nullptr, ASC && ASC->GetAvatarActor() == Pawn, Playing(GetWorld()));
		return false;
	}
	State->bOwner = Pawn->IsLocallyControlled();
	State->SlideInput = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Slide"));
	State->JumpInput = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Jump"));
	State->SlidingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sliding"));
	int32 SlideBindings = 0, JumpBindings = 0;
	State->Slide = FGameplayAbilitySpecHandle(); State->Jump = FGameplayAbilitySpecHandle();
	for (const auto& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(State->SlideInput))
		{
			++SlideBindings;
			if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_Slide>()) State->Slide = Spec.Handle;
		}
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(State->JumpInput))
		{
			++JumpBindings;
			if (Spec.Ability && Spec.Ability->IsA<ULyraGameplayAbility_Jump>()) State->Jump = Spec.Handle;
		}
	}
	if ((HasAuthority() || State->bOwner) && (SlideBindings != 1 || JumpBindings != 1 || !State->Slide.IsValid() || !State->Jump.IsValid()))
	{
		State->SetupReason = FString::Printf(TEXT("ability-bindings slide=%d jump=%d slideFamily=%d jumpFamily=%d"), SlideBindings, JumpBindings, State->Slide.IsValid(), State->Jump.IsValid());
		return false;
	}
	if (!FMath::IsFinite(Move->MinSlideSpeed) || Move->MinSlideSpeed < 0.f || !FMath::IsNearlyEqual(Move->MinSlideSpeed, Frame.AuthoredMinSpeed) ||
		!Move->IsMovingOnGround() || Move->Velocity.Size() > (Frame.Trajectory ? 0.1f : 10.f) || Pawn->bIsCrouched || Pawn->bPressedJump ||
		Active(ASC, State->Slide) || Active(ASC, State->Jump) || ASC->HasMatchingGameplayTag(State->SlidingTag))
	{
		State->SetupReason = FString::Printf(TEXT("idle-baseline mode=%d speed=%.2f min=%.2f authorityMin=%.2f crouched=%d jumpPressed=%d slideActive=%d jumpActive=%d tag=%d"),
			int32(Move->MovementMode), Move->Velocity.Size(), Move->MinSlideSpeed, Frame.AuthoredMinSpeed, Pawn->bIsCrouched, Pawn->bPressedJump,
			Active(ASC, State->Slide), Active(ASC, State->Jump), ASC->HasMatchingGameplayTag(State->SlidingTag));
		return false;
	}
	if (State->bOwner && (ASC->FindAbilitySpecFromHandle(State->Slide)->InputPressed || ASC->FindAbilitySpecFromHandle(State->Jump)->InputPressed))
	{ State->SetupReason = TEXT("owner-input-held-at-baseline"); return false; }
	auto* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!Manager) { State->SetupReason = TEXT("cue-manager-unavailable"); return false; }
	State->Pawn = Pawn; State->ASC = ASC; State->Driver = GetWorld()->GetNetDriver(); State->CueManager = Manager;
	State->ConfiguredMinSpeed = Move->MinSlideSpeed;
	if (Frame.Trajectory)
	{
		if (Move->MinSlideSpeed != 0.f) { Fail(TEXT("setup"), TEXT("trajectory-requires-authored-zero-threshold")); return false; }
		if (Move->bIgnoreClientMovementErrorChecksAndCorrection || Move->bClientIgnoreMovementCorrections || Move->bServerAcceptClientAuthoritativePosition
			|| GetDefault<AGameNetworkManager>()->ClientAuthorativePosition)
		{ Fail(TEXT("setup"), TEXT("trajectory-movement-corrections-bypassed")); return false; }
		State->AuthoredTuning = SlideTuning(Move);
		// Autonomous pawns need not share the server's actor yaw. Capture each
		// peer and its actual source controller before any fixture mutation.
		auto* SourceController = Pawn->GetController();
		if ((HasAuthority() || State->bOwner) && !SourceController)
		{ Fail(TEXT("setup"), TEXT("source-controller-missing-at-orientation-baseline")); return false; }
		State->OriginalPeerRotation = Pawn->GetActorRotation();
		State->OriginalController = SourceController; State->bHadController = SourceController != nullptr;
		State->OriginalControllerPath = GetPathNameSafe(SourceController);
		State->OriginalControlRotation = SourceController ? SourceController->GetControlRotation() : FRotator::ZeroRotator;
		if (State->OriginalPeerRotation.ContainsNaN() || State->OriginalControlRotation.ContainsNaN())
		{ Fail(TEXT("setup"), TEXT("invalid-orientation-baseline")); return false; }
		State->bPeerOrientationSaved = true;
		if (State->bCoordinator)
		{
			State->OriginalTransform = Pawn->GetActorTransform(); State->OriginalVelocity = Move->Velocity;
			State->OriginalMode = Move->MovementMode; State->OriginalCustom = Move->CustomMovementMode; State->bOriginalSaved = true;
		}
		Pawn->OnCharacterMovementUpdated.AddUniqueDynamic(this, &ABwaySlideLifecycleProbe::OnTrajectoryMovement);
		State->bMovementBound = true;
		if (State->bOwner)
		{
			if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.NetShowCorrections")))
			{
				State->OriginalCorrections = CVar->GetInt(); State->bCorrectionsSaved = true;
				CVar->SetWithCurrentPriority(1);
				if (CVar->GetInt() != 1) { Fail(TEXT("setup"), TEXT("correction-cvar-not-enabled")); return false; }
			}
			else { Fail(TEXT("setup"), TEXT("correction-cvar-unavailable")); return false; }
		}
	}
	State->PacketSettings = State->Driver->PacketSimulationSettings; State->bPacketSaved = true;
	GEngine->Exec(GetWorld(), TEXT("Net PktLag=100"));
	if (State->Driver->PacketSimulationSettings.PktLag != 100) { Fail(TEXT("setup"), TEXT("packet-lag-not-applied")); return false; }
	const TWeakObjectPtr<ABwaySlideLifecycleProbe> Weak(this);
	if (Frame.ReviewRegression)
	{
		State->OriginalJumpHold = Pawn->JumpMaxHoldTime; State->bReviewHoldSaved = true;
		Pawn->JumpMaxHoldTime = 0.3f;
		if (!BwaySlideReviewCourse::Spawn(GetWorld(), State->ReviewCourse, Frame.ReviewCourseRadius, Frame.Token))
		{ Fail(TEXT("setup"), TEXT("review-course-spawn-failed")); return false; }
		State->ReviewObserver = Move->SlideReviewObserver.AddLambda([Weak](const FBwaySlideReviewObservation& Observation)
		{
			if (auto* Probe = Weak.Get()) Probe->OnReviewStep(Observation);
		});
	}
	if (HasAuthority() || State->bOwner)
	{
		State->Activated = ASC->AbilityActivatedCallbacks.AddLambda([Weak](UGameplayAbility* Ability)
		{
			if (auto* P = Weak.Get(); P && P->State && !P->State->bCleaned && Ability && Ability->GetCurrentAbilitySpecHandle() == P->State->Slide) ++P->State->Activations;
		});
		State->Ended = ASC->AbilityEndedCallbacks.AddLambda([Weak](UGameplayAbility* Ability)
		{
			if (auto* P = Weak.Get(); P && P->State && !P->State->bCleaned && Ability && Ability->GetCurrentAbilitySpecHandle() == P->State->Slide) ++P->State->Ends;
		});
		State->Cue = Manager->OnGameplayCueRouted().AddLambda([Weak](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event, const FGameplayCueParameters& Params, EGameplayCueExecutionOptions)
		{
			auto* P = Weak.Get();
			if (!P || !P->State || P->State->bCleaned || Target != P->State->Pawn.Get() || Tag != BwayGameplayCueTags::MovementSlide || Event != EGameplayCueEvent::Executed || Params.Instigator.Get() != Target) return;
			auto* Move = P->State->Pawn->GetBwayCharacterMovement();
			if (Move->IsSliding())
			{
				++P->State->Entries; P->State->EntryAt = FPlatformTime::Seconds();
				if (P->Frame.ReviewRegression && P->Frame.Case == 1 && P->State->Entries == 1)
				{
					// Dedicated collision fixture: stage identical incoming state only
					// after the first natural GAS slide-entry cue on owner/authority.
					// PhysSliding and all subsequent movement/prediction remain production.
					auto* ReviewPawn = P->State->Pawn.Get();
					const float HalfHeight = ReviewPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					const FVector InitialPosition = BwaySlideReviewCourse::ConcaveStart(HalfHeight);
					ReviewPawn->SetActorLocation(InitialPosition, false, nullptr, ETeleportType::TeleportPhysics);
					Move->Velocity = BwaySlideReviewCourse::ConcaveVelocity();
					Move->FindFloor(InitialPosition, Move->CurrentFloor, false);
					P->TrajectoryRecord(TEXT("ReviewConcaveSeed"), FString::Printf(TEXT("\"seededCollisionFixture\":1,\"productionGasEntry\":1,\"entries\":%d,\"sliding\":%d,\"courseVersion\":2,\"radius\":%.6f,\"halfHeight\":%.6f,\"position\":%s,\"velocity\":%s,\"floorDistance\":%.6f,\"floorNormal\":%s"),
						P->State->Entries, Move->IsSliding(), ReviewPawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), HalfHeight,
						*JsonVector(ReviewPawn->GetActorLocation()), *JsonVector(Move->Velocity), Move->CurrentFloor.GetDistanceToFloor(), *JsonVector(Move->CurrentFloor.HitResult.ImpactNormal)));
					if (P->Frame.Phase != Execute || !FMath::IsNearlyEqual(ReviewPawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), P->Frame.ReviewCourseRadius, 0.001f)
						|| !Move->CurrentFloor.IsWalkableFloor() || Move->CurrentFloor.HitResult.bStartPenetrating
						|| Move->CurrentFloor.HitResult.ImpactNormal.Z < 0.999f || CapsulePenetrating(ReviewPawn))
						P->State->ReviewError = TEXT("review-concave-seed-invalid");
				}
				if (P->Frame.Trajectory && P->State->Entries == 1) P->State->TrajectoryEntryPosition = Target->GetActorLocation();
			}
			UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Entry serial=%d case=%d peer=%d player=%d sliding=%d entries=%d"), P->Frame.Serial, P->Frame.Case, P->Frame.Peer, P->Frame.Source, Move->IsSliding(), P->State->Entries);
		});
	}
	State->bBound = true;
	if (Frame.ReviewRegression) TrajectoryRecord(TEXT("ReviewSetup"), FString::Printf(TEXT("\"reviewSchema\":1,\"originalJumpHold\":%.6f,\"fixtureJumpHold\":%.6f,\"courseActors\":%d,\"courseRadius\":%.6f,\"courseVersion\":2,\"seededConcaveFixture\":1,\"authoritativeInput\":1,\"physicalInput\":0"), State->OriginalJumpHold, Pawn->JumpMaxHoldTime, State->ReviewCourse.Num(), Frame.ReviewCourseRadius));
	if (Frame.Trajectory) TrajectoryRecord(TEXT("Setup"), FString::Printf(TEXT("\"tuning\":\"%s\",\"radius\":%.3f,\"positionTolerance\":10,\"velocityTolerance\":60,\"convergenceTolerance\":10,\"penetrationToleranceCm\":0.5,\"customFloorDistanceMin\":-0.5,\"customFloorDistanceMax\":15,\"customFloorNormalMinZ\":0.80,\"lagMs\":%d,\"pawnName\":\"%s\",\"correctionsEnabled\":%d,\"orientationCaptured\":%d,\"originalRotation\":%s,\"hadController\":%d,\"originalController\":\"%s\",\"originalControlRotation\":%s"),
		*State->AuthoredTuning, Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), State->Driver->PacketSimulationSettings.PktLag,
		*Pawn->GetName().ReplaceCharWithEscapedChar(), State->bCorrectionsSaved, State->bPeerOrientationSaved, *JsonVector(State->OriginalPeerRotation.Euler()),
		State->bHadController, *State->OriginalControllerPath.ReplaceCharWithEscapedChar(), *JsonVector(State->OriginalControlRotation.Euler())));
	UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Connected peer=%d player=%d role=%s minSlide=%.2f lagMs=%d"), Frame.Peer, Frame.Source,
		HasAuthority() ? TEXT("authority") : State->bOwner ? TEXT("owner") : TEXT("observer"), Move->MinSlideSpeed, State->Driver->PacketSimulationSettings.PktLag);
	return true;
}

void ABwaySlideLifecycleProbe::Input(bool bJump, bool bPressed)
{
	if (!State->bOwner || !State->ASC.IsValid()) return;
	const auto Tag = bJump ? State->JumpInput : State->SlideInput;
	if (bPressed) State->ASC->AbilityInputTagPressed(Tag); else State->ASC->AbilityInputTagReleased(Tag);
	State->ASC->ProcessAbilityInput(0.f, false);
}
void ABwaySlideLifecycleProbe::Sample(const TCHAR* Label) const
{
	const auto* Move = State->Pawn.IsValid() ? State->Pawn->GetBwayCharacterMovement() : nullptr;
	UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Sample serial=%d case=%d peer=%d player=%d label=%s mode=%d custom=%d speed=%.2f slideJump=%d active=%d tag=%d activated=%d ended=%d entries=%d"),
		Frame.Serial, Frame.Case, Frame.Peer, Frame.Source, Label, Move ? int32(Move->MovementMode) : -1, Move ? int32(Move->CustomMovementMode) : -1,
		Move ? Move->Velocity.Size2D() : -1.f, Move && Move->IsSlideJumping(), Active(State->ASC.Get(), State->Slide),
		State->ASC.IsValid() && State->ASC->HasMatchingGameplayTag(State->SlidingTag), State->Activations, State->Ends, State->Entries);
}
void ABwaySlideLifecycleProbe::Report(bool bPassed, const TCHAR* Reason)
{
	if (bPassed && Frame.Phase < Stop && Frame.SupportDiagnostic && State && State->SupportRejectedAt > 0) return;
	if (State->bAckSent) return;
	State->bAckSent = true;
	UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Report serial=%d case=%d phase=%d peer=%d player=%d passed=%d reason=%s"), Frame.Serial, Frame.Case, Frame.Phase, Frame.Peer, Frame.Source, bPassed, Reason);
	if (State->bCoordinator) ReceiveReport(this, Frame.Serial, Frame.Case, Frame.Phase, bPassed, Reason);
	else ServerReport(Frame.Serial, Frame.Case, Frame.Phase, bPassed, Reason);
}
void ABwaySlideLifecycleProbe::ReceiveReport(ABwaySlideLifecycleProbe* Relay, int32 Serial, int32 Case, int32 Phase, bool bPassed, const FString& Reason)
{
	if (!State || !State->bCoordinator || Serial != Frame.Serial || Case != Frame.Case || Phase != Frame.Phase) return;
	const int32 Index = State->Relays.IndexOfByPredicate([Relay](const auto& Item) { return Item.Get() == Relay; });
	const auto* PC = Relay ? Cast<APlayerController>(Relay->GetOwner()) : nullptr;
	if (Index < 0 || Index >= 4 || !PC || !PC->PlayerState || (Index == 1 && PC->PlayerState->GetPlayerId() != Frame.Source)) { Fail(TEXT("setup"), TEXT("relay-owner-mismatch")); return; }
	if (State->Acks[Index]) return;
	if (!bPassed) { Fail(TEXT("remote"), *Reason); return; }
	State->Acks[Index] = true;
	UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Ack serial=%d case=%d phase=%d peer=%d player=%d"), Serial, Case, Phase, Index, Frame.Source);
}
void ABwaySlideLifecycleProbe::Fail(const TCHAR* Category, const TCHAR* Reason)
{
	if (!State || State->bFailed) return;
	if (Frame.SupportDiagnostic && State->SupportRejectedAt > 0)
	{
		const auto* Move = State->Pawn.IsValid() ? State->Pawn->GetBwayCharacterMovement() : nullptr;
		UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: SupportDiagnosticEnd token=%s peer=%d case=%d elapsed=%.6f acceptance=0 reason=%s mode=%d custom=%d active=%d floorDistance=%.6f position=%s velocity=%s"),
			*Frame.Token, Frame.Peer, Frame.Case, FPlatformTime::Seconds() - State->SupportRejectedAt, Reason,
			Move ? int32(Move->MovementMode) : -1, Move ? int32(Move->CustomMovementMode) : -1, Active(State->ASC.Get(), State->Slide),
			Move ? Move->CurrentFloor.GetDistanceToFloor() : -1.f, *JsonVector(State->Pawn.IsValid() ? State->Pawn->GetActorLocation() : FVector::ZeroVector),
			*JsonVector(Move ? Move->Velocity : FVector::ZeroVector));
	}
	State->bFailed = true; State->FailureReason = Reason;
	Sample(TEXT("failure"));
	UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL peer=%d case=%d phase=%d player=%d category=%s reason=%s"), Frame.Peer, Frame.Case, Frame.Phase, Frame.Source, Category, Reason);
	if (State->bCoordinator) { if (Frame.Phase < Stop) Publish(Stop); }
	else { State->bAckSent = false; Report(false, Reason); CleanupLocal(); }
}

void ABwaySlideLifecycleProbe::Observe()
{
	const double Now = FPlatformTime::Seconds();
	if (State->SeenSerial != Frame.Serial)
	{
		State->SeenSerial = Frame.Serial; State->PhaseStarted = Now; State->StableSince = 0; State->bAckSent = false;
		if (Frame.Phase == Armed && !State->bFailed && !State->bCleaned)
		{
			State->Activations = 0; State->Ends = 0; State->Entries = 0; State->EntryAt = 0; State->bExecuted = false; State->bJumpProof = false;
			if (State->Pawn.IsValid() && !Frame.Trajectory) State->Pawn->GetBwayCharacterMovement()->MinSlideSpeed = Frame.Case == 1 ? 100.f : State->ConfiguredMinSpeed;
			UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Tuning serial=%d case=%d name=%s peer=%d player=%d originalMin=%.2f activeMin=%.2f conditionalOverride=%d"),
				Frame.Serial, Frame.Case, CaseName(Frame.Case, Frame.Trajectory, Frame.ReviewRegression), Frame.Peer, Frame.Source, State->ConfiguredMinSpeed,
				State->Pawn.IsValid() ? State->Pawn->GetBwayCharacterMovement()->MinSlideSpeed : -1.f, !Frame.Trajectory && Frame.Case == 1);
			if (Frame.Trajectory)
			{
				State->SlideSeconds = 0; State->FirstSlideTimestamp = -1; State->LastSlideTimestamp = -1;
				State->LastMoveTimestamp = -1; State->GeometryMask = 0; State->TrajectorySamples = 0; State->ApproachInputFrames = 0;
				State->bTerminalIssued = false; State->bAirborne = false; State->bLanded = false;
				State->ReviewBoosts = 0; State->ReviewHeldCalls = 0; State->ReviewConcaveSteps = 0; State->ReviewDecisiveSteps = 0;
				State->ReviewHeldSeconds = 0; State->ReviewFirstJumpTime = 0; State->ReviewLaunchSpeed = 0;
				State->ReviewFirstHeldTimestamp = -1.f; State->ReviewLastHeldTimestamp = -1.f;
				State->ReviewError.Reset();
				State->GroundAirExpectedComponent.Reset(); State->GroundAirRouteComponents.Reset(); State->GroundAirStartedAt = 0; State->GroundAirLastGroundTimestamp = -1;
				State->GroundAirReturnAt = 0; State->GroundAirLastGroundPosition = FVector::ZeroVector; State->GroundAirGroundTravel = 0;
				State->GroundAirCoverageMask = 0; State->GroundAirGroundSamples = 0; State->GroundAirAirSamples = 0; State->GroundAirSlideJumpAirSamples = 0; State->GroundAirInvalidSamples = 0;
				State->bGroundAirSawAir = false; State->bGroundAirSawReturn = false; State->bGroundAirGroundContiguous = false; State->bGroundAirObservationEnded = false;
				TrajectoryRecord(TEXT("Tuning"), FString::Printf(TEXT("\"phase\":\"before\",\"tuning\":\"%s\",\"unchanged\":%d"), *SlideTuning(State->Pawn->GetBwayCharacterMovement()), State->AuthoredTuning == SlideTuning(State->Pawn->GetBwayCharacterMovement())));
			}
		}
		// Restore the conditional threshold on every peer before acknowledging this case's settled state.
		if (Frame.Phase == Settled && State->Pawn.IsValid() && !Frame.Trajectory) State->Pawn->GetBwayCharacterMovement()->MinSlideSpeed = State->ConfiguredMinSpeed;
		if (Frame.Phase == Stop) CleanupLocal();
	}
	if (Frame.Phase == Baseline)
	{
		// A local failure may precede the replicated Stop phase; never rebind after cleanup.
		if (State->bFailed || State->bCleaned) return;
		if (!State->bBound && !BindSource())
		{
			if (Now - State->PhaseStarted > 25.0)
			{
				UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: SetupDiagnostic peer=%d player=%d reason=%s"), Frame.Peer, Frame.Source, *State->SetupReason);
				Fail(TEXT("setup"), TEXT("source-binding-or-idle-baseline-timeout"));
			}
			return;
		}
		Report(true, TEXT("baseline")); return;
	}
	if (Frame.Phase == Stop)
	{
		// Let in-flight 100ms input/end traffic drain before the authority restores the transform.
		if (Now - State->PhaseStarted >= 0.5) Report(State->bCleanupOkay, TEXT("local-input-delegates-packets-threshold-restored"));
		return;
	}
	if (Frame.Phase == Complete)
	{
		if (!State->bAckSent)
		{
			State->bFinalConfirmed = Frame.Passed && !State->bFailed && State->bCleanupOkay && State->CompletedCases == (Frame.ReviewRegression ? 2 : 4);
			UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: %s %s peer=%d player=%d cases=%d cleanup=1 physicalInput=0 trajectoryParity=0"),
				HasAuthority() ? TEXT("Host") : TEXT("Client"), State->bFinalConfirmed ? TEXT("PASS") : TEXT("FAIL"), Frame.Peer, Frame.Source, State->CompletedCases);
			if (Frame.Trajectory) TrajectoryRecord(TEXT("Complete"), FString::Printf(TEXT("\"passed\":%d,\"cases\":%d,\"cleanup\":%d,\"physicalInput\":0,\"trajectoryParity\":0"), State->bFinalConfirmed, State->CompletedCases, State->bCleanupOkay));
			Report(!Frame.Passed || State->bFinalConfirmed, TEXT("final-result-observed"));
		}
		return;
	}
	if (!State->Pawn.IsValid() || !State->ASC.IsValid() || FindPawn(GetWorld(), Frame.Source) != State->Pawn.Get()) { Fail(TEXT("setup"), TEXT("source-avatar-changed")); return; }
	if (Frame.Phase < Stop && !Playing(GetWorld())) { Fail(TEXT("setup"), TEXT("playing-phase-changed")); return; }
	if (State->bFailed && Frame.Phase < Stop) return;
	if (Frame.Trajectory && Frame.Phase >= Armed && Frame.Phase <= Settled)
	{
		if (Frame.ReviewRegression) ObserveReviewRegression();
		else if (Frame.GroundAirDiagnostic) ObserveGroundAirDiagnostic();
		else ObserveTrajectory();
		return;
	}
	auto* Move = State->Pawn->GetBwayCharacterMovement();
	const bool bClean = !Move->IsSliding() && !Active(State->ASC.Get(), State->Slide) && !State->ASC->HasMatchingGameplayTag(State->SlidingTag);
	const bool bExpectedDestination = Frame.Case == 3 ? (Move->IsFalling() || Move->MovementMode == MOVE_Walking) : Move->MovementMode == MOVE_Walking;
	if (Frame.Phase == Relocated || Frame.Phase == Restored)
	{
		const FVector Delta = State->Pawn->GetActorLocation() - Frame.Position;
		const bool bSpatialMotionReady = Delta.Size2D() < 10.f && FMath::Abs(Delta.Z) < 10.f
			&& State->Pawn->GetActorScale3D().Equals(Frame.RestoreScale, 0.01f)
			&& Move->MovementMode == Frame.RestoreMode && Move->CustomMovementMode == Frame.RestoreCustom
			&& Move->Velocity.Equals(Frame.RestoreVelocity, 1.f) && Move->IsMovingOnGround() && Move->Velocity.Size() <= 10.f;
		if (Frame.Trajectory && Frame.Phase == Restored && !State->bPeerOrientationAttempted)
		{
			const auto* SlideSpec = State->ASC->FindAbilitySpecFromHandle(State->Slide);
			const auto* JumpSpec = State->ASC->FindAbilitySpecFromHandle(State->Jump);
			const bool bInputsClean = !State->bSlidePressed && !State->bJumpPressed && !State->Pawn->bPressedJump
				&& State->Pawn->GetPendingMovementInputVector().IsNearlyZero() && (!State->bOwner ||
					(SlideSpec && JumpSpec && !SlideSpec->InputPressed && !JumpSpec->InputPressed));
			const bool bReady = bSpatialMotionReady && bClean && bInputsClean && !Active(State->ASC.Get(), State->Jump)
				&& State->bCleaned && State->bCleanupOkay;
			if (bReady)
			{
				if (State->OrientationReadySince == 0) State->OrientationReadySince = Now;
				if (Now - State->OrientationReadySince >= 0.5)
				{
					// A late positional correction or pending combined move can restore
					// an older actor rotation. Wait for motion to settle before the one
					// orientation action, then require the normal independent stable readback.
					State->StableSince = 0;
					if (!RestorePeerOrientation())
					{
						State->bCleanupOkay = false;
						UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=cleanup reason=peer-orientation-restore-failed peer=%d player=%d"), Frame.Peer, Frame.Source);
					}
				}
			}
			else State->OrientationReadySince = 0;
		}
		const FRotator TargetRotation = Frame.Trajectory ? State->OriginalPeerRotation : Frame.Rotation;
		const auto* SourceController = State->Pawn->GetController();
		const bool bControllerRestored = State->bPeerOrientationSaved && (State->bHadController
			? State->OriginalController.IsValid() && SourceController == State->OriginalController.Get()
				&& SourceController->GetControlRotation().Equals(State->OriginalControlRotation, 1.f)
			: SourceController == nullptr);
		const bool bPosition = Delta.Size2D() < 10.f && FMath::Abs(Delta.Z) < 10.f &&
			(Frame.Phase != Restored || (State->Pawn->GetActorRotation().Equals(TargetRotation, 1.f)
				&& (!Frame.Trajectory || State->Pawn->GetActorScale3D().Equals(Frame.RestoreScale, 0.01f))));
		const bool bRestoredMotion = !Frame.Trajectory || Frame.Phase != Restored ||
			(State->bPeerOrientationRestored && bControllerRestored && Move->MovementMode == Frame.RestoreMode && Move->CustomMovementMode == Frame.RestoreCustom && Move->Velocity.Equals(Frame.RestoreVelocity, 1.f));
		if (bPosition && bClean && bRestoredMotion && Move->IsMovingOnGround() && Move->Velocity.Size() <= 10.f)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (!State->bAckSent && Now - State->StableSince >= 0.5)
			{
				if (Frame.Trajectory && Frame.Phase == Restored) TrajectoryRecord(TEXT("Restored"), FString::Printf(TEXT("\"position\":%s,\"targetPosition\":%s,\"scale\":%s,\"targetScale\":%s,\"rotation\":%s,\"targetRotation\":%s,\"velocity\":%s,\"targetVelocity\":%s,\"mode\":%d,\"targetMode\":%d,\"custom\":%d,\"targetCustom\":%d,\"clean\":1,\"orientationRestored\":1,\"hadController\":%d,\"controller\":\"%s\",\"targetController\":\"%s\",\"controlRotation\":%s,\"targetControlRotation\":%s"),
					*JsonVector(State->Pawn->GetActorLocation()), *JsonVector(Frame.Position), *JsonVector(State->Pawn->GetActorScale3D()), *JsonVector(Frame.RestoreScale),
					*JsonVector(State->Pawn->GetActorRotation().Euler()), *JsonVector(TargetRotation.Euler()), *JsonVector(Move->Velocity), *JsonVector(Frame.RestoreVelocity),
					int32(Move->MovementMode), Frame.RestoreMode, Move->CustomMovementMode, Frame.RestoreCustom, SourceController != nullptr,
					*GetPathNameSafe(SourceController).ReplaceCharWithEscapedChar(), *State->OriginalControllerPath.ReplaceCharWithEscapedChar(),
					*JsonVector(SourceController ? SourceController->GetControlRotation().Euler() : FVector::ZeroVector), *JsonVector(State->OriginalControlRotation.Euler())));
				Report(true, Frame.Phase == Restored ? TEXT("original-position-restored") : TEXT("lane-ready"));
			}
		}
		else State->StableSince = 0;
	}
	else if (Frame.Phase == Armed) Report(true, TEXT("observer-armed"));
	else if (Frame.Phase == RunUp)
	{
		if (State->bOwner)
		{
			if (!Move->IsMovingOnGround()) { Fail(TEXT("setup"), TEXT("run-up-left-ground")); return; }
			State->Pawn->AddMovementInput(Frame.Direction, 1.f);
			if (Move->Velocity.Size2D() >= FMath::Max(300.f, Move->MinSlideSpeed * 2.f)) Report(true, TEXT("owner-speed-ready"));
		}
	}
	else if (Frame.Phase == Execute)
	{
		if (State->bOwner && !State->bExecuted)
		{
			State->bExecuted = true; State->bSlidePressed = true; Input(false, true); Sample(TEXT("activated-owner"));
			if (!Move->IsSliding() || !Active(State->ASC.Get(), State->Slide)) { Fail(TEXT("gameplay"), TEXT("owner-slide-did-not-activate")); return; }
			if (Frame.Case == 2)
			{
				Input(false, false); State->bSlidePressed = false; Sample(TEXT("released-owner"));
				if (Move->MovementMode != MOVE_Walking || Active(State->ASC.Get(), State->Slide) || State->ASC->HasMatchingGameplayTag(State->SlidingTag)) { Fail(TEXT("gameplay"), TEXT("release-did-not-end-owner-slide-walking")); return; }
			}
			if (Frame.Case == 3) { State->bJumpPressed = true; Input(true, true); }
			return;
		}
		if (State->bOwner && Frame.Case == 3 && !State->bJumpProof)
		{
			if (Move->IsFalling())
			{
				Sample(TEXT("immediate-jump-owner"));
				State->bJumpProof = Move->IsSlideJumping() && bClean;
				if (!State->bJumpProof) { Fail(TEXT("gameplay"), TEXT("jump-destination-or-slide-cleanup")); return; }
				Input(true, false); State->bJumpPressed = false;
			}
			else if (Now - State->PhaseStarted > 0.5) { Fail(TEXT("gameplay"), TEXT("jump-did-not-enter-falling")); return; }
		}
		if (Frame.Case == 0 && State->ConfiguredMinSpeed == 0.f && (HasAuthority() || State->bOwner))
		{
			if (State->Entries == 1 && Now - State->EntryAt >= 0.5)
			{
				const auto* Spec = State->ASC->FindAbilitySpecFromHandle(State->Slide);
				const bool bHeld = Move->IsSliding() && Active(State->ASC.Get(), State->Slide) && State->ASC->HasMatchingGameplayTag(State->SlidingTag) &&
					Move->Velocity.Size2D() <= 10.f && State->Activations == 1 && State->Ends == 0 && (!State->bOwner || (State->bSlidePressed && Spec && Spec->InputPressed));
				if (!bHeld) { Fail(TEXT("gameplay"), TEXT("authored-zero-speed-held-slide-ended")); return; }
				if (!State->bAckSent) { Sample(TEXT("authored-held-at-zero")); Report(true, TEXT("authored-held-slide-proved")); }
			}
		}
		else if ((HasAuthority() || State->bOwner) && bClean && bExpectedDestination && State->Activations == 1 && State->Ends == 1 && State->Entries == 1 &&
			(!State->bOwner || Frame.Case != 3 || State->bJumpProof))
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (!State->bAckSent && Now - State->StableSince >= 0.5) { Sample(TEXT("lifecycle-complete")); Report(true, TEXT("entry-activation-end-proved")); }
		}
		else State->StableSince = 0;
	}
	else if (Frame.Phase == ReleaseHeld)
	{
		if (State->bOwner && State->bSlidePressed)
		{
			Input(false, false); State->bSlidePressed = false; Sample(TEXT("released-authored-held-owner"));
			if (Move->MovementMode != MOVE_Walking || Active(State->ASC.Get(), State->Slide) || State->ASC->HasMatchingGameplayTag(State->SlidingTag))
			{ Fail(TEXT("gameplay"), TEXT("authored-held-release-did-not-end-slide")); return; }
			return;
		}
		if ((HasAuthority() || State->bOwner) && bClean && bExpectedDestination && State->Activations == 1 && State->Ends == 1 && State->Entries == 1)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (!State->bAckSent && Now - State->StableSince >= 0.5) { Sample(TEXT("authored-held-lifecycle-complete")); Report(true, TEXT("entry-activation-end-proved")); }
		}
		else State->StableSince = 0;
	}
	else if (Frame.Phase == Settled)
	{
		if (State->bSlidePressed) { Input(false, false); State->bSlidePressed = false; }
		if (bClean && bExpectedDestination && Move->MinSlideSpeed == State->ConfiguredMinSpeed)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (!State->bAckSent && Now - State->StableSince >= 0.5)
			{
				Sample(TEXT("settled")); ++State->CompletedCases;
				UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: PeerCasePASS serial=%d case=%d name=%s peer=%d player=%d thresholdRestored=1 minSlide=%.2f"), Frame.Serial, Frame.Case, CaseName(Frame.Case), Frame.Peer, Frame.Source, Move->MinSlideSpeed);
				Report(true, TEXT("settled-no-slide-tag-or-ability"));
			}
		}
		else State->StableSince = 0;
	}
	if (Now - State->PhaseStarted > 12.0 && !State->bAckSent) Fail(Frame.Phase == Execute || Frame.Phase == ReleaseHeld || Frame.Phase == Settled ? TEXT("gameplay") : TEXT("setup"), TEXT("local-phase-timeout"));
}

void ABwaySlideLifecycleProbe::TrajectoryRecord(const TCHAR* Event, const FString& Fields) const
{
	if (!Frame.Trajectory || !State) return;
	if (Frame.GroundAirDiagnostic)
	{
		GroundAirRecord(Event, Fields);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("BwaySlideTrajectory: {\"schema\":1,\"token\":\"%s\",\"event\":\"%s\",\"serial\":%d,\"phaseId\":%d,\"seq\":%d,\"wallSeconds\":%.6f,\"case\":%d,\"caseName\":\"%s\",\"peer\":%d,\"player\":%d,\"role\":\"%s\",%s}"),
		*Frame.Token.ReplaceCharWithEscapedChar(), Event, Frame.Serial, Frame.Phase, ++State->EventSequence, FPlatformTime::Seconds() - State->Started,
		Frame.Case, CaseName(Frame.Case, true, Frame.ReviewRegression), Frame.Peer, Frame.Source,
		HasAuthority() ? TEXT("authority") : State->bOwner ? TEXT("owner") : TEXT("proxy"), *Fields);
}

void ABwaySlideLifecycleProbe::GroundAirRecord(const TCHAR* Event, const FString& Fields) const
{
	if (!Frame.GroundAirDiagnostic || !State) return;
	UE_LOG(LogTemp, Display, TEXT("BwaySlideGroundAirDiagnostic: {\"schema\":1,\"token\":\"%s\",\"event\":\"%s\",\"serial\":%d,\"phaseId\":%d,\"seq\":%d,\"wallSeconds\":%.6f,\"case\":%d,\"caseName\":\"%s\",\"peer\":%d,\"player\":%d,\"role\":\"%s\",\"diagnosticOnly\":1,\"acceptancePassed\":0,%s}"),
		*Frame.Token.ReplaceCharWithEscapedChar(), Event, Frame.Serial, Frame.Phase, ++State->EventSequence, FPlatformTime::Seconds() - State->Started,
		Frame.Case, CaseName(Frame.Case, true, Frame.ReviewRegression), Frame.Peer, Frame.Source,
		HasAuthority() ? TEXT("authority") : State->bOwner ? TEXT("owner") : TEXT("proxy"), *Fields);
}

void ABwaySlideLifecycleProbe::OnReviewStep(const FBwaySlideReviewObservation& O)
{
	if (!State || !Frame.ReviewRegression || State->bCleaned || State->bFailed || !State->Pawn.IsValid()
		|| Frame.Phase < RunUp || Frame.Phase > ReleaseHeld || (!HasAuthority() && !State->bOwner)) return;
	auto* Pawn = State->Pawn.Get(); auto* Move = Pawn->GetBwayCharacterMovement();
	const bool bReplay = State->bOwner && Pawn->bClientUpdating;
	const float Timestamp = HasAuthority() ? Move->GetPredictionData_Server_Character()->CurrentClientTimeStamp
		: Move->GetPredictionData_Client_Character()->CurrentTimeStamp;
	const auto Reject = [this](const TCHAR* Reason) { if (State->ReviewError.IsEmpty()) State->ReviewError = Reason; };
	if (O.bJump)
	{
		if (!bReplay)
		{
			if (O.bBoostApplied)
			{
				++State->ReviewBoosts; State->ReviewLaunchSpeed = O.AfterVelocity.Size2D();
				if (State->ReviewFirstJumpTime == 0) State->ReviewFirstJumpTime = FPlatformTime::Seconds();
				const float Expected = FMath::Clamp(float(O.BeforeVelocity.Size2D()) * Move->SlideJumpMomentumBoost,
					Move->SlideJumpMinHorizontalSpeed, Move->SlideJumpMaxHorizontalSpeed);
				if (State->ReviewBoosts != 1 || O.bWasJumping || !FMath::IsNearlyEqual(State->ReviewLaunchSpeed, Expected, 0.1f)) Reject(TEXT("review-jump-first-boost-invalid"));
			}
			if (O.bWasJumping)
			{
				// UE's legacy DoJump overload forwards DeltaTime=0. Measure only
				// the observed span between distinct non-replay held-jump moves.
				if (!FMath::IsFinite(Timestamp) || Timestamp < 0.f || Timestamp <= State->ReviewLastHeldTimestamp)
					Reject(TEXT("review-held-jump-timestamp-not-advancing"));
				else
				{
					if (State->ReviewFirstHeldTimestamp < 0.f) State->ReviewFirstHeldTimestamp = Timestamp;
					State->ReviewLastHeldTimestamp = Timestamp;
					++State->ReviewHeldCalls;
					State->ReviewHeldSeconds = Timestamp - State->ReviewFirstHeldTimestamp;
				}
				if (O.bBoostApplied || State->ReviewBoosts != 1 || O.AfterVelocity.Size2D() > State->ReviewLaunchSpeed + 1.f)
					Reject(TEXT("review-held-jump-compounded"));
			}
		}
		TrajectoryRecord(TEXT("ReviewJump"), FString::Printf(TEXT("\"moveTs\":%.7f,\"dt\":%.7f,\"replay\":%d,\"wasJumping\":%d,\"boostApplied\":%d,\"pressed\":%d,\"jumpCount\":%d,\"beforeSpeed\":%.6f,\"afterSpeed\":%.6f,\"boost\":%.6f,\"minSpeed\":%.6f,\"maxSpeed\":%.6f"),
			Timestamp, O.DeltaSeconds, bReplay, O.bWasJumping, O.bBoostApplied, Pawn->bPressedJump, Pawn->JumpCurrentCount,
			O.BeforeVelocity.Size2D(), O.AfterVelocity.Size2D(), Move->SlideJumpMomentumBoost, Move->SlideJumpMinHorizontalSpeed, Move->SlideJumpMaxHorizontalSpeed));
	}
	else
	{
		const float FirstRemaining = 1.f - O.FirstHitFraction;
		const float Budget = O.FirstHitFraction + FirstRemaining * O.SlideConsumedFraction + O.ContinuationFraction;
		const float LegacyBudget = O.FirstHitFraction + FirstRemaining * O.SlideConsumedFraction + FirstRemaining * (1.f - O.FinalHitFraction);
		const bool bDecisive = LegacyBudget > 1.001f && O.SlideConsumedFraction > O.FinalHitFraction + 0.001f;
		const float Distance = FVector::Dist2D(O.Start, O.End), Bound = O.BeforeVelocity.Size() * O.DeltaSeconds + 1.f;
		if (!bReplay)
		{
			++State->ReviewConcaveSteps; if (bDecisive) ++State->ReviewDecisiveSteps;
			if (!FMath::IsFinite(Budget) || Budget > 1.0001f || O.ContinuationFraction < -KINDA_SMALL_NUMBER || Distance > Bound)
				Reject(TEXT("review-concave-time-or-distance-overrun"));
		}
		TrajectoryRecord(TEXT("ReviewConcave"), FString::Printf(TEXT("\"moveTs\":%.7f,\"dt\":%.7f,\"replay\":%d,\"firstHit\":%.7f,\"slideConsumed\":%.7f,\"finalHit\":%.7f,\"continuation\":%.7f,\"budget\":%.7f,\"legacyBudget\":%.7f,\"decisive\":%d,\"distance2D\":%.6f,\"distanceBound\":%.6f,\"beforeVelocity\":%s,\"start\":%s,\"end\":%s,\"firstNormal\":%s,\"finalNormal\":%s"),
			Timestamp, O.DeltaSeconds, bReplay, O.FirstHitFraction, O.SlideConsumedFraction, O.FinalHitFraction, O.ContinuationFraction,
			Budget, LegacyBudget, bDecisive, Distance, Bound, *JsonVector(O.BeforeVelocity), *JsonVector(O.Start), *JsonVector(O.End), *JsonVector(O.FirstNormal), *JsonVector(O.FinalNormal)));
	}
}

void ABwaySlideLifecycleProbe::ReviewMovementSample(float DeltaSeconds, const FVector& OldPosition, const FVector& OldVelocity)
{
	if (!State || State->bCleaned || State->bFailed || !State->Pawn.IsValid() || Frame.Phase < RunUp || Frame.Phase > Settled
		|| (HasAuthority() && !State->bCoordinator)) return;
	auto* Pawn = State->Pawn.Get(); auto* Move = Pawn->GetBwayCharacterMovement();
	const double Now = FPlatformTime::Seconds();
	if (!HasAuthority() && !State->bOwner && Now - State->LastProxySample < 0.1) return;
	State->LastProxySample = Now;
	const bool bReplay = State->bOwner && Pawn->bClientUpdating;
	const float Timestamp = HasAuthority() ? Move->GetPredictionData_Server_Character()->CurrentClientTimeStamp
		: State->bOwner ? Move->GetPredictionData_Client_Character()->CurrentTimeStamp : -1.f;
	if (++State->TrajectorySamples > 2000 || DeltaSeconds <= 0 || !FMath::IsFinite(DeltaSeconds)
		|| Pawn->GetActorLocation().ContainsNaN() || Move->Velocity.ContainsNaN())
	{ Fail(TEXT("gameplay"), TEXT("invalid-review-movement-sample")); return; }
	const bool bPenetrating = CapsulePenetrating(Pawn);
	if (bPenetrating) { Fail(TEXT("gameplay"), TEXT("review-capsule-penetration")); return; }
	TrajectoryRecord(TEXT("ReviewSample"), FString::Printf(TEXT("\"moveTs\":%.7f,\"dt\":%.7f,\"replay\":%d,\"position\":%s,\"velocity\":%s,\"oldPosition\":%s,\"oldVelocity\":%s,\"mode\":%d,\"custom\":%d,\"sliding\":%d,\"slideJump\":%d,\"pressed\":%d,\"wasJumping\":%d,\"jumpCount\":%d,\"penetrating\":%d"),
		Timestamp, DeltaSeconds, bReplay, *JsonVector(Pawn->GetActorLocation()), *JsonVector(Move->Velocity), *JsonVector(OldPosition), *JsonVector(OldVelocity),
		int32(Move->MovementMode), Move->CustomMovementMode, Move->IsSliding(), Move->IsSlideJumping(), Pawn->bPressedJump, Pawn->bWasJumping, Pawn->JumpCurrentCount, bPenetrating));
}

void ABwaySlideLifecycleProbe::ObserveReviewRegression()
{
	const double Now = FPlatformTime::Seconds();
	auto* Pawn = State->Pawn.Get(); auto* Move = Pawn->GetBwayCharacterMovement();
	const bool bExecutor = HasAuthority() || State->bOwner;
	const bool bClean = !Move->IsSliding() && !Active(State->ASC.Get(), State->Slide) && !State->ASC->HasMatchingGameplayTag(State->SlidingTag);
	if (!State->ReviewError.IsEmpty()) { Fail(TEXT("gameplay"), *State->ReviewError); return; }
	if (SlideTuning(Move) != State->AuthoredTuning || Pawn->JumpMaxHoldTime != 0.3f || !State->Driver.IsValid()
		|| State->Driver->PacketSimulationSettings.PktLag != 100)
	{ Fail(TEXT("setup"), TEXT("review-tuning-or-network-settings-changed")); return; }
	if (Frame.Phase == Armed)
	{
		if (State->ReviewCourse.Num() != 4 || CapsulePenetrating(Pawn) || !Move->CurrentFloor.IsWalkableFloor())
		{ Fail(TEXT("setup"), TEXT("review-course-or-capsule-invalid")); return; }
		Report(true, TEXT("review-observer-armed"));
	}
	else if (Frame.Phase == RunUp && State->bOwner)
	{
		Pawn->AddMovementInput(Frame.Direction, 1.f);
		if (Move->Velocity.Size2D() >= 300.f) Report(true, TEXT("review-owner-speed-ready"));
	}
	else if (Frame.Phase == Execute)
	{
		if (State->bOwner && !State->bExecuted)
		{
			State->bExecuted = true; State->bSlidePressed = true; Input(false, true);
			if (!Move->IsSliding()) { Fail(TEXT("gameplay"), TEXT("review-slide-activation-failed")); return; }
		}
		if (Frame.Case == 0)
		{
			if (State->bOwner && State->Entries == 1 && !State->bTerminalIssued && Now - State->EntryAt >= 0.1)
			{ State->bTerminalIssued = true; State->bJumpPressed = true; Input(true, true); }
			if (bExecutor && Move->IsFalling()) State->bAirborne = true;
			if (State->bOwner && State->bAirborne && State->bSlidePressed) { Input(false, false); State->bSlidePressed = false; }
			if (State->bOwner && State->ReviewFirstJumpTime > 0 && Now - State->ReviewFirstJumpTime >= 0.25 && State->bJumpPressed)
			{ Input(true, false); State->bJumpPressed = false; }
			if (bExecutor && State->bAirborne && State->ReviewBoosts == 1 && State->ReviewHeldCalls >= 3
				&& State->ReviewHeldSeconds >= 0.08 && !Pawn->bPressedJump && bClean)
				Report(true, TEXT("review-held-input-momentum-proved"));
		}
		else if (bExecutor && State->ReviewConcaveSteps > 0 && State->ReviewDecisiveSteps > 0)
			Report(true, TEXT("review-cumulative-contact-time-proved"));
	}
	else if (Frame.Phase == ReleaseHeld)
	{
		if (State->bOwner && State->bSlidePressed) { Input(false, false); State->bSlidePressed = false; }
		if (bExecutor && bClean && Move->IsMovingOnGround() && Move->Velocity.Size() <= 10.f && State->Activations == 1 && State->Ends == 1 && State->Entries == 1)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (!State->bAckSent && Now - State->StableSince >= 0.5)
			{
				TrajectoryRecord(TEXT("ReviewProof"), FString::Printf(TEXT("\"boosts\":%d,\"heldCalls\":%d,\"heldSeconds\":%.7f,\"concaveSteps\":%d,\"decisiveSteps\":%d,\"activations\":%d,\"ends\":%d,\"entries\":%d,\"position\":%s"),
					State->ReviewBoosts, State->ReviewHeldCalls, State->ReviewHeldSeconds, State->ReviewConcaveSteps, State->ReviewDecisiveSteps,
					State->Activations, State->Ends, State->Entries, *JsonVector(Pawn->GetActorLocation())));
				Report(true, TEXT("review-terminal-lifecycle-proved"));
			}
		}
		else State->StableSince = 0;
	}
	else if (Frame.Phase == Settled)
	{
		const bool bSettled = bClean && !Pawn->bPressedJump && Move->IsMovingOnGround() && Move->Velocity.Size() <= 10.f
			&& FVector::Dist(Pawn->GetActorLocation(), Frame.Position) <= 10.f && !CapsulePenetrating(Pawn);
		if (bSettled)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (!State->bAckSent && Now - State->StableSince >= 0.5)
			{
				TrajectoryRecord(TEXT("Settled"), FString::Printf(TEXT("\"position\":%s,\"targetPosition\":%s,\"velocity\":%s,\"clean\":1,\"supported\":1,\"penetrating\":0"),
					*JsonVector(Pawn->GetActorLocation()), *JsonVector(Frame.Position), *JsonVector(Move->Velocity)));
				++State->CompletedCases;
				UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: PeerCasePASS serial=%d case=%d name=%s peer=%d player=%d thresholdRestored=1 minSlide=%.2f"),
					Frame.Serial, Frame.Case, CaseName(Frame.Case, true, true), Frame.Peer, Frame.Source, Move->MinSlideSpeed);
				Report(true, TEXT("review-owner-authority-proxy-settled"));
			}
		}
		else State->StableSince = 0;
	}
	if (Now - State->PhaseStarted > 12.0 && !State->bAckSent) Fail(TEXT("gameplay"), TEXT("review-phase-or-required-branch-timeout"));
}

void ABwaySlideLifecycleProbe::OnTrajectoryMovement(float DeltaSeconds, FVector OldPosition, FVector OldVelocity)
{
	if (Frame.ReviewRegression) { ReviewMovementSample(DeltaSeconds, OldPosition, OldVelocity); return; }
	if (!Frame.Trajectory || !State || State->bCleaned || State->bFailed || !State->Pawn.IsValid()
		|| Frame.Phase < RunUp || Frame.Phase > Settled || (Frame.Phase == Settled && State->bAckSent) || (HasAuthority() && !State->bCoordinator)) return;
	const double Now = FPlatformTime::Seconds();
	if (!HasAuthority() && !State->bOwner && Now - State->LastProxySample < 0.1) return;
	State->LastProxySample = Now;
	if (Frame.SupportDiagnostic && State->SupportRejectedAt > 0 && Now - State->SupportRejectedAt >= SupportDiagnosticSeconds)
	{ Fail(TEXT("diagnostic"), TEXT("support-diagnostic-window-ended")); return; }
	auto* Pawn = State->Pawn.Get(); auto* Move = Pawn->GetBwayCharacterMovement();
	const bool bReplay = State->bOwner && Pawn->bClientUpdating;
	float Timestamp = -1.f;
	if (HasAuthority()) Timestamp = Move->GetPredictionData_Server_Character()->CurrentClientTimeStamp;
	else if (State->bOwner && !bReplay) Timestamp = Move->GetPredictionData_Client_Character()->CurrentTimeStamp;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f || Pawn->GetActorLocation().ContainsNaN() || Move->Velocity.ContainsNaN())
	{ Fail(TEXT("gameplay"), TEXT("invalid-trajectory-sample")); return; }
	if (++State->TrajectorySamples > 2000) { Fail(TEXT("gameplay"), TEXT("trajectory-sample-limit")); return; }
	const FFindFloorResult& Floor = Move->CurrentFloor;
	const TCHAR* Geometry = GeometryPhase(Frame.Case, Floor);
	const bool bAdvanced = (HasAuthority() || State->bOwner) && !bReplay && Timestamp > State->LastMoveTimestamp;
	const bool bCapsuleOverlap = CapsulePenetrating(Pawn);
	const bool bPenetrating = Floor.HitResult.bStartPenetrating || bCapsuleOverlap;
	const bool bCustomSupport = HasTrajectorySlideSupport(Floor);
	const FVector Location = Pawn->GetActorLocation();
	const bool bWithinRoute = Frame.Case < 2 ? Location.X >= -710.f && Location.X <= 710.f && FMath::Abs(Location.Y - 2000.f) <= 25.f
		: Location.Y >= -3710.f && Location.Y <= -1490.f && FMath::Abs(Location.X) <= 25.f;
	if (!bWithinRoute) { Fail(TEXT("setup"), TEXT("trajectory-left-verified-route-envelope")); return; }
	const FString FloorComponent = GetPathNameSafe(Floor.HitResult.GetComponent());
	if (Frame.GroundAirDiagnostic && Floor.HitResult.GetComponent()
		&& (State->GroundAirExpectedComponent.IsEmpty() || State->GroundAirRouteComponents.Contains(FloorComponent)))
	{
		State->GroundAirExpectedComponent = FloorComponent;
	}
	const bool bGroundAirFloorValid = Floor.bBlockingHit && Floor.HitResult.IsValidBlockingHit() && Floor.HitResult.GetComponent()
		&& FMath::IsFinite(Floor.GetDistanceToFloor());
	// A slide jump intentionally ends the GAS slide state and enters MOVE_Falling,
	// but the production movement component preserves IsSlideJumping until landing.
	// Keep that valid airborne interval separate from held-slide support so the
	// diagnostic does not misclassify a legitimate jump as invalid state. This is
	// observer classification only; it does not alter movement or acceptance.
	const bool bSlideAbilityOrProxyActive = (HasAuthority() || State->bOwner) ? Active(State->ASC.Get(), State->Slide) : true;
	const bool bGroundAirHeldSlide = Move->IsSliding() && bSlideAbilityOrProxyActive
		&& State->ASC->GetTagCount(State->SlidingTag) > 0;
	const bool bGroundAirSlideJump = Move->IsSlideJumping() && Move->MovementMode == MOVE_Falling && Move->CustomMovementMode == 0
		&& !bGroundAirHeldSlide && !Move->IsSliding() && !Active(State->ASC.Get(), State->Slide)
		&& State->ASC->GetTagCount(State->SlidingTag) == 0 && (!Floor.bBlockingHit || Floor.GetDistanceToFloor() > 15.f);
	const bool bGroundAirStateActive = bGroundAirHeldSlide || bGroundAirSlideJump;
	const bool bGroundAirComponentMatches = bGroundAirFloorValid && !State->GroundAirExpectedComponent.IsEmpty()
		&& FloorComponent == State->GroundAirExpectedComponent;
	const bool bGroundAirContactNearGround = bGroundAirFloorValid && !bPenetrating
		&& Floor.GetDistanceToFloor() <= 15.f;
	const bool bGroundAirGroundCoverage = Frame.GroundAirDiagnostic && bGroundAirContactNearGround && bCustomSupport
		&& bGroundAirComponentMatches && bWithinRoute && bGroundAirHeldSlide;
	const bool bGroundAirNearGroundSample = bGroundAirGroundCoverage
		|| (Frame.GroundAirDiagnostic && !bGroundAirHeldSlide && !bGroundAirSlideJump && bGroundAirContactNearGround && bWithinRoute);
	// Preserve the original bounded-separated classification for a held slide
	// whose floor is still valid but outside the walkable support distance. A
	// held slide that loses real support remains invalid. Slide-jump flight has a
	// separate diagnostic class below because it intentionally has no active GAS
	// slide state or floor hit while the production movement component is falling.
	const bool bGroundAirBoundedAir = Frame.GroundAirDiagnostic && bGroundAirFloorValid && !bGroundAirContactNearGround
		&& bGroundAirComponentMatches && !bPenetrating && bWithinRoute && bGroundAirHeldSlide;
	const bool bGroundAirSlideJumpAirborne = Frame.GroundAirDiagnostic && ((Frame.Case % 2) == 1)
		&& bGroundAirSlideJump && !bPenetrating && bWithinRoute;
	const bool bGroundAirInvalid = Frame.GroundAirDiagnostic && !bGroundAirNearGroundSample && !bGroundAirBoundedAir
		&& !bGroundAirSlideJumpAirborne;
	if (Frame.GroundAirDiagnostic && !HasAuthority() && !State->bOwner && bGroundAirInvalid)
	{
		--State->TrajectorySamples;
		return;
	}
	if (bAdvanced)
	{
		State->LastMoveTimestamp = Timestamp;
		if (Frame.Phase == Execute && Move->IsSliding())
		{
			if (!bCustomSupport || bPenetrating || (Frame.Case < 2 && Floor.HitResult.ImpactNormal.Z < 0.995f))
			{
				// Fail immediately cleans up input and abilities. Preserve the rejected
				// physical sample first so its precise contact failure is not erased.
				UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: MovementRejected token=%s serial=%d case=%d phase=%d peer=%d player=%d reason=held-slide-lost-real-support ts=%.6f dt=%.7f replay=%d timestampAdvanced=%d sampleIndex=%d position=%s velocity=%s mode=%d custom=%d sliding=%d active=%d tagCount=%d slideJump=%d geometry=%s floorBlocking=%d floorHitValid=%d floorWalkable=%d floorDistance=%.6f floorSweepDistance=%.6f floorLineDistance=%.6f floorLineTrace=%d floorNormal=%s floorImpact=%s floorComponent=%s floorPenetrating=%d customSupport=%d capsuleOverlap=%d penetrating=%d"),
					*Frame.Token, Frame.Serial, Frame.Case, Frame.Phase, Frame.Peer, Frame.Source, Timestamp, DeltaSeconds, bReplay, bAdvanced, State->TrajectorySamples,
					*JsonVector(Location), *JsonVector(Move->Velocity), int32(Move->MovementMode), Move->CustomMovementMode, Move->IsSliding(),
					Active(State->ASC.Get(), State->Slide), State->ASC->GetTagCount(State->SlidingTag), Move->IsSlideJumping(), Geometry,
					Floor.bBlockingHit, Floor.HitResult.IsValidBlockingHit(), Floor.IsWalkableFloor(), Floor.GetDistanceToFloor(), Floor.FloorDist, Floor.LineDist, Floor.bLineTrace,
					*JsonVector(Floor.HitResult.ImpactNormal), *JsonVector(Floor.HitResult.ImpactPoint), *GetPathNameSafe(Floor.HitResult.GetComponent()),
					Floor.HitResult.bStartPenetrating, bCustomSupport, bCapsuleOverlap, bPenetrating);
				// An explicit diagnostic may observe only this already-rejected distance
				// gap. All collision/state limits and ordinary acceptance remain strict.
				const bool bDistanceOnly = Frame.Case >= 2 && !bPenetrating && Floor.bBlockingHit && Floor.HitResult.IsValidBlockingHit()
					&& Floor.HitResult.GetComponent() && Floor.HitResult.ImpactNormal.Z >= 0.80f
					&& FMath::IsFinite(Floor.GetDistanceToFloor()) && Floor.GetDistanceToFloor() > 15.f;
				if ((!Frame.SupportDiagnostic && !Frame.GroundAirDiagnostic) || !bDistanceOnly)
				{ Fail(TEXT("gameplay"), TEXT("held-slide-lost-real-support")); return; }
				if (Frame.SupportDiagnostic && State->SupportRejectedAt == 0)
				{
					State->SupportRejectedAt = Now;
					UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL peer=%d case=%d phase=%d player=%d category=diagnostic reason=held-slide-lost-real-support token=%s failureLatched=1 acceptance=0 continuationSeconds=%.1f wallSeconds=%.6f"),
						Frame.Peer, Frame.Case, Frame.Phase, Frame.Source, *Frame.Token, SupportDiagnosticSeconds, Now - State->Started);
				}
			}
			// Combined client moves re-simulate a pending interval, so summing callback
			// deltas overcounts held time. Count only the unique client timestamp span.
			if (State->SupportRejectedAt == 0)
			{
				if (State->FirstSlideTimestamp < 0) State->FirstSlideTimestamp = Timestamp;
				State->LastSlideTimestamp = Timestamp;
				State->SlideSeconds = Timestamp - State->FirstSlideTimestamp;
				if (FCString::Strcmp(Geometry, TEXT("ramp1")) == 0) State->GeometryMask |= 1;
				if (FCString::Strcmp(Geometry, TEXT("middle")) == 0 && (State->GeometryMask & 1)) State->GeometryMask |= 2;
				if (FCString::Strcmp(Geometry, TEXT("ramp2")) == 0 && (State->GeometryMask & 2)) State->GeometryMask |= 4;
			}
		}
	}
	const bool bProxySample = !HasAuthority() && !State->bOwner;
	if (Frame.GroundAirDiagnostic && Frame.Phase == Execute && (bAdvanced || bProxySample))
	{
		const double SampleTime = bProxySample ? Now : double(Timestamp);
		if (bGroundAirStateActive && State->GroundAirStartedAt == 0) State->GroundAirStartedAt = Now;
		if (bGroundAirGroundCoverage)
		{
			if (State->bGroundAirGroundContiguous && State->GroundAirLastGroundTimestamp >= 0 && SampleTime > State->GroundAirLastGroundTimestamp)
			{
				++State->GroundAirGroundSamples;
				State->GroundAirGroundTravel += FVector::Dist2D(Location, State->GroundAirLastGroundPosition);
			}
			else
			{
				State->GroundAirGroundSamples = 1;
				State->GroundAirGroundTravel = 0;
			}
			State->bGroundAirGroundContiguous = true;
			State->GroundAirLastGroundTimestamp = SampleTime;
			State->GroundAirLastGroundPosition = Location;
			if (FCString::Strcmp(Geometry, TEXT("ramp1")) == 0) State->GroundAirCoverageMask |= 1;
			if (FCString::Strcmp(Geometry, TEXT("middle")) == 0 && (State->GroundAirCoverageMask & 1)) State->GroundAirCoverageMask |= 2;
			if (FCString::Strcmp(Geometry, TEXT("ramp2")) == 0 && (State->GroundAirCoverageMask & 2)) State->GroundAirCoverageMask |= 4;
		}
		else
		{
			State->bGroundAirGroundContiguous = false;
			State->GroundAirLastGroundTimestamp = -1;
		}
		if (bGroundAirBoundedAir || bGroundAirSlideJumpAirborne)
		{
			State->bGroundAirSawAir = true;
			++State->GroundAirAirSamples;
			if (bGroundAirSlideJumpAirborne) ++State->GroundAirSlideJumpAirSamples;
		}
		if (bGroundAirNearGroundSample && State->bGroundAirSawAir && !State->bGroundAirSawReturn)
		{
			State->bGroundAirSawReturn = true; State->GroundAirReturnAt = Now;
		}
		if (bGroundAirInvalid) ++State->GroundAirInvalidSamples;
	}
	FString ContactDiagnostic;
	if (Frame.SupportDiagnostic)
	{
		const auto* Capsule = Pawn->GetCapsuleComponent();
		const FVector Center = Capsule->GetComponentLocation();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SlideContactDiagnostic), false, Pawn);
		FCollisionResponseParams Responses;
		Capsule->InitSweepCollisionParams(Query, Responses);
		const bool bOldStateFinite = !OldPosition.ContainsNaN() && !OldVelocity.ContainsNaN();
		const bool bMatchesCapsule = Move->UpdatedComponent == Capsule;
		const bool bSweepPerformed = bOldStateFinite && bMatchesCapsule && Capsule->IsQueryCollisionEnabled();
		FHitResult IndependentHit;
		bool bSweepBlocking = false;
		if (bSweepPerformed)
		{
			// Read-only chord query at the post-step orientation, using the capsule's
			// movement filters. This is NOT the CMC's swept/deflected impact history.
			bSweepBlocking = GetWorld()->SweepSingleByChannel(IndependentHit, OldPosition, Center, Capsule->GetComponentQuat(),
				Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query, Responses);
		}
		ContactDiagnostic = FString::Printf(TEXT(",\"contactDiagnostic\":{\"schema\":1,\"callbackOldStateFinite\":%d,\"callbackOldPosition\":%s,\"callbackOldVelocity\":%s,\"capsuleCenter\":%s,\"updatedComponentMatchesCapsule\":%d,\"capsuleQuery\":%s,\"floor\":%s,\"bounds\":%s,\"independentSweep\":{\"kind\":\"post-step-old-to-new-straight-capsule\",\"productionImpactHistory\":0,\"filterSource\":\"InitSweepCollisionParams-plus-self-ignore\",\"performed\":%d,\"blocking\":%d,\"start\":%s,\"end\":%s,\"hit\":%s}}"),
			bOldStateFinite, *(bOldStateFinite ? JsonVector(OldPosition) : TEXT("null")), *(bOldStateFinite ? JsonVector(OldVelocity) : TEXT("null")),
			*JsonVector(Center), bMatchesCapsule, *ContactCapsuleQueryJson(Capsule, Query, Responses), *ContactFloorJson(Center, Capsule, Floor), *ContactBoundsJson(Move),
			bSweepPerformed, bSweepBlocking, *(bOldStateFinite ? JsonVector(OldPosition) : TEXT("null")), *JsonVector(Center), *ContactHitJson(IndependentHit));
	}
	FString GroundAirFields;
	if (Frame.GroundAirDiagnostic)
	{
		const TCHAR* Classification = bGroundAirSlideJumpAirborne ? TEXT("slideJumpAirborne") : bGroundAirInvalid ? TEXT("invalidState") : bGroundAirNearGroundSample ? TEXT("contactNearGround") : TEXT("boundedSeparatedTraversal");
		GroundAirFields = FString::Printf(TEXT(",\"groundAirClass\":\"%s\",\"contactNearGround\":%d,\"boundedSeparatedTraversal\":%d,\"slideJumpAirborne\":%d,\"invalidState\":%d,\"expectedComponent\":\"%s\",\"componentMatches\":%d,\"heldSlideState\":%d,\"airborneSlideJump\":%d,\"groundCoverageMask\":%d,\"groundSamples\":%d,\"groundTangentTravel\":%.6f,\"airSamples\":%d,\"slideJumpAirSamples\":%d,\"invalidSamples\":%d,\"sawAir\":%d,\"sawReturn\":%d"),
			Classification, bGroundAirNearGroundSample, bGroundAirBoundedAir, bGroundAirSlideJumpAirborne, bGroundAirInvalid,
			*State->GroundAirExpectedComponent.ReplaceCharWithEscapedChar(), bGroundAirComponentMatches, bGroundAirHeldSlide, bGroundAirSlideJump,
			State->GroundAirCoverageMask, State->GroundAirGroundSamples, State->GroundAirGroundTravel,
			State->GroundAirAirSamples, State->GroundAirSlideJumpAirSamples, State->GroundAirInvalidSamples, State->bGroundAirSawAir, State->bGroundAirSawReturn);
	}
	TrajectoryRecord(TEXT("Sample"), FString::Printf(TEXT("\"ts\":%.6f,\"dt\":%.7f,\"replay\":%d,\"timestampValid\":%d,\"sampleIndex\":%d,\"moveTimestampAdvanced\":%d,\"slideSeconds\":%.6f,\"position\":%s,\"velocity\":%s,\"floorNormal\":%s,\"floorComponent\":\"%s\",\"geometryPhase\":\"%s\",\"mode\":%d,\"custom\":%d,\"sliding\":%d,\"active\":%d,\"tagCount\":%d,\"slideJump\":%d,\"supported\":%d,\"walkable\":%d,\"floorBlocking\":%d,\"floorDistance\":%.6f,\"customSupport\":%d,\"penetrating\":%d,\"capsuleOverlap\":%d,\"frameId\":%llu,\"frameDt\":%.7f%s%s"),
		Timestamp, DeltaSeconds, bReplay, Timestamp > 0.f && !bReplay, State->TrajectorySamples, bAdvanced, State->SlideSeconds,
		*JsonVector(Pawn->GetActorLocation()), *JsonVector(Move->Velocity), *JsonVector(Floor.HitResult.ImpactNormal),
		*FloorComponent.ReplaceCharWithEscapedChar(), Geometry, int32(Move->MovementMode), Move->CustomMovementMode, Move->IsSliding(),
		Active(State->ASC.Get(), State->Slide), State->ASC->GetTagCount(State->SlidingTag), Move->IsSlideJumping(), Floor.IsWalkableFloor(), Floor.IsWalkableFloor(),
		Floor.bBlockingHit, Floor.GetDistanceToFloor(), bCustomSupport, bPenetrating, bCapsuleOverlap,
		GFrameCounter, GetWorld()->GetDeltaSeconds(), *ContactDiagnostic, *GroundAirFields));
}

bool ABwaySlideLifecycleProbe::ValidateTrajectoryGeometry(FVector& OutStart, FVector& OutDirection) const
{
	const auto* Pawn = State->Pawn.Get();
	if (!Pawn) return false;
	const auto* Capsule = Pawn->GetCapsuleComponent();
	const auto* Move = Pawn->GetBwayCharacterMovement();
	const float Radius = Capsule->GetScaledCapsuleRadius(), Half = Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SlideTrajectoryGeometry), false, Pawn);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const bool bRamp = Frame.Case >= 2;
	OutDirection = bRamp ? FVector(0, 1, 0) : FVector(1, 0, 0);
	FVector Previous = FVector::ZeroVector;
	int32 GeometryBits = 0;
	const int32 Count = bRamp ? 89 : 57; // 25cm samples; ramp includes lower landing/runout.
	const auto Reject = [this, Capsule, Move](const TCHAR* Reason, int32 Index, const FVector& XY, const FVector& Center,
		const FHitResult& Hit, const FFindFloorResult& Support)
	{
		// Ordinary acceptance emits only the causal rejection. The opt-in support
		// diagnostic also exports successful geometry without weakening a query.
		UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: GeometryRejected token=%s peer=%d case=%d reason=%s index=%d xy=%s center=%s hitActor=%s hitComponent=%s hitBlocking=%d hitPenetrating=%d hitTime=%.6f impactPoint=%s impactNormal=%s floorBlocking=%d floorWalkable=%d floorPenetrating=%d floorDist=%.6f floorActor=%s floorComponent=%s floorNormal=%s capsuleRadius=%.6f capsuleHalfHeight=%.6f collisionChannel=%d walkableFloorAngle=%.6f queryCollision=%d"),
			*Frame.Token, Frame.Peer, Frame.Case, Reason, Index, *JsonVector(XY), *JsonVector(Center),
			*GetPathNameSafe(Hit.GetActor()), *GetPathNameSafe(Hit.GetComponent()), Hit.bBlockingHit, Hit.bStartPenetrating, Hit.Time,
			*JsonVector(Hit.ImpactPoint), *JsonVector(Hit.ImpactNormal), Support.bBlockingHit, Support.IsWalkableFloor(), Support.HitResult.bStartPenetrating,
			Support.FloorDist, *GetPathNameSafe(Support.HitResult.GetActor()), *GetPathNameSafe(Support.HitResult.GetComponent()),
			*JsonVector(Support.HitResult.ImpactNormal), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(),
			int32(Capsule->GetCollisionObjectType()), Move->GetWalkableFloorAngle(), Capsule->IsQueryCollisionEnabled());
		return false;
	};
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector XY = bRamp ? FVector(0, -3700.f + Index * 25.f, 0) : FVector(-700.f + Index * 25.f, 2000, 0);
		FHitResult Surface;
		FFindFloorResult Support;
		if (!GetWorld()->LineTraceSingleByChannel(Surface, XY + FVector(0, 0, 1500), XY - FVector(0, 0, 1000), ECC_Visibility, Query))
			return Reject(TEXT("surface-trace-miss"), Index, XY, FVector::ZeroVector, Surface, Support);
		if (Surface.ImpactNormal.Z < 0.80f)
			return Reject(TEXT("surface-normal-too-steep"), Index, XY, FVector::ZeroVector, Surface, Support);
		const FVector Center = Surface.ImpactPoint + FVector(0, 0, Half - Radius + Radius / Surface.ImpactNormal.Z + 5.f);
		// FindFloor clears bForceNextFloorCheck; the public test wrapper preserves
		// that movement flag while executing the same uncached collision query.
		Move->K2_FindFloor(Center, Support);
		if (!Support.IsWalkableFloor() || Support.HitResult.bStartPenetrating || Support.FloorDist > 15.f)
			return Reject(TEXT("floor-not-supported-clear-and-near"), Index, XY, Center, Surface, Support);
		if (Support.HitResult.GetComponent())
			State->GroundAirRouteComponents.Add(GetPathNameSafe(Support.HitResult.GetComponent()));
		TArray<FOverlapResult> Overlaps;
		if (GetWorld()->OverlapMultiByChannel(Overlaps, Center, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Radius, Half), Query, Responses))
		{
			int32 Reported = 0;
			for (const auto& Overlap : Overlaps)
				if (Overlap.bBlockingHit && Reported++ < 4)
					UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: GeometryBlockingOverlap token=%s peer=%d case=%d index=%d actor=%s component=%s"),
						*Frame.Token, Frame.Peer, Frame.Case, Index, *GetPathNameSafe(Overlap.GetActor()), *GetPathNameSafe(Overlap.GetComponent()));
			return Reject(TEXT("capsule-blocking-overlap"), Index, XY, Center, Surface, Support);
		}
		FHitResult Obstacle;
		const FVector ConnectingStart = Previous + FVector(0, 0, 10), ConnectingEnd = Center + FVector(0, 0, 10);
		if (Index > 0)
		{
			if (GetWorld()->SweepSingleByChannel(Obstacle, ConnectingStart, ConnectingEnd, Capsule->GetComponentQuat(),
				Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Radius, Half), Query, Responses))
				return Reject(TEXT("connecting-capsule-sweep-blocked"), Index, XY, Center, Obstacle, Support);
		}
		const TCHAR* Geometry = GeometryPhase(Frame.Case, Support);
		if (FCString::Strcmp(Geometry, TEXT("ramp1")) == 0) GeometryBits |= 1;
		if (FCString::Strcmp(Geometry, TEXT("middle")) == 0) GeometryBits |= 2;
		if (FCString::Strcmp(Geometry, TEXT("ramp2")) == 0) GeometryBits |= 4;
		if ((!bRamp && Index == 0) || (bRamp && Index == 6)) OutStart = Center; // Upper plateau y=-3550.
		if (!bRamp && Support.HitResult.ImpactNormal.Z < 0.995f)
			return Reject(TEXT("flat-floor-normal-invalid"), Index, XY, Center, Surface, Support);
		if (Frame.SupportDiagnostic)
		{
			// Separate diagnostic namespace: do not add acceptance events or change
			// their sequence/count. Every successful existing capsule sample is kept.
			UE_LOG(LogTemp, Display, TEXT("BwaySlideContactDiagnostic: {\"schema\":1,\"kind\":\"GeometrySample\",\"token\":\"%s\",\"serial\":%d,\"phaseId\":%d,\"case\":%d,\"peer\":%d,\"player\":%d,\"role\":\"%s\",\"index\":%d,\"count\":%d,\"xy\":%s,\"center\":%s,\"geometryPhase\":\"%s\",\"capsuleQuery\":%s,\"filterSource\":\"existing-preflight-self-ignore\",\"bounds\":%s,\"surfaceQuery\":{\"kind\":\"vertical-visibility-line\",\"channel\":%d,\"start\":%s,\"end\":%s,\"hit\":%s,\"plane\":%s},\"floorQuery\":{\"kind\":\"K2_FindFloor-uncached\",\"center\":%s,\"result\":%s},\"capsuleBlockingOverlap\":0,\"connectingSweep\":{\"performed\":%d,\"blocking\":0,\"verticalOffset\":10,\"start\":%s,\"end\":%s,\"hit\":%s}}"),
				*Frame.Token.ReplaceCharWithEscapedChar(), Frame.Serial, Frame.Phase, Frame.Case, Frame.Peer, Frame.Source,
				HasAuthority() ? TEXT("authority") : State->bOwner ? TEXT("owner") : TEXT("proxy"), Index, Count,
				*JsonVector(XY), *JsonVector(Center), Geometry, *ContactCapsuleQueryJson(Capsule, Query, Responses), *ContactBoundsJson(Move), int32(ECC_Visibility),
				*JsonVector(XY + FVector(0, 0, 1500)), *JsonVector(XY - FVector(0, 0, 1000)), *ContactHitJson(Surface), *ContactPlaneJson(Center, Capsule, Surface),
				*JsonVector(Center), *ContactFloorJson(Center, Capsule, Support), Index > 0, *JsonVector(ConnectingStart), *JsonVector(ConnectingEnd), *ContactHitJson(Obstacle));
		}
		Previous = Center;
	}
	TrajectoryRecord(TEXT("Geometry"), FString::Printf(TEXT("\"samples\":%d,\"geometryMask\":%d,\"capsuleRadius\":%.3f,\"capsuleHalfHeight\":%.3f,\"start\":%s,\"direction\":%s,\"capsuleClearance\":1,\"syntheticGeometry\":0,\"routeMin\":%s,\"routeMax\":%s,\"runtimeEnvelopeRequired\":1"),
		Count, GeometryBits, Radius, Half, *JsonVector(OutStart), *JsonVector(OutDirection),
		*JsonVector(bRamp ? FVector(0, -3700, 0) : FVector(-700, 2000, 0)), *JsonVector(bRamp ? FVector(0, -1500, 0) : FVector(700, 2000, 0))));
	if (bRamp && GeometryBits != 7)
	{
		UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: GeometryRejected token=%s peer=%d case=%d reason=missing-route-phases samples=%d geometryMask=%d"),
			*Frame.Token, Frame.Peer, Frame.Case, Count, GeometryBits);
		return false;
	}
	return true;
}

bool ABwaySlideLifecycleProbe::PrepareTrajectoryCase()
{
	FVector Start, Direction;
	if (Frame.ReviewRegression)
	{
		Start = BwaySlideReviewCourse::Origin(Frame.Case == 1) + FVector(-300.f, 0.f, State->Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f);
		Direction = FVector::ForwardVector;
	}
	else if (!ValidateTrajectoryGeometry(Start, Direction)) { Fail(TEXT("setup"), TEXT("authored-route-capsule-clearance-or-floor-invalid")); return false; }
	Frame.Position = Start; Frame.Direction = Direction;
	auto* Pawn = State->Pawn.Get();
	if (Frame.ReviewRegression)
	{
		State->ReviewRelocationStartedAt = FPlatformTime::Seconds();
		TrajectoryRecord(TEXT("ReviewRelocationBegin"), FString::Printf(TEXT("\"nextSerial\":%d,\"targetPhase\":2,\"from\":%s,\"to\":%s,\"pawnName\":\"%s\",\"maxSeconds\":12,\"positionTolerance\":10,\"intentionalTeleport\":1"),
			Frame.Serial + 1, *JsonVector(Pawn->GetActorLocation()), *JsonVector(Start), *Pawn->GetName().ReplaceCharWithEscapedChar()));
	}
	Pawn->GetBwayCharacterMovement()->StopMovementImmediately();
	Pawn->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics); Pawn->ForceNetUpdate();
	Publish(Relocated);
	return true;
}

void ABwaySlideLifecycleProbe::ObserveTrajectory()
{
	const double Now = FPlatformTime::Seconds();
	auto* Pawn = State->Pawn.Get(); auto* Move = Pawn->GetBwayCharacterMovement();
	if (SlideTuning(Move) != State->AuthoredTuning || !State->Driver.IsValid() || State->Driver->PacketSimulationSettings.PktLag != 100)
	{ Fail(TEXT("setup"), TEXT("authored-tuning-or-packet-settings-changed")); return; }
	const bool bExecutor = HasAuthority() || State->bOwner;
	const bool bJump = (Frame.Case % 2) == 1;
	const bool bClean = !Move->IsSliding() && !Active(State->ASC.Get(), State->Slide) && !State->ASC->HasMatchingGameplayTag(State->SlidingTag);
	if (Frame.SupportDiagnostic && State->SupportRejectedAt > 0)
	{
		// No Hold/terminal success acknowledgment can follow the latched failure.
		// Observe natural movement only; do not add input, velocity, or floor snaps.
		if (Now - State->SupportRejectedAt >= SupportDiagnosticSeconds)
			Fail(TEXT("diagnostic"), TEXT("support-diagnostic-window-ended"));
		else if (!Move->IsSliding() || !Active(State->ASC.Get(), State->Slide) || !State->ASC->HasMatchingGameplayTag(State->SlidingTag))
			Fail(TEXT("diagnostic"), TEXT("support-diagnostic-natural-slide-ended"));
		return;
	}
	if (Frame.Phase == Armed)
	{
		if (State->bAckSent) return;
		FVector Start, Direction;
		if (!ValidateTrajectoryGeometry(Start, Direction)) { Fail(TEXT("setup"), TEXT("peer-route-collision-mismatch")); return; }
		Report(true, TEXT("trajectory-observer-armed"));
	}
	else if (Frame.Phase == RunUp && State->bOwner)
	{
		if (!Move->IsMovingOnGround()) { Fail(TEXT("setup"), TEXT("trajectory-run-up-left-ground")); return; }
		Pawn->AddMovementInput(Frame.Direction, 1.f);
		++State->ApproachInputFrames;
		if (Move->Velocity.Size2D() >= 300.f) Report(true, TEXT("owner-speed-ready"));
	}
	else if (Frame.Phase == Execute)
	{
		if (State->bOwner && !State->bExecuted)
		{
			bool bApproachReady = true;
			if (Frame.Case >= 2)
			{
				const auto& Floor = Move->CurrentFloor;
				const TCHAR* Geometry = GeometryPhase(Frame.Case, Floor);
				const bool bOverlap = CapsulePenetrating(Pawn);
				const float ForwardSpeed = FVector::DotProduct(Move->Velocity, Frame.Direction);
				if (!bClean || Move->MovementMode != MOVE_Walking || !Floor.IsWalkableFloor() || !HasTrajectorySlideSupport(Floor) || bOverlap)
				{ Fail(TEXT("setup"), TEXT("trajectory-ramp-approach-lost-walking-support")); return; }
				if (Floor.HitResult.ImpactPoint.Y >= -2950.f)
				{ Fail(TEXT("setup"), TEXT("trajectory-ramp-approach-missed-first-ramp")); return; }
				bApproachReady = FCString::Strcmp(Geometry, TEXT("ramp1")) == 0 && ForwardSpeed >= 300.f;
				if (!bApproachReady)
				{
					// A speed-only readiness handshake can activate too early on the
					// upper plateau, where authored friction stops the slide before
					// the slope. Keep using real input until the actual first ramp.
					Pawn->AddMovementInput(Frame.Direction, 1.f);
					++State->ApproachInputFrames;
				}
				else
				{
					TrajectoryRecord(TEXT("ApproachReady"), FString::Printf(TEXT("\"position\":%s,\"velocity\":%s,\"forwardSpeed\":%.6f,\"floorNormal\":%s,\"floorComponent\":\"%s\",\"floorBlocking\":%d,\"floorDistance\":%.6f,\"supported\":%d,\"walkable\":%d,\"customSupport\":%d,\"capsuleOverlap\":%d,\"penetrating\":%d,\"mode\":%d,\"custom\":%d,\"inputPath\":\"AddMovementInput\",\"movementInputFrames\":%d,\"geometryPhase\":\"%s\""),
						*JsonVector(Pawn->GetActorLocation()), *JsonVector(Move->Velocity), ForwardSpeed, *JsonVector(Floor.HitResult.ImpactNormal),
						*GetPathNameSafe(Floor.HitResult.GetComponent()).ReplaceCharWithEscapedChar(), Floor.bBlockingHit, Floor.GetDistanceToFloor(),
						Floor.IsWalkableFloor(), Floor.IsWalkableFloor(), HasTrajectorySlideSupport(Floor), bOverlap, Floor.HitResult.bStartPenetrating || bOverlap, int32(Move->MovementMode), Move->CustomMovementMode,
						State->ApproachInputFrames, Geometry));
				}
			}
			if (bApproachReady)
			{
				State->bExecuted = true; State->bSlidePressed = true; Input(false, true);
				if (!Move->IsSliding() || !Active(State->ASC.Get(), State->Slide)) { Fail(TEXT("gameplay"), TEXT("trajectory-slide-did-not-activate")); return; }
			}
		}
		if (bExecutor && State->Entries > 0)
		{
			if (!Move->IsSliding() || !Active(State->ASC.Get(), State->Slide) || !State->ASC->HasMatchingGameplayTag(State->SlidingTag))
			{ Fail(TEXT("gameplay"), TEXT("trajectory-held-slide-ended-early")); return; }
			const double Distance = FVector::Dist2D(Pawn->GetActorLocation(), State->TrajectoryEntryPosition);
			if (!State->bAckSent && State->SlideSeconds >= 0.5 && Distance >= 20.0 && (Frame.Case < 2 || State->GeometryMask == 7))
			{
				TrajectoryRecord(TEXT("Hold"), FString::Printf(TEXT("\"seconds\":%.6f,\"firstSlideTs\":%.6f,\"lastSlideTs\":%.6f,\"distance\":%.3f,\"geometryMask\":%d,\"activations\":%d,\"ends\":%d,\"entries\":%d"),
					State->SlideSeconds, State->FirstSlideTimestamp, State->LastSlideTimestamp, Distance, State->GeometryMask, State->Activations, State->Ends, State->Entries));
				Report(State->Activations == 1 && State->Ends == 0 && State->Entries == 1, TEXT("sustained-authored-trajectory"));
			}
		}
	}
	else if (Frame.Phase == ReleaseHeld)
	{
		// A relay can enter terminal release before its local observation clock
		// reaches the nominal window.  Close that peer's diagnostic interval at
		// the terminal boundary so the four-peer record remains complete without
		// turning the bounded observation into an acceptance requirement.
		if (Frame.GroundAirDiagnostic && !State->bGroundAirObservationEnded)
		{
			State->bGroundAirObservationEnded = true;
			TrajectoryRecord(TEXT("ObservationEnd"), FString::Printf(TEXT("\"groundCoverageMask\":%d,\"groundSamples\":%d,\"groundTangentTravel\":%.6f,\"airSamples\":%d,\"slideJumpAirSamples\":%d,\"invalidSamples\":%d,\"sawAir\":%d,\"sawReturn\":%d,\"deadlineReached\":%d,\"terminalBoundary\":1"),
				State->GroundAirCoverageMask, State->GroundAirGroundSamples, State->GroundAirGroundTravel, State->GroundAirAirSamples, State->GroundAirSlideJumpAirSamples,
				State->GroundAirInvalidSamples, State->bGroundAirSawAir, State->bGroundAirSawReturn,
				Frame.Case >= 2 && State->GroundAirCoverageMask != 7));
			if (bExecutor && State->Entries > 0) Report(true, TEXT("ground-air-observation-terminal-boundary"));
		}
		if (State->bOwner && !State->bTerminalIssued)
		{
			State->bTerminalIssued = true;
			if (bJump) { State->bJumpPressed = true; Input(true, true); }
			else { Input(false, false); State->bSlidePressed = false; }
			TrajectoryRecord(TEXT("Terminal"), FString::Printf(TEXT("\"action\":\"%s\",\"position\":%s,\"geometryPhase\":\"%s\""), bJump ? TEXT("jump") : TEXT("release"), *JsonVector(Pawn->GetActorLocation()), GeometryPhase(Frame.Case, Move->CurrentFloor)));
			// Jump() queues bPressedJump; CMC must process it while still sliding.
			// Observe the result next tick, also avoiding a pre-action bClean value.
			return;
		}
		if (bExecutor && bJump && !State->bAirborne && Move->IsFalling())
		{
			State->bAirborne = true;
			TrajectoryRecord(TEXT("Airborne"), FString::Printf(TEXT("\"slideJump\":%d,\"clean\":%d,\"position\":%s,\"velocity\":%s"), Move->IsSlideJumping(), bClean, *JsonVector(Pawn->GetActorLocation()), *JsonVector(Move->Velocity)));
			if (!Move->IsSlideJumping() || !bClean) { Fail(TEXT("gameplay"), TEXT("trajectory-jump-destination-or-cleanup")); return; }
			if (State->bOwner)
			{
				if (State->bJumpPressed) { Input(true, false); State->bJumpPressed = false; }
				if (State->bSlidePressed) { Input(false, false); State->bSlidePressed = false; }
			}
		}
		if (bExecutor && bJump && State->bAirborne && !State->bLanded && Move->MovementMode == MOVE_Walking)
		{
			State->bLanded = true;
			const bool bOverlap = CapsulePenetrating(Pawn);
			const bool bPenetrating = Move->CurrentFloor.HitResult.bStartPenetrating || bOverlap;
			TrajectoryRecord(TEXT("Landed"), FString::Printf(TEXT("\"clean\":%d,\"supported\":%d,\"walkable\":%d,\"floorBlocking\":%d,\"floorDistance\":%.6f,\"floorNormal\":%s,\"mode\":%d,\"custom\":%d,\"penetrating\":%d,\"capsuleOverlap\":%d,\"position\":%s"),
				bClean, Move->CurrentFloor.IsWalkableFloor(), Move->CurrentFloor.IsWalkableFloor(), Move->CurrentFloor.bBlockingHit,
				Move->CurrentFloor.GetDistanceToFloor(), *JsonVector(Move->CurrentFloor.HitResult.ImpactNormal), int32(Move->MovementMode), Move->CustomMovementMode,
				bPenetrating, bOverlap, *JsonVector(Pawn->GetActorLocation())));
			if (!bClean || !Move->CurrentFloor.IsWalkableFloor() || bPenetrating) { Fail(TEXT("gameplay"), TEXT("trajectory-invalid-landing")); return; }
		}
		const bool bDone = bExecutor && bClean && Move->MovementMode == MOVE_Walking && Move->Velocity.Size() <= 10.f
			&& Move->CurrentFloor.IsWalkableFloor() && !Move->CurrentFloor.HitResult.bStartPenetrating && !CapsulePenetrating(Pawn)
			&& (!bJump || State->bAirborne && State->bLanded) && State->Activations == 1 && State->Ends == 1 && State->Entries == 1;
		if (bDone)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (Now - State->StableSince >= 0.5) Report(true, TEXT("trajectory-terminal-and-landing-clean"));
		}
		else State->StableSince = 0;
	}
	else if (Frame.Phase == Settled)
	{
		const auto& Floor = Move->CurrentFloor;
		const bool bOverlap = CapsulePenetrating(Pawn);
		const bool bPenetrating = Floor.HitResult.bStartPenetrating || bOverlap;
		const bool bConverged = bClean && Move->MovementMode == MOVE_Walking && Floor.IsWalkableFloor() && !bPenetrating
			&& Move->Velocity.Size() <= 10.f && FVector::Dist(Pawn->GetActorLocation(), Frame.Position) <= 10.f;
		if (bConverged)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (!State->bAckSent && Now - State->StableSince >= 0.5)
			{
				TrajectoryRecord(TEXT("Settled"), FString::Printf(TEXT("\"position\":%s,\"targetPosition\":%s,\"velocity\":%s,\"clean\":1,\"mode\":%d,\"custom\":%d,\"supported\":%d,\"walkable\":%d,\"floorBlocking\":%d,\"floorDistance\":%.6f,\"floorNormal\":%s,\"penetrating\":%d,\"capsuleOverlap\":%d,\"activations\":%d,\"ends\":%d,\"entries\":%d"),
					*JsonVector(Pawn->GetActorLocation()), *JsonVector(Frame.Position), *JsonVector(Move->Velocity), int32(Move->MovementMode), Move->CustomMovementMode,
					Floor.IsWalkableFloor(), Floor.IsWalkableFloor(), Floor.bBlockingHit, Floor.GetDistanceToFloor(), *JsonVector(Floor.HitResult.ImpactNormal),
					bPenetrating, bOverlap, State->Activations, State->Ends, State->Entries));
				TrajectoryRecord(TEXT("Tuning"), FString::Printf(TEXT("\"phase\":\"after\",\"tuning\":\"%s\",\"unchanged\":1"), *SlideTuning(Move)));
				++State->CompletedCases;
				UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: PeerCasePASS serial=%d case=%d name=%s peer=%d player=%d thresholdRestored=1 minSlide=%.2f"),
					Frame.Serial, Frame.Case, CaseName(Frame.Case, true, Frame.ReviewRegression), Frame.Peer, Frame.Source, Move->MinSlideSpeed);
				Report(true, TEXT("trajectory-position-and-state-converged"));
			}
		}
		else State->StableSince = 0;
	}
	if (Now - State->PhaseStarted > 12.0 && !State->bAckSent) Fail(TEXT("gameplay"), TEXT("trajectory-phase-timeout"));
}

void ABwaySlideLifecycleProbe::ObserveGroundAirDiagnostic()
{
	const double Now = FPlatformTime::Seconds();
	auto* Pawn = State->Pawn.Get(); auto* Move = Pawn->GetBwayCharacterMovement();
	if (SlideTuning(Move) != State->AuthoredTuning || !State->Driver.IsValid() || State->Driver->PacketSimulationSettings.PktLag != 100)
	{ Fail(TEXT("setup"), TEXT("authored-tuning-or-packet-settings-changed")); return; }
	const bool bExecutor = HasAuthority() || State->bOwner;
	const bool bJump = (Frame.Case % 2) == 1;
	const bool bClean = !Move->IsSliding() && !Active(State->ASC.Get(), State->Slide) && !State->ASC->HasMatchingGameplayTag(State->SlidingTag);

	if (Frame.Phase == Armed)
	{
		if (State->bAckSent) return;
		FVector Start, Direction;
		if (!ValidateTrajectoryGeometry(Start, Direction)) { Fail(TEXT("setup"), TEXT("peer-route-collision-mismatch")); return; }
		if (State->GroundAirExpectedComponent.IsEmpty() && Move->CurrentFloor.HitResult.GetComponent())
			State->GroundAirExpectedComponent = GetPathNameSafe(Move->CurrentFloor.HitResult.GetComponent());
		TrajectoryRecord(TEXT("ObserverArmed"), FString::Printf(TEXT("\"expectedComponent\":\"%s\",\"observationSeconds\":%.1f,\"acceptancePolicyChanged\":0"),
			*State->GroundAirExpectedComponent.ReplaceCharWithEscapedChar(), GroundAirDiagnosticSeconds));
		Report(true, TEXT("ground-air-observer-armed"));
	}
	else if (Frame.Phase == RunUp && State->bOwner)
	{
		if (!Move->IsMovingOnGround()) { Fail(TEXT("setup"), TEXT("ground-air-run-up-left-ground")); return; }
		Pawn->AddMovementInput(Frame.Direction, 1.f);
		++State->ApproachInputFrames;
		if (Move->Velocity.Size2D() >= 300.f) Report(true, TEXT("ground-air-owner-speed-ready"));
	}
	else if (Frame.Phase == Execute)
	{
		// Every relay owns its own observation clock.  Authority and owner peers
		// drive the fixture state machine, while proxies still emit a complete
		// diagnostic record for the same bounded window.
		if (State->GroundAirStartedAt == 0) State->GroundAirStartedAt = Now;
		if (State->bOwner && !State->bExecuted)
		{
			bool bApproachReady = Frame.Case < 2;
			if (!bApproachReady)
			{
				const auto& Floor = Move->CurrentFloor;
				const TCHAR* Geometry = GeometryPhase(Frame.Case, Floor);
				const float ForwardSpeed = FVector::DotProduct(Move->Velocity, Frame.Direction);
				const bool bOverlap = CapsulePenetrating(Pawn);
				if (!bClean || Move->MovementMode != MOVE_Walking || !Floor.IsWalkableFloor() || !HasTrajectorySlideSupport(Floor) || bOverlap)
				{ Fail(TEXT("setup"), TEXT("ground-air-ramp-approach-lost-walking-support")); return; }
				if (Floor.HitResult.ImpactPoint.Y >= -2950.f)
				{ Fail(TEXT("setup"), TEXT("ground-air-ramp-approach-missed-first-ramp")); return; }
				bApproachReady = FCString::Strcmp(Geometry, TEXT("ramp1")) == 0 && ForwardSpeed >= 300.f;
				if (!bApproachReady)
				{
					Pawn->AddMovementInput(Frame.Direction, 1.f);
					++State->ApproachInputFrames;
				}
			}
			if (bApproachReady)
			{
				State->GroundAirStartedAt = Now;
				State->bExecuted = true; State->bSlidePressed = true; Input(false, true);
				TrajectoryRecord(TEXT("Execute"), FString::Printf(TEXT("\"inputPath\":\"AbilityInputTagPressed\",\"groundAirDeadlineSeconds\":%.1f"), GroundAirDiagnosticSeconds));
				if (!Move->IsSliding() || !Active(State->ASC.Get(), State->Slide)) { Fail(TEXT("gameplay"), TEXT("ground-air-slide-did-not-activate")); return; }
			}
			return;
		}
		bool bExecutorObservationComplete = false;
		if (bExecutor && State->Entries > 0)
		{
			if (!Move->IsSliding() || !Active(State->ASC.Get(), State->Slide) || !State->ASC->HasMatchingGameplayTag(State->SlidingTag))
			{ Fail(TEXT("diagnostic"), TEXT("ground-air-slide-ended-before-observation")); return; }
			const double HeldDistance = FVector::Dist2D(Pawn->GetActorLocation(), State->TrajectoryEntryPosition);
			bExecutorObservationComplete = Frame.Case < 2 && State->SlideSeconds >= 0.5 && HeldDistance >= 20.0;
		}
		const bool bProxyObservationComplete = !bExecutor && State->TrajectorySamples > 0 && Now - State->GroundAirStartedAt >= (Frame.Case < 2 ? 0.5 : GroundAirDiagnosticSeconds);
		const bool bRampObservationComplete = Frame.Case >= 2 && State->TrajectorySamples > 0
			&& (State->GroundAirCoverageMask == 7 || Now - State->GroundAirStartedAt >= GroundAirDiagnosticSeconds);
		if (!State->bGroundAirObservationEnded && (bExecutorObservationComplete || bProxyObservationComplete || bRampObservationComplete))
		{
			State->bGroundAirObservationEnded = true;
			TrajectoryRecord(TEXT("ObservationEnd"), FString::Printf(TEXT("\"groundCoverageMask\":%d,\"groundSamples\":%d,\"groundTangentTravel\":%.6f,\"airSamples\":%d,\"slideJumpAirSamples\":%d,\"invalidSamples\":%d,\"sawAir\":%d,\"sawReturn\":%d,\"deadlineReached\":%d"),
				State->GroundAirCoverageMask, State->GroundAirGroundSamples, State->GroundAirGroundTravel, State->GroundAirAirSamples, State->GroundAirSlideJumpAirSamples,
				State->GroundAirInvalidSamples, State->bGroundAirSawAir, State->bGroundAirSawReturn,
				Frame.Case >= 2 && State->GroundAirCoverageMask != 7));
			if (bExecutor && State->Entries > 0) Report(true, TEXT("ground-air-observation-complete"));
		}
	}
	else if (Frame.Phase == ReleaseHeld)
	{
		if (!State->bGroundAirObservationEnded)
		{
			State->bGroundAirObservationEnded = true;
			TrajectoryRecord(TEXT("ObservationEnd"), FString::Printf(TEXT("\"groundCoverageMask\":%d,\"groundSamples\":%d,\"groundTangentTravel\":%.6f,\"airSamples\":%d,\"slideJumpAirSamples\":%d,\"invalidSamples\":%d,\"sawAir\":%d,\"sawReturn\":%d,\"deadlineReached\":%d,\"terminalBoundary\":1"),
				State->GroundAirCoverageMask, State->GroundAirGroundSamples, State->GroundAirGroundTravel, State->GroundAirAirSamples, State->GroundAirSlideJumpAirSamples,
				State->GroundAirInvalidSamples, State->bGroundAirSawAir, State->bGroundAirSawReturn,
				Frame.Case >= 2 && State->GroundAirCoverageMask != 7));
		}
		if (State->bOwner && !State->bTerminalIssued)
		{
			State->bTerminalIssued = true;
			if (bJump) { State->bJumpPressed = true; Input(true, true); }
			else { Input(false, false); State->bSlidePressed = false; }
			TrajectoryRecord(TEXT("Terminal"), FString::Printf(TEXT("\"action\":\"%s\",\"position\":%s,\"geometryPhase\":\"%s\",\"groundCoverageMask\":%d"),
				bJump ? TEXT("jump") : TEXT("release"), *JsonVector(Pawn->GetActorLocation()), GeometryPhase(Frame.Case, Move->CurrentFloor), State->GroundAirCoverageMask));
			return;
		}
		if (bExecutor && bJump && !State->bAirborne && Move->IsFalling())
		{
			State->bAirborne = true;
			TrajectoryRecord(TEXT("Airborne"), FString::Printf(TEXT("\"slideJump\":%d,\"clean\":%d,\"position\":%s,\"velocity\":%s"),
				Move->IsSlideJumping(), bClean, *JsonVector(Pawn->GetActorLocation()), *JsonVector(Move->Velocity)));
			if (!Move->IsSlideJumping() || !bClean) { Fail(TEXT("gameplay"), TEXT("ground-air-jump-destination-or-cleanup")); return; }
			if (State->bOwner)
			{
				if (State->bJumpPressed) { Input(true, false); State->bJumpPressed = false; }
				if (State->bSlidePressed) { Input(false, false); State->bSlidePressed = false; }
			}
		}
		if (bExecutor && bJump && State->bAirborne && !State->bLanded && Move->MovementMode == MOVE_Walking)
		{
			State->bLanded = true;
			const bool bOverlap = CapsulePenetrating(Pawn);
			const bool bPenetrating = Move->CurrentFloor.HitResult.bStartPenetrating || bOverlap;
			TrajectoryRecord(TEXT("Landed"), FString::Printf(TEXT("\"clean\":%d,\"supported\":%d,\"walkable\":%d,\"floorBlocking\":%d,\"floorDistance\":%.6f,\"floorNormal\":%s,\"mode\":%d,\"custom\":%d,\"penetrating\":%d,\"capsuleOverlap\":%d,\"position\":%s"),
				bClean, Move->CurrentFloor.IsWalkableFloor(), Move->CurrentFloor.IsWalkableFloor(), Move->CurrentFloor.bBlockingHit,
				Move->CurrentFloor.GetDistanceToFloor(), *JsonVector(Move->CurrentFloor.HitResult.ImpactNormal), int32(Move->MovementMode), Move->CustomMovementMode,
				bPenetrating, bOverlap, *JsonVector(Pawn->GetActorLocation())));
			if (!bClean || !Move->CurrentFloor.IsWalkableFloor() || bPenetrating) { Fail(TEXT("gameplay"), TEXT("ground-air-invalid-landing")); return; }
		}
		const bool bDone = bExecutor && bClean && Move->MovementMode == MOVE_Walking && Move->Velocity.Size() <= 10.f
			&& Move->CurrentFloor.IsWalkableFloor() && !Move->CurrentFloor.HitResult.bStartPenetrating && !CapsulePenetrating(Pawn)
			&& (!bJump || State->bAirborne && State->bLanded) && State->Activations == 1 && State->Ends == 1 && State->Entries == 1;
		if (bDone)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (Now - State->StableSince >= 0.5) Report(true, TEXT("ground-air-terminal-and-landing-clean"));
		}
		else State->StableSince = 0;
	}
	else if (Frame.Phase == Settled)
	{
		const auto& Floor = Move->CurrentFloor;
		const bool bOverlap = CapsulePenetrating(Pawn);
		const bool bPenetrating = Floor.HitResult.bStartPenetrating || bOverlap;
		const bool bConverged = bClean && Move->MovementMode == MOVE_Walking && Floor.IsWalkableFloor() && !bPenetrating
			&& Move->Velocity.Size() <= 10.f && FVector::Dist(Pawn->GetActorLocation(), Frame.Position) <= 10.f;
		if (bConverged)
		{
			if (State->StableSince == 0) State->StableSince = Now;
			if (!State->bAckSent && Now - State->StableSince >= 0.5)
			{
				TrajectoryRecord(TEXT("Settled"), FString::Printf(TEXT("\"position\":%s,\"targetPosition\":%s,\"velocity\":%s,\"clean\":1,\"mode\":%d,\"custom\":%d,\"supported\":%d,\"walkable\":%d,\"floorBlocking\":%d,\"floorDistance\":%.6f,\"floorNormal\":%s,\"penetrating\":%d,\"capsuleOverlap\":%d,\"activations\":%d,\"ends\":%d,\"entries\":%d"),
					*JsonVector(Pawn->GetActorLocation()), *JsonVector(Frame.Position), *JsonVector(Move->Velocity), int32(Move->MovementMode), Move->CustomMovementMode,
					Floor.IsWalkableFloor(), Floor.IsWalkableFloor(), Floor.bBlockingHit, Floor.GetDistanceToFloor(), *JsonVector(Floor.HitResult.ImpactNormal),
					bPenetrating, bOverlap, State->Activations, State->Ends, State->Entries));
				TrajectoryRecord(TEXT("Tuning"), FString::Printf(TEXT("\"phase\":\"after\",\"tuning\":\"%s\",\"unchanged\":1"), *SlideTuning(Move)));
				++State->CompletedCases;
				Report(true, TEXT("ground-air-settled-clean"));
			}
		}
		else State->StableSince = 0;
	}
	if (Now - State->PhaseStarted > 16.0 && !State->bAckSent) Fail(TEXT("diagnostic"), TEXT("ground-air-phase-timeout"));
}

void ABwaySlideLifecycleProbe::AuthorityTick()
{
	if (Frame.SupportDiagnostic && State && State->SupportRejectedAt > 0 && Frame.Phase < Stop) return;
	const double Now = FPlatformTime::Seconds();
	const bool bAll = State->Relays.Num() == 4 && State->Acks.Num() == 4 && !State->Acks.Contains(false);
	for (const auto& Relay : State->Relays) if (!Relay.IsValid() && Frame.Phase < Stop) { Fail(TEXT("setup"), TEXT("relay-lost")); return; }
	if (Frame.Phase == Baseline && bAll)
	{
		if (!State->bOriginalSaved)
		{
			State->OriginalTransform = State->Pawn->GetActorTransform(); State->OriginalVelocity = State->Pawn->GetVelocity();
			const auto* Move = State->Pawn->GetBwayCharacterMovement(); State->OriginalMode = Move->MovementMode; State->OriginalCustom = Move->CustomMovementMode; State->bOriginalSaved = true;
			State->IsolatedBystanders.Reset();
			int32 BystanderIdx = 0;
			for (TActorIterator<ABwayCharacterWithAbilities> It(GetWorld()); It; ++It)
			{
				ABwayCharacterWithAbilities* OtherPawn = *It;
				if (!OtherPawn || OtherPawn == State->Pawn.Get()) continue;
				FBwaySlideLifecycleProbeState::FIsolatedBystander Saved;
				Saved.Pawn = OtherPawn;
				Saved.Transform = OtherPawn->GetActorTransform();
				Saved.Velocity = OtherPawn->GetVelocity();
				if (auto* OtherMove = OtherPawn->GetBwayCharacterMovement())
				{
					Saved.Mode = OtherMove->MovementMode;
					Saved.Custom = OtherMove->CustomMovementMode;
					OtherMove->StopMovementImmediately();
				}
				State->IsolatedBystanders.Add(Saved);
				OtherPawn->SetActorLocation(FVector(500.f + 150.f * BystanderIdx++, 3500.f, 330.4f), false, nullptr, ETeleportType::TeleportPhysics);
				OtherPawn->ForceNetUpdate();
			}
		}
		if (Frame.Trajectory) { PrepareTrajectoryCase(); return; }
		Frame.Position = FVector(0.f, 2000.f, 6.25f + State->Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f);
		FHitResult Floor; FCollisionQueryParams Query(SCENE_QUERY_STAT(SlideProbeLane), false, State->Pawn.Get());
		if (!GetWorld()->LineTraceSingleByChannel(Floor, Frame.Position, Frame.Position - FVector(0, 0, 200), ECC_Visibility, Query) || Floor.ImpactNormal.Z < 0.995f || FMath::Abs(Floor.ImpactPoint.Z - 6.25f) > 1.f)
		{ Fail(TEXT("setup"), TEXT("known-lane-floor-changed")); return; }
		State->Pawn->SetActorLocation(Frame.Position, false, nullptr, ETeleportType::TeleportPhysics); State->Pawn->ForceNetUpdate();
		Publish(Relocated); return;
	}
	if (Frame.Phase == Relocated && bAll)
	{
		if (Frame.ReviewRegression)
		{
			TrajectoryRecord(TEXT("ReviewRelocationEnd"), FString::Printf(TEXT("\"position\":%s,\"targetPosition\":%s,\"elapsedSeconds\":%.7f,\"peerAcks\":4"),
				*JsonVector(State->Pawn->GetActorLocation()), *JsonVector(Frame.Position), Now - State->ReviewRelocationStartedAt));
		}
		if (Frame.Trajectory) { Publish(Armed); return; }
		if (Frame.Case >= 2 && !Runway(State->Pawn.Get(), Frame.Direction)) { Fail(TEXT("setup"), TEXT("no-flat-clear-runway")); return; }
		if (Frame.Case >= 2) UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Runway case=%d player=%d direction=%s length=1400"), Frame.Case, Frame.Source, *Frame.Direction.ToCompactString());
		Publish(Armed); return;
	}
	if (Frame.Phase == Armed && bAll) { Publish(!Frame.Trajectory && Frame.Case < 2 ? Execute : RunUp); return; }
	if (Frame.Phase == RunUp && State->Acks.IsValidIndex(1) && State->Acks[1])
	{
		const auto* Move = State->Pawn->GetBwayCharacterMovement();
		if (Move->IsMovingOnGround() && Move->Velocity.Size2D() >= FMath::Max(300.f, Move->MinSlideSpeed * 2.f)) { Publish(Execute); return; }
	}
	if (Frame.Phase == Execute && State->Acks[0] && State->Acks[1]) { Publish(Frame.Trajectory || Frame.Case == 0 && Frame.AuthoredMinSpeed == 0.f ? ReleaseHeld : Settled); return; }
	if (Frame.Phase == ReleaseHeld && State->Acks[0] && State->Acks[1])
	{
		if (Frame.Trajectory) Frame.Position = State->Pawn->GetActorLocation();
		Publish(Settled); return;
	}
	if (Frame.Phase == Settled && bAll)
	{
		UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: CasePASS case=%d name=%s player=%d peers=4 lagMs=100"), Frame.Case, CaseName(Frame.Case, Frame.Trajectory, Frame.ReviewRegression), Frame.Source);
		if (Frame.Case == (Frame.ReviewRegression ? 1 : 3)) { Publish(Stop); return; }
		++Frame.Case;
		if (Frame.Trajectory) { PrepareTrajectoryCase(); return; }
		// Each case begins from the same authority-only lane teleport after the previous case settled.
		State->Pawn->GetBwayCharacterMovement()->StopMovementImmediately();
		State->Pawn->SetActorLocation(Frame.Position, false, nullptr, ETeleportType::TeleportPhysics); State->Pawn->ForceNetUpdate();
		Publish(Relocated); return;
	}
	if (Frame.Phase == Stop && bAll)
	{
		if (!RestoreAuthority())
		{
			State->bFailed = true; State->FailureReason = TEXT("authority-restore-failed");
			UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=cleanup reason=authority-restore-failed player=%d"), Frame.Source);
		}
		Frame.Position = State->bOriginalSaved ? State->OriginalTransform.GetLocation() : FVector::ZeroVector;
		Frame.Rotation = State->bOriginalSaved ? State->OriginalTransform.Rotator() : FRotator::ZeroRotator;
		Frame.RestoreScale = State->bOriginalSaved ? State->OriginalTransform.GetScale3D() : FVector::OneVector;
		Frame.RestoreVelocity = State->OriginalVelocity;
		Frame.RestoreMode = State->OriginalMode; Frame.RestoreCustom = State->OriginalCustom;
		Publish(Restored); return;
	}
	if (Frame.Phase == Restored && bAll) { Publish(Complete); return; }
	if (Frame.Phase == Complete && bAll)
	{
		if (State->FinalSince == 0)
		{
			State->FinalSince = Now;
			UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: COMPLETE passed=%d player=%d cases=%d peers=4 cleanupAcks=4 finalAcks=4 authorityTransformRestored=%d"),
				Frame.Passed, Frame.Source, State->CompletedCases, State->bAuthorityRestored);
		}
		if (Now - State->FinalSince >= 8.0) DestroyRelays();
		return;
	}
	const double Limit = Frame.Phase == Baseline ? 30.0 : 16.0;
	if (Now - State->Published > Limit)
	{
		if (Frame.Phase < Stop) Fail(Frame.Phase == Execute || Frame.Phase == ReleaseHeld || Frame.Phase == Settled ? TEXT("gameplay") : TEXT("setup"), TEXT("peer-phase-ack-timeout"));
		else
		{
			UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=cleanup-ack-timeout phase=%d serial=%d"), Frame.Phase, Frame.Serial);
			CleanupLocal(); RestoreAuthority(); DestroyRelays();
		}
	}
}

void ABwaySlideLifecycleProbe::CleanupLocal()
{
	if (!State || State->bCleaned) return;
	State->bCleaned = true;
	bool bReviewRestored = true;
	if (Frame.ReviewRegression)
	{
		if (State->Pawn.IsValid())
		{
			State->Pawn->GetBwayCharacterMovement()->SlideReviewObserver.Remove(State->ReviewObserver);
			if (State->bReviewHoldSaved) State->Pawn->JumpMaxHoldTime = State->OriginalJumpHold;
			bReviewRestored = !State->bReviewHoldSaved || State->Pawn->JumpMaxHoldTime == State->OriginalJumpHold;
		}
		else bReviewRestored = !State->bReviewHoldSaved;
		State->ReviewObserver.Reset();
		TrajectoryRecord(TEXT("ReviewCleanup"), FString::Printf(TEXT("\"jumpHoldRestored\":%d,\"observerRemoved\":1,\"courseActorsPendingRestore\":%d"), bReviewRestored, State->ReviewCourse.Num()));
	}
	bool bMovementUnbound = !State->bMovementBound;
	if (State->Pawn.IsValid() && State->bMovementBound)
	{
		State->Pawn->OnCharacterMovementUpdated.RemoveDynamic(this, &ABwaySlideLifecycleProbe::OnTrajectoryMovement);
		bMovementUnbound = !State->Pawn->OnCharacterMovementUpdated.Contains(this, GET_FUNCTION_NAME_CHECKED(ABwaySlideLifecycleProbe, OnTrajectoryMovement));
	}
	bool bCorrectionsRestored = !State->bCorrectionsSaved;
	if (State->bCorrectionsSaved)
	{
		if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.NetShowCorrections")))
		{
			CVar->SetWithCurrentPriority(State->OriginalCorrections);
			bCorrectionsRestored = CVar->GetInt() == State->OriginalCorrections;
		}
	}
	if (State->ASC.IsValid()) { State->ASC->AbilityActivatedCallbacks.Remove(State->Activated); State->ASC->AbilityEndedCallbacks.Remove(State->Ended); }
	if (State->CueManager.IsValid()) State->CueManager->OnGameplayCueRouted().Remove(State->Cue);
	State->CueManager.Reset(); State->Activated.Reset(); State->Ended.Reset(); State->Cue.Reset();
	if (State->bSlidePressed) { Input(false, false); State->bSlidePressed = false; }
	if (State->bJumpPressed) { Input(true, false); State->bJumpPressed = false; }
	if ((HasAuthority() || State->bOwner) && State->ASC.IsValid())
	{
		State->ASC->CancelAbilityHandle(State->Slide); State->ASC->CancelAbilityHandle(State->Jump);
		if (State->Pawn.IsValid()) { State->Pawn->ConsumeMovementInputVector(); State->Pawn->StopJumping(); }
	}
	bool bPacketsRestored = !State->bPacketSaved;
	if (State->bPacketSaved && State->Driver.IsValid())
	{
		State->Driver->SetPacketSimulationSettings(State->PacketSettings);
		bPacketsRestored = FPacketSimulationSettings::StaticStruct()->CompareScriptStruct(&State->Driver->PacketSimulationSettings, &State->PacketSettings, 0);
	}
	bool bThresholdRestored = !State->bBound;
	const bool bTuningUnchanged = !Frame.Trajectory || State->Pawn.IsValid() && SlideTuning(State->Pawn->GetBwayCharacterMovement()) == State->AuthoredTuning;
	if (State->Pawn.IsValid()) { State->Pawn->GetBwayCharacterMovement()->MinSlideSpeed = State->ConfiguredMinSpeed; bThresholdRestored = State->Pawn->GetBwayCharacterMovement()->MinSlideSpeed == State->ConfiguredMinSpeed; }
	const auto* SlideSpec = State->ASC.IsValid() ? State->ASC->FindAbilitySpecFromHandle(State->Slide) : nullptr;
	const auto* JumpSpec = State->ASC.IsValid() ? State->ASC->FindAbilitySpecFromHandle(State->Jump) : nullptr;
	const bool bInputReleased = !State->bSlidePressed && !State->bJumpPressed && (!State->bOwner ||
		(SlideSpec && JumpSpec && !SlideSpec->InputPressed && !JumpSpec->InputPressed && State->Pawn.IsValid() && !State->Pawn->bPressedJump
			&& State->Pawn->GetPendingMovementInputVector().IsNearlyZero()));
	const bool bAbilitiesInactive = !Active(State->ASC.Get(), State->Slide) && !Active(State->ASC.Get(), State->Jump)
		&& (!State->ASC.IsValid() || !State->ASC->HasMatchingGameplayTag(State->SlidingTag));
	State->bCleanupOkay = bPacketsRestored && bThresholdRestored && bTuningUnchanged && bMovementUnbound && bCorrectionsRestored && bReviewRestored
		&& (!Frame.Trajectory || (bInputReleased && bAbilitiesInactive));
	if (Frame.Trajectory) TrajectoryRecord(TEXT("Cleanup"), FString::Printf(TEXT("\"packetsRestored\":%d,\"tuningUnchanged\":%d,\"movementCallbackRemoved\":%d,\"correctionsRestored\":%d,\"inputReleased\":%d,\"abilitiesInactive\":%d,\"observerCallbacksRemoved\":1,\"lagMs\":%d,\"expectedLagMs\":%d"),
		bPacketsRestored, bTuningUnchanged, bMovementUnbound, bCorrectionsRestored, bInputReleased, bAbilitiesInactive,
		State->Driver.IsValid() ? State->Driver->PacketSimulationSettings.PktLag : -1, State->PacketSettings.PktLag));
	UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Cleanup peer=%d player=%d callbacksRemoved=1 packetsRestored=%d originalLag=%d thresholdRestored=%d originalMin=%.2f"), Frame.Peer, Frame.Source, bPacketsRestored, State->PacketSettings.PktLag, bThresholdRestored, State->ConfiguredMinSpeed);
	if (!State->bCleanupOkay)
	{
		State->bFailed = true; State->FailureReason = TEXT("local-restore-failed");
		UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=cleanup reason=local-restore-failed peer=%d player=%d"), Frame.Peer, Frame.Source);
	}
}
bool ABwaySlideLifecycleProbe::RestoreAuthority()
{
	if (!State || !State->bCoordinator || !State->bOriginalSaved) return true;
	if (State->bAuthorityRestored) return true;
	if (!State->Pawn.IsValid()) return false;
	bool bBystandersRestored = true;
	for (const auto& Saved : State->IsolatedBystanders)
	{
		auto* OtherPawn = Saved.Pawn.Get();
		auto* OtherMove = OtherPawn ? OtherPawn->GetBwayCharacterMovement() : nullptr;
		if (!OtherPawn || !OtherMove) { bBystandersRestored = false; continue; }
		OtherMove->StopMovementImmediately();
		OtherMove->SetMovementMode(Saved.Mode, Saved.Custom);
		OtherPawn->SetActorTransform(Saved.Transform, false, nullptr, ETeleportType::TeleportPhysics);
		OtherMove->Velocity = Saved.Velocity;
		OtherPawn->ForceNetUpdate();
		if (!OtherPawn->GetActorTransform().Equals(Saved.Transform, 0.01f))
			bBystandersRestored = false;
	}
	auto* Move = State->Pawn->GetBwayCharacterMovement();
	Move->StopMovementImmediately(); Move->SetMovementMode(State->OriginalMode, State->OriginalCustom);
	State->Pawn->SetActorTransform(State->OriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);
	Move->Velocity = State->OriginalVelocity; State->Pawn->ForceNetUpdate();
	State->bAuthorityRestored = bBystandersRestored && State->Pawn->GetActorTransform().Equals(State->OriginalTransform, 0.01f)
		&& Move->Velocity.Equals(State->OriginalVelocity, 0.01f) && Move->MovementMode == State->OriginalMode && Move->CustomMovementMode == State->OriginalCustom;
	return State->bAuthorityRestored;
}
bool ABwaySlideLifecycleProbe::RestorePeerOrientation()
{
	if (!State || !Frame.Trajectory || !State->bPeerOrientationSaved) return true;
	if (State->bPeerOrientationAttempted) return State->bPeerOrientationRestored;
	State->bPeerOrientationAttempted = true;
	if (!State->Pawn.IsValid()) return false;
	auto* SourceController = State->Pawn->GetController();
	if (State->bHadController ? !State->OriginalController.IsValid() || SourceController != State->OriginalController.Get() : SourceController != nullptr)
		return false;
	if (State->bOwner)
	{
		auto* Move = State->Pawn->GetBwayCharacterMovement();
		if (!Move || !State->Pawn->IsReplicatingMovement() || State->Pawn->GetLocalRole() != ROLE_AutonomousProxy) return false;
		auto* ClientData = Move->GetPredictionData_Client_Character();
		if (!ClientData) return false;
		const bool bHadPendingMove = ClientData->PendingMove.IsValid();
		const float PendingStartYaw = bHadPendingMove ? ClientData->PendingMove->StartRotation.Yaw : 0.f;
		// Send the pending move normally rather than discarding prediction history
		// or allowing CombineWith to roll this explicit restoration back next tick.
		Move->FlushServerMoves();
		const bool bPendingCleared = !ClientData->PendingMove.IsValid();
		UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: OrientationRestore token=%s peer=%d player=%d pendingMove=%d pendingStartYaw=%.5f pendingCleared=%d readinessSeconds=%.6f actorYaw=%.5f targetYaw=%.5f"),
			*Frame.Token, Frame.Peer, Frame.Source, bHadPendingMove, PendingStartYaw, bPendingCleared,
			State->OrientationReadySince > 0 ? FPlatformTime::Seconds() - State->OrientationReadySince : 0.,
			State->Pawn->GetActorRotation().Yaw, State->OriginalPeerRotation.Yaw);
		if (!bPendingCleared) return false;
	}
	if (SourceController) SourceController->SetControlRotation(State->OriginalControlRotation);
	State->Pawn->SetActorRotation(State->OriginalPeerRotation, ETeleportType::TeleportPhysics);
	State->bPeerOrientationRestored = State->Pawn->GetActorRotation().Equals(State->OriginalPeerRotation, 1.f)
		&& (!SourceController || SourceController->GetControlRotation().Equals(State->OriginalControlRotation, 1.f));
	if (Frame.ReviewRegression)
	{
		const bool bCourseRemoved = BwaySlideReviewCourse::Destroy(State->ReviewCourse);
		State->bPeerOrientationRestored = State->bPeerOrientationRestored && bCourseRemoved;
		TrajectoryRecord(TEXT("ReviewRestored"), FString::Printf(TEXT("\"courseRemoved\":%d,\"courseActors\":%d,\"jumpHold\":%.6f,\"originalJumpHold\":%.6f"),
			bCourseRemoved, State->ReviewCourse.Num(), State->Pawn->JumpMaxHoldTime, State->OriginalJumpHold));
	}
	return State->bPeerOrientationRestored;
}
void ABwaySlideLifecycleProbe::DestroyRelays()
{
	for (const auto& Relay : State->Relays) if (Relay.IsValid() && Relay.Get() != this) Relay->Destroy();
	Destroy();
}

namespace BwaySlidePackaged
{
bool bStarting = false;
bool bClientStarting = false;
void StartTrajectory(const TArray<FString>& Args, bool bPIE, bool bSupportDiagnostic = false, bool bGroundAirDiagnostic = false, bool bReviewRegression = false)
{
	bool bValid = Args.Num() == 1 && !Args[0].IsEmpty() && Args[0].Len() <= 48;
	if (bValid) for (const TCHAR C : Args[0]) if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '-')) bValid = false;
	if (bStarting || !bValid) { UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=invalid-or-duplicate-trajectory-command")); return; }
	const FString Token = Args[0];
	UWorld* StartingWorld = nullptr;
	if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
	{
		auto* World = Context.World();
		if (Context.WorldType != (bPIE ? EWorldType::PIE : EWorldType::Game) || !World || World->GetNetMode() != NM_ListenServer || World->bIsTearingDown) continue;
		if (StartingWorld) { UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=ambiguous-trajectory-listen-world")); return; }
		StartingWorld = World;
	}
	if (!StartingWorld || !StartingWorld->GetGameInstance())
	{ UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=trajectory-listen-world-not-ready token=%s"), *Token); return; }
	for (TActorIterator<ABwaySlideLifecycleProbe> It(StartingWorld); It; ++It)
	{ UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=trajectory-probe-already-running token=%s"), *Token); return; }
	// PIE is dispatched only after the recorded Dorado readiness checkpoint. No
	// pending startup callback may survive PIE stop and mutate a later play session.
	if (bPIE)
	{
		if (!ABwaySlideLifecycleProbe::StartInWorld(StartingWorld, true, Token, bSupportDiagnostic, bGroundAirDiagnostic, bReviewRegression))
			UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=trajectory-pie-gameplay-not-ready token=%s"), *Token);
		return;
	}
	bStarting = true;
	const TWeakObjectPtr<UGameInstance> StartingInstance(StartingWorld->GetGameInstance());
	const double Started = FPlatformTime::Seconds();
	const TSharedRef<bool> Selected = MakeShared<bool>(false);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Started, Selected, Token, StartingInstance, bReviewRegression](float)
	{
		if (!StartingInstance.IsValid())
		{ bStarting = false; UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=trajectory-starting-session-ended token=%s"), *Token); return false; }
		if (FPlatformTime::Seconds() - Started > 120.0)
		{ bStarting = false; UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=trajectory-startup-timeout token=%s"), *Token); return false; }
		if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
		{
			auto* World = Context.World();
			if (Context.WorldType != EWorldType::Game || !World || World->GetGameInstance() != StartingInstance.Get() || World->GetNetMode() != NM_ListenServer || World->bIsTearingDown) continue;
			auto* GS = World->GetGameState<ABwayGameState>();
			if (!*Selected && World->GetMapName().Contains(TEXT("HeroSelect_Staging")) && GS && GS->PlayerArray.Num() == 4
				&& GS->HeroSelectionManager && GS->HeroSelectionPhaseComponent && GS->HeroSelectionManager->IsSelectionActive() && GS->HeroSelectionManager->GetTotalPlayers() == 4)
			{
				int32 Humans = 0;
				for (APlayerState* PS : GS->PlayerArray)
				{
					const auto* PC = PS ? PS->GetPlayerController() : nullptr;
					if (PS && !PS->IsABot() && PC && (PC->IsLocalController() || PC->GetNetConnection())) ++Humans;
				}
				if (Humans == 4)
				{
					*Selected = true;
					GS->HeroSelectionManager->ResolveMissingSelectionsAndLockAll(GS->HeroSelectionPhaseComponent->ResolveFallbackHeroId());
					UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Selection humans=4 registered=4 syntheticSetup=1 physicalFrontend=0"));
				}
			}
			if (ABwaySlideLifecycleProbe::StartInWorld(World, true, Token, false, false, bReviewRegression)) { bStarting = false; return false; }
		}
		return true;
	}), 0.25f);
}
FAutoConsoleCommand TrajectoryHostCommand(TEXT("bway.Test.SlideTrajectoryHost"),
	TEXT("Development packaged sustained authored slide trajectory probe; requires one fresh attempt token."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { StartTrajectory(Args, false); }));
FAutoConsoleCommand ReviewRegressionHostCommand(TEXT("bway.Test.SlideReviewHost"),
	TEXT("Development held-jump and actual concave multi-contact regression; requires fresh token and four peers; original trajectory cases are unchanged."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { StartTrajectory(Args, false, false, false, true); }));
FAutoConsoleCommand ReviewRegressionPIECommand(TEXT("bway.Test.SlideReviewPIE"),
	TEXT("Development four-world PIE slide review regression; ready Dorado Playing and fresh token required."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { StartTrajectory(Args, true, false, false, true); }));
FAutoConsoleCommand TrajectoryPIECommand(TEXT("bway.Test.SlideTrajectoryPIE"),
	TEXT("Development four-world PIE sustained slide trajectory probe; requires ready Dorado Playing and one fresh attempt token; never exits the editor."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { StartTrajectory(Args, true); }));
FAutoConsoleCommand SupportDiagnosticPIECommand(TEXT("bway.Test.SlideSupportDiagnosticPIE"),
	TEXT("Development PIE only: observe at most one second after a rejected ramp support gap; always fails acceptance and restores through fixture cleanup. Requires ready Dorado and a fresh token."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { StartTrajectory(Args, true, true); }));
FAutoConsoleCommand GroundAirDiagnosticPIECommand(TEXT("bway.Test.SlideGroundAirDiagnosticPIE"),
	TEXT("Development PIE only: observe contact-near-ground and bounded separated slide traversal; always fails acceptance and restores through fixture cleanup. Requires ready Dorado and a fresh token."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { StartTrajectory(Args, true, false, true); }));
FAutoConsoleCommand GroundAirDiagnosticCommand(TEXT("bway.Test.SlideGroundAirDiagnostic"),
	TEXT("Development PIE alias for bway.Test.SlideGroundAirDiagnosticPIE with optional token argument."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		TArray<FString> ResolvedArgs = Args;
		if (ResolvedArgs.IsEmpty()) ResolvedArgs.Add(TEXT("goal4-ground-air-diag"));
		StartTrajectory(ResolvedArgs, true, false, true);
	}));
FAutoConsoleCommand HostCommand(TEXT("bway.Test.SlideLifecycleHost"),
	TEXT("Development four-process slide lifecycle probe. Waits for four joined humans, locks via production selection manager, then observes Dorado Playing. Synthetic setup only."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (bStarting) { UE_LOG(LogTemp, Warning, TEXT("BwaySlidePackaged: SetupBlocked reason=start-already-pending")); return; }
		bStarting = true;
		const double Started = FPlatformTime::Seconds();
		const TSharedRef<bool> Selected = MakeShared<bool>(false);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Started, Selected](float)
		{
			if (FPlatformTime::Seconds() - Started > 120.0) { bStarting = false; UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=selection-or-gameplay-startup-timeout")); return false; }
			if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* World = Context.World();
				if (Context.WorldType != EWorldType::Game || !World || World->GetNetMode() != NM_ListenServer) continue;
				auto* GS = World->GetGameState<ABwayGameState>();
				if (!*Selected && World->GetMapName().Contains(TEXT("HeroSelect_Staging")) && GS && GS->PlayerArray.Num() == 4 &&
					GS->HeroSelectionManager && GS->HeroSelectionPhaseComponent && GS->HeroSelectionManager->IsSelectionActive() && GS->HeroSelectionManager->GetTotalPlayers() == 4)
				{
					int32 ReadyHumans = 0;
					for (APlayerState* PS : GS->PlayerArray)
					{
						const auto* Player = Cast<ABwayPlayerState>(PS);
						const auto* PC = Player ? Player->GetPlayerController() : nullptr;
						if (Player && !Player->IsABot() && PC && (PC->IsLocalController() || PC->GetNetConnection())) ++ReadyHumans;
					}
					if (ReadyHumans == 4)
					{
						*Selected = true;
						GS->HeroSelectionManager->ResolveMissingSelectionsAndLockAll(GS->HeroSelectionPhaseComponent->ResolveFallbackHeroId());
						UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: Selection humans=4 registered=4 syntheticSetup=1 physicalFrontend=0"));
					}
				}
				if (ABwaySlideLifecycleProbe::StartInWorld(World)) { bStarting = false; return false; }
			}
			return true;
		}), 0.25f);
	}));
FAutoConsoleCommand ClientCommand(TEXT("bway.Test.SlideLifecycleClient"),
	TEXT("Development slide fixture client startup marker; the owned replicated relay performs the test after travel."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (bClientStarting) return;
		bClientStarting = true;
		const double Started = FPlatformTime::Seconds();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Started](float)
		{
			if (FPlatformTime::Seconds() - Started > 120.0) { bClientStarting = false; UE_LOG(LogTemp, Error, TEXT("BwaySlidePackaged: FAIL category=setup reason=client-startup-timeout")); return false; }
			if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* World = Context.World();
				auto* PC = World && Context.WorldType == EWorldType::Game && World->GetNetMode() == NM_Client ? World->GetFirstPlayerController() : nullptr;
				if (PC && PC->IsLocalController() && PC->PlayerState)
				{
					UE_LOG(LogTemp, Display, TEXT("BwaySlidePackaged: StartupConnected role=client player=%d world=%s"), PC->PlayerState->GetPlayerId(), *World->GetPathName());
					bClientStarting = false; return false;
				}
			}
			return true;
		}), 0.25f);
	}));
}
#else
bool ABwaySlideLifecycleProbe::StartInWorld(UWorld*, bool, const FString&, bool, bool, bool) { return false; }
void ABwaySlideLifecycleProbe::OnTrajectoryMovement(float, FVector, FVector) {}
#endif
