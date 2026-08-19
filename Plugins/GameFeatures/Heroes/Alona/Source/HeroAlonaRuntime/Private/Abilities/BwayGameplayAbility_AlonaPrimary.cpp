#include "Abilities/BwayGameplayAbility_AlonaPrimary.h"

#include "BwayGameplayTags.h"
#include "Combat/BwayProjectilePresentationData.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Projectiles/BwayAlonaPrimaryProjectile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_AlonaPrimary)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Alona_PrimaryAttack, "Ability.Alona.PrimaryAttack");

UBwayGameplayAbility_AlonaPrimary::UBwayGameplayAbility_AlonaPrimary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Alona_PrimaryAttack);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationBlockedTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	ProjectileClass = ABwayAlonaPrimaryProjectile::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Alona", "PrimaryName", "Primary Attack");
	DisplayData.Description = NSLOCTEXT("Alona", "PrimaryDesc", "A light projectile that fires forward.");
}

void UBwayGameplayAbility_AlonaPrimary::ActivateAbility(
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
	float LocalOffset = SpawnForwardOffset;
	TSoftObjectPtr<UBwayProjectilePresentationData> PresentationSoft;
	if (const UBwayAlonaKitConfig* Config = ResolveKitConfig())
	{
		LocalBaseDamage = Config->PrimaryBaseDamage;
		LocalScaling = Config->PrimaryDamageScaling;
		LocalSpeed = Config->PrimaryProjectileSpeed;
		LocalLife = Config->PrimaryProjectileLifeSpan;
		LocalOffset = Config->PrimarySpawnForwardOffset;
		PresentationSoft = Config->PrimaryProjectilePresentation;
	}

	FVector SpawnLocation = CachedCharacter->GetActorLocation() + CachedCharacter->GetActorForwardVector() * LocalOffset;
	FRotator SpawnRotation = CachedCharacter->GetActorRotation();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		SpawnRotation = ViewRot;
		SpawnLocation = CachedCharacter->GetActorLocation() + ViewRot.Vector() * LocalOffset;
	}

	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
	ABwayAlonaPrimaryProjectile* Projectile = World->SpawnActorDeferred<ABwayAlonaPrimaryProjectile>(
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
	if (UBwayProjectilePresentationData* Presentation = PresentationSoft.LoadSynchronous())
	{
		Projectile->ApplyPresentation(Presentation);
	}
	Projectile->ConfigureProjectile(CachedCharacter, Damage, LocalSpeed, LocalLife);
	Projectile->FinishSpawning(SpawnTransform);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
