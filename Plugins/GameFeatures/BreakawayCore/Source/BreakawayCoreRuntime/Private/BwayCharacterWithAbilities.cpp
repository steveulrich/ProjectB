// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h" // Assuming Lyra's ASC
#include "BwayHeroDataAsset.h"
#include "Animation/AnimBlueprint.h" // For UAnimBlueprint


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

void ABwayCharacterWithAbilities::InitializeHeroData(const UBwayHeroDataAsset* HeroData)
{
	check(HeroData);

	HeroDataAsset = HeroData;

    // Visuals
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (MeshComp)
    {
        MeshComp->SetSkeletalMesh(HeroData->HeroMesh);
        if (HeroData->AnimationBP)
        {
            MeshComp->SetAnimInstanceClass(HeroData->AnimationBP->GeneratedClass);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: HeroData->AnimationBP is null for %s"), *GetNameSafe(HeroData));
        }
    }

    // Attributes
    // The InitAttributeSet function must be declared in ABwayCharacterWithAbilities.h
    // and implemented in ABwayCharacterWithAbilities.cpp.
    // It should use the AbilitySystemComponent to initialize attributes.
    // For example: void ABwayCharacterWithAbilities::InitAttributeSet(ULyraAbilitySystemComponent* ASC, TSubclassOf<UAttributeSet> AttributeSetClass, float BaseHealth, float BaseSpeed);
    //const float Health = HeroData->MaxHealth;
    //const float Speed = HeroData->MoveSpeed;
    
    ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent();
    if (!ASC)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHeroData: LyraAbilitySystemComponent is null for %s."), *GetName());
        return;
    }

    // Assuming InitAttributeSet is a member function of ABwayCharacterWithAbilities
    // and it correctly uses the ASC to initialize attributes from HeroData->AttributeSetClass
    // You might need to pass ASC to it, or it retrieves it internally.
    // For example: this->InitAttributeSet(ASC, HeroData->AttributeSetClass, Health, Speed);
    // If InitAttributeSet is defined and handles ASC internally:
    //this->InitAttributeSet(HeroData->AttributeSetClass, Health, Speed);

    // Ability Sets
    if (!HeroData->AbilitySets.IsEmpty())
    {
        FLyraAbilitySet_GrantedHandles GrantedHandles; // Declare GrantedHandles here
        for (const ULyraAbilitySet* Set : HeroData->AbilitySets)
        {
            if (Set)
            {
                // InitAbilityActorInfo should be called on the ASC.
                // It's often called once when the ASC is initialized or the avatar changes.
                // If it's already been called appropriately elsewhere, this specific call might be redundant
                // or only needed if the avatar/owner wasn't set previously.
                ASC->InitAbilityActorInfo(this, this); 
                Set->GiveToAbilitySystem(ASC, &GrantedHandles);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: Found a null AbilitySet in HeroData for %s"), *GetNameSafe(HeroData));
            }
        }
    }
}
