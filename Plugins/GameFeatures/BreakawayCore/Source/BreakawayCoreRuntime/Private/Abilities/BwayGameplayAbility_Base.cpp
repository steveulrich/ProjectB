// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayCharacterWithAbilities.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
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

UBwayGameplayAbility_Base::UBwayGameplayAbility_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
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
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
	
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	// Get damage gameplay effect from LyraGameData
	const ULyraGameData& GameData = ULyraGameData::Get();
	TSubclassOf<UGameplayEffect> DamageEffectClass = ULyraAssetManager::GetSubclass(GameData.DamageGameplayEffect_SetByCaller);
	
	if (!DamageEffectClass)
	{
		return;
	}

	// Use the ability's MakeEffectContext method which properly sets up the ability handle
	FGameplayEffectContextHandle EffectContextHandle = MakeEffectContext(StoredSpecHandle, CurrentActorInfo);

	// Apply damage effect with SetByCaller magnitude
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContextHandle);
	if (SpecHandle.IsValid())
	{
		FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
		if (Spec)
		{
			// Set damage magnitude using SetByCaller tag from Lyra
			Spec->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, DamageAmount);
		}
		
		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
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

