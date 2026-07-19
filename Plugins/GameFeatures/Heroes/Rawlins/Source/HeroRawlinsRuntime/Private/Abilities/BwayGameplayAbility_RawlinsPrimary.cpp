#include "Abilities/BwayGameplayAbility_RawlinsPrimary.h"

#include "BwayGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Projectiles/BwayRawlinsBulletProjectile.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RawlinsPrimary)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Rawlins_PrimaryAttack, "Ability.Rawlins.PrimaryAttack");

UBwayGameplayAbility_RawlinsPrimary::UBwayGameplayAbility_RawlinsPrimary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Rawlins_PrimaryAttack);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationBlockedTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	ProjectileClass = ABwayRawlinsBulletProjectile::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Rawlins", "PrimaryName", "Primary Attack");
	DisplayData.Description = NSLOCTEXT("Rawlins", "PrimaryDesc", "Two consecutive pistol shots.");
}

void UBwayGameplayAbility_RawlinsPrimary::ActivateAbility(
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

	ActiveBaseDamage = AbilityBaseDamage;
	ActiveDamageScaling = DamageScaling;
	ActiveProjectileSpeed = ProjectileSpeed;
	ActiveProjectileLifeSpan = ProjectileLifeSpan;
	ActiveSpawnForwardOffset = SpawnForwardOffset;
	ActiveShotCount = ShotCount;
	ActiveShotInterval = ShotInterval;
	if (const UBwayRawlinsKitConfig* Config = ResolveKitConfig())
	{
		ActiveBaseDamage = Config->PrimaryBaseDamage;
		ActiveDamageScaling = Config->PrimaryDamageScaling;
		ActiveProjectileSpeed = Config->PrimaryProjectileSpeed;
		ActiveProjectileLifeSpan = Config->PrimaryProjectileLifeSpan;
		ActiveSpawnForwardOffset = Config->PrimarySpawnForwardOffset;
		ActiveShotCount = Config->PrimaryShotCount;
		ActiveShotInterval = Config->PrimaryShotInterval;
	}

	if (!ProjectileClass || ActiveShotCount < 1)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ShotsFired = 0;
	SpawnShot();

	if (ShotsFired >= ActiveShotCount)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotTimerHandle);
		World->GetTimerManager().SetTimer(
			ShotTimerHandle,
			this,
			&UBwayGameplayAbility_RawlinsPrimary::FireNextShot,
			ActiveShotInterval,
			true);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_RawlinsPrimary::FireNextShot()
{
	if (!IsActive() || !CachedCharacter)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ShotTimerHandle);
		}
		return;
	}

	SpawnShot();

	if (ShotsFired >= ActiveShotCount)
	{
		EndAbility(StoredSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_RawlinsPrimary::SpawnShot()
{
	if (!CachedCharacter || !ProjectileClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector SpawnLocation = CachedCharacter->GetActorLocation() + CachedCharacter->GetActorForwardVector() * ActiveSpawnForwardOffset;
	FRotator SpawnRotation = CachedCharacter->GetActorRotation();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		SpawnRotation = ViewRot;
		SpawnLocation = CachedCharacter->GetActorLocation() + ViewRot.Vector() * ActiveSpawnForwardOffset;
	}

	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
	ABwayRawlinsBulletProjectile* Projectile = World->SpawnActorDeferred<ABwayRawlinsBulletProjectile>(
		ProjectileClass,
		SpawnTransform,
		CachedCharacter,
		CachedCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Projectile)
	{
		return;
	}

	const float Damage = CalculateScaledDamage(ActiveBaseDamage, ActiveDamageScaling);
	Projectile->ConfigureProjectile(CachedCharacter, Damage, ActiveProjectileSpeed, ActiveProjectileLifeSpan);
	Projectile->FinishSpawning(SpawnTransform);
	++ShotsFired;
}

void UBwayGameplayAbility_RawlinsPrimary::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotTimerHandle);
	}

	ShotsFired = 0;
	CachedCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
