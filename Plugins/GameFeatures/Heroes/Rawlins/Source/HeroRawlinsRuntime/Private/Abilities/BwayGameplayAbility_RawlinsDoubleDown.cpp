#include "Abilities/BwayGameplayAbility_RawlinsDoubleDown.h"

#include "Abilities/BwayGameplayEffect_RawlinsCooldowns.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RawlinsDoubleDown)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Rawlins_DoubleDown, "Ability.Rawlins.DoubleDown");

UBwayGameplayAbility_RawlinsDoubleDown::UBwayGameplayAbility_RawlinsDoubleDown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Rawlins_DoubleDown);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationOwnedTags.AddTag(TAG_Gameplay_DamageImmunity);
	CooldownGameplayEffectClass = UGE_Bway_Cooldown_RawlinsDoubleDown::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Rawlins", "DoubleDownName", "Double Down");
	DisplayData.Description = NSLOCTEXT("Rawlins", "DoubleDownDesc", "Invulnerable roll forward.");
}

void UBwayGameplayAbility_RawlinsDoubleDown::ActivateAbility(
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

	float LocalDashDistance = DashDistance;
	float LocalDashDuration = DashDuration;
	if (const UBwayRawlinsKitConfig* Config = ResolveKitConfig())
	{
		LocalDashDistance = Config->DoubleDownDashDistance;
		LocalDashDuration = Config->DoubleDownDashDuration;
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

	const FVector DashVelocity = DashDirection * (LocalDashDistance / FMath::Max(LocalDashDuration, 0.05f));
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
			LocalDashDuration,
			false);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_RawlinsDoubleDown::EndAbility(
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
