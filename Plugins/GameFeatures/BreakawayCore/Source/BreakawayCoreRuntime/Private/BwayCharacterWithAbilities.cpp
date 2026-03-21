// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterWithAbilities.h"
#include "Camera/LyraCameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "BwayCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "AbilitySystemGlobals.h"
#include "BreakawayGameMode.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h" // Assuming Lyra's ASC
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Animation/AnimBlueprint.h" // For UAnimBlueprint


ABwayCharacterWithAbilities::ABwayCharacterWithAbilities(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UBwayCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	BwayCharacterMovementComponent = Cast<UBwayCharacterMovementComponent>(GetCharacterMovement());
	BwayCharacterMovementComponent->SetIsReplicated(true);

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = bUseControllerRotationYaw = bUseControllerRotationRoll = false;

}

void ABwayCharacterWithAbilities::OnDeathStarted(AActor* OwningActor)
{
	if (HasAuthority())
	{
		if (ABreakawayGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakawayGameMode>())
		{
			AController* VictimController = GetController();
			AController* KillerController = nullptr; // Set if you track killer
			GameMode->OnPlayerDied(VictimController, KillerController);
		}
	}
}

void ABwayCharacterWithAbilities::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
	{
		ULyraCameraComponent* CameraComp = ULyraCameraComponent::FindCameraComponent(this);
		if (CameraComp)
		{
			// --- FOV Scaling ---
			const float TargetFOVOffset = MoveComp->IsSliding() ? (MoveComp->GetSlideIntensity() * MoveComp->SlideFOVOffsetMax) : 0.0f;
			CurrentSlideFOVOffset = FMath::FInterpTo(CurrentSlideFOVOffset, TargetFOVOffset, DeltaSeconds, MoveComp->SlideFOVInterpSpeed);

			if (FMath::Abs(CurrentSlideFOVOffset) > KINDA_SMALL_NUMBER)
			{
				CameraComp->AddFieldOfViewOffset(CurrentSlideFOVOffset);
			}

			// --- Camera Shake ---
			if (MoveComp->IsSliding() && MoveComp->SlideCameraShakeClass)
			{
				if (APlayerController* PC = Cast<APlayerController>(GetController()))
				{
					if (PC->PlayerCameraManager)
					{
						// Scale the shake intensity by the slide intensity
						PC->PlayerCameraManager->StartCameraShake(MoveComp->SlideCameraShakeClass, MoveComp->GetSlideIntensity());
					}
				}
			}
		}
	}
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
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: BEGIN for character %s"), *GetName());
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	
	check(HeroData);

	UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: HeroData = %s (DisplayName: %s)"), 
		*GetNameSafe(HeroData), *HeroData->DisplayName.ToString());

	HeroDataAsset = HeroData;

    // Visuals
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (MeshComp)
    {
        UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: Setting HeroMesh = %s"), *GetNameSafe(HeroData->HeroMesh));
        MeshComp->SetSkeletalMesh(HeroData->HeroMesh);
        
        if (HeroData->AnimationBP)
        {
            UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: Setting AnimInstanceClass = %s"), 
                *GetNameSafe(HeroData->AnimationBP->GeneratedClass));
            MeshComp->SetAnimInstanceClass(HeroData->AnimationBP->GeneratedClass);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: HeroData->AnimationBP is null for %s"), *GetNameSafe(HeroData));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHeroData: MeshComponent is NULL!"));
    }

    // Get ASC
    ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent();
    if (!ASC)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHeroData: LyraAbilitySystemComponent is null for %s."), *GetName());
        UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: END (FAILED - No ASC)"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: ASC = %s"), *GetNameSafe(ASC));

    // Ability Sets
    if (!HeroData->AbilitySets.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: Processing %d AbilitySets"), HeroData->AbilitySets.Num());
        
        FLyraAbilitySet_GrantedHandles GrantedHandles;
        for (int32 i = 0; i < HeroData->AbilitySets.Num(); ++i)
        {
            const ULyraAbilitySet* Set = HeroData->AbilitySets[i];
            if (Set)
            {
                UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: Granting AbilitySet[%d] = %s"), i, *GetNameSafe(Set));
                
                // Initialize ability actor info
                ASC->InitAbilityActorInfo(this, this);
                
                // Grant the ability set
                Set->GiveToAbilitySystem(ASC, &GrantedHandles);
                
                UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: AbilitySet[%d] granted successfully"), i);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: AbilitySet[%d] is NULL!"), i);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: HeroData has NO AbilitySets configured!"));
    }

    // Log summary
    TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();
    UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: SUMMARY - Total activatable abilities on ASC: %d"), ActivatableAbilities.Num());
    
    for (const FGameplayAbilitySpec& Spec : ActivatableAbilities)
    {
        if (Spec.Ability)
        {
            FGameplayTagContainer DynamicTags = Spec.GetDynamicSpecSourceTags();
            FString TagsStr = DynamicTags.ToStringSimple();
            UE_LOG(LogTemp, Log, TEXT("  - %s [InputTags: %s]"), 
                *Spec.Ability->GetClass()->GetName(), 
                TagsStr.IsEmpty() ? TEXT("None") : *TagsStr);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: END for character %s"), *GetName());
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
}