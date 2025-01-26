// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayPickupable.h"
#include "Net/UnrealNetwork.h"


// Implementation of PickupableComponent
UBwayPickupable::UBwayPickupable() 
{
	SetIsReplicatedByDefault(true);
	AttachSocketName = FName("hand_r");
}

void UBwayPickupable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBwayPickupable, HoldingActor);
}

void UBwayPickupable::PickUp(AActor* PickupActor)
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		// Implement ServerPickUp RPC here
		return;
	}

	HoldingActor = PickupActor;
	AActor* Owner = GetOwner();
	if (Owner && PickupActor)
	{
		Owner->AttachToComponent(PickupActor->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			PrimComp->SetSimulatePhysics(false);
		}
	}
}

void UBwayPickupable::Drop()
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		// Implement ServerDrop RPC here
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner)
	{
		Owner->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			PrimComp->SetSimulatePhysics(true);
		}
	}
	HoldingActor = nullptr;
}

void UBwayPickupable::OnRep_HoldingActor()
{
	if (HoldingActor)
	{
		PickUp(HoldingActor);
	}
	else
	{
		Drop();
	}
}

