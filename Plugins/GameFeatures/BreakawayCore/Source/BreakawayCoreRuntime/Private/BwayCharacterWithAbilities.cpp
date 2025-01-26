// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayPickupable.h"
#include "BwayThrowable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"


ABwayCharacterWithAbilities::ABwayCharacterWithAbilities(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UBwayCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	BwayCharacterMovementComponent = Cast<UBwayCharacterMovementComponent>(GetCharacterMovement());
	BwayCharacterMovementComponent->SetIsReplicated(true);

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = bUseControllerRotationYaw = bUseControllerRotationRoll = false;

}

FCollisionQueryParams ABwayCharacterWithAbilities::GetIgnoreCharacterParams() const
{
	FCollisionQueryParams Params;

	TArray<AActor*> CharacterChildren;
	GetAllChildActors(CharacterChildren);
	Params.AddIgnoredActors(CharacterChildren);
	Params.AddIgnoredActor(this);

	return Params;
}

void ABwayCharacterWithAbilities::PickUpObject()
{
	FVector Start = GetActorLocation();
	FVector End = Start + GetActorForwardVector() * 200.0f;
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
	{
		if (UBwayPickupable* PickupableComp = HitResult.GetActor()->FindComponentByClass<UBwayPickupable>())
		{
			PickupableComp->PickUp(this);
		}
	}
}

void ABwayCharacterWithAbilities::ThrowHeldObject()
{
	TArray<UBwayPickupable*> PickupableComps;
	GetComponents<UBwayPickupable>(PickupableComps);

	for (UBwayPickupable* PickupableComp : PickupableComps)
	{
		if (PickupableComp->HoldingActor == this)
		{
			if (UBwayThrowable* ThrowableComp = PickupableComp->GetOwner()->FindComponentByClass<UBwayThrowable>())
			{
				FVector ThrowDirection = GetActorForwardVector();
				ThrowableComp->Throw(ThrowDirection, ThrowableComp->DefaultThrowStrength);
				break;
			}
		}
	}
}