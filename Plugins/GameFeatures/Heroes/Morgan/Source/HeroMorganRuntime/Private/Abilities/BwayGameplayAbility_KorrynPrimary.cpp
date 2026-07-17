#include "Abilities/BwayGameplayAbility_KorrynPrimary.h"

#include "Abilities/BwayGameplayEffect_KorrynEffects.h"
#include "BwayGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Projectiles/BwayKorrynPrimaryProjectile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_KorrynPrimary)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Korryn_PrimaryAttack, "Ability.Korryn.PrimaryAttack");

UBwayGameplayAbility_KorrynPrimary::UBwayGameplayAbility_KorrynPrimary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Korryn_PrimaryAttack);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationBlockedTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	ProjectileClass = ABwayKorrynPrimaryProjectile::StaticClass();
	ArmorShredEffectClass = UGE_Bway_KorrynArmorShred::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Korryn", "PrimaryName", "Primary Attack");
	DisplayData.Description = NSLOCTEXT("Korryn", "PrimaryDesc", "Orb that damages and shreds armor.");
}

void UBwayGameplayAbility_KorrynPrimary::ActivateAbility(
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

	if (!HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !ProjectileClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	float LocalBaseDamage = AbilityBaseDamage;
	float LocalScaling = DamageScaling;
	float LocalSpeed = ProjectileSpeed;
	float LocalLife = ProjectileLifeSpan;
	if (const UBwayKorrynKitConfig* Config = ResolveKitConfig())
	{
		LocalBaseDamage = Config->PrimaryBaseDamage;
		LocalScaling = Config->PrimaryDamageScaling;
		LocalSpeed = Config->PrimaryProjectileSpeed;
		LocalLife = Config->PrimaryProjectileLifeSpan;
	}

	FVector SpawnLocation = CachedCharacter->GetActorLocation() + CachedCharacter->GetActorForwardVector() * SpawnForwardOffset;
	FRotator SpawnRotation = CachedCharacter->GetActorRotation();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		SpawnRotation = ViewRot;
		SpawnLocation = CachedCharacter->GetActorLocation() + ViewRot.Vector() * SpawnForwardOffset;
	}

	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
	ABwayKorrynPrimaryProjectile* Projectile = World->SpawnActorDeferred<ABwayKorrynPrimaryProjectile>(
		ProjectileClass,
		SpawnTransform,
		CachedCharacter,
		CachedCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Projectile)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float Damage = CalculateScaledDamage(LocalBaseDamage, LocalScaling);
	Projectile->ConfigureProjectile(CachedCharacter, Damage, LocalSpeed, LocalLife, ArmorShredEffectClass);
	Projectile->FinishSpawning(SpawnTransform);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
