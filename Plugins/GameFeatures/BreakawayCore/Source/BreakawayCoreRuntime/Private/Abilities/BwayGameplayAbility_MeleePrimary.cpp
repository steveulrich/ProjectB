#include "Abilities/BwayGameplayAbility_MeleePrimary.h"

#include "BwayCharacterWithAbilities.h"
#include "BwayGameplayTags.h"
#include "Engine/World.h"
#include "NativeGameplayTags.h"
#include "Combat/BwayDamageLibrary.h"
#include "Buildable/BuildableBase.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Argus_PrimaryAttack, "Ability.Argus.PrimaryAttack");

UBwayGameplayAbility_MeleePrimary::UBwayGameplayAbility_MeleePrimary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Argus_PrimaryAttack);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationBlockedTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	CancelAbilitiesWithTag.Reset();

	DisplayData.AbilityName = NSLOCTEXT("Argus", "PrimaryName", "Primary Attack");
	DisplayData.Description = NSLOCTEXT("Argus", "PrimaryDesc", "Slashing melee attack.");
}

void UBwayGameplayAbility_MeleePrimary::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StoredSpecHandle = Handle;

	ABwayCharacterWithAbilities* Character = GetBwayCharacterFromActorInfo();
	if (!Character || !HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	float LocalBaseDamage = AbilityBaseDamage;
	float LocalScaling = DamageScaling;
	float LocalTraceRadius = TraceRadius;
	float LocalTraceDistance = TraceDistance;
	if (const UBwayArgusKitConfig* Config = ResolveKitConfig())
	{
		LocalBaseDamage = Config->PrimaryBaseDamage;
		LocalScaling = Config->PrimaryDamageScaling;
		LocalTraceRadius = Config->PrimaryTraceRadius;
		LocalTraceDistance = Config->PrimaryTraceDistance;
	}

	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + Character->GetActorForwardVector() * LocalTraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleePrimary), false, Character);
	FCollisionShape Shape = FCollisionShape::MakeSphere(LocalTraceRadius);

	TArray<FHitResult> Hits;
	if (UWorld* World = Character->GetWorld())
	{
		World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, Shape, QueryParams);
	}

	const float AppliedDamage = CalculateScaledDamage(LocalBaseDamage, LocalScaling);

	TSet<TObjectPtr<AActor>> DamagedTargets;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || DamagedTargets.Contains(HitActor)) continue;
		DamagedTargets.Add(HitActor);
		if (HitActor->IsA<ABuildableActor>())
		{
			UBwayDamageLibrary::ApplyDamageToEnemyBuildable(Character, HitActor, AppliedDamage, Character);
			continue;
		}
		ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Hit.GetActor());
		if (!HitCharacter || !IsEnemy(HitCharacter))
		{
			continue;
		}

		ApplyDamageToEnemy(HitCharacter, AppliedDamage);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
