#include "Abilities/BwayGameplayAbility_RawlinsPowerShot.h"

#include "Abilities/BwayGameplayEffect_RawlinsCooldowns.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RawlinsPowerShot)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Rawlins_PowerShot, "Ability.Rawlins.PowerShot");

UBwayGameplayAbility_RawlinsPowerShot::UBwayGameplayAbility_RawlinsPowerShot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Rawlins_PowerShot);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_RawlinsPowerShot::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Rawlins", "PowerShotName", "Power Shot");
	DisplayData.Description = NSLOCTEXT("Rawlins", "PowerShotDesc", "Dual pistol blast that knocks opponents back.");
}

void UBwayGameplayAbility_RawlinsPowerShot::ActivateAbility(
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
		PerformBlast();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UBwayGameplayAbility_RawlinsPowerShot::PerformBlast()
{
	if (!CachedCharacter)
	{
		return;
	}

	float LocalBaseDamage = AbilityBaseDamage;
	float LocalScaling = DamageScaling;
	float LocalTraceRadius = TraceRadius;
	float LocalTraceDistance = TraceDistance;
	float LocalKnockbackStrength = KnockbackStrength;
	float LocalKnockbackUpward = KnockbackUpward;
	if (const UBwayRawlinsKitConfig* Config = ResolveKitConfig())
	{
		LocalBaseDamage = Config->PowerShotBaseDamage;
		LocalScaling = Config->PowerShotDamageScaling;
		LocalTraceRadius = Config->PowerShotTraceRadius;
		LocalTraceDistance = Config->PowerShotTraceDistance;
		LocalKnockbackStrength = Config->PowerShotKnockbackStrength;
		LocalKnockbackUpward = Config->PowerShotKnockbackUpward;
	}

	FVector AimDirection = CachedCharacter->GetActorForwardVector();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		AimDirection = ViewRot.Vector();
	}

	const FVector Start = CachedCharacter->GetActorLocation();
	const FVector End = Start + AimDirection * LocalTraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RawlinsPowerShot), false, CachedCharacter);
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
	const FVector KnockDir2D = AimDirection.GetSafeNormal2D();

	for (const FHitResult& Hit : Hits)
	{
		ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Hit.GetActor());
		if (!HitCharacter || Damaged.Contains(HitCharacter) || !IsEnemy(HitCharacter))
		{
			continue;
		}

		Damaged.Add(HitCharacter);
		ApplyDamageToEnemy(HitCharacter, Damage);

		FVector KnockDir = KnockDir2D;
		if (KnockDir.IsNearlyZero())
		{
			KnockDir = (HitCharacter->GetActorLocation() - Start).GetSafeNormal2D();
		}
		ApplyKnockbackToEnemy(HitCharacter, KnockDir * LocalKnockbackStrength + FVector(0.f, 0.f, LocalKnockbackUpward));
	}
}
