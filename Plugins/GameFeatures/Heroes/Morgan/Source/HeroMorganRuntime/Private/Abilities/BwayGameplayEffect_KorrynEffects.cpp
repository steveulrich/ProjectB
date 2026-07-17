#include "Abilities/BwayGameplayEffect_KorrynEffects.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayEffect_KorrynEffects)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Korryn_Flock, "Cooldown.Korryn.Flock");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Korryn_BurdenOfSin, "Cooldown.Korryn.BurdenOfSin");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Korryn_CircleOfSpite, "Cooldown.Korryn.CircleOfSpite");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Korryn_AuraOfSilence, "Cooldown.Korryn.AuraOfSilence");

UGE_Bway_KorrynEffectBase::UGE_Bway_KorrynEffectBase()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

void UGE_Bway_KorrynEffectBase::ConfigureDuration(float DurationSeconds)
{
	DurationMagnitude = FScalableFloat(DurationSeconds);
}

void UGE_Bway_KorrynEffectBase::ConfigureGrantedTag(const FGameplayTag& Tag)
{
	PendingGrantedTags.Reset();
	if (Tag.IsValid())
	{
		PendingGrantedTags.Add(Tag);
	}
}

void UGE_Bway_KorrynEffectBase::AddGrantedTag(const FGameplayTag& Tag)
{
	if (Tag.IsValid())
	{
		PendingGrantedTags.AddUnique(Tag);
	}
}

void UGE_Bway_KorrynEffectBase::PostInitProperties()
{
	Super::PostInitProperties();

	if (PendingGrantedTags.Num() == 0)
	{
		return;
	}

	FInheritedTagContainer TagChanges;
	for (const FGameplayTag& Tag : PendingGrantedTags)
	{
		TagChanges.AddTag(Tag);
	}

	UTargetTagsGameplayEffectComponent& TargetTags = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();
	TargetTags.SetAndApplyTargetTagChanges(TagChanges);
}

UGE_Bway_Cooldown_KorrynFlock::UGE_Bway_Cooldown_KorrynFlock()
{
	ConfigureDuration(22.f);
	ConfigureGrantedTag(TAG_Cooldown_Korryn_Flock);
}

UGE_Bway_Cooldown_KorrynBurdenOfSin::UGE_Bway_Cooldown_KorrynBurdenOfSin()
{
	ConfigureDuration(14.f);
	ConfigureGrantedTag(TAG_Cooldown_Korryn_BurdenOfSin);
}

UGE_Bway_Cooldown_KorrynCircleOfSpite::UGE_Bway_Cooldown_KorrynCircleOfSpite()
{
	ConfigureDuration(20.f);
	ConfigureGrantedTag(TAG_Cooldown_Korryn_CircleOfSpite);
}

UGE_Bway_Cooldown_KorrynAuraOfSilence::UGE_Bway_Cooldown_KorrynAuraOfSilence()
{
	ConfigureDuration(30.f);
	ConfigureGrantedTag(TAG_Cooldown_Korryn_AuraOfSilence);
}

UGE_Bway_KorrynArmorShred::UGE_Bway_KorrynArmorShred()
{
	ConfigureDuration(2.f);
	ConfigureGrantedTag(BwayGameplayTags::State_Status_ArmorShred);

	// SetStackingType is not DLL-exported from GameplayAbilities — assign the public
	// (deprecated) property from GF modules instead (same pattern as unexported Lyra helpers).
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateBySource;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 5;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;

	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = UBwayHeroAttributeSet::GetArmorAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FScalableFloat(-2.f);
}

UGE_Bway_KorrynBurdenSlow::UGE_Bway_KorrynBurdenSlow()
{
	ConfigureDuration(2.f);
	ConfigureGrantedTag(BwayGameplayTags::State_Status_Slowed);

	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute();
	Mod.ModifierOp = EGameplayModOp::MultiplyCompound;
	Mod.ModifierMagnitude = FScalableFloat(0.5f);
}

UGE_Bway_KorrynCircleSlow::UGE_Bway_KorrynCircleSlow()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	ConfigureGrantedTag(BwayGameplayTags::State_Status_Slowed);

	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute();
	Mod.ModifierOp = EGameplayModOp::MultiplyCompound;
	Mod.ModifierMagnitude = FScalableFloat(0.85f);
}

UGE_Bway_KorrynCircleDamageAmp::UGE_Bway_KorrynCircleDamageAmp()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	ConfigureGrantedTag(BwayGameplayTags::State_Status_DamageAmplified);

	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute();
	Mod.ModifierOp = EGameplayModOp::Override;
	Mod.ModifierMagnitude = FScalableFloat(1.35f);
}

UGE_Bway_KorrynSilence::UGE_Bway_KorrynSilence()
{
	ConfigureDuration(5.f);
	ConfigureGrantedTag(BwayGameplayTags::State_Status_Silenced);
}

UGE_Bway_KorrynFlockEthereal::UGE_Bway_KorrynFlockEthereal()
{
	ConfigureDuration(1.5f);
	ConfigureGrantedTag(BwayGameplayTags::State_Status_Ethereal);
	AddGrantedTag(TAG_Gameplay_DamageImmunity);
}
