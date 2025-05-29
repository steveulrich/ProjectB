
#include "BwayActorWithAbilities.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"

const FName ABwayActorWithAbilities::AbilitySystemComponentName = TEXT("AbilitySystemComponent");


ABwayActorWithAbilities::ABwayActorWithAbilities(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Must replicate Actor for multiplayer
	bReplicates = true;

	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<ULyraAbilitySystemComponent>(this, AbilitySystemComponentName);
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// AbilitySystemComponent needs to be updated at a high frequency.
	NetUpdateFrequency = 100.0f;
}


void ABwayActorWithAbilities::PostInitializeComponents()
{
	// BEFORE PostInit Components:
	InitializeAbilitySystem();

	// Now during PostInit Components, there is a functional ASC for other components to use
	Super::PostInitializeComponents();
}


void ABwayActorWithAbilities::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && AbilitySetOnSpawn)
	{
		UE_LOG(LogTemp, Display, TEXT("Granting OnSpawn Ability Set [%s]"), *GetNameSafe(AbilitySetOnSpawn));

		AbilitySetOnSpawn->GiveToAbilitySystem(AbilitySystemComponent, &GrantedHandlesOnSpawn);
	}
}


void ABwayActorWithAbilities::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeAbilitySystem();

	Super::EndPlay(EndPlayReason);
}


void ABwayActorWithAbilities::InitializeAbilitySystem()
{
	UE_LOG(LogTemp, Display, TEXT("Initializing Ability System"));

	// We expect this to have been set in the constructor
	check(IsValid(AbilitySystemComponent));

	// Initialize ASC on this Actor
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}


void ABwayActorWithAbilities::UninitializeAbilitySystem()
{
	UE_LOG(LogTemp, Display, TEXT("Uninitializing Ability System"));

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->ClearAbilityInput();
		AbilitySystemComponent->RemoveAllGameplayCues();
		AbilitySystemComponent->ClearActorInfo();

		GrantedHandlesOnSpawn.TakeFromAbilitySystem(AbilitySystemComponent);
	}
}


UAbilitySystemComponent* ABwayActorWithAbilities::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}


ULyraAbilitySystemComponent* ABwayActorWithAbilities::GetLyraAbilitySystemComponentChecked() const
{
	check(AbilitySystemComponent);
	return AbilitySystemComponent;
}