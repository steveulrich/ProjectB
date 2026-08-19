#include "Abilities/BwayGameplayAbility_RawlinsBlazingBarrage.h"

#include "Abilities/BwayGameplayEffect_RawlinsCooldowns.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayProjectilePresentationData.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Projectiles/BwayRawlinsBulletProjectile.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RawlinsBlazingBarrage)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Rawlins_BlazingBarrage, "Ability.Rawlins.BlazingBarrage");

UBwayGameplayAbility_RawlinsBlazingBarrage::UBwayGameplayAbility_RawlinsBlazingBarrage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Rawlins_BlazingBarrage);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationOwnedTags.AddTag(TAG_Gameplay_MovementStopped);
	CooldownGameplayEffectClass = UGE_Bway_Cooldown_RawlinsBlazingBarrage::StaticClass();
	ProjectileClass = ABwayRawlinsBulletProjectile::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Rawlins", "BarrageName", "Blazing Barrage");
	DisplayData.Description = NSLOCTEXT("Rawlins", "BarrageDesc", "Stationary 12-shot burst. Aimable. Interruptible.");
	DisplayData.bIsUltimate = true;
}

void UBwayGameplayAbility_RawlinsBlazingBarrage::ActivateAbility(
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

	ActiveBaseDamage = AbilityBaseDamage;
	ActiveDamageScaling = DamageScaling;
	ActiveShotCount = ShotCount;
	ActiveShotInterval = ShotInterval;
	ActiveProjectileSpeed = ProjectileSpeed;
	ActiveProjectileLifeSpan = ProjectileLifeSpan;
	ActiveSpawnForwardOffset = SpawnForwardOffset;
	ActiveJuggleUpward = JuggleUpward;
	ActiveProjectilePresentation.Reset();
	if (const UBwayRawlinsKitConfig* Config = ResolveKitConfig())
	{
		ActiveBaseDamage = Config->BarrageBaseDamagePerBullet;
		ActiveDamageScaling = Config->BarrageDamageScaling;
		ActiveShotCount = Config->BarrageShotCount;
		ActiveShotInterval = Config->BarrageShotInterval;
		ActiveProjectileSpeed = Config->BarrageProjectileSpeed;
		ActiveProjectileLifeSpan = Config->BarrageProjectileLifeSpan;
		ActiveSpawnForwardOffset = Config->BarrageSpawnForwardOffset;
		ActiveJuggleUpward = Config->BarrageJuggleUpward;
		ActiveProjectilePresentation = Config->BarrageProjectilePresentation;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ProjectileClass || ActiveShotCount < 1)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ULyraHealthComponent* HealthComp = ULyraHealthComponent::FindHealthComponent(CachedCharacter))
	{
		BoundHealthComponent = HealthComp;
		HealthAtActivation = HealthComp->GetHealth();
		HealthComp->OnHealthChanged.AddDynamic(this, &UBwayGameplayAbility_RawlinsBlazingBarrage::HandleHealthChanged);
		bBoundToHealth = true;
	}

	ShotsFired = 0;

	if (HasAuthority(&ActivationInfo))
	{
		SpawnShot();
	}
	else
	{
		// Keep predicted client channel duration aligned with authority (first shot is immediate).
		++ShotsFired;
	}

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
			&UBwayGameplayAbility_RawlinsBlazingBarrage::FireNextShot,
			ActiveShotInterval,
			true);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_RawlinsBlazingBarrage::FireNextShot()
{
	if (!IsActive() || !CachedCharacter)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ShotTimerHandle);
		}
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		SpawnShot();
	}
	else
	{
		++ShotsFired;
	}

	if (ShotsFired >= ActiveShotCount)
	{
		EndAbility(StoredSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_RawlinsBlazingBarrage::SpawnShot()
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
	const FVector JuggleImpulse(0.f, 0.f, ActiveJuggleUpward);
	if (UBwayProjectilePresentationData* Presentation = ActiveProjectilePresentation.LoadSynchronous())
	{
		Projectile->ApplyPresentation(Presentation);
	}
	Projectile->ConfigureProjectile(CachedCharacter, Damage, ActiveProjectileSpeed, ActiveProjectileLifeSpan, JuggleImpulse);
	Projectile->FinishSpawning(SpawnTransform);
	++ShotsFired;
}

void UBwayGameplayAbility_RawlinsBlazingBarrage::HandleHealthChanged(
	ULyraHealthComponent* HealthComp,
	float OldValue,
	float NewValue,
	AActor* Instigator)
{
	(void)HealthComp;
	(void)OldValue;
	(void)Instigator;

	if (!IsActive())
	{
		return;
	}

	// Interrupt when health decreases from the value at activation (took damage).
	if (NewValue < HealthAtActivation - KINDA_SMALL_NUMBER)
	{
		CancelAbility(StoredSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void UBwayGameplayAbility_RawlinsBlazingBarrage::UnbindHealthInterrupt()
{
	if (bBoundToHealth && BoundHealthComponent)
	{
		BoundHealthComponent->OnHealthChanged.RemoveDynamic(this, &UBwayGameplayAbility_RawlinsBlazingBarrage::HandleHealthChanged);
	}
	bBoundToHealth = false;
	BoundHealthComponent = nullptr;
	HealthAtActivation = 0.f;
}

void UBwayGameplayAbility_RawlinsBlazingBarrage::EndAbility(
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

	UnbindHealthInterrupt();
	ShotsFired = 0;
	CachedCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
