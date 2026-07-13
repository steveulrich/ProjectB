// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/GameplayAbilityWorldReticle.h"

#include "BwayWorldReticle_ActorVisualization.generated.h"

class AGameplayAbilityTargetActor;
class UMaterialInterface;
class USkeletalMesh;

/** This is a dummy reticle for internal use by visualization placement tasks. It builds a custom visual model of the visualization being placed. */
UCLASS(notplaceable)
class BREAKAWAYCORERUNTIME_API ABwayWorldReticle_ActorVisualization : public AGameplayAbilityWorldReticle
{
	GENERATED_UCLASS_BODY()

public:
	void InitializeReticleVisualizationInformation(AActor* InTargetingActor, AActor* VisualizationActor, UMaterialInterface* InValidMaterial, UMaterialInterface* InInvalidMaterial);
	/** Lightweight preview path: one skeletal mesh component, no temporary actor spawn. */
	void InitializeReticleVisualizationFromPreviewMesh(AActor* InTargetingActor, USkeletalMesh* PreviewMesh, UMaterialInterface* InValidMaterial, UMaterialInterface* InInvalidMaterial);
	void UpdatePlacementVisuals(bool bIsValid);
	void SetIsReticleVisible(bool bVisible);
private:
	/** Hardcoded collision component, so other objects don't think they can collide with the visualization actor */
	UPROPERTY()
	TObjectPtr<class UCapsuleComponent> CollisionComponent;
	
	UPROPERTY()
	TObjectPtr<UMaterialInterface> CurrentValidMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> CurrentInvalidMaterial;
	
public:

	UPROPERTY()
	TArray<TObjectPtr<UActorComponent>> VisualizationComponents;

	/** Overridable function called whenever this actor is being removed from a level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Returns CollisionComponent subobject **/
	class UCapsuleComponent* GetCollisionComponent() { return CollisionComponent; }

};
