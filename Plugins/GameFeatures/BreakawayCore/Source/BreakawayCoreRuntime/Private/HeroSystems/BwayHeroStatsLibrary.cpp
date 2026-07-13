#include "HeroSystems/BwayHeroStatsLibrary.h"

#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroStatsLibrary)

namespace BwayHeroStatsLibraryPrivate
{
	static bool HasSpawnedAttributeSetOfClass(const UAbilitySystemComponent* ASC, TSubclassOf<UAttributeSet> AttributeSetClass)
	{
		if (!ASC || !AttributeSetClass)
		{
			return false;
		}

		for (const UAttributeSet* SpawnedSet : ASC->GetSpawnedAttributes())
		{
			if (SpawnedSet && SpawnedSet->IsA(AttributeSetClass))
			{
				return true;
			}
		}

		return false;
	}
}

float UBwayHeroStatsLibrary::ConvertSpeedRatingToMaxWalkSpeed(float SpeedRating)
{
	if (SpeedRating > 100.f)
	{
		return SpeedRating;
	}

	return FMath::Max(100.f, SpeedRating * 60.f);
}

void UBwayHeroStatsLibrary::EnsureHeroAttributeSet(UAbilitySystemComponent* ASC)
{
	if (!ASC || ASC->GetSet<UBwayHeroAttributeSet>())
	{
		return;
	}

	UBwayHeroAttributeSet* HeroSet = NewObject<UBwayHeroAttributeSet>(ASC->GetOwner());
	ASC->AddSpawnedAttribute(HeroSet);
}

void UBwayHeroStatsLibrary::ApplyHeroStatsFromDataAsset(ABwayCharacterWithAbilities* Character, const UBwayHeroDataAsset* HeroData)
{
	if (!Character || !HeroData || !Character->HasAuthority())
	{
		return;
	}

	ULyraAbilitySystemComponent* ASC = Character->GetLyraAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroStatsLibrary: No ASC on %s — stats not applied"), *Character->GetName());
		return;
	}

	if (HeroData->AttributeSetClass)
	{
		if (!BwayHeroStatsLibraryPrivate::HasSpawnedAttributeSetOfClass(ASC, HeroData->AttributeSetClass))
		{
			UAttributeSet* SpawnedSet = NewObject<UAttributeSet>(ASC->GetOwner(), HeroData->AttributeSetClass);
			ASC->AddSpawnedAttribute(SpawnedSet);
		}
	}

	EnsureHeroAttributeSet(ASC);

	const FHeroStats& Stats = HeroData->HeroStats;
	const float MaxHealth = FMath::Max(1.f, Stats.MaxHealth);
	const float BaseDamage = FMath::Max(0.f, Stats.BaseDamage);
	const float MaxWalkSpeed = ConvertSpeedRatingToMaxWalkSpeed(Stats.MoveSpeed);

	if (const ULyraHealthSet* HealthSet = ASC->GetSet<ULyraHealthSet>())
	{
		ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), MaxHealth);
		ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), MaxHealth);
	}

	if (const ULyraCombatSet* CombatSet = ASC->GetSet<ULyraCombatSet>())
	{
		ASC->SetNumericAttributeBase(ULyraCombatSet::GetBaseDamageAttribute(), BaseDamage);
	}

	if (UBwayHeroAttributeSet* HeroSet = const_cast<UBwayHeroAttributeSet*>(ASC->GetSet<UBwayHeroAttributeSet>()))
	{
		HeroSet->SetAttackStrength(BaseDamage);
		HeroSet->SetArmor(Stats.Armor);
		HeroSet->SetMoveSpeedRating(Stats.MoveSpeed);
	}

	if (UBwayCharacterMovementComponent* MoveComp = Character->GetBwayCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = MaxWalkSpeed;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroStatsLibrary: Applied stats to %s — HP %.0f Armor %.0f Atk %.0f SpeedRating %.1f (%.0f uu/s)"),
		*Character->GetName(), MaxHealth, Stats.Armor, BaseDamage, Stats.MoveSpeed, MaxWalkSpeed);
}
