// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayThrowable.h"

#include "BwayPickupable.h"
#include "GameFramework/ProjectileMovementComponent.h"


UBwayThrowable::UBwayThrowable()
{
	SetIsReplicatedByDefault(true);
}

void UBwayThrowable::Throw(FVector ThrowDirection, float Strength)
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerThrow(ThrowDirection, Strength);
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner)
	{
		if (UBwayPickupable* PickupableComp = Owner->FindComponentByClass<UBwayPickupable>())
		{
			PickupableComp->Drop();
		}
		if (UProjectileMovementComponent* ProjectileComp = Owner->FindComponentByClass<UProjectileMovementComponent>())
		{
			ProjectileComp->Velocity = ThrowDirection * Strength;
		}
	}
}

void UBwayThrowable::ServerThrow_Implementation(FVector ThrowDirection, float Strength)
{
	Throw(ThrowDirection, Strength);
}

bool UBwayThrowable::ServerThrow_Validate(FVector ThrowDirection, float Strength)
{
	return true; // Add validation logic if needed
}