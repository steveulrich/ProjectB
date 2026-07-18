#include "Abilities/BwayGameplayAbility_AlonaBlessing.h"

#include "Abilities/BwayGameplayEffect_AlonaCooldowns.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Zones/BwayAlonaBlessingZone.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_AlonaBlessing)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Alona_Blessing, "Ability.Alona.Blessing");

UBwayGameplayAbility_AlonaBlessing::UBwayGameplayAbility_AlonaBlessing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Alona_Blessing);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_AlonaBlessing::StaticClass();
	BlessingZoneClass = ABwayAlonaBlessingZone::StaticClass();
	DisplayData.bIsUltimate = true;

	DisplayData.AbilityName = NSLOCTEXT("Alona", "BlessingName", "Blessing of the Sun");
	DisplayData.Description = NSLOCTEXT("Alona", "BlessingDesc", "Cast a healing circle on the ground in front of you.");
}

void UBwayGameplayAbility_AlonaBlessing::ActivateAbility(
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

	float LocalForward = ForwardPlacementDistance;
	float LocalRadius = ZoneRadius;
	float LocalDuration = ZoneDuration;
	float LocalInitialHeal = InitialHeal;
	float LocalHealPerSecond = HealPerSecond;
	if (const UBwayAlonaKitConfig* Config = ResolveKitConfig())
	{
		LocalForward = Config->BlessingForwardPlacementDistance;
		LocalRadius = Config->BlessingZoneRadius;
		LocalDuration = Config->BlessingZoneDuration;
		LocalInitialHeal = Config->BlessingInitialHeal;
		LocalHealPerSecond = Config->BlessingHealPerSecond;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		UWorld* World = GetWorld();
		if (World && BlessingZoneClass)
		{
			const FVector TargetLocation = ResolveGroundTargetLocation(LocalForward);
			const FTransform SpawnTransform(FRotator::ZeroRotator, TargetLocation);

			ABwayAlonaBlessingZone* Zone = World->SpawnActorDeferred<ABwayAlonaBlessingZone>(
				BlessingZoneClass,
				SpawnTransform,
				CachedCharacter,
				CachedCharacter,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			if (Zone)
			{
				Zone->ConfigureZone(CachedCharacter, LocalRadius, LocalDuration, LocalInitialHeal, LocalHealPerSecond);
				Zone->FinishSpawning(SpawnTransform);
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

FVector UBwayGameplayAbility_AlonaBlessing::ResolveGroundTargetLocation(float ForwardDistance) const
{
	if (!CachedCharacter)
	{
		return FVector::ZeroVector;
	}

	FVector Forward = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FRotator YawOnly = Controller->GetControlRotation();
		YawOnly.Pitch = 0.f;
		YawOnly.Roll = 0.f;
		Forward = YawOnly.Vector().GetSafeNormal2D();
	}

	const FVector IdealPoint = CachedCharacter->GetActorLocation() + Forward * ForwardDistance;
	UWorld* World = GetWorld();
	if (!World)
	{
		return IdealPoint;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AlonaBlessingGround), false, CachedCharacter);
	const FVector TraceStart = IdealPoint + FVector(0.f, 0.f, GroundTraceUp);
	const FVector TraceEnd = IdealPoint - FVector(0.f, 0.f, GroundTraceDown);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, 5.f);
	}

	return IdealPoint;
}
