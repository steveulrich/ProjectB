#include "Abilities/BwayGameplayEffect_ArgusCooldowns.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayEffect_ArgusCooldowns)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Argus_Slide, "Cooldown.Argus.Slide");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Argus_NoRetreat, "Cooldown.Argus.NoRetreat");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Argus_ForGlory, "Cooldown.Argus.ForGlory");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Argus_Retribution, "Cooldown.Argus.Retribution");

UGE_Bway_Cooldown_ArgusBase::UGE_Bway_Cooldown_ArgusBase()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

void UGE_Bway_Cooldown_ArgusBase::ConfigureDuration(float DurationSeconds)
{
	DurationMagnitude = FScalableFloat(DurationSeconds);
}

void UGE_Bway_Cooldown_ArgusBase::ConfigureGrantedCooldownTag(const FGameplayTag& CooldownTag)
{
	PendingCooldownTag = CooldownTag;
}

void UGE_Bway_Cooldown_ArgusBase::PostInitProperties()
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

UGE_Bway_Cooldown_ArgusSlide::UGE_Bway_Cooldown_ArgusSlide()
{
	ConfigureDuration(18.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Argus_Slide);
}

UGE_Bway_Cooldown_NoRetreat::UGE_Bway_Cooldown_NoRetreat()
{
	ConfigureDuration(12.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Argus_NoRetreat);
}

UGE_Bway_Cooldown_ForGlory::UGE_Bway_Cooldown_ForGlory()
{
	ConfigureDuration(25.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Argus_ForGlory);
}

UGE_Bway_Cooldown_Retribution::UGE_Bway_Cooldown_Retribution()
{
	ConfigureDuration(30.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Argus_Retribution);
}
