#include "Abilities/BwayGameplayAbility_ArgusSlide.h"

#include "Abilities/BwayGameplayEffect_ArgusCooldowns.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_ArgusSlide)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Argus_Slide, "Ability.Argus.Slide");

UBwayGameplayAbility_ArgusSlide::UBwayGameplayAbility_ArgusSlide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Argus_Slide);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Invulnerable for the dash window (LyraHealthSet checks Gameplay.DamageImmunity).
	ActivationOwnedTags.AddTag(TAG_Gameplay_DamageImmunity);

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_ArgusSlide::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Argus", "SlideName", "Slide");
	DisplayData.Description = NSLOCTEXT("Argus", "SlideDesc", "Invulnerable dash in the held direction.");
}

void UBwayGameplayAbility_ArgusSlide::ActivateAbility(
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

	FVector DashDirection = CachedCharacter->GetLastMovementInputVector().GetSafeNormal2D();
	if (DashDirection.IsNearlyZero())
	{
		if (const AController* Controller = CachedCharacter->GetController())
		{
			FRotator YawOnly = Controller->GetControlRotation();
			YawOnly.Pitch = 0.f;
			YawOnly.Roll = 0.f;
			DashDirection = YawOnly.Vector().GetSafeNormal2D();
		}
	}
	if (DashDirection.IsNearlyZero())
	{
		DashDirection = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector DashVelocity = DashDirection * (DashDistance / FMath::Max(DashDuration, 0.05f));
	CachedCharacter->LaunchCharacter(DashVelocity, true, true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndDashTimerHandle);
		World->GetTimerManager().SetTimer(
			EndDashTimerHandle,
			[this, Handle, ActorInfo, ActivationInfo]()
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			},
			DashDuration,
			false);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_ArgusSlide::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndDashTimerHandle);
	}

	CachedCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
