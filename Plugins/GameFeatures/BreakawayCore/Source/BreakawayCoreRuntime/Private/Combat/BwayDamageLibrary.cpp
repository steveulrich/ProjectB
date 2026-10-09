#include "Combat/BwayDamageLibrary.h"

#include "AbilitySystemGlobals.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "Combat/BwayGameplayEffect_DamageSetByCaller.h"
#include "LyraGameplayTags.h"
#include "Buildable/BuildableBase.h"
#include "Teams/LyraTeamSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayDamageLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogBwayDamage, Log, All);

bool UBwayDamageLibrary::ApplyDamageToEnemyBuildable(ABwayCharacterWithAbilities* InstigatorCharacter,
	AActor* TargetActor, float RawDamage, AActor* EffectCauser)
{
	const ABuildableActor* Buildable = Cast<ABuildableActor>(TargetActor);
	UE_LOG(LogBwayDamage, Verbose, TEXT("BuildablePrimary source=%s target=%s buildable=%d sourceAuthority=%d damageable=%d raw=%.3f"),
		*GetNameSafe(InstigatorCharacter), *GetNameSafe(TargetActor), Buildable != nullptr, InstigatorCharacter && InstigatorCharacter->HasAuthority(),
		Buildable && Buildable->CanBeDamaged(), RawDamage);
	if (!InstigatorCharacter || !InstigatorCharacter->HasAuthority() || !Buildable
		|| !Buildable->CanBeDamaged() || Buildable->IsActorBeingDestroyed()) return false;
	const ULyraTeamSubsystem* Teams = InstigatorCharacter->GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	const int32 SourceTeam = Teams ? Teams->FindTeamFromObject(InstigatorCharacter) : INDEX_NONE;
	const int32 TargetTeam = Buildable->GetTeamId().GetId();
	UE_LOG(LogBwayDamage, Verbose, TEXT("BuildablePrimary teams=%d/%d sourceASC=%s"), SourceTeam, TargetTeam, *GetNameSafe(InstigatorCharacter->GetAbilitySystemComponent()));
	if (SourceTeam == INDEX_NONE || Buildable->GetTeamId() == FGenericTeamId::NoTeam
		|| TargetTeam == 0 || SourceTeam == TargetTeam) return false;
	return ApplyDamageFromSource(InstigatorCharacter->GetAbilitySystemComponent(), TargetActor,
		RawDamage, EffectCauser ? EffectCauser : InstigatorCharacter, InstigatorCharacter);
}

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
	// GAS does not consult AActor::CanBeDamaged: enforce the construction/trap contract here.
	if (const ABuildableActor* Buildable = Cast<ABuildableActor>(TargetActor))
	{
		if (!Buildable->HasAuthority() || !Buildable->CanBeDamaged() || Buildable->IsActorBeingDestroyed()) return false;
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

	// Use Breakaway's pure SetByCaller→Damage GE. Lyra's GameData damage GE runs
	// ULyraDamageExecution and adds Source CombatSet::BaseDamage on top of SetByCaller.
	const TSubclassOf<UGameplayEffect> DamageEffectClass = UGE_Bway_Damage_SetByCaller::StaticClass();

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
	UE_LOG(LogBwayDamage, Verbose, TEXT("ApplyDamageFromSource: raw=%.1f final=%.1f target=%s"),
		RawDamage, FinalDamage, *GetNameSafe(TargetActor));
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
