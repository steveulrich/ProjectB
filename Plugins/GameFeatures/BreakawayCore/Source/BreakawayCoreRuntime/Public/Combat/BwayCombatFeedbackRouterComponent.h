// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messages/LyraVerbMessage.h"
#include "BwayCombatFeedbackRouterComponent.generated.h"

class ABwayPlayerState;
class UBwayCombatReadabilityConfig;

/**
 * Server-authoritative combat feedback router.
 * Listens for Lyra damage/heal verb messages, updates match stats, and fans out
 * owning-client number-pop events (incoming damage red, outgoing white, heal green).
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayCombatFeedbackRouterComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayCombatFeedbackRouterComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OnDamageMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);
	void OnHealMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);

	void RouteDamageFeedback(const FLyraVerbMessage& Payload);
	void RouteHealFeedback(const FLyraVerbMessage& Payload);

	static ABwayPlayerState* ResolvePlayerState(UObject* Object);
	static FVector ResolveWorldLocation(UObject* TargetObject);

	void SendNumberPopToPlayer(
		ABwayPlayerState* RecipientPS,
		FGameplayTag NumberTag,
		int32 Magnitude,
		const FVector& WorldLocation,
		UObject* InstigatorObject,
		UObject* TargetObject);

	FGameplayMessageListenerHandle DamageListenerHandle;
	FGameplayMessageListenerHandle HealListenerHandle;
};
