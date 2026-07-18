#include "Abilities/BwayGameplayAbility_AlonaSunBurst.h"

#include "Abilities/BwayGameplayEffect_AlonaCooldowns.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_AlonaSunBurst)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Alona_SunBurst, "Ability.Alona.SunBurst");

UBwayGameplayAbility_AlonaSunBurst::UBwayGameplayAbility_AlonaSunBurst(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Alona_SunBurst);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_AlonaSunBurst::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Alona", "SunBurstName", "Sun Burst");
	DisplayData.Description = NSLOCTEXT("Alona", "SunBurstDesc", "Knock away nearby opponents.");
}

void UBwayGameplayAbility_AlonaSunBurst::ActivateAbility(
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
		PerformBurst();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UBwayGameplayAbility_AlonaSunBurst::PerformBurst()
{
	if (!CachedCharacter)
	{
		return;
	}

	float LocalRadius = BurstRadius;
	float LocalBaseDamage = AbilityBaseDamage;
	float LocalScaling = DamageScaling;
	float LocalKnockbackStrength = KnockbackStrength;
	float LocalKnockbackUpward = KnockbackUpward;
	if (const UBwayAlonaKitConfig* Config = ResolveKitConfig())
	{
		LocalRadius = Config->SunBurstRadius;
		LocalBaseDamage = Config->SunBurstBaseDamage;
		LocalScaling = Config->SunBurstDamageScaling;
		LocalKnockbackStrength = Config->SunBurstKnockbackStrength;
		LocalKnockbackUpward = Config->SunBurstKnockbackUpward;
	}

	const FVector Origin = CachedCharacter->GetActorLocation();
	const float Damage = CalculateScaledDamage(LocalBaseDamage, LocalScaling);
	TArray<ABwayCharacterWithAbilities*> Enemies = GetEnemiesInRadius(Origin, LocalRadius);

	for (ABwayCharacterWithAbilities* Enemy : Enemies)
	{
		if (!Enemy)
		{
			continue;
		}

		ApplyDamageToEnemy(Enemy, Damage);

		FVector KnockDir = (Enemy->GetActorLocation() - Origin).GetSafeNormal2D();
		if (KnockDir.IsNearlyZero())
		{
			KnockDir = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
		}

		ApplyKnockbackToEnemy(Enemy, KnockDir * LocalKnockbackStrength + FVector(0.f, 0.f, LocalKnockbackUpward));
	}
}
