#include "Projectiles/BwayKorrynPrimaryProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayKorrynPrimaryProjectile)

ABwayKorrynPrimaryProjectile::ABwayKorrynPrimaryProjectile()
{
	SphereRadius = 18.f;
	if (CollisionSphere)
	{
		CollisionSphere->InitSphereRadius(SphereRadius);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = 2800.f;
		ProjectileMovement->MaxSpeed = 2800.f;
	}
}

void ABwayKorrynPrimaryProjectile::ConfigureProjectile(
	ABwayCharacterWithAbilities* InInstigator,
	float InDamage,
	float InSpeed,
	float InLifeSpan,
	TSubclassOf<UGameplayEffect> InArmorShredEffectClass)
{
	InstigatorCharacter = InInstigator;
	DamageAmount = InDamage;
	ArmorShredEffectClass = InArmorShredEffectClass;

	if (InInstigator)
	{
		SetOwner(InInstigator);
		SetInstigator(InInstigator);
	}

	ConfigureMovement(InSpeed, InLifeSpan);
}

bool ABwayKorrynPrimaryProjectile::HandleDamageHit(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!HasAuthority() || bHasAppliedHit || !HitActor || HitActor == InstigatorCharacter)
	{
		return false;
	}

	ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(HitActor);
	if (!HitCharacter || !InstigatorCharacter)
	{
		return false;
	}

	bool bIsEnemy = false;
	if (UWorld* World = GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			bIsEnemy = !GameState->AreOnSameTeam(InstigatorCharacter, HitCharacter);
		}
		else if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			bIsEnemy = TeamSubsystem->CompareTeams(InstigatorCharacter, HitCharacter) == ELyraTeamComparison::DifferentTeams;
		}
	}

	if (!bIsEnemy)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorCharacter);
	if (DamageAmount > 0.f)
	{
		UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCharacter, DamageAmount, this, InstigatorCharacter);
	}

	if (ArmorShredEffectClass && SourceASC)
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitCharacter))
		{
			FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
			Context.AddSourceObject(this);
			Context.AddInstigator(InstigatorCharacter, this);

			const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(ArmorShredEffectClass, 1.f, Context);
			if (SpecHandle.IsValid())
			{
				TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}

	bHasAppliedHit = true;
	return true;
}
