// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h" // Assuming Lyra's ASC



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

// Optional method to update character appearance based on team
void ABwayCharacterWithAbilities::UpdateAppearanceForTeam()
{
	// This would be implemented to change mesh colors, effects, etc.
	// based on the current TeamId
}

void ABwayCharacterWithAbilities::TryPickupOverlappingRelic()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	// Use the character's capsule or another relevant component for overlap checks
	GetCapsuleComponent()->GetOverlappingActors(OverlappingActors, ARelicActor::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		ARelicActor* Relic = Cast<ARelicActor>(OverlappingActor);
		if (Relic && Relic->CanBePickedUpBy(this))
		{
			APlayerState* MyPlayerState = GetPlayerState();
			if (MyPlayerState)
			{
				ULyraAbilitySystemComponent* PlayerStateASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MyPlayerState));
				const URelicSettings* Settings = Relic->GetRelicSettings(); // Assuming a getter for RelicSettings

				if (PlayerStateASC && Settings && Settings->PickupEventTag.IsValid())
				{
					FGameplayEventData Payload;
					Payload.EventTag = Settings->PickupEventTag;
					Payload.Instigator = this;
					Payload.Target = Relic; // Target is the relic

					PlayerStateASC->HandleGameplayEvent(Payload.EventTag, &Payload);
					UE_LOG(LogTemp, Log, TEXT("Server: Sent PickupRelic event from Character overlap check to PlayerState ASC of %s for Relic %s"), *GetNameSafe(MyPlayerState), *GetNameSafe(Relic));

					// Found a valid relic and sent the event, stop checking
					return;
				}
			}
		}
	}
}