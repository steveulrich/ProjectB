#include "Abilities/BwayGameplayEffect_AlonaCooldowns.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayEffect_AlonaCooldowns)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Alona_SunsGrace, "Cooldown.Alona.SunsGrace");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Alona_Radiance, "Cooldown.Alona.Radiance");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Alona_SunBurst, "Cooldown.Alona.SunBurst");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Alona_Blessing, "Cooldown.Alona.Blessing");

UGE_Bway_Cooldown_AlonaBase::UGE_Bway_Cooldown_AlonaBase()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

void UGE_Bway_Cooldown_AlonaBase::ConfigureDuration(float DurationSeconds)
{
	DurationMagnitude = FScalableFloat(DurationSeconds);
}

void UGE_Bway_Cooldown_AlonaBase::ConfigureGrantedCooldownTag(const FGameplayTag& CooldownTag)
{
	PendingCooldownTag = CooldownTag;
}

void UGE_Bway_Cooldown_AlonaBase::PostInitProperties()
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

UGE_Bway_Cooldown_AlonaSunsGrace::UGE_Bway_Cooldown_AlonaSunsGrace()
{
	ConfigureDuration(22.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Alona_SunsGrace);
}

UGE_Bway_Cooldown_AlonaRadiance::UGE_Bway_Cooldown_AlonaRadiance()
{
	ConfigureDuration(8.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Alona_Radiance);
}

UGE_Bway_Cooldown_AlonaSunBurst::UGE_Bway_Cooldown_AlonaSunBurst()
{
	ConfigureDuration(20.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Alona_SunBurst);
}

UGE_Bway_Cooldown_AlonaBlessing::UGE_Bway_Cooldown_AlonaBlessing()
{
	ConfigureDuration(25.f);
	ConfigureGrantedCooldownTag(TAG_Cooldown_Alona_Blessing);
}
