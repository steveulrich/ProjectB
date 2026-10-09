#include "GameplayCues/BwayGameplayCueNotify_PrimaryAction.h"

#include "Animation/AnimMontage.h"
#include "BwayCharacterWithAbilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayCueNotify_PrimaryAction)

bool UBwayGameplayCueNotify_PrimaryAction::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	const TWeakObjectPtr<ABwayCharacterWithAbilities> WeakCharacter(Cast<ABwayCharacterWithAbilities>(Target));
	const bool bBurstHandled = Super::OnExecute_Implementation(Target, Parameters);
	ABwayCharacterWithAbilities* Character = WeakCharacter.Get();
	// GAS supplies its avatar as Target. The pawn validates that ownership again and
	// rejects late cues after death. Do not use an EffectCauser or instigator fallback.
	// Body action is independent of the inherited audio/VFX spawn chance and local-only filters.
	const bool bActionPlayed = Character && Character->PlayPrimaryActionMontage(PrimaryActionMontage, PrimaryActionPlayRate);
	return bBurstHandled || bActionPlayed;
}
