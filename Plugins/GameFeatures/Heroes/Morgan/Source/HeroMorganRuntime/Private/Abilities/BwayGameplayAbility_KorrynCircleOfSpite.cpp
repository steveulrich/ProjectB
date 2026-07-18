#include "Abilities/BwayGameplayAbility_KorrynCircleOfSpite.h"

#include "Abilities/BwayGameplayEffect_KorrynEffects.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Zones/BwayKorrynSpiteZone.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_KorrynCircleOfSpite)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Korryn_CircleOfSpite, "Ability.Korryn.CircleOfSpite");

UBwayGameplayAbility_KorrynCircleOfSpite::UBwayGameplayAbility_KorrynCircleOfSpite(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Korryn_CircleOfSpite);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_KorrynCircleOfSpite::StaticClass();
	ZoneClass = ABwayKorrynSpiteZone::StaticClass();
	SlowEffectClass = UGE_Bway_KorrynCircleSlow::StaticClass();
	DamageAmpEffectClass = UGE_Bway_KorrynCircleDamageAmp::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Korryn", "CircleName", "Circle of Spite");
	DisplayData.Description = NSLOCTEXT("Korryn", "CircleDesc", "Hex circle that slows and amplifies damage.");
}

void UBwayGameplayAbility_KorrynCircleOfSpite::ActivateAbility(
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

	UWorld* World = GetWorld();
	if (!World || !ZoneClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	float LocalRadius = ZoneRadius;
	float LocalDuration = ZoneDuration;
	float LocalOffset = SpawnForwardOffset;
	float LocalSlowMultiplier = 0.85f;
	float LocalDamageAmpMultiplier = 1.35f;
	if (const UBwayKorrynKitConfig* Config = ResolveKitConfig())
	{
		LocalRadius = Config->CircleRadius;
		LocalDuration = Config->CircleDuration;
		LocalOffset = Config->CircleSpawnForwardOffset;
		LocalSlowMultiplier = Config->CircleSlowMultiplier;
		LocalDamageAmpMultiplier = Config->CircleIncomingDamageMultiplier;
	}

	FVector Forward = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FRotator YawOnly = Controller->GetControlRotation();
		YawOnly.Pitch = 0.f;
		YawOnly.Roll = 0.f;
		Forward = YawOnly.Vector().GetSafeNormal2D();
	}

	FVector SpawnLocation = CachedCharacter->GetActorLocation() + Forward * LocalOffset;
	SpawnLocation.Z = CachedCharacter->GetActorLocation().Z;

	// Snap to floor.
	FHitResult FloorHit;
	const FVector TraceStart = SpawnLocation + FVector(0.f, 0.f, 200.f);
	const FVector TraceEnd = SpawnLocation - FVector(0.f, 0.f, 1000.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KorrynCircleFloor), false, CachedCharacter);
	if (World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		SpawnLocation = FloorHit.ImpactPoint + FVector(0.f, 0.f, 5.f);
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
	ABwayKorrynSpiteZone* Zone = World->SpawnActorDeferred<ABwayKorrynSpiteZone>(
		ZoneClass,
		SpawnTransform,
		CachedCharacter,
		CachedCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Zone)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Zone->ConfigureZone(
		CachedCharacter,
		LocalRadius,
		LocalDuration,
		SlowEffectClass,
		DamageAmpEffectClass,
		LocalSlowMultiplier,
		LocalDamageAmpMultiplier);
	Zone->FinishSpawning(SpawnTransform);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
