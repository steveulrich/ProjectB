#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "Buildable/BuildableBase.h"
#include "Components/SceneComponent.h"
#include "BwayActionPresentationTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class ABwayCuePresentationTestBuildable : public ABuildableActor
{
	GENERATED_BODY()
public:
	ABwayCuePresentationTestBuildable()
	{
		PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
		PresentationRoot->SetupAttachment(RootComponent);
		PresentationRoot->SetIsReplicated(false);
	}
	UPROPERTY()
	TObjectPtr<USceneComponent> PresentationRoot;
};

/** Replicated lifetime fixture deliberately has no ASC, matching native ground zones. */
UCLASS(Transient, NotBlueprintable)
class ABwayCuePresentationTestZone : public AActor
{
	GENERATED_BODY()
public:
	ABwayCuePresentationTestZone()
	{
		bReplicates = true;
		SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ZoneRoot")));
		Presentation = CreateDefaultSubobject<UBwayCuePresentationComponent>(TEXT("Presentation"));
		Presentation->bFollowOwnerLifetime = true;
		Presentation->Cue_Active = BwayGameplayCueTags::ZoneHealing;
		Presentation->CueAttachComponent = RootComponent;
	}
	UPROPERTY()
	TObjectPtr<UBwayCuePresentationComponent> Presentation;
};

/** Runs the real GAS activation/commit/end path, with controllable commit rejection. */
UCLASS(Transient, NotBlueprintable)
class UBwayActionPresentationTestAbility : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()
public:
	UBwayActionPresentationTestAbility(const FObjectInitializer& Initializer = FObjectInitializer::Get()) : Super(Initializer)
	{
		NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
		Cue_Cast = BwayGameplayCueTags::AbilityCastStatus;
		Cue_Loop = BwayGameplayCueTags::AbilityActiveDefense;
		Cue_End = BwayGameplayCueTags::AbilityEnd;
	}
	bool bAllowCommit = true;
	bool bCommitResult = false;
	virtual bool CommitCheck(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override
	{
		return bAllowCommit && Super::CommitCheck(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);
	}
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override
	{
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
		bCommitResult = CommitAbility(Handle, ActorInfo, ActivationInfo);
		if (!bCommitResult) EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
	void RepeatCommit() { CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()); }
	void Finish() { EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false); }
	void ClearCueBindings() { Cue_Cast = FGameplayTag(); Cue_Loop = FGameplayTag(); Cue_End = FGameplayTag(); }
};
