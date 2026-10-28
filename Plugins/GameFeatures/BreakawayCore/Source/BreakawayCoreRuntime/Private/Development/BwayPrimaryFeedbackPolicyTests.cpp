#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraShakeBase.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/InputDeviceProperties.h"
#include "GameplayCues/BwayGameplayCueNotify_PrimaryAction.h"
#include "Sound/SoundWave.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayPrimaryFeedbackPolicyTest,
	"Breakaway.CombatPolish.Cues.PrimaryAccessibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayPrimaryFeedbackPolicyTest::RunTest(const FString& Parameters)
{
	const auto GetShake = [](FGameplayCueNotify_BurstEffects& Value) -> FGameplayCueNotify_CameraShakeInfo& {
		return *FindFieldChecked<FStructProperty>(Value.StaticStruct(), TEXT("BurstCameraShake"))->ContainerPtrToValuePtr<FGameplayCueNotify_CameraShakeInfo>(&Value);
	};
	const auto GetHaptics = [](FGameplayCueNotify_BurstEffects& Value) -> FGameplayCueNotify_ForceFeedbackInfo& {
		return *FindFieldChecked<FStructProperty>(Value.StaticStruct(), TEXT("BurstForceFeedback"))->ContainerPtrToValuePtr<FGameplayCueNotify_ForceFeedbackInfo>(&Value);
	};
	const auto GetSounds = [](FGameplayCueNotify_BurstEffects& Value) -> TArray<FGameplayCueNotify_SoundInfo>& {
		return *FindFieldChecked<FArrayProperty>(Value.StaticStruct(), TEXT("BurstSounds"))->ContainerPtrToValuePtr<TArray<FGameplayCueNotify_SoundInfo>>(&Value);
	};
	const auto GetDevice = [](FGameplayCueNotify_BurstEffects& Value) -> FGameplayCueNotify_InputDevicePropertyInfo& {
		return *FindFieldChecked<FStructProperty>(Value.StaticStruct(), TEXT("BurstDevicePropertyEffect"))->ContainerPtrToValuePtr<FGameplayCueNotify_InputDevicePropertyInfo>(&Value);
	};
	FGameplayCueNotify_BurstEffects Authored;
	GetShake(Authored).CameraShake = UCameraShakeBase::StaticClass();
	GetShake(Authored).bPlayInWorld = true;
	GetHaptics(Authored).ForceFeedbackEffect = NewObject<UForceFeedbackEffect>();
	GetHaptics(Authored).bPlayInWorld = true;
	GetSounds(Authored).AddDefaulted_GetRef().Sound = NewObject<USoundWave>();
	GetDevice(Authored).DeviceProperties = { UInputDeviceTriggerVibrationProperty::StaticClass(),
		UInputDeviceAudioBasedVibrationProperty::StaticClass(), UColorInputDeviceProperty::StaticClass() };
	for (int32 Flags = 0; Flags < 8; ++Flags)
	{
		const bool bOwner = (Flags & 1) != 0;
		const bool bReduceShake = (Flags & 2) != 0;
		const bool bReduceHaptics = (Flags & 4) != 0;
		auto Effects = UBwayGameplayCueNotify_PrimaryAction::ResolveBurstEffects(
			Authored, bOwner, bReduceShake, bReduceHaptics);
		TestEqual(TEXT("Recoil respects local owner and independent shake setting"),
			GetShake(Effects).CameraShake != nullptr, bOwner && !bReduceShake);
		TestEqual(TEXT("Haptics respect local owner and independent haptics setting"),
			GetHaptics(Effects).ForceFeedbackEffect != nullptr, bOwner && !bReduceHaptics);
		TestFalse(TEXT("Replacement asset cannot enable world recoil"), GetShake(Effects).bPlayInWorld);
		TestFalse(TEXT("Replacement asset cannot enable world haptics"), GetHaptics(Effects).bPlayInWorld);
		TestTrue(TEXT("Spatial sound remains observable by every peer"), GetSounds(Effects)[0].Sound == GetSounds(Authored)[0].Sound);
		TestEqual(TEXT("Trigger/audio vibration respects haptics while controller color remains available"),
			GetDevice(Effects).DeviceProperties.Num(), bOwner ? (bReduceHaptics ? 1 : 3) : 0);
	}
	TestTrue(TEXT("Shared asset recoil is not mutated by local preferences"), GetShake(Authored).CameraShake != nullptr);
	TestTrue(TEXT("Shared asset haptics are not mutated by local preferences"), GetHaptics(Authored).ForceFeedbackEffect != nullptr);
	TestTrue(TEXT("Shared asset placement policy is unchanged"), GetShake(Authored).bPlayInWorld);
	TestEqual(TEXT("Shared device properties are unchanged"), GetDevice(Authored).DeviceProperties.Num(), 3);
	return true;
}

#endif
