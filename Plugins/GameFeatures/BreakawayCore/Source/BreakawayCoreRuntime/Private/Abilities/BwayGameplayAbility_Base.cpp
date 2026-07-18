// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayCharacterWithAbilities.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameplayTags.h"
#include "Combat/BwayDamageLibrary.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "System/LyraGameData.h"
#include "LyraGameplayTags.h"
#include "BwayGameState.h"
#include "Teams/LyraTeamSubsystem.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "System/LyraAssetManager.h"
#include "Development/BwayHeroDebugComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBwayAbility, Log, All);

// Static member initialization - set to true to enable debug logging
bool UBwayGameplayAbility_Base::bEnableAbilityDebugLogging = true;

UBwayGameplayAbility_Base::UBwayGameplayAbility_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	const FGameplayTag RelicCarrierTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.RelicCarrier"), /*ErrorIfNotFound*/ false);
	if (RelicCarrierTag.IsValid())
	{
		ActivationBlockedTags.AddTag(RelicCarrierTag);
	}

	// Silence blocks combat abilities; Slide / RelicRequest inherit UBwayGameplayAbility directly.
	ActivationBlockedTags.AddTag(BwayGameplayTags::State_Status_Silenced);
}

bool UBwayGameplayAbility_Base::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const FString AbilityName = GetClass()->GetName();
	
	// Check base class first
	bool bCanActivate = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
	
	if (bEnableAbilityDebugLogging)
	{
		if (!bCanActivate)
		{
			// Try to determine why it failed
			FString FailReason = TEXT("Unknown");
			
			if (!ActorInfo)
			{
				FailReason = TEXT("No ActorInfo");
			}
			else if (!ActorInfo->AbilitySystemComponent.IsValid())
			{
				FailReason = TEXT("No AbilitySystemComponent");
			}
			else if (!ActorInfo->AvatarActor.IsValid())
			{
				FailReason = TEXT("No AvatarActor");
			}
			else
			{
				// Check for blocking tags
				UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
				if (ASC)
				{
					FGameplayTagContainer OwnedTags;
					ASC->GetOwnedGameplayTags(OwnedTags);
					
					// Check if activation is blocked by tags
					if (ActivationBlockedTags.Num() > 0 && OwnedTags.HasAny(ActivationBlockedTags))
					{
						FailReason = TEXT("Blocked by ActivationBlockedTags");
					}
					else if (ActivationRequiredTags.Num() > 0 && !OwnedTags.HasAll(ActivationRequiredTags))
					{
						FailReason = TEXT("Missing ActivationRequiredTags");
					}
					else if (IsActive())
					{
						FailReason = TEXT("Already Active");
					}
					else
					{
						// Check cooldown using the ability's cooldown check
						const FGameplayTagContainer* CooldownTags = GetCooldownTags();
						if (CooldownTags && CooldownTags->Num() > 0 && ASC->HasAnyMatchingGameplayTags(*CooldownTags))
						{
							FailReason = TEXT("On Cooldown");
						}
						else
						{
							FailReason = TEXT("Base class rejected (check parent ability conditions)");
						}
					}
				}
			}
			
			UE_LOG(LogBwayAbility, Warning, TEXT("[%s] CanActivate: FALSE - Reason: %s"), *AbilityName, *FailReason);
			UBwayHeroDebugComponent::LogAbilityActivation(AbilityName, false, FailReason);
		}
		else
		{
			UE_LOG(LogBwayAbility, Log, TEXT("[%s] CanActivate: TRUE"), *AbilityName);
		}
	}
	
	return bCanActivate;
}

