// Copyright Epic Games, Inc. All Rights Reserved.

#include "Match/BwayMidfieldDividerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMidfieldDividerActor)

ABwayMidfieldDividerActor::ABwayMidfieldDividerActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
	DividerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DividerMesh"));
	DividerMesh->SetupAttachment(RootComponent);
	DividerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DividerMesh->SetGenerateOverlapEvents(false);
	DividerMesh->SetCastShadow(false);
	DividerMesh->SetVisibility(false, true);
	DividerMesh->SetHiddenInGame(true, true);

	// Default dev fallback mesh — override in B_BW_MidfieldDivider for production art.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshFinder.Succeeded())
	{
		DividerMesh->SetStaticMesh(PlaneMeshFinder.Object);
	}

	// The ground stripe spans local Y; its thin local X axis follows the goal axis.
	// Keep rotation on the actor so the authored Blueprint's floor styling survives.
	DividerMesh->SetRelativeRotation(FRotator::ZeroRotator);
	DividerMesh->SetRelativeScale3D(FVector(0.1f, 50.0f, 1.0f));
}

void ABwayMidfieldDividerActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwayMidfieldDividerActor, bSuddenDeathVisualActive);
}

void ABwayMidfieldDividerActor::SetSuddenDeathVisualActive(const bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bSuddenDeathVisualActive == bActive)
	{
		return;
	}

	bSuddenDeathVisualActive = bActive;
	OnRep_SuddenDeathVisualActive();
}

void ABwayMidfieldDividerActor::OnRep_SuddenDeathVisualActive()
{
	ApplyVisualState();
}

void ABwayMidfieldDividerActor::ApplyVisualState()
{
	if (!DividerMesh)
	{
		return;
	}

	const bool bShow = bSuddenDeathVisualActive;
	DividerMesh->SetVisibility(bShow, true);
	DividerMesh->SetHiddenInGame(!bShow, true);
}
