// Copyright 2022 xist.gg
//
// THIS CODE WILL NOT COMPILE
// See README for more info: https://github.com/x157/Lyra-ActorWithAbilities

#pragma once

#include "CoreMinimal.h"
#include "BwayActorWithAbilities.h"
#include "BwayActorWithAbilitiesAndHealth.generated.h"


class ULyraCombatSet;
class ULyraHealthSet;
class ULyraHealthComponent;


/**
 * Bway Actor with Abilities and Health
 *
 * This adds a Health Set and Health Component to the actor, so that
 * it can be damaged, healed, killed, etc.
 */
UCLASS(DisplayName="Bway Actor with Abilities and Health")
class BREAKAWAYCORERUNTIME_API ABwayActorWithAbilitiesAndHealth : public ABwayActorWithAbilities
{
	GENERATED_BODY()

public:
	// Set Class Defaults
	ABwayActorWithAbilitiesAndHealth(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ABwayActorWithAbilities implementation
	virtual void InitializeAbilitySystem() override;
	virtual void UninitializeAbilitySystem() override;
	//~End of ABwayActorWithAbilities implementation

public:
	// Public Static Component Names
	static const FName CombatSetName;
	static const FName HealthComponentName;
	static const FName HealthSetName;

protected:

	/**
	 * Lyra "Combat Set" is required by Lyra Healing & Damage Effect Calculations.
	 *
	 * This set defines base healing or damage values.
	 *
	 * It should probably be optional (it defaults to all zeros),
	 * but if it does not exist, Lyra will often complain via warning log messages.
	 * 
	 * By default we just use all zeros for these attributes.
	 *
	 * Override as needed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bway|Ability")
	TObjectPtr<ULyraCombatSet> CombatSet;

	/**
	 * Lyra "Health Set" is required by Lyra Healing & Damage Effect Calculations.
	 *
	 * This defines health, max health, incoming healing and incoming damage.
	 *
	 * You probably want to derive your own class from this to add your custom
	 * attributes to this one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bway|Ability")
	TObjectPtr<ULyraHealthSet> HealthSet;

	/**
	 * Lyra "Health Component" is a convenience interface for HealthSet
	 *
	 * This requires the HealthSet and plugs into its event handlers.
	 * This makes it easier to respond to these events in BPs, for example.
	 *
	 * This component also seems to be what is responsible for handling the
	 * death of an Actor via network replication.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bway|Ability")
	TObjectPtr<ULyraHealthComponent> HealthComponent;

};