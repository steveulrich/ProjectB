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

	PreviewSecondaryEquipmentComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewSecondaryEquipmentComponent"));
	PreviewSecondaryEquipmentComponent->SetupAttachment(PreviewMeshComponent);
	PreviewSecondaryEquipmentComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewSecondaryEquipmentComponent->SetGenerateOverlapEvents(false);
	PreviewSecondaryEquipmentComponent->SetCanEverAffectNavigation(false);
	PreviewSecondaryEquipmentComponent->SetVisibility(false);

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
		for (UStaticMeshComponent* Comp : { PreviewEquipmentComponent.Get(), PreviewSecondaryEquipmentComponent.Get() })
		{
			if (Comp)
			{
				Comp->SetStaticMesh(nullptr);
				Comp->SetVisibility(false);
			}
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

	const auto UpdatePreviewSlot = [this](
		UStaticMeshComponent* EquipComp,
		const TSoftObjectPtr<UStaticMesh>& EquipMeshSoft,
		const FName& EquipSocket,
		const FTransform& EquipRelativeTransform)
	{
		if (!EquipComp)
		{
			return;
		}
		UStaticMesh* EquipMesh = !EquipMeshSoft.IsNull() ? EquipMeshSoft.LoadSynchronous() : nullptr;
		if (EquipMesh && !EquipSocket.IsNone())
		{
			EquipComp->AttachToComponent(
				PreviewMeshComponent,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				EquipSocket);
			EquipComp->SetStaticMesh(EquipMesh);
			EquipComp->SetRelativeTransform(EquipRelativeTransform);
			EquipComp->SetVisibility(true);
		}
		else
		{
			EquipComp->SetStaticMesh(nullptr);
			EquipComp->SetVisibility(false);
		}
	};

	UpdatePreviewSlot(
		PreviewEquipmentComponent,
		HeroData->HeroEquipmentMesh,
		HeroData->HeroEquipmentSocket,
		HeroData->HeroEquipmentRelativeTransform);
	UpdatePreviewSlot(
		PreviewSecondaryEquipmentComponent,
		HeroData->HeroSecondaryEquipmentMesh,
		HeroData->HeroSecondaryEquipmentSocket,
		HeroData->HeroSecondaryEquipmentRelativeTransform);
}
