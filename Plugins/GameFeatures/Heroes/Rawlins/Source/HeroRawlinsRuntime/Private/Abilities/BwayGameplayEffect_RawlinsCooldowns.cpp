#include "Abilities/BwayGameplayEffect_RawlinsCooldowns.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayEffect_RawlinsCooldowns)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Rawlins_DoubleDown, "Cooldown.Rawlins.DoubleDown");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Rawlins_PowerShot, "Cooldown.Rawlins.PowerShot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Rawlins_SlideShot, "Cooldown.Rawlins.SlideShot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Rawlins_BlazingBarrage, "Cooldown.Rawlins.BlazingBarrage");

UGE_Bway_Cooldown_RawlinsBase::UGE_Bway_Cooldown_RawlinsBase()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

void UGE_Bway_Cooldown_RawlinsBase::ConfigureDuration(float DurationSeconds)
{
	DurationMagnitude = FScalableFloat(DurationSeconds);
}

void UGE_Bway_Cooldown_RawlinsBase::ConfigureGrantedCooldownTag(const FGameplayTag& CooldownTag)
{
	PendingCooldownTag = CooldownTag;
}

void UGE_Bway_Cooldown_RawlinsBase::PostInitProperties()
{
	Super::PostInitProperties();

	if (!PendingCooldownTag.IsValid())
	{
		return;
	}

	FInheritedTagContainer TagChanges;
	TagChanges.AddTag(PendingCooldownTag);

	UTargetTagsGameplayEffectComponent& TargetTags = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();
	TargetTags.SetAndApplyTargetTagChanges(TagChanges);
}

UGE_Bway_Cooldown_RawlinsDoubleDown::UGE_Bway_Cooldown_RawlinsDoubleDown()
{
	ConfigureDuration(14.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Rawlins_DoubleDown);
}

UGE_Bway_Cooldown_RawlinsPowerShot::UGE_Bway_Cooldown_RawlinsPowerShot()
{
	ConfigureDuration(8.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Rawlins_PowerShot);
}

UGE_Bway_Cooldown_RawlinsSlideShot::UGE_Bway_Cooldown_RawlinsSlideShot()
{
	ConfigureDuration(18.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Rawlins_SlideShot);
}

UGE_Bway_Cooldown_RawlinsBlazingBarrage::UGE_Bway_Cooldown_RawlinsBlazingBarrage()
{
	ConfigureDuration(25.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Rawlins_BlazingBarrage);
}
