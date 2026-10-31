// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroPreviewActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	// Frame full-body character (including tall headdresses and held equipment) in 3/4 front view on the right side of the viewport
	PreviewMeshComponent->SetRelativeLocation(FVector(450.0f, 142.0f, -126.0f));
	PreviewMeshComponent->SetRelativeRotation(FRotator(0.0f, 82.0f, 0.0f));
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PreviewEquipmentComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewEquipmentComponent"));
	PreviewEquipmentComponent->SetupAttachment(PreviewMeshComponent);
	PreviewEquipmentComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewEquipmentComponent->SetGenerateOverlapEvents(false);
	PreviewEquipmentComponent->SetCanEverAffectNavigation(false);
	PreviewEquipmentComponent->SetVisibility(false);

	PreviewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCameraComponent"));
	PreviewCameraComponent->SetupAttachment(SceneRoot);
	PreviewCameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
	PreviewCameraComponent->SetRelativeRotation(FRotator(-2.0f, 0.0f, 0.0f));
	PreviewCameraComponent->FieldOfView = 60.0f;

	KeyLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLightComponent"));
	KeyLightComponent->SetupAttachment(SceneRoot);
	KeyLightComponent->SetRelativeLocation(FVector(250.0f, 135.0f, 120.0f));
	KeyLightComponent->SetIntensity(8500.0f);
	KeyLightComponent->SetAttenuationRadius(1200.0f);
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
		if (PreviewEquipmentComponent)
		{
			PreviewEquipmentComponent->SetStaticMesh(nullptr);
			PreviewEquipmentComponent->SetVisibility(false);
		}
		return;
	}

	PreviewMeshComponent->SetSkeletalMesh(HeroData->HeroMesh);
	if (HeroData->AnimationClass)
	{
		PreviewMeshComponent->SetAnimInstanceClass(HeroData->AnimationClass);
		PreviewMeshComponent->InitAnim(true);
		if (HeroData->AnimationLayerClass)
		{
			PreviewMeshComponent->LinkAnimClassLayers(HeroData->AnimationLayerClass);
		}
	}

	if (PreviewEquipmentComponent)
	{
		UStaticMesh* EquipMesh = !HeroData->HeroEquipmentMesh.IsNull() ? HeroData->HeroEquipmentMesh.LoadSynchronous() : nullptr;
		if (EquipMesh && !HeroData->HeroEquipmentSocket.IsNone())
		{
			PreviewEquipmentComponent->AttachToComponent(
				PreviewMeshComponent,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				HeroData->HeroEquipmentSocket);
			PreviewEquipmentComponent->SetStaticMesh(EquipMesh);
			PreviewEquipmentComponent->SetRelativeTransform(HeroData->HeroEquipmentRelativeTransform);
			PreviewEquipmentComponent->SetVisibility(true);
		}
		else
		{
			PreviewEquipmentComponent->SetStaticMesh(nullptr);
			PreviewEquipmentComponent->SetVisibility(false);
		}
	}
}
