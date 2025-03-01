// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"


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

void ABwayCharacterWithAbilities::SetTeamId(int32 NewTeamId)
{
	// Only the server can set the team ID
	if (HasAuthority())
	{
		TeamId = NewTeamId;
        
		// Update team tag based on ID
		switch (TeamId)
		{
		case 0:
			TeamTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.Team.1"));
			break;
		case 1:
			TeamTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.Team.2"));
			break;
		default:
			TeamTag = FGameplayTag();
			break;
		}
        
		// Optional: Update character appearance based on team
		UpdateAppearanceForTeam();
	}
}

void ABwayCharacterWithAbilities::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
	// Replicate team ID to all clients
	DOREPLIFETIME(ABwayCharacterWithAbilities, TeamId);
}

// Optional method to update character appearance based on team
void ABwayCharacterWithAbilities::UpdateAppearanceForTeam()
{
	// This would be implemented to change mesh colors, effects, etc.
	// based on the current TeamId
}