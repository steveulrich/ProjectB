#include "Combat/BwayGameplayEffect_DamageSetByCaller.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "LyraGameplayTags.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayEffect_DamageSetByCaller)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayEffect_DamageType_Basic, "GameplayEffect.DamageType.Basic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayEffect_DamageTrait_Instant, "GameplayEffect.DamageTrait.Instant");

UGE_Bway_Damage_SetByCaller::UGE_Bway_Damage_SetByCaller()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = LyraGameplayTags::SetByCaller_Damage;

	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = ULyraHealthSet::GetDamageAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

void UGE_Bway_Damage_SetByCaller::PostInitProperties()
{
	Super::PostInitProperties();

	// Match Lyra GE_Damage_Basic_SetByCaller asset tags (damage-type styling / filters).
	FInheritedTagContainer TagChanges;
	TagChanges.AddTag(TAG_GameplayEffect_DamageType_Basic);
	TagChanges.AddTag(TAG_GameplayEffect_DamageTrait_Instant);

	UAssetTagsGameplayEffectComponent& AssetTags = FindOrAddComponent<UAssetTagsGameplayEffectComponent>();
	AssetTags.SetAndApplyAssetTagChanges(TagChanges);
}
