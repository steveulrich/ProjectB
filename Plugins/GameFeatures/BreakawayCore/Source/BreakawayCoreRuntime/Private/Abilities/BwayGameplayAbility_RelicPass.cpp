#include "Abilities/BwayGameplayAbility_RelicPass.h"

#include "BwayCharacterWithAbilities.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "LyraGameplayTags.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"

namespace
{
	ARelicActor* FindCarriedRelic(const ABwayCharacterWithAbilities* Character)
	{
		if (!Character)
		{
			return nullptr;
		}

		TArray<AActor*> AttachedActors;
		Character->GetAttachedActors(AttachedActors);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (ARelicActor* Relic = Cast<ARelicActor>(AttachedActor))
			{
				return Relic;
			}
		}

		return nullptr;
	}
}

UBwayGameplayAbility_RelicPass::UBwayGameplayAbility_RelicPass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Relic.Pass"), false));
	const FGameplayTag RelicCarrierTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.RelicCarrier"), false);
	if (RelicCarrierTag.IsValid())
	{
		// UBwayGameplayAbility_Base blocks combat while carrying; the pass is the
		// intentional carrier exception alongside throw/drop/dash.
		ActivationBlockedTags.RemoveTag(RelicCarrierTag);
		ActivationRequiredTags.AddTag(RelicCarrierTag);
	}
	ActivationBlockedTags.AddTag(LyraGameplayTags::Status_Death);

	DisplayData.AbilityName = NSLOCTEXT("Relic", "PassName", "Pass");
	DisplayData.Description = NSLOCTEXT("Relic", "PassDescription", "Pass the relic to an allied teammate in your aim direction.");
}

void UBwayGameplayAbility_RelicPass::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	StoredSpecHandle = Handle;

	ABwayCharacterWithAbilities* Character = GetBwayCharacterFromActorInfo();
	ARelicActor* Relic = FindCarriedRelic(Character);
	if (!Character || !Relic || !HasAuthority(&ActivationInfo) ||
		Relic->CurrentCarrier != Character || Relic->GetCurrentState() != ERelicState::Carried)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FVector AimDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	if (const AController* Controller = Character->GetController())
	{
		AimDirection = Controller->GetControlRotation().Vector().GetSafeNormal2D();
	}
	if (AimDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const float MinAimDot = FMath::Cos(FMath::DegreesToRadians(FMath::Max(0.0f, PassTargetConeDegrees)));
	ABwayCharacterWithAbilities* BestTeammate = nullptr;
	float BestDot = MinAimDot;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	UWorld* World = Character->GetWorld();
	if (World)
	{
		for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
		{
			ABwayCharacterWithAbilities* Candidate = *It;
			if (!Candidate || Candidate == Character || !IsAlly(Candidate))
			{
				continue;
			}

			if (UAbilitySystemComponent* CandidateASC = Candidate->GetAbilitySystemComponent())
			{
				if (CandidateASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death) ||
					CandidateASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death_Dying))
				{
					continue;
				}
			}

			const FVector ToCandidate = Candidate->GetActorLocation() - Character->GetActorLocation();
			const float DistanceSquared = ToCandidate.SizeSquared2D();
			if (DistanceSquared <= KINDA_SMALL_NUMBER || DistanceSquared > FMath::Square(MaxPassTargetRange))
			{
				continue;
			}

			const float Dot = FVector::DotProduct(AimDirection, ToCandidate.GetSafeNormal2D());
			if (Dot < MinAimDot || Dot < BestDot || (FMath::IsNearlyEqual(Dot, BestDot) && DistanceSquared >= BestDistanceSquared))
			{
				continue;
			}

			BestTeammate = Candidate;
			BestDot = Dot;
			BestDistanceSquared = DistanceSquared;
		}
	}

	const URelicSettings* Settings = Relic->GetRelicSettings();
	const float EffectivePassSpeed = (Settings && Settings->PassSpeed > 0.0f)
		? Settings->PassSpeed
		: (Settings ? Settings->ThrowVelocity : 1500.0f);
	if (!BestTeammate || EffectivePassSpeed <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FVector ToTeammate = BestTeammate->GetActorLocation() - Character->GetActorLocation();
	const float EffectiveThrowAngle = Settings ? Settings->ThrowAngle : 30.0f;
	ToTeammate.Z += FMath::Tan(FMath::DegreesToRadians(EffectiveThrowAngle)) * ToTeammate.Size2D();
	Relic->Server_PassRelic(ToTeammate.GetSafeNormal() * EffectivePassSpeed);

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