void UBwayGameplayAbility_Base::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FString AbilityName = GetClass()->GetName();
	
	if (bEnableAbilityDebugLogging)
	{
		FString OwnerName = TEXT("Unknown");
		if (ActorInfo && ActorInfo->AvatarActor.IsValid())
		{
			OwnerName = ActorInfo->AvatarActor->GetName();
		}
		
		UE_LOG(LogBwayAbility, Log, TEXT("[%s] ActivateAbility called on %s (Local: %s, Server: %s)"), 
			*AbilityName, 
			*OwnerName,
			ActivationInfo.bCanBeEndedByOtherInstance ? TEXT("Yes") : TEXT("No"),
			HasAuthority(&ActivationInfo) ? TEXT("Yes") : TEXT("No"));
		
		UBwayHeroDebugComponent::LogAbilityActivation(AbilityName, true);
	}
	
	// Call parent implementation
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UBwayGameplayAbility_Base::IsEnemy(ABwayCharacterWithAbilities* OtherCharacter) const
{
	// Ensure CachedCharacter is initialized if not already set by child class
	if (!CachedCharacter)
	{
		CachedCharacter = GetBwayCharacterFromActorInfo();
	}

	if (!CachedCharacter || !OtherCharacter)
	{
		return false;
	}

	// Cache world pointer to avoid multiple GetWorld() calls
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Use BwayGameState to check teams
	if (ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		return !GameState->AreOnSameTeam(CachedCharacter, OtherCharacter);
	}

	// Fallback to Lyra team subsystem
	if (ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
	{
		ELyraTeamComparison Comparison = TeamSubsystem->CompareTeams(CachedCharacter, OtherCharacter);
		return Comparison == ELyraTeamComparison::DifferentTeams;
	}

	return false;
}

bool UBwayGameplayAbility_Base::IsAlly(ABwayCharacterWithAbilities* OtherCharacter) const
{
	// Ensure CachedCharacter is initialized if not already set by child class
	if (!CachedCharacter)
	{
		CachedCharacter = GetBwayCharacterFromActorInfo();
	}

	if (!CachedCharacter || !OtherCharacter)
	{
		return false;
	}

	// Cache world pointer to avoid multiple GetWorld() calls
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Use BwayGameState to check teams
	if (ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		return GameState->AreOnSameTeam(CachedCharacter, OtherCharacter);
	}

	// Fallback to Lyra team subsystem
	if (ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
	{
		ELyraTeamComparison Comparison = TeamSubsystem->CompareTeams(CachedCharacter, OtherCharacter);
		return Comparison == ELyraTeamComparison::OnSameTeam;
	}

	return false;
}

TArray<ABwayCharacterWithAbilities*> UBwayGameplayAbility_Base::GetEnemiesInRadius(const FVector& Location, float Radius) const
{
	TArray<ABwayCharacterWithAbilities*> Enemies;

	// Ensure CachedCharacter is initialized if not already set by child class
	if (!CachedCharacter)
	{
		CachedCharacter = GetBwayCharacterFromActorInfo();
		if (!CachedCharacter)
		{
			return Enemies;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Enemies;
	}

	// Prepare collision query
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CachedCharacter);
	QueryParams.bTraceComplex = false;

	// Sphere overlap for nearby characters
	TArray<FOverlapResult> OverlapResults;
	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(Radius);

	bool bOverlap = World->OverlapMultiByChannel(
		OverlapResults,
		Location,
		FQuat::Identity,
		ECC_Pawn,
		CollisionShape,
		QueryParams
	);

	if (bOverlap)
	{
		// Filter for enemies (different team)
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Overlap.GetActor());
			if (HitCharacter && HitCharacter != CachedCharacter)
			{
				// Check if enemy (different team)
				if (IsEnemy(HitCharacter))
				{
					Enemies.Add(HitCharacter);
				}
			}
		}
	}

	return Enemies;
}

TArray<ABwayCharacterWithAbilities*> UBwayGameplayAbility_Base::GetAlliesInRadius(const FVector& Location, float Radius) const
{
	TArray<ABwayCharacterWithAbilities*> Allies;

	// Ensure CachedCharacter is initialized if not already set by child class
	if (!CachedCharacter)
	{
		CachedCharacter = GetBwayCharacterFromActorInfo();
		if (!CachedCharacter)
		{
			return Allies;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Allies;
	}

	// Prepare collision query
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CachedCharacter);
	QueryParams.bTraceComplex = false;

	// Sphere overlap for nearby characters
	TArray<FOverlapResult> OverlapResults;
	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(Radius);

	bool bOverlap = World->OverlapMultiByChannel(
		OverlapResults,
		Location,
		FQuat::Identity,
		ECC_Pawn,
		CollisionShape,
		QueryParams
	);

	if (bOverlap)
	{
		// Filter for allies (same team)
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Overlap.GetActor());
			if (HitCharacter && HitCharacter != CachedCharacter)
			{
				// Check if ally (same team)
				if (IsAlly(HitCharacter))
				{
					Allies.Add(HitCharacter);
				}
			}
		}
	}

	return Allies;
}

