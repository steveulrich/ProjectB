// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroPreviewActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "HeroSystems/BwayHeroDataAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroPreviewActor)

ABwayHeroPreviewActor::ABwayHeroPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PreviewMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMeshComponent"));
	PreviewMeshComponent->SetupAttachment(SceneRoot);
	// Frame character on the right-hand side of the viewport (Y = +65) facing the camera
	PreviewMeshComponent->SetRelativeLocation(FVector(240.0f, 55.0f, -88.0f));
	PreviewMeshComponent->SetRelativeRotation(FRotator(0.0f, 155.0f, 0.0f));
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PreviewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCameraComponent"));
	PreviewCameraComponent->SetupAttachment(SceneRoot);
	PreviewCameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
	PreviewCameraComponent->SetRelativeRotation(FRotator(-4.0f, 0.0f, 0.0f));
	PreviewCameraComponent->FieldOfView = 55.0f;

	KeyLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLightComponent"));
	KeyLightComponent->SetupAttachment(SceneRoot);
	KeyLightComponent->SetRelativeLocation(FVector(120.0f, 90.0f, 110.0f));
	KeyLightComponent->SetIntensity(4500.0f);
	KeyLightComponent->SetAttenuationRadius(800.0f);
	KeyLightComponent->SetLightColor(FLinearColor(1.0f, 0.94f, 0.82f));
}

void ABwayHeroPreviewActor::UpdatePreviewHero(UBwayHeroDataAsset* HeroData)
{
	if (!PreviewMeshComponent)
	{
		return;
	}

	if (!HeroData || !HeroData->HeroMesh)
	{
		PreviewMeshComponent->SetSkeletalMesh(nullptr);
		return;
	}

	PreviewMeshComponent->SetSkeletalMesh(HeroData->HeroMesh);
	if (HeroData->AnimationClass)
	{
		PreviewMeshComponent->SetAnimInstanceClass(HeroData->AnimationClass);
	}
}
