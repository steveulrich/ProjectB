#include "Combat/BwayDamageLibrary.h"

#include "AbilitySystemGlobals.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "LyraGameplayTags.h"
#include "System/LyraAssetManager.h"
#include "System/LyraGameData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayDamageLibrary)

float UBwayDamageLibrary::ResolveFinalDamage(float RawDamage, const UAbilitySystemComponent* TargetASC)
{
	if (RawDamage <= 0.f)
	{
		return 0.f;
	}

	float Armor = 0.f;
	float IncomingMult = 1.f;

	if (TargetASC)
	{
		if (const UBwayHeroAttributeSet* HeroSet = TargetASC->GetSet<UBwayHeroAttributeSet>())
		{
			Armor = HeroSet->GetArmor();
			IncomingMult = HeroSet->GetIncomingDamageMultiplier();
		}
	}

	// Sheet: 1 Armor = 1% reduction; negative Armor increases damage. Floor at 0.
	const float AfterArmor = RawDamage * (1.f - Armor * 0.01f);
	const float FinalDamage = FMath::Max(0.f, AfterArmor * IncomingMult);
	return FinalDamage;
}

bool UBwayDamageLibrary::ApplyDamageFromSource(
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	float RawDamage,
	AActor* EffectCauser,
	AActor* InstigatorActor)
{
	if (!SourceASC || !TargetActor || RawDamage <= 0.f)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC)
	{
		return false;
	}

	const float FinalDamage = ResolveFinalDamage(RawDamage, TargetASC);
	if (FinalDamage <= 0.f)
	{
		return false;
	}

	const ULyraGameData& GameData = ULyraGameData::Get();
	const TSubclassOf<UGameplayEffect> DamageEffectClass = ULyraAssetManager::GetSubclass(GameData.DamageGameplayEffect_SetByCaller);
	if (!DamageEffectClass)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(EffectCauser ? EffectCauser : SourceASC->GetOwnerActor());
	if (InstigatorActor || EffectCauser)
	{
		EffectContext.AddInstigator(InstigatorActor, EffectCauser);
	}

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return false;
	}

	Spec->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, FinalDamage);
	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec);
	return true;
}

bool UBwayDamageLibrary::ApplyDamageToEnemy(
	ABwayCharacterWithAbilities* InstigatorCharacter,
	ABwayCharacterWithAbilities* Enemy,
	float RawDamage,
	AActor* EffectCauser)
{
	if (!InstigatorCharacter || !Enemy || RawDamage <= 0.f)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = InstigatorCharacter->GetAbilitySystemComponent();
	return ApplyDamageFromSource(
		SourceASC,
		Enemy,
		RawDamage,
		EffectCauser ? EffectCauser : InstigatorCharacter,
		InstigatorCharacter);
}
