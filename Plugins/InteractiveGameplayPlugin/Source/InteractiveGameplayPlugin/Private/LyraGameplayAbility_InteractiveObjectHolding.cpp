// LyraGameplayAbility_InteractiveObjectHolding.cpp
#include "LyraGameplayAbility_InteractiveObjectHolding.h"
#include "InteractivePhysicsObject.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Player/LyraPlayerState.h"

ULyraGameplayAbility_InteractiveObjectHolding::ULyraGameplayAbility_InteractiveObjectHolding()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

UInteractivePhysicsObject* ULyraGameplayAbility_InteractiveObjectHolding::GetHeldObject() const
{
	if (ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(GetOwningActorFromActorInfo()))
	{
		// Find the object we're holding by checking nearby components
		// We might want to cache this or store it directly in the player state later
		if (APawn* Pawn = LyraPS->GetPawn())
		{
			TArray<UInteractivePhysicsObject*> AttachedObjects;
			Pawn->GetComponents<UInteractivePhysicsObject>(AttachedObjects);
            
			for (UInteractivePhysicsObject* Object : AttachedObjects)
			{
				if (Object->GetCurrentHolder() == LyraPS)
				{
					return Object;
				}
			}
		}
	}
	return nullptr;
}

void ULyraGameplayAbility_InteractiveObjectHolding::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicated, bool bWasCancelled)
{
	// Your code to end the ability goes here 
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicated, bWasCancelled);
}