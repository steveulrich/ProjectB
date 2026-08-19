// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayCombatFeedbackRouterComponent.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Combat/BwayCombatFeedbackTags.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Messages/LyraVerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCombatFeedbackRouterComponent)

UBwayCombatFeedbackRouterComponent::UBwayCombatFeedbackRouterComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayCombatFeedbackRouterComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	DamageListenerHandle = MessageSubsystem.RegisterListener(
		TAG_Lyra_Damage_Message, this, &ThisClass::OnDamageMessage);
	HealListenerHandle = MessageSubsystem.RegisterListener(
		TAG_Lyra_Heal_Message, this, &ThisClass::OnHealMessage);
}

void UBwayCombatFeedbackRouterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.UnregisterListener(DamageListenerHandle);
		MessageSubsystem.UnregisterListener(HealListenerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UBwayCombatFeedbackRouterComponent::OnDamageMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	(void)Channel;
	RouteDamageFeedback(Payload);
}

void UBwayCombatFeedbackRouterComponent::OnHealMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	(void)Channel;
	RouteHealFeedback(Payload);
}

ABwayPlayerState* UBwayCombatFeedbackRouterComponent::ResolvePlayerState(UObject* Object)
{
	if (ABwayPlayerState* DirectPS = Cast<ABwayPlayerState>(ULyraVerbMessageHelpers::GetPlayerStateFromObject(Object)))
	{
		return DirectPS;
	}

	if (AActor* Actor = Cast<AActor>(Object))
	{
		if (APawn* InstigatorPawn = Cast<APawn>(Actor->GetInstigator()))
		{
			return Cast<ABwayPlayerState>(InstigatorPawn->GetPlayerState());
		}
		if (const AController* Controller = Actor->GetInstigatorController())
		{
			return Cast<ABwayPlayerState>(Controller->PlayerState);
		}
	}

	return nullptr;
}

FVector UBwayCombatFeedbackRouterComponent::ResolveWorldLocation(UObject* TargetObject)
{
	if (const AActor* TargetActor = Cast<AActor>(TargetObject))
	{
		FVector Location = TargetActor->GetActorLocation();
		if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
		{
			Location.Z += TargetPawn->GetDefaultHalfHeight() * 1.5f;
		}
		else
		{
			Location.Z += 90.f;
		}
		return Location;
	}

	if (const APlayerState* PS = Cast<APlayerState>(TargetObject))
	{
		if (const APawn* Pawn = PS->GetPawn())
		{
			FVector Location = Pawn->GetActorLocation();
			Location.Z += Pawn->GetDefaultHalfHeight() * 1.5f;
			return Location;
		}
	}

	return FVector::ZeroVector;
}

void UBwayCombatFeedbackRouterComponent::SendNumberPopToPlayer(
	ABwayPlayerState* RecipientPS,
	FGameplayTag NumberTag,
	int32 Magnitude,
	const FVector& WorldLocation,
	UObject* InstigatorObject,
	UObject* TargetObject)
{
	if (!RecipientPS || Magnitude <= 0 || !NumberTag.IsValid())
	{
		return;
	}

	FLyraVerbMessage ClientMessage;
	ClientMessage.Verb = NumberTag;
	ClientMessage.Instigator = InstigatorObject;
	ClientMessage.Target = TargetObject;
	ClientMessage.Magnitude = static_cast<double>(Magnitude);
	ClientMessage.ContextTags.AddTag(NumberTag);

	// Encode world location into ContextTags is not possible; stash via Magnitude + Verb.
	// Clients resolve location from Target actor. Also push location via a dedicated path:
	// ClientBroadcastMessage only carries FLyraVerbMessage. Target actor location is used client-side.
	(void)WorldLocation;

	RecipientPS->ClientBroadcastMessage(ClientMessage);
}

void UBwayCombatFeedbackRouterComponent::RouteDamageFeedback(const FLyraVerbMessage& Payload)
{
	const int32 Magnitude = FMath::RoundToInt(static_cast<float>(Payload.Magnitude));
	if (Magnitude <= 0)
	{
		return;
	}

	ABwayPlayerState* InstigatorPS = ResolvePlayerState(Payload.Instigator);
	ABwayPlayerState* TargetPS = ResolvePlayerState(Payload.Target);
	const FVector WorldLocation = ResolveWorldLocation(Payload.Target);

	if (InstigatorPS && InstigatorPS != TargetPS)
	{
		InstigatorPS->AddDamageDealt(Magnitude);
	}

	const ABwayGameState* BwayGS = GetGameState<ABwayGameState>();
	bool bSameTeam = false;
	if (BwayGS && InstigatorPS && TargetPS)
	{
		const int32 InstigatorTeam = BwayGS->GetPlayerTeam(InstigatorPS);
		const int32 TargetTeam = BwayGS->GetPlayerTeam(TargetPS);
		bSameTeam = (InstigatorTeam != INDEX_NONE && InstigatorTeam == TargetTeam);
	}

	// Incoming damage (red): owning client of the damaged player.
	if (TargetPS)
	{
		SendNumberPopToPlayer(
			TargetPS,
			BwayCombatFeedbackTags::Number_IncomingDamage,
			Magnitude,
			WorldLocation,
			Payload.Instigator,
			Payload.Target);
	}

	// Outgoing damage (white): owning client of the attacker, enemies only.
	if (InstigatorPS && InstigatorPS != TargetPS && !bSameTeam)
	{
		SendNumberPopToPlayer(
			InstigatorPS,
			BwayCombatFeedbackTags::Number_OutgoingDamage,
			Magnitude,
			WorldLocation,
			Payload.Instigator,
			Payload.Target);

		if (const UBwayCombatReadabilityConfig* Config =
			UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(this))
		{
			if (Config->HitConfirmationCueTag.IsValid())
			{
				if (AActor* InstigatorActor = Cast<AActor>(Payload.Instigator))
				{
					if (UAbilitySystemComponent* InstigatorASC =
						UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor))
					{
						FGameplayCueParameters CueParams;
						CueParams.Location = FVector(WorldLocation);
						CueParams.NormalizedMagnitude = static_cast<float>(Magnitude);
						InstigatorASC->ExecuteGameplayCue(Config->HitConfirmationCueTag, CueParams);
					}
				}
			}
		}
	}
}

void UBwayCombatFeedbackRouterComponent::RouteHealFeedback(const FLyraVerbMessage& Payload)
{
	const int32 Magnitude = FMath::RoundToInt(static_cast<float>(Payload.Magnitude));
	if (Magnitude <= 0)
	{
		return;
	}

	ABwayPlayerState* InstigatorPS = ResolvePlayerState(Payload.Instigator);
	ABwayPlayerState* TargetPS = ResolvePlayerState(Payload.Target);
	const FVector WorldLocation = ResolveWorldLocation(Payload.Target);

	if (InstigatorPS)
	{
		InstigatorPS->AddHealingDone(Magnitude);
	}

	// Healing numbers only for the healed player (never enemies).
	if (TargetPS)
	{
		SendNumberPopToPlayer(
			TargetPS,
			BwayCombatFeedbackTags::Number_IncomingHeal,
			Magnitude,
			WorldLocation,
			Payload.Instigator,
			Payload.Target);
	}
}
