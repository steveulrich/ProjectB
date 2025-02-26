// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterMovementComponent.h"

#include "BwayCharacterWithAbilities.h"
#include "Character/LyraHeroComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

float MacroDuration = 2.f;
#define SLOG(x) GEngine->AddOnScreenDebugMessage(-1, MacroDuration ? MacroDuration : -1.f, FColor::Yellow, x);
#define POINT(x, c) DrawDebugPoint(GetWorld(), x, 10, c, !MacroDuration, MacroDuration);
#define LINE(x1, x2, c) DrawDebugLine(GetWorld(), x1, x2, c, !MacroDuration, MacroDuration);
#define CAPSULE(x, c) DrawDebugCapsule(GetWorld(), x, CapHH(), CapR(), FQuat::Identity, c, !MacroDuration, MacroDuration);

#pragma region Saved Move

UBwayCharacterMovementComponent::FSavedMove_Bway::FSavedMove_Bway()
{
	Saved_bPrevWantsToCrouch=0;
}

bool UBwayCharacterMovementComponent::FSavedMove_Bway::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Bway* NewBwayMove = static_cast<FSavedMove_Bway*>(NewMove.Get());
	
	if (Saved_bWantsToSprint != NewBwayMove->Saved_bWantsToSprint)
	{
		return false;
	}

	if (Saved_bWantsToDash != NewBwayMove->Saved_bWantsToDash)
	{
		return false;
	}

	if (Saved_bWallRunIsRight != NewBwayMove->Saved_bWallRunIsRight)
	{
		return false;
	}
	
	return FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);

}

void UBwayCharacterMovementComponent::FSavedMove_Bway::Clear()
{
	FSavedMove_Character::Clear();

	Saved_bWantsToSprint = 0;
	Saved_bWantsToDash = 0;

	Saved_bHadAnimRootMotion = 0;
	Saved_bTransitionFinished = 0;
	
	Saved_bPrevWantsToCrouch = 0;

	Saved_bWallRunIsRight = 0;
}

uint8 UBwayCharacterMovementComponent::FSavedMove_Bway::GetCompressedFlags() const
{
	uint8 Result = FSavedMove_Character::GetCompressedFlags();

	if (Saved_bWantsToSprint) Result |= FLAG_Sprint;
	if (Saved_bWantsToDash) Result |= FLAG_Dash;

	return Result;
}

void UBwayCharacterMovementComponent::FSavedMove_Bway::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	
	const UBwayCharacterMovementComponent* CharacterMovement = Cast<UBwayCharacterMovementComponent>(C->GetCharacterMovement());

	Saved_bPrevWantsToCrouch = CharacterMovement->Safe_bPrevWantsToCrouch;

	Saved_bHadAnimRootMotion = CharacterMovement->Safe_bHadAnimRootMotion;
	Saved_bTransitionFinished = CharacterMovement->Safe_bTransitionFinished;
	Saved_bWantsToSprint = CharacterMovement->Safe_bWantsToSprint;
	Saved_bWantsToDash = CharacterMovement->Safe_bWantsToDash;
	Saved_bWallRunIsRight = CharacterMovement->Safe_bWallRunIsRight;

}

void UBwayCharacterMovementComponent::FSavedMove_Bway::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);

	UBwayCharacterMovementComponent* CharacterMovement = Cast<UBwayCharacterMovementComponent>(C->GetCharacterMovement());

	CharacterMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
	CharacterMovement->Safe_bPrevWantsToCrouch = Saved_bPrevWantsToCrouch;

	CharacterMovement->Safe_bHadAnimRootMotion = Saved_bHadAnimRootMotion;
	CharacterMovement->Safe_bTransitionFinished = Saved_bTransitionFinished;

	CharacterMovement->Safe_bWantsToDash = Saved_bWantsToDash;

	CharacterMovement->Safe_bWallRunIsRight = Saved_bWallRunIsRight;
}

#pragma endregion

#pragma region Client Network Prediction Data

UBwayCharacterMovementComponent::FNetworkPredictionData_Client_Bway::FNetworkPredictionData_Client_Bway(const UCharacterMovementComponent& ClientMovement)
: Super(ClientMovement)
{
}

FSavedMovePtr UBwayCharacterMovementComponent::FNetworkPredictionData_Client_Bway::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Bway());
}

