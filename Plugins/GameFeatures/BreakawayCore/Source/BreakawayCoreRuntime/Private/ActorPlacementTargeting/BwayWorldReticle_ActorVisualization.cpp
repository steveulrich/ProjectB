// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorPlacementTargeting/BwayWorldReticle_ActorVisualization.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayWorldReticle_ActorVisualization)

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	AGAWorldReticle_ActorVisualization
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

ABwayWorldReticle_ActorVisualization::ABwayWorldReticle_ActorVisualization(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CollisionComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule0"));
	CollisionComponent->InitCapsuleSize(0.f, 0.f);
	CollisionComponent->SetCanEverAffectNavigation(false);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = CollisionComponent;

	PrimaryActorTick.bCanEverTick = false; // This reticle itself doesn't need to tick; TargetActor drives it.
}


void ABwayWorldReticle_ActorVisualization::InitializeReticleVisualizationInformation(AActor* InTargetingActor, AActor* VisualizationActor, UMaterialInterface *InValidMaterial, UMaterialInterface* InInvalidMaterial)
{
	CurrentValidMaterial = InValidMaterial;
	CurrentInvalidMaterial = InInvalidMaterial;
	VisualizationComponents.Empty(); // Clear any previous components
	
	if (VisualizationActor)
	{
		//Get components
		TInlineComponentArray<UMeshComponent*> MeshComps;
		USceneComponent* MyRoot = GetRootComponent();
		VisualizationActor->GetComponents(MeshComps);
		check(MyRoot);

		TargetingActor = Cast<AGameplayAbilityTargetActor>(InTargetingActor);
		AddTickPrerequisiteActor(TargetingActor);		//We want the reticle to tick after the targeting actor so that designers have the final say on the position

		for (UMeshComponent* MeshComp : MeshComps)
		{
			//Special case: If we don't clear the root component explicitly, the component will be destroyed along with the original visualization actor.
			if (MeshComp == VisualizationActor->GetRootComponent())
			{
				VisualizationActor->SetRootComponent(NULL);
			}

			//Disable collision on visualization mesh parts so it doesn't interfere with aiming or any other client-side collision/prediction/physics stuff
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);		//All mesh components are primitive components, so no cast is needed

			// Detach, rename (re-parent), attach
			//Move components from one actor to the other, attaching as needed. Hierarchy should not be important, but we can do fixups if it becomes important later.
			MeshComp->Rename(nullptr, this);
			MeshComp->RegisterComponent();
			MeshComp->AttachToComponent(MyRoot, FAttachmentTransformRules::KeepRelativeTransform);

			//MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			
			VisualizationComponents.Add(MeshComp); // Store for material updates

			// Apply initial material (assume valid to start, or a default)
			if (CurrentValidMaterial)
			{
				MeshComp->SetMaterial(0, CurrentValidMaterial);
			}
			else if (CurrentInvalidMaterial) // Fallback if valid isn't set but invalid is
			{
				MeshComp->SetMaterial(0, CurrentInvalidMaterial);
			}
		}
	}
}

void ABwayWorldReticle_ActorVisualization::SetIsReticleVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
}

void ABwayWorldReticle_ActorVisualization::UpdatePlacementVisuals(bool bIsValid)
{
	UMaterialInterface* MaterialToApply = bIsValid ? CurrentValidMaterial : CurrentInvalidMaterial;

	// If the desired material (e.g. invalid) is null, fallback to the other (e.g. valid)
	// to ensure something is always shown if at least one material is set.
	if (!MaterialToApply)
	{
		MaterialToApply = bIsValid ? CurrentInvalidMaterial : CurrentValidMaterial; // Try the other one
		if (!MaterialToApply) return; // Both are null, nothing to do
	}

	for (TObjectPtr<UActorComponent>& Comp : VisualizationComponents)
	{
		if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Comp.Get()))
		{
			if (MeshComp->GetMaterial(0) != MaterialToApply) // Avoid redundant calls
			{
				MeshComp->SetMaterial(0, MaterialToApply);
			}
		}
	}
}

void ABwayWorldReticle_ActorVisualization::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}
