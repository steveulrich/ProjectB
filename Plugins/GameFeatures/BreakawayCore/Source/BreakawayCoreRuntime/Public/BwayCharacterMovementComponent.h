// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterWithAbilities.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "BwayCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDashStartDelegate);

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None			UMETA(Hidden),
	CMOVE_Slide			UMETA(DisplayName = "Slide"),
	CMOVE_WallRun		UMETA(DisplayName = "Wall Run"),
	CMOVE_MAX			UMETA(Hidden),
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayCharacterMovementComponent : public ULyraCharacterMovementComponent
{
	GENERATED_BODY()

	class FSavedMove_Bway : public FSavedMove_Character
	{
	public:
		enum CompressedFlags
		{
			FLAG_Sprint			= 0x10,
			FLAG_Dash			= 0x20,
			FLAG_Custom_2		= 0x40,
			FLAG_Custom_3		= 0x80,
		};
		
		// Flags
		uint8 Saved_bWantsToSprint:1;
		uint8 Saved_bWantsToDash:1;

		// Other Variables
		uint8 Saved_bHadAnimRootMotion:1;
		uint8 Saved_bTransitionFinished:1;
		uint8 Saved_bPrevWantsToCrouch:1;
		uint8 Saved_bWallRunIsRight:1;

		FSavedMove_Bway();

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(ACharacter* C) override;
	};

	class FNetworkPredictionData_Client_Bway : public FNetworkPredictionData_Client_Character
	{
	public:
		FNetworkPredictionData_Client_Bway(const UCharacterMovementComponent& ClientMovement);

		typedef FNetworkPredictionData_Client_Character Super;

		virtual FSavedMovePtr AllocateNewMove() override;
	};
	
public:
	// Parameters
	UPROPERTY(EditDefaultsOnly) float MaxSprintSpeed=750.f;

	// Slide
	UPROPERTY(EditDefaultsOnly) float MinSpeedToEnterSlide=400.f;
	UPROPERTY(EditDefaultsOnly) float MaxSlideSpeed=400.f;
	UPROPERTY(EditDefaultsOnly) float SlideEnterImpulse=400.f;
	UPROPERTY(EditDefaultsOnly) float SlideGravityForce=4000.f;
	UPROPERTY(EditDefaultsOnly) float SlideFrictionFactor=.06f;
	UPROPERTY(EditDefaultsOnly) float BrakingDecelerationSliding=1000.f;
	UPROPERTY(EditDefaultsOnly) TObjectPtr<UAnimMontage> SlideMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Slide) float SlideDirectionalControlStrength = 100.0f;

	// Dash
	UPROPERTY(EditDefaultsOnly) float DashCooldownDuration=1.f;
	UPROPERTY(EditDefaultsOnly) float AuthDashCooldownDuration=.9f;
	UPROPERTY(EditDefaultsOnly) TObjectPtr<UAnimMontage> DashMontage;

	// Wall Run
	UPROPERTY(EditDefaultsOnly) bool bCanWallRun=false;
	UPROPERTY(EditDefaultsOnly) float MinWallRunSpeed=200.f;
	UPROPERTY(EditDefaultsOnly) float MaxWallRunSpeed=800.f;
	UPROPERTY(EditDefaultsOnly) float MaxVerticalWallRunSpeed=200.f;
	UPROPERTY(EditDefaultsOnly) float WallRunPullAwayAngle=75;
	UPROPERTY(EditDefaultsOnly) float WallAttractionForce = 200.f;
	UPROPERTY(EditDefaultsOnly) float MinWallRunHeight=50.f;
	UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> WallRunGravityScaleCurve;
	UPROPERTY(EditDefaultsOnly) float WallJumpOffForce = 300.f;

	// Transient
	UPROPERTY(Transient) TObjectPtr<ABwayCharacterWithAbilities> BwayCharacterOwner;

	// Flags
	bool Safe_bWantsToSprint;
	bool Safe_bWantsToProne;
	bool Safe_bWantsToDash;

	bool Safe_bHadAnimRootMotion;
	bool Safe_bPrevWantsToCrouch;

	float DashStartTime;
	FTimerHandle TimerHandle_EnterProne;
	FTimerHandle TimerHandle_DashCooldown;

	bool Safe_bTransitionFinished;
	TSharedPtr<FRootMotionSource_MoveToForce> TransitionRMS;
	FString TransitionName;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> TransitionQueuedMontage;
	float TransitionQueuedMontageSpeed;
	int TransitionRMS_ID;

	bool Safe_bWallRunIsRight;

	float AccumulatedClientLocationError=0.f;


	int TickCount=0;
	int CorrectionCount=0;
	int TotalBitsSent=0;

	// Replication
	UPROPERTY(ReplicatedUsing=OnRep_Dash) bool Proxy_bDash;
	
	// Delegates
public:

	UPROPERTY(BlueprintAssignable) FDashStartDelegate DashStartDelegate;
public:
	UBwayCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);
	virtual	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void InitializeComponent() override;

public:
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual bool IsMovingOnGround() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;

public:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
protected:
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	FNetBitWriter BwayServerMoveBitWriter;

	virtual void CallServerMovePacked(const FSavedMove_Character* NewMove, const FSavedMove_Character* PendingMove, const FSavedMove_Character* OldMove) override;

public:
	
	// Slide state variables
	bool bWantsToSlide = false; 
	FTimerHandle SlideTimerHandle;
	
	//Slide
private:
	void EnterSlide(EMovementMode PrevMode, ECustomMovementMode PrevCustomMode);
	void ExitSlide();
	bool CanSlide() const;
	void PhysSlide(float deltaTime, int32 Iterations);
	bool GetSlideSurface(FHitResult& Hit) const;

	//Dash
private:
	void OnDashCooldownFinished();

	bool CanDash() const;
	void PerformDash();

	// Wall Run
private:
	bool TryWallRun();
	void PhysWallRun(float deltaTime, int32 Iterations);
	
	// Helpers
private:
	bool IsServer() const;
	float CapR() const;
	float CapHH() const;

public:
	UFUNCTION(BlueprintCallable) void SprintPressed();
	UFUNCTION(BlueprintCallable) void SprintReleased();

	UFUNCTION(BlueprintCallable) void CrouchPressed();
	UFUNCTION(BlueprintCallable) void CrouchReleased();
	
	UFUNCTION(BlueprintCallable) void DashPressed();
	UFUNCTION(BlueprintCallable) void DashReleased();
	
	UFUNCTION(BlueprintPure) bool IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const;
	UFUNCTION(BlueprintPure) bool IsMovementMode(EMovementMode InMovementMode) const;

	UFUNCTION(BlueprintPure) bool IsWallRunning() const { return IsCustomMovementMode(CMOVE_WallRun); }
	UFUNCTION(BlueprintPure) bool WallRunningIsRight() const { return Safe_bWallRunIsRight; }

	UFUNCTION(BlueprintPure) bool IsSliding() const { return IsCustomMovementMode(CMOVE_Slide); }

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	UFUNCTION() void OnRep_Dash();
};
