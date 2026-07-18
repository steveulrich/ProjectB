#include "Abilities/BwayGameplayAbility_ForGlory.h"

#include "Abilities/BwayGameplayEffect_ArgusCooldowns.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_ForGlory)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Argus_ForGlory, "Ability.Argus.ForGlory");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Unstoppable, "State.Unstoppable");

UBwayGameplayAbility_ForGlory::UBwayGameplayAbility_ForGlory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Argus_ForGlory);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationOwnedTags.AddTag(TAG_State_Unstoppable);
	CooldownGameplayEffectClass = UGE_Bway_Cooldown_ForGlory::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Argus", "ForGloryName", "For Glory");
	DisplayData.Description = NSLOCTEXT("Argus", "ForGloryDesc", "Strong unstoppable kick that knocks the opponent backwards.");
}

void UBwayGameplayAbility_ForGlory::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StoredSpecHandle = Handle;
	CachedCharacter = GetBwayCharacterFromActorInfo();
	if (!CachedCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		PerformKick();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UBwayGameplayAbility_ForGlory::PerformKick()
{
	if (!CachedCharacter)
	{
		return;
	}

	float LocalTraceRadius = TraceRadius;
	float LocalTraceDistance = TraceDistance;
	float LocalBaseDamage = AbilityBaseDamage;
	float LocalScaling = DamageScaling;
	float LocalKnockbackStrength = KnockbackStrength;
	float LocalKnockbackUpward = KnockbackUpward;
	if (const UBwayArgusKitConfig* Config = ResolveKitConfig())
	{
		LocalTraceRadius = Config->ForGloryTraceRadius;
		LocalTraceDistance = Config->ForGloryTraceDistance;
		LocalBaseDamage = Config->ForGloryBaseDamage;
		LocalScaling = Config->ForGloryDamageScaling;
		LocalKnockbackStrength = Config->ForGloryKnockbackStrength;
		LocalKnockbackUpward = Config->ForGloryKnockbackUpward;
	}

	const FVector Start = CachedCharacter->GetActorLocation();
	const FVector End = Start + CachedCharacter->GetActorForwardVector() * LocalTraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ForGloryKick), false, CachedCharacter);
	TArray<FHitResult> Hits;
	if (UWorld* World = CachedCharacter->GetWorld())
	{
		World->SweepMultiByChannel(
			Hits,
			Start,
			End,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeSphere(LocalTraceRadius),
			QueryParams);
	}

	const float Damage = CalculateScaledDamage(LocalBaseDamage, LocalScaling);
	TSet<TObjectPtr<ABwayCharacterWithAbilities>> Damaged;
	for (const FHitResult& Hit : Hits)
	{
		ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Hit.GetActor());
		if (!HitCharacter || Damaged.Contains(HitCharacter) || !IsEnemy(HitCharacter))
		{
			continue;
		}

		Damaged.Add(HitCharacter);
		ApplyDamageToEnemy(HitCharacter, Damage);

		FVector KnockDir = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
		ApplyKnockbackToEnemy(HitCharacter, KnockDir * LocalKnockbackStrength + FVector(0.f, 0.f, LocalKnockbackUpward));
		break; // Single-target kick
	}
}
