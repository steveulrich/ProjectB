#include "Abilities/BwayGameplayAbility_KorrynBurdenOfSin.h"

#include "Abilities/BwayGameplayEffect_KorrynEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_KorrynBurdenOfSin)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Korryn_BurdenOfSin, "Ability.Korryn.BurdenOfSin");

UBwayGameplayAbility_KorrynBurdenOfSin::UBwayGameplayAbility_KorrynBurdenOfSin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Korryn_BurdenOfSin);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_KorrynBurdenOfSin::StaticClass();
	SlowEffectClass = UGE_Bway_KorrynBurdenSlow::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Korryn", "BurdenName", "Burden of Sin");
	DisplayData.Description = NSLOCTEXT("Korryn", "BurdenDesc", "Raven cone that damages and slows enemies.");
}

void UBwayGameplayAbility_KorrynBurdenOfSin::ActivateAbility(
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
	float LocalRange = ConeRange;
	float LocalHalfAngle = ConeHalfAngleDegrees;
	if (const UBwayKorrynKitConfig* Config = ResolveKitConfig())
	{
		LocalBase = Config->BurdenBaseDamage;
		LocalScale = Config->BurdenDamageScaling;
		LocalRange = Config->BurdenConeRange;
		LocalHalfAngle = Config->BurdenConeHalfAngleDegrees;
	}
	ConeRange = LocalRange;
	ConeHalfAngleDegrees = LocalHalfAngle;

	FVector Origin = CachedCharacter->GetActorLocation();
	FVector Forward = CachedCharacter->GetActorForwardVector();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		Forward = ViewRot.Vector().GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			Forward = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
		}
	}

	const float Damage = CalculateScaledDamage(LocalBase, LocalScale);
	const TArray<ABwayCharacterWithAbilities*> Enemies = FindEnemiesInCone(Origin, Forward);

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	for (ABwayCharacterWithAbilities* Enemy : Enemies)
	{
		ApplyDamageToEnemy(Enemy, Damage);

		if (SlowEffectClass && SourceASC)
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy))
			{
				FGameplayEffectContextHandle Context = MakeEffectContextForAbility();
				const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(SlowEffectClass, 1.f, Context);
				if (Spec.IsValid())
				{
					TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				}
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

TArray<ABwayCharacterWithAbilities*> UBwayGameplayAbility_KorrynBurdenOfSin::FindEnemiesInCone(
	const FVector& Origin,
	const FVector& Forward) const
{
	TArray<ABwayCharacterWithAbilities*> Hits;
	const TArray<ABwayCharacterWithAbilities*> Nearby = GetEnemiesInRadius(Origin, ConeRange);
	const FVector Dir = Forward.GetSafeNormal2D();
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDegrees));

	for (ABwayCharacterWithAbilities* Enemy : Nearby)
	{
		if (!Enemy)
		{
			continue;
		}

		const FVector ToEnemy = (Enemy->GetActorLocation() - Origin).GetSafeNormal2D();
		if (ToEnemy.IsNearlyZero())
		{
			Hits.Add(Enemy);
			continue;
		}

		if (FVector::DotProduct(Dir, ToEnemy) >= CosHalf)
		{
			Hits.Add(Enemy);
		}
	}

	return Hits;
}