#pragma endregion

UBwayCharacterMovementComponent::UBwayCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	NavAgentProps.bCanCrouch = true;
	BwayServerMoveBitWriter.SetAllowResize(true);
}

void UBwayCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickCount++;
	if (IsNetMode(NM_Client))
	{
		GEngine->AddOnScreenDebugMessage(2, 100.f, FColor::Yellow, FString::Printf(TEXT("Correction: %.2f"), 100.f * (float)CorrectionCount / (float)TickCount));
		GEngine->AddOnScreenDebugMessage(9, 100.f, FColor::Yellow, FString::Printf(TEXT("Bitrate: %.3f"), (float)TotalBitsSent / GetWorld()->GetTimeSeconds() / 1000.f));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(3, 100.f, FColor::Yellow, FString::Printf(TEXT("Location Error: %.4f cm/s"), 100.f * AccumulatedClientLocationError / GetWorld()->GetTimeSeconds()));
	}
}

void UBwayCharacterMovementComponent::InitializeComponent()
{
    Super::InitializeComponent();
    
    BwayCharacterOwner = Cast<ABwayCharacterWithAbilities>(GetOwner());
}

// Network
void UBwayCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	Safe_bWantsToSprint = (Flags & FSavedMove_Bway::FLAG_Sprint) != 0;
	Safe_bWantsToDash = (Flags & FSavedMove_Bway::FLAG_Dash) != 0;
}

void UBwayCharacterMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData,
	float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName,
	bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
									  bHasBase, bBaseRelativePosition,
									  ServerMovementMode, ServerGravityDirection);

	CorrectionCount++;
}

FNetworkPredictionData_Client* UBwayCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr)

	if (ClientPredictionData == nullptr)
	{
		UBwayCharacterMovementComponent* MutableThis = const_cast<UBwayCharacterMovementComponent*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Bway(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f; //TODO Magic numbers
	}
	return ClientPredictionData;
}

bool UBwayCharacterMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsCustomMovementMode(CMOVE_Slide);
}

bool UBwayCharacterMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && IsMovingOnGround();
}

float UBwayCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsMovementMode(MOVE_Walking) && Safe_bWantsToSprint && !IsCrouching()) return MaxSprintSpeed;
	
	if (MovementMode != MOVE_Custom) return Super::GetMaxSpeed();

	switch (CustomMovementMode)
	{
	case CMOVE_Slide:
		return MaxSlideSpeed;
	case CMOVE_WallRun:
		return MaxWallRunSpeed;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
		return -1.f;
	}
}

float UBwayCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (MovementMode != MOVE_Custom) return Super::GetMaxBrakingDeceleration();

	switch (CustomMovementMode)
	{
	case CMOVE_Slide:
		return BrakingDecelerationSliding;
	case CMOVE_WallRun:
		return 0.f;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
		return -1.f;
	}
}

// Movement Pipeline
void UBwayCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Slide
	if (MovementMode == MOVE_Walking && bWantsToCrouch && !Safe_bPrevWantsToCrouch) // Check for the moment crouch is pressed
	{
		if (CanSlide())
		{
			SetMovementMode(MOVE_Custom, CMOVE_Slide);
		}
	}
	else if (IsCustomMovementMode(CMOVE_Slide) && !bWantsToCrouch)
	{
		SetMovementMode(MOVE_Walking);
	}

	// Dash
	bool bAuthProxy = CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled();
	if (Safe_bWantsToDash && CanDash())
	{
		if (!bAuthProxy || GetWorld()->GetTimeSeconds() - DashStartTime > AuthDashCooldownDuration)
		{
			PerformDash();
			Safe_bWantsToDash = false;
			Proxy_bDash = !Proxy_bDash;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Client tried to cheat"))
		}
	}

	// Transition
	if (Safe_bTransitionFinished)
	{
		SLOG("Transition Finished")
		UE_LOG(LogTemp, Warning, TEXT("FINISHED RM"))

		if (TransitionName == "Mantle")
		{
			if (IsValid(TransitionQueuedMontage))
			{
				SetMovementMode(MOVE_Flying);
				CharacterOwner->PlayAnimMontage(TransitionQueuedMontage, TransitionQueuedMontageSpeed);
				TransitionQueuedMontageSpeed = 0.f;
				TransitionQueuedMontage = nullptr;
			}
			else
			{
				SetMovementMode(MOVE_Walking);
			}
		}

		TransitionName = "";
		Safe_bTransitionFinished = false;
	}

	// Wall Run
	if (IsFalling())
	{
		TryWallRun();
	}


	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UBwayCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);

	if (!HasAnimRootMotion() && Safe_bHadAnimRootMotion && IsMovementMode(MOVE_Flying))
	{
		UE_LOG(LogTemp, Warning, TEXT("Ending Anim Root Motion"))
		SetMovementMode(MOVE_Walking);
	}

	if (GetRootMotionSourceByID(TransitionRMS_ID) && GetRootMotionSourceByID(TransitionRMS_ID)->Status.HasFlag(ERootMotionSourceStatusFlags::Finished))
	{
		RemoveRootMotionSourceByID(TransitionRMS_ID);
		Safe_bTransitionFinished = true;
	}

	
	Safe_bHadAnimRootMotion = HasAnimRootMotion();
}

void UBwayCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	switch (CustomMovementMode)
	{
	case CMOVE_Slide:
		PhysSlide(deltaTime, Iterations);
		break;
	case CMOVE_WallRun:
		PhysWallRun(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
	}
}
void UBwayCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	
	Safe_bPrevWantsToCrouch = bWantsToCrouch;
}

// Movement Event
void UBwayCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Slide) ExitSlide();
	
	if (IsCustomMovementMode(CMOVE_Slide)) EnterSlide(PreviousMovementMode, (ECustomMovementMode)PreviousCustomMode);

	if (IsWallRunning() && GetOwnerRole() == ROLE_SimulatedProxy)
	{
		FVector Start = UpdatedComponent->GetComponentLocation();
		FVector End = Start + UpdatedComponent->GetRightVector() * CapR() * 2;
		auto Params = BwayCharacterOwner->GetIgnoreCharacterParams();
		FHitResult WallHit;
		Safe_bWallRunIsRight = GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params);
	}
}

bool UBwayCharacterMovementComponent::ServerCheckClientError(float ClientTimeStamp, float DeltaTime,
	const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
	UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	if (GetCurrentNetworkMoveData()->NetworkMoveType == FCharacterNetworkMoveData::ENetworkMoveType::NewMove)
	{
		float LocationError = FVector::Dist(UpdatedComponent->GetComponentLocation(), ClientWorldLocation);
		GEngine->AddOnScreenDebugMessage(6, 100.f, FColor::Yellow, FString::Printf(TEXT("Loc: %s"), *ClientWorldLocation.ToString()));
		AccumulatedClientLocationError += LocationError * DeltaTime;
	}


	
	return Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation,
	                                     ClientMovementBase,
	                                     ClientBaseBoneName, ClientMovementMode);

}


void UBwayCharacterMovementComponent::CallServerMovePacked(const FSavedMove_Character* NewMove, const FSavedMove_Character* PendingMove, const FSavedMove_Character* OldMove)
{
	// Get storage container we'll be using and fill it with movement data
	FCharacterNetworkMoveDataContainer& MoveDataContainer = GetNetworkMoveDataContainer();
	MoveDataContainer.ClientFillNetworkMoveData(NewMove, PendingMove, OldMove);

	// Reset bit writer without affecting allocations
	FBitWriterMark BitWriterReset;
	BitWriterReset.Pop(BwayServerMoveBitWriter);

	// 'static' to avoid reallocation each invocation
	static FCharacterServerMovePackedBits PackedBits;
	UNetConnection* NetConnection = CharacterOwner->GetNetConnection();	

	// Extract the net package map used for serializing object references.
	BwayServerMoveBitWriter.PackageMap = NetConnection ? ToRawPtr(NetConnection->PackageMap) : nullptr;

	if (BwayServerMoveBitWriter.PackageMap == nullptr)
	{
		UE_LOG(LogNetPlayerMovement, Error, TEXT("CallServerMovePacked: Failed to find a NetConnection/PackageMap for data serialization!"));
		return;
	}

	// Serialize move struct into a bit stream
	if (!MoveDataContainer.Serialize(*this, BwayServerMoveBitWriter, BwayServerMoveBitWriter.PackageMap) || BwayServerMoveBitWriter.IsError())
	{
		UE_LOG(LogNetPlayerMovement, Error, TEXT("CallServerMovePacked: Failed to serialize out movement data!"));
		return;
	}

	// Copy bits to our struct that we can NetSerialize to the server.
	PackedBits.DataBits.SetNumUninitialized(BwayServerMoveBitWriter.GetNumBits());
	
	check(PackedBits.DataBits.Num() >= BwayServerMoveBitWriter.GetNumBits());
	FMemory::Memcpy(PackedBits.DataBits.GetData(), BwayServerMoveBitWriter.GetData(), BwayServerMoveBitWriter.GetNumBytes());

	TotalBitsSent += PackedBits.DataBits.Num();

	// Send bits to server!
	ServerMovePacked_ClientSend(PackedBits);

	MarkForClientCameraUpdate();
}

