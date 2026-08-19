// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Feedback/NumberPops/LyraNumberPopComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messages/LyraVerbMessage.h"
#include "BwayCombatNumberPopComponent.generated.h"

class UBwayCombatReadabilityConfig;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Local-controller combat number display using Niagara with data-driven colors.
 * Listens for Breakaway number-pop verb messages replicated via ClientBroadcastMessage.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayCombatNumberPopComponent : public ULyraNumberPopComponent
{
	GENERATED_BODY()

public:
	UBwayCombatNumberPopComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void AddNumberPop(const FLyraNumberPopRequest& NewRequest) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OnCombatNumberMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);
	void EnsureConfig() const;
	FVector ResolvePopLocation(const FLyraVerbMessage& Payload) const;
	FLinearColor ResolveColor(const FGameplayTagContainer& TargetTags) const;
	UNiagaraComponent* GetOrCreateNiagaraComponent();

	FGameplayMessageListenerHandle IncomingDamageHandle;
	FGameplayMessageListenerHandle OutgoingDamageHandle;
	FGameplayMessageListenerHandle IncomingHealHandle;

	UPROPERTY(Transient)
	mutable TObjectPtr<const UBwayCombatReadabilityConfig> CachedConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop")
	TSoftObjectPtr<UNiagaraSystem> NumberNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop")
	FName NiagaraArrayName = TEXT("User.DamageInfo");

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop")
	FName NiagaraColorParamName = TEXT("User.Color");

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> NiagaraComp;
};
