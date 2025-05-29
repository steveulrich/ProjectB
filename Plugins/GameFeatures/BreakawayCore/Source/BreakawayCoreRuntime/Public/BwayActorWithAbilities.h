// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "GameFramework/Actor.h"
#include "BwayActorWithAbilities.generated.h"

class ULyraAbilitySystemComponent;

UCLASS()
class BREAKAWAYCORERUNTIME_API ABwayActorWithAbilities : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** AbilitySet this actor should be granted on spawn, if any */
	UPROPERTY(EditDefaultsOnly, Category="BWay|Ability")
	TObjectPtr<ULyraAbilitySet> AbilitySetOnSpawn;

public:
	// Set Class Defaults
	ABwayActorWithAbilities(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FORCEINLINE FGenericTeamId GetTeamId() const { return TeamId; }

protected:
	//~AActor implementation
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor implementation

	/**
	 * This gets called when it's time to initialize the actor's ASC
	 *
	 * Override this as needed.  In your overrides, you MUST call Super()
	 * **BEFORE** you try to use the ASC at all.  The ASC is not functional
	 * before you call Super().
	 */
	virtual void InitializeAbilitySystem();

	/**
	 * This gets called when it's time to uninitialize the actor's ASC
	 */
	virtual void UninitializeAbilitySystem();
	
	FGenericTeamId TeamId;

public:
	//~IAbilitySystemComponent interface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	ULyraAbilitySystemComponent* GetLyraAbilitySystemComponentChecked() const;
	//~End IAbilitySystemComponent interface

	// Public Static Const Component Names
	static const FName AbilitySystemComponentName;

private:
	// The ability system component for this actor
	UPROPERTY(EditDefaultsOnly, Category="BWay|Ability")
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

	// List of handles granted to this actor on spawn; these get cleaned up in EndPlay
	UPROPERTY(VisibleInstanceOnly, Category="BWay|Ability")
	FLyraAbilitySet_GrantedHandles GrantedHandlesOnSpawn;

};