#pragma region Slide
void UBwayCharacterMovementComponent::EnterSlide(EMovementMode PrevMode, ECustomMovementMode PrevCustomMode)
{
	bWantsToCrouch = true;
	Velocity += Velocity.GetSafeNormal2D();

	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, true, NULL);
}

void UBwayCharacterMovementComponent::ExitSlide()
{
	bWantsToCrouch = false;
}

bool UBwayCharacterMovementComponent::CanSlide() const
{
	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector End = Start + CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.5f * FVector::DownVector;
	FName ProfileName = TEXT("BlockAll");
	bool bValidSurface = GetWorld()->LineTraceTestByProfile(Start, End, ProfileName, BwayCharacterOwner->GetIgnoreCharacterParams());
	
	return bValidSurface;
}

void UBwayCharacterMovementComponent::PhysSlide(float deltaTime, int32 Iterations)
{
    if(deltaTime < MIN_TICK_TIME)
    {
        return;
    }

    if (!CanSlide())
	{
		SetMovementMode(MOVE_Walking);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;

	// Perform the move
	while ( (remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)) )
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		UPrimitiveComponent * const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		// Ensure velocity is horizontal.
		MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;

		FVector SlopeForce = CurrentFloor.HitResult.Normal;
		SlopeForce.Z = 0.f;
		Velocity += SlopeForce * SlideGravityForce * deltaTime;
		
		// Apply directional controlG
		if (AActor* OwningActor = GetOwner())
		{
			if (APawn* ControlledPawn = Cast<APawn>(OwningActor))
			{
				FVector InputVector = ControlledPawn->GetLastMovementInputVector();
				InputVector = UpdatedComponent->GetComponentTransform().TransformVectorNoScale(InputVector).GetSafeNormal2D(); // Convert to world space and normalize

				if (!InputVector.IsZero())
				{
					Velocity += InputVector * SlideDirectionalControlStrength * deltaTime;
				}
			}
		}
		
		Acceleration = Acceleration.ProjectOnTo(UpdatedComponent->GetRightVector().GetSafeNormal2D());

		// Apply acceleration
		CalcVelocity(timeTick, GroundFriction * SlideFrictionFactor, false, GetMaxBrakingDeceleration());
		
		// Compute move parameters
		const FVector MoveVelocity = Velocity; // Cap this
		const FVector Delta = timeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;
		bool bFloorWalkable = CurrentFloor.IsWalkableFloor();

		if ( bZeroDelta )
		{
			remainingTime = 0.f;
		}
		else
		{
			// try to move forward
			MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult);

			if ( IsFalling() )
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > KINDA_SMALL_NUMBER)
				{
					const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
					remainingTime += timeTick * (1.f - FMath::Min(1.f,ActualDist/DesiredDist));
				}
				StartNewPhysics(remainingTime,Iterations);
				return;
			}
			else if ( IsSwimming() ) //just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}


		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges();
		if ( bCheckLedges && !CurrentFloor.IsWalkableFloor() )
		{
			// calculate possible alternate movement
			const FVector GravDir = FVector(0.f,0.f,-1.f);
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, CurrentFloor);
			if ( !NewDelta.IsZero() )
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta / timeTick;
				remainingTime += timeTick;
				continue;
			}
			else
			{
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ( (bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump) )
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
					if (IsMovingOnGround())
					{
						// If still walking, then fall. If not, assume the user set a different mode they want to keep.
						StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
					}
					return;
				}

				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, MAX_FLOOR_DIST);
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true;
			}

			// check if just entered water
			if ( IsSwimming() )
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump) )
				{
					return;
				}
				bCheckedFall = true;
			}
		}
		
		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround() && bFloorWalkable)
		{
			// Make velocity reflect actual move
			if( !bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && timeTick >= MIN_TICK_TIME)
			{
				// TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
				MaintainHorizontalGroundVelocity();
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
	}


	FHitResult Hit;
	FQuat NewRotation = FRotationMatrix::MakeFromXZ(Velocity.GetSafeNormal2D(), FVector::UpVector).ToQuat();
	SafeMoveUpdatedComponent(FVector::ZeroVector, NewRotation, false, Hit);
}

