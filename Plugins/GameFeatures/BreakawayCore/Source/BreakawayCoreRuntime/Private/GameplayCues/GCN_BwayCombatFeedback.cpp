// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayCues/GCN_BwayCombatFeedback.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GCN_BwayCombatFeedback)

UGCN_BwayCombatFeedback::UGCN_BwayCombatFeedback()
{
}

bool UGCN_BwayCombatFeedback::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return false;
	}

	FVector EffectLocation = MyTarget->GetActorLocation();
	if (!Parameters.Location.IsNearlyZero())
	{
		EffectLocation = FVector(Parameters.Location);
	}

	// Play feedback sound
	if (FeedbackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, FeedbackSound, EffectLocation);
	}

	// Apply camera shake to the instigator
	if (FeedbackCameraShake)
	{
		// Get the instigator's player controller for client camera shake
		if (const AActor* Instigator = Parameters.EffectContext.GetInstigator())
		{
			if (const APawn* InstigatorPawn = Cast<APawn>(Instigator))
			{
				if (APlayerController* PC = Cast<APlayerController>(InstigatorPawn->GetController()))
				{
					PC->ClientStartCameraShake(FeedbackCameraShake, CameraShakeScale);
				}
			}
		}
	}

	// Spawn VFX
	if (FeedbackVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			MyTarget,
			FeedbackVFX,
			EffectLocation,
			Parameters.Normal.Rotation(),
			FVector::OneVector,
			true,
			true,
			ENCPoolMethod::AutoRelease
		);
	}

	return true;
}
