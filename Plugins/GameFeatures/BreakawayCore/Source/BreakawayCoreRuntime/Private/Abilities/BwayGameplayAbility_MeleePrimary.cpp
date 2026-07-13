#include "Abilities/BwayGameplayAbility_MeleePrimary.h"

#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Argus_PrimaryAttack, "Ability.Argus.PrimaryAttack");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputTag_Ability_Primary, "InputTag.Ability.Primary");

UBwayGameplayAbility_MeleePrimary::UBwayGameplayAbility_MeleePrimary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Argus_PrimaryAttack);
	AbilityTags.AddTag(TAG_InputTag_Ability_Primary);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationBlockedTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	CancelAbilitiesWithTag.Reset();
}

void UBwayGameplayAbility_MeleePrimary::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, /*ForceCooldown*/ true))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ABwayCharacterWithAbilities* Character = GetBwayCharacterFromActorInfo();
	if (!Character || !HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + Character->GetActorForwardVector() * TraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleePrimary), false, Character);
	FCollisionShape Shape = FCollisionShape::MakeSphere(TraceRadius);

	TArray<FHitResult> Hits;
	if (UWorld* World = Character->GetWorld())
	{
		World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, Shape, QueryParams);
	}

	float AppliedDamage = DamageAmount;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const ULyraCombatSet* CombatSet = ASC->GetSet<ULyraCombatSet>())
		{
			AppliedDamage = FMath::Max(AppliedDamage, CombatSet->GetBaseDamage() * 0.4f);
		}
	}

	TSet<TObjectPtr<ABwayCharacterWithAbilities>> DamagedTargets;
	for (const FHitResult& Hit : Hits)
	{
		ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Hit.GetActor());
		if (!HitCharacter || DamagedTargets.Contains(HitCharacter) || !IsEnemy(HitCharacter))
		{
			continue;
		}

		DamagedTargets.Add(HitCharacter);
		ApplyDamageToEnemy(HitCharacter, AppliedDamage);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