bool UBwayCharacterMovementComponent::GetSlideSurface(FHitResult& Hit) const
{
    FVector Start = UpdatedComponent->GetComponentLocation();
    FVector End = Start + CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f * FVector::DownVector;
    FName ProfileName = TEXT("BlockAll");
    return GetWorld()->LineTraceSingleByProfile(Hit, Start, End, ProfileName, BwayCharacterOwner->GetIgnoreCharacterParams());
}

#pragma endregion
#pragma region Dash

void UBwayCharacterMovementComponent::OnDashCooldownFinished()
{
	Safe_bWantsToDash = true;
}

bool UBwayCharacterMovementComponent::CanDash() const
{
	return IsWalking() && !IsCrouching() || IsFalling();
}

void UBwayCharacterMovementComponent::PerformDash()
{
	DashStartTime = GetWorld()->GetTimeSeconds();
	
	SetMovementMode(MOVE_Flying);
	
	CharacterOwner->PlayAnimMontage(DashMontage);
	
	DashStartDelegate.Broadcast();
}

#pragma endregion

#pragma region Wall Run
bool UBwayCharacterMovementComponent::TryWallRun()
{
	if(!bCanWallRun) return false;
	if (!IsFalling()) return false;
	if (Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2)) return false;
	if (Velocity.Z < -MaxVerticalWallRunSpeed) return false;
	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector LeftEnd = Start - UpdatedComponent->GetRightVector() * CapR() * 2;
	FVector RightEnd = Start + UpdatedComponent->GetRightVector() * CapR() * 2;
	auto Params = BwayCharacterOwner->GetIgnoreCharacterParams();
	FHitResult FloorHit, WallHit;
	// Check Player Height
	if (GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (CapHH() + MinWallRunHeight), "BlockAll", Params))
	{
		return false;
	}
	
	// Left Cast
	GetWorld()->LineTraceSingleByProfile(WallHit, Start, LeftEnd, "BlockAll", Params);
	if (WallHit.IsValidBlockingHit() && (Velocity | WallHit.Normal) < 0)
	{
		Safe_bWallRunIsRight = false;
	}
	// Right Cast
	else
	{
		GetWorld()->LineTraceSingleByProfile(WallHit, Start, RightEnd, "BlockAll", Params);
		if (WallHit.IsValidBlockingHit() && (Velocity | WallHit.Normal) < 0)
		{
			Safe_bWallRunIsRight = true;
		}
		else
		{
			return false;
		}
	}
	FVector ProjectedVelocity = FVector::VectorPlaneProject(Velocity, WallHit.Normal);
	if (ProjectedVelocity.SizeSquared2D() < pow(MinWallRunSpeed, 2)) return false;
	
	// Passed all conditions
	Velocity = ProjectedVelocity;
	Velocity.Z = FMath::Clamp(Velocity.Z, 0.f, MaxVerticalWallRunSpeed);
	SetMovementMode(MOVE_Custom, CMOVE_WallRun);
SLOG("Starting WallRun")
	return true;
}

