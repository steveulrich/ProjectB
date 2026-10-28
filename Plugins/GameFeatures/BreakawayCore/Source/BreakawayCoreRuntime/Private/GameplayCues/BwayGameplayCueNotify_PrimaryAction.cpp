#include "GameplayCues/BwayGameplayCueNotify_PrimaryAction.h"

#include "Animation/AnimMontage.h"
#include "BwayCharacterWithAbilities.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "Character/LyraHealthComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputDeviceProperties.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayCueNotify_PrimaryAction)

FGameplayCueNotify_BurstEffects UBwayGameplayCueNotify_PrimaryAction::ResolveBurstEffects(
	const FGameplayCueNotify_BurstEffects& AuthoredEffects, bool bLocalOwner,
	bool bReduceCameraShake, bool bReduceMotionHaptics)
{
	FGameplayCueNotify_BurstEffects Effects = AuthoredEffects;
	// A primary action may be observed by everyone, but recoil and haptics belong
	// only to its local owner, even if a replacement asset requests world feedback.
	const FStructProperty* ShakeProperty = FindFieldChecked<FStructProperty>(FGameplayCueNotify_BurstEffects::StaticStruct(), TEXT("BurstCameraShake"));
	const FStructProperty* HapticsProperty = FindFieldChecked<FStructProperty>(FGameplayCueNotify_BurstEffects::StaticStruct(), TEXT("BurstForceFeedback"));
	auto& Shake = *ShakeProperty->ContainerPtrToValuePtr<FGameplayCueNotify_CameraShakeInfo>(&Effects);
	auto& Haptics = *HapticsProperty->ContainerPtrToValuePtr<FGameplayCueNotify_ForceFeedbackInfo>(&Effects);
	Shake.bPlayInWorld = false;
	Haptics.bPlayInWorld = false;
	if (!bLocalOwner || bReduceCameraShake)
	{
		Shake.CameraShake = nullptr;
	}
	if (!bLocalOwner || bReduceMotionHaptics)
	{
		Haptics.ForceFeedbackEffect = nullptr;
	}
	const FStructProperty* DeviceProperty = FindFieldChecked<FStructProperty>(
		FGameplayCueNotify_BurstEffects::StaticStruct(), TEXT("BurstDevicePropertyEffect"));
	auto& Device = *DeviceProperty->ContainerPtrToValuePtr<FGameplayCueNotify_InputDevicePropertyInfo>(&Effects);
	Device.DeviceProperties.RemoveAll([bLocalOwner, bReduceMotionHaptics](const TSubclassOf<UInputDeviceProperty>& Property)
	{
		return !bLocalOwner || (bReduceMotionHaptics && Property
			&& (Property->IsChildOf(UInputDeviceTriggerEffect::StaticClass())
				|| Property->IsChildOf(UInputDeviceAudioBasedVibrationProperty::StaticClass())));
	});
	return Effects;
}

bool UBwayGameplayCueNotify_PrimaryAction::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	const TWeakObjectPtr<ABwayCharacterWithAbilities> WeakCharacter(Cast<ABwayCharacterWithAbilities>(Target));
	const auto* Health = Target ? ULyraHealthComponent::FindHealthComponent(Target) : nullptr;
	if (!WeakCharacter.IsValid() || !Health || Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)
	{
		return false;
	}
	const auto* Config = UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(Target);
	const auto* PC = Cast<APlayerController>(WeakCharacter->GetController());
	const auto Effects = ResolveBurstEffects(BurstEffects, PC && PC->IsLocalController(),
		Config && Config->bReduceCameraShake, Config && Config->bReduceMotionHaptics);
	FGameplayCueNotify_SpawnContext Context(Target->GetWorld(), Target, Parameters);
	Context.SetDefaultSpawnCondition(&DefaultSpawnCondition);
	Context.SetDefaultPlacementInfo(&DefaultPlacementInfo);
	const bool bBurstHandled = DefaultSpawnCondition.ShouldSpawn(Context);
	if (bBurstHandled)
	{
		FGameplayCueNotify_SpawnResult Result;
		Effects.ExecuteEffects(Context, Result);
		OnBurst(Target, Parameters, Result);
#if !UE_BUILD_SHIPPING
		const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("bway.Combat.DebugCueFeedback"));
		if (Debug && Debug->GetInt() != 0)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayPrimaryFeedback: tag=%s net=%d local=%d reduceShake=%d reduceHaptics=%d shakes=%d"),
				*GameplayCueTag.ToString(), int32(Target->GetNetMode()), PC && PC->IsLocalController(),
				Config && Config->bReduceCameraShake, Config && Config->bReduceMotionHaptics,
				Result.CameraShakes.Num());
		}
#endif
	}
	ABwayCharacterWithAbilities* Character = WeakCharacter.Get();
	// GAS supplies its avatar as Target. The pawn validates that ownership again and
	// rejects late cues after death. Do not use an EffectCauser or instigator fallback.
	// Body action is independent of the inherited audio/VFX spawn chance and local-only filters.
	const bool bActionPlayed = Character && Character->PlayPrimaryActionMontage(PrimaryActionMontage, PrimaryActionPlayRate);
	return bBurstHandled || bActionPlayed;
}
