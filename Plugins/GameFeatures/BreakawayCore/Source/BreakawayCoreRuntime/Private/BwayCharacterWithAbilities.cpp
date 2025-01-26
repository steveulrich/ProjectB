// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
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