// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "BwayTargetActor_ActorPlacementFace.generated.h"

class ABwayWorldReticle_ActorVisualization;
class UGameplayAbility;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FPlacementValidationResult
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FVector AdjustedLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FRotator PlacementRotation = FRotator::ZeroRotator; // Actual rotation for the object

	FHitResult OriginalGroundHit; // Internal, not UPROPERTY for BP
};

UCLASS()
class BREAKAWAYCORERUNTIME_API ABwayTargetActor_ActorPlacementFace : public AGameplayAbilityTargetActor_GroundTrace
{
	GENERATED_UCLASS_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void StartTargeting(UGameplayAbility* InAbility) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Actor we intend to place. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
	TObjectPtr<UClass> PlacedActorClass;
	
	/** Material for visualization when placement is valid. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
	TObjectPtr<UMaterialInterface> ValidPlacementMaterial;

	/** Material for visualization when placement is invalid. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
	TObjectPtr<UMaterialInterface> InvalidPlacementMaterial;
	
	/** Maximum surface angle (in degrees) allowed for placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Validation)
	float MaxPlacementAngleDegrees = 30.0f;

	/** How far apart (in Z) the footprint samples can be. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Validation)
	float MaxFootprintZDifference = 10.0f;
	
	/** How far down from footprint sample points to trace for ground. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Validation)
	float FootprintTraceDownLength = 50.0f;

	/** How much to offset footprint sample points upwards before tracing down. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Validation)
	float FootprintTraceUpOffset = 20.0f;

protected:
	/** Channel used for overhang and obstruction checks */
	UPROPERTY(EditAnywhere, Category="Validation")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Visibility;
	
	/** Visualization for the intended location of the placed actor. */
	TWeakObjectPtr<ABwayWorldReticle_ActorVisualization> ActorVisualizationReticle;

	virtual FHitResult PerformTrace(AActor* InSourceActor) override;

	virtual void ConfirmTargetingAndContinue() override;
	virtual FGameplayAbilityTargetDataHandle CreateTargetData(const FHitResult& InHitResult) const;
	
	FPlacementValidationResult PerformPlacementValidation(const FHitResult& GroundHit) const;
	bool CheckSurfaceAngle(const FVector& SurfaceNormal) const;
	bool CheckOverhangs(const FVector& CenterLocationOnGround, const FRotator& InitialPlacementRotation, FVector& OutAdjustedFootprintCenter, const FVector& SurfaceNormal) const;
	bool CheckObstructions(const FVector& CenterLocation, const FRotator& PlacementRotation) const;

	// To update material in Tick based on NewTraceResult's findings
	mutable bool bLastTickPlacementValid = false;
	mutable FPlacementValidationResult LastValidatedPlacementResult; // Store full result for MakeTargetData

};