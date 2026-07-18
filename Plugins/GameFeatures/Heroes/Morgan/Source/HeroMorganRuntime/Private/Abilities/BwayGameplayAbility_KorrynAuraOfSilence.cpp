#include "Abilities/BwayGameplayAbility_KorrynAuraOfSilence.h"

#include "Abilities/BwayGameplayEffect_KorrynEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_KorrynAuraOfSilence)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Korryn_AuraOfSilence, "Ability.Korryn.AuraOfSilence");

UBwayGameplayAbility_KorrynAuraOfSilence::UBwayGameplayAbility_KorrynAuraOfSilence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Korryn_AuraOfSilence);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_KorrynAuraOfSilence::StaticClass();
	SilenceEffectClass = UGE_Bway_KorrynSilence::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Korryn", "AuraName", "Aura of Silence");
	DisplayData.Description = NSLOCTEXT("Korryn", "AuraDesc", "Silence nearby enemies and deal damage.");
}

void UBwayGameplayAbility_KorrynAuraOfSilence::ActivateAbility(
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

	if (!HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	float LocalBase = AbilityBaseDamage;
	float LocalScale = DamageScaling;
	float LocalRadius = AuraRadius;
	float LocalSilenceDuration = 5.f;
	if (const UBwayKorrynKitConfig* Config = ResolveKitConfig())
	{
		LocalBase = Config->AuraBaseDamage;
		LocalScale = Config->AuraDamageScaling;
		LocalRadius = Config->AuraRadius;
		LocalSilenceDuration = Config->AuraSilenceDuration;
	}

	const float Damage = CalculateScaledDamage(LocalBase, LocalScale);
	const TArray<ABwayCharacterWithAbilities*> Enemies = GetEnemiesInRadius(CachedCharacter->GetActorLocation(), LocalRadius);

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	for (ABwayCharacterWithAbilities* Enemy : Enemies)
	{
		ApplyDamageToEnemy(Enemy, Damage);

		if (SilenceEffectClass && SourceASC)
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy))
			{
				FGameplayEffectContextHandle Context = MakeEffectContextForAbility();
				const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(SilenceEffectClass, 1.f, Context);
				if (Spec.IsValid() && Spec.Data.IsValid())
				{
					Spec.Data->SetDuration(LocalSilenceDuration, /*bLockDuration*/ true);
					TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				}
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
