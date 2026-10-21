// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayHeroPreviewActor.generated.h"

class USkeletalMeshComponent;
class UCameraComponent;
class UPointLightComponent;
class UBwayHeroDataAsset;

/**
 * 3D Character Preview Actor spawned during Hero Selection (Phase 4).
 * Positions the selected hero's SkeletalMesh and Animation Class in front of a dedicated
 * Hero Select camera offset so the right side of the Hero Selection screen frames the character.
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API ABwayHeroPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ABwayHeroPreviewActor();

	/** Updates the 3D skeletal mesh and animation instance to match the given HeroDataAsset */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection|Preview")
	void UpdatePreviewHero(UBwayHeroDataAsset* HeroData);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection|Preview")
	USkeletalMeshComponent* GetPreviewMeshComponent() const { return PreviewMeshComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection|Preview")
	UCameraComponent* GetPreviewCameraComponent() const { return PreviewCameraComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero Selection|Preview")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero Selection|Preview")
	TObjectPtr<USkeletalMeshComponent> PreviewMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero Selection|Preview")
	TObjectPtr<UCameraComponent> PreviewCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero Selection|Preview")
	TObjectPtr<UPointLightComponent> KeyLightComponent;
};