void UBwayGameplayAbility_Base::ApplyGameplayEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> EffectClass, float EffectLevel)
{
	if (!TargetActor || !EffectClass)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	// Use the ability's MakeEffectContext method which properly sets up the ability handle
	FGameplayEffectContextHandle EffectContextHandle = MakeEffectContext(StoredSpecHandle, CurrentActorInfo);

	// Apply effect
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectClass, EffectLevel, EffectContextHandle);
	if (SpecHandle.IsValid())
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void UBwayGameplayAbility_Base::ApplyDamageToEnemy(ABwayCharacterWithAbilities* Enemy, float DamageAmount)
{
	if (!Enemy || DamageAmount <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	AActor* Avatar = CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()
		? CurrentActorInfo->AvatarActor.Get()
		: nullptr;

	UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, Enemy, DamageAmount, Avatar, Avatar);
}

void UBwayGameplayAbility_Base::ApplyHealToAlly(ABwayCharacterWithAbilities* Ally, float HealAmount)
{
	if (!Ally || HealAmount <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Ally);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	const ULyraGameData& GameData = ULyraGameData::Get();
	TSubclassOf<UGameplayEffect> HealEffectClass = ULyraAssetManager::GetSubclass(GameData.HealGameplayEffect_SetByCaller);
	if (!HealEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContextHandle = MakeEffectContext(StoredSpecHandle, CurrentActorInfo);
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(HealEffectClass, 1.0f, EffectContextHandle);
	if (SpecHandle.IsValid())
	{
		if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
		{
			Spec->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Heal, HealAmount);
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

float UBwayGameplayAbility_Base::CalculateScaledDamage(float AbilityBaseDamage, float ScalingCoefficient) const
{
	float AttackStrength = 0.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const UBwayHeroAttributeSet* HeroSet = ASC->GetSet<UBwayHeroAttributeSet>())
		{
			AttackStrength = HeroSet->GetAttackStrength();
		}
		else if (const ULyraCombatSet* CombatSet = ASC->GetSet<ULyraCombatSet>())
		{
			AttackStrength = CombatSet->GetBaseDamage();
		}
	}

	const float FinalDamage = AbilityBaseDamage + (AttackStrength * ScalingCoefficient);
	UE_LOG(LogBwayAbility, Log, TEXT("[%s] ScaledDamage: base=%.1f atk=%.1f scale=%.2f -> %.1f"),
		*GetClass()->GetName(), AbilityBaseDamage, AttackStrength, ScalingCoefficient, FinalDamage);
	return FinalDamage;
}

void UBwayGameplayAbility_Base::ApplyKnockbackToEnemy(ABwayCharacterWithAbilities* Enemy, const FVector& Impulse) const
{
	if (!Enemy || Impulse.IsNearlyZero())
	{
		return;
	}

	Enemy->LaunchCharacter(Impulse, true, true);
}

FGameplayEffectContextHandle UBwayGameplayAbility_Base::MakeEffectContextForAbility() const
{
	// Use the ability's MakeEffectContext method which properly sets up the ability handle
	return MakeEffectContext(StoredSpecHandle, CurrentActorInfo);
}

ABwayCharacterWithAbilities* UBwayGameplayAbility_Base::GetBwayCharacterFromActorInfo() const
{
	if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
	{
		return Cast<ABwayCharacterWithAbilities>(CurrentActorInfo->AvatarActor.Get());
	}
	return nullptr;
}

void UBwayGameplayAbility_Base::ApplyCooldownWithOptionalDuration(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	float DurationSeconds) const
{
	if (DurationSeconds <= 0.f || !CooldownGameplayEffectClass)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownGameplayEffectClass,
		GetAbilityLevel(Handle, ActorInfo));

	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	SpecHandle.Data->SetDuration(DurationSeconds, /*bLockDuration*/ true);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