void UBwayCharacterMovementComponent::PhysWallRun(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}
	
	bJustTeleported = false;
	float remainingTime = deltaTime;
	// Perform the move
	while ( (remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)) )
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		
		FVector Start = UpdatedComponent->GetComponentLocation();
		FVector CastDelta = UpdatedComponent->GetRightVector() * CapR() * 2;
		FVector End = Safe_bWallRunIsRight ? Start + CastDelta : Start - CastDelta;
		auto Params = BwayCharacterOwner->GetIgnoreCharacterParams();
		float SinPullAwayAngle = FMath::Sin(FMath::DegreesToRadians(WallRunPullAwayAngle));
		FHitResult WallHit;
		GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params);
		bool bWantsToPullAway = WallHit.IsValidBlockingHit() && !Acceleration.IsNearlyZero() && (Acceleration.GetSafeNormal() | WallHit.Normal) > SinPullAwayAngle;
		if (!WallHit.IsValidBlockingHit() || bWantsToPullAway)
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}
		// Clamp Acceleration
		Acceleration = FVector::VectorPlaneProject(Acceleration, WallHit.Normal);
		Acceleration.Z = 0.f;
		// Apply acceleration
		CalcVelocity(timeTick, 0.f, false, GetMaxBrakingDeceleration());
		Velocity = FVector::VectorPlaneProject(Velocity, WallHit.Normal);
		float TangentAccel = Acceleration.GetSafeNormal() | Velocity.GetSafeNormal2D();
		bool bVelUp = Velocity.Z > 0.f;
		Velocity.Z += GetGravityZ() * WallRunGravityScaleCurve->GetFloatValue(bVelUp ? 0.f : TangentAccel) * timeTick;
		if (Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2) || Velocity.Z < -MaxVerticalWallRunSpeed)
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}
		
		// Compute move parameters
		const FVector Delta = timeTick * Velocity; // dx = v * dt
		const bool bZeroDelta = Delta.IsNearlyZero();
		if ( bZeroDelta )
		{
			remainingTime = 0.f;
		}
		else
		{
			FHitResult Hit;
			SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
			FVector WallAttractionDelta = -WallHit.Normal * WallAttractionForce * timeTick;
			SafeMoveUpdatedComponent(WallAttractionDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
		}
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick; // v = dx / dt
	}

	
	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector CastDelta = UpdatedComponent->GetRightVector() * CapR() * 2;
	FVector End = Safe_bWallRunIsRight ? Start + CastDelta : Start - CastDelta;
	auto Params = BwayCharacterOwner->GetIgnoreCharacterParams();
	FHitResult FloorHit, WallHit;
	GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params);
	GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (CapHH() + MinWallRunHeight * .5f), "BlockAll", Params);
	if (FloorHit.IsValidBlockingHit() || !WallHit.IsValidBlockingHit() || Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2))
	{
		SetMovementMode(MOVE_Falling);
	}
}

#pragma endregion


#pragma region Helpers

bool UBwayCharacterMovementComponent::IsServer() const
{
	return CharacterOwner->HasAuthority();
}

float UBwayCharacterMovementComponent::CapR() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UBwayCharacterMovementComponent::CapHH() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

#pragma endregion

#pragma region Interface

void UBwayCharacterMovementComponent::SprintPressed()
{
	Safe_bWantsToSprint = true;
}
void UBwayCharacterMovementComponent::SprintReleased()
{
	Safe_bWantsToSprint = false;
}

void UBwayCharacterMovementComponent::CrouchPressed()
{
	bWantsToCrouch = !bWantsToCrouch;
}
void UBwayCharacterMovementComponent::CrouchReleased()
{
}

void UBwayCharacterMovementComponent::DashPressed()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - DashStartTime >= DashCooldownDuration)
	{
		Safe_bWantsToDash = true;
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_DashCooldown, this, &UBwayCharacterMovementComponent::OnDashCooldownFinished, DashCooldownDuration - (CurrentTime - DashStartTime));
	}
}
void UBwayCharacterMovementComponent::DashReleased()
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_DashCooldown);
	Safe_bWantsToDash = false;
}

bool UBwayCharacterMovementComponent::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}
bool UBwayCharacterMovementComponent::IsMovementMode(EMovementMode InMovementMode) const
{
	return InMovementMode == MovementMode;
}

#pragma endregion


#pragma region Replication

void UBwayCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UBwayCharacterMovementComponent, Proxy_bDash, COND_SkipOwner)
	
}

void UBwayCharacterMovementComponent::OnRep_Dash()
{
	CharacterOwner->PlayAnimMontage(DashMontage);
	DashStartDelegate.Broadcast();
}

#pragma endregion