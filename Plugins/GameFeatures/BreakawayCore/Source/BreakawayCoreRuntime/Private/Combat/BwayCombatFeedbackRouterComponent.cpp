// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayCombatFeedbackRouterComponent.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Combat/BwayCombatFeedbackTags.h"
#include "Combat/BwayCombatNumberPopComponent.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCombatFeedbackRouterComponent)

DEFINE_LOG_CATEGORY_STATIC(LogBwayCombatFeedback, Log, All);

static TAutoConsoleVariable<int32> CVarBwayDebugCombatNumbersRouter(
	TEXT("bway.Combat.DebugNumbersRouter"),
	0,
	TEXT("Log combat number-pop routing (0=off, 1=on)."),
	ECVF_Default);

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
	// Only authority registered these listeners. Each handle owns its subsystem.
	DamageListenerHandle.Unregister();
	HealListenerHandle.Unregister();

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

FBwayDamageFeedbackRecipients UBwayCombatFeedbackRouterComponent::ResolveDamageRecipients(
	UObject* Instigator, UObject* Target, const ABwayGameState* GameState)
{
	FBwayDamageFeedbackRecipients Recipients;
	Recipients.IncomingPlayer = UBwayCombatReadabilityLibrary::ResolveFeedbackPlayerTarget(Target);
	ABwayPlayerState* SourcePlayer = UBwayCombatReadabilityLibrary::ResolveFeedbackInstigator(Instigator);
	// Structure allegiance matters for outgoing confirmation, but never makes its owner a damaged player.
	const ABwayPlayerState* TargetAllegiance = UBwayCombatReadabilityLibrary::ResolveFeedbackInstigator(Target);
	bool bSameTeam = SourcePlayer && SourcePlayer == TargetAllegiance;
	if (GameState && SourcePlayer && TargetAllegiance)
	{
		const int32 SourceTeam = GameState->GetPlayerTeam(SourcePlayer);
		bSameTeam |= SourceTeam != INDEX_NONE && SourceTeam == GameState->GetPlayerTeam(TargetAllegiance);
	}
	if (SourcePlayer && SourcePlayer != Recipients.IncomingPlayer && !bSameTeam)
	{
		Recipients.OutgoingPlayer = SourcePlayer;
	}
	return Recipients;
}

FVector UBwayCombatFeedbackRouterComponent::ResolveWorldLocation(UObject* TargetObject) const
{
	const UBwayCombatReadabilityConfig* Config =
		UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(this);
	const FVector Offset = Config ? Config->NumberWorldOffset : FVector(0.f, 0.f, 90.f);
	return UBwayCombatReadabilityLibrary::ResolveNumberPopWorldLocation(TargetObject, Offset);
}

void UBwayCombatFeedbackRouterComponent::SendNumberPopToPlayer(
	ABwayPlayerState* RecipientPS,
	FGameplayTag NumberTag,
	const FVector& WorldLocation,
	const FLyraVerbMessage& Payload)
{
	if (!RecipientPS || !FMath::IsFinite(Payload.Magnitude) || Payload.Magnitude <= 0.0 || !NumberTag.IsValid())
	{
		return;
	}

	// Keep the effective post-clamp amount; integer match stats are a separate concern.
	FLyraVerbMessage ClientMessage = Payload;
	ClientMessage.Verb = NumberTag;
	// Preserve stable source identity when a projectile/buildable disappears before this RPC arrives.
	if (ABwayPlayerState* SourcePlayer = UBwayCombatReadabilityLibrary::ResolveFeedbackInstigator(Payload.Instigator))
	{
		ClientMessage.Instigator = SourcePlayer;
	}
	ClientMessage.ContextTags.AddTag(NumberTag);
	ClientMessage.bHasWorldLocation = !WorldLocation.ContainsNaN();
	ClientMessage.WorldLocation = WorldLocation;
	ClientMessage.bHasInstigatorWorldLocation = UBwayCombatReadabilityLibrary::ResolveFeedbackSourceLocation(
		Payload.Instigator, ClientMessage.InstigatorWorldLocation);

	// ClientBroadcastMessage_Implementation only broadcasts when NetMode == NM_Client.
	// Listen-server hosts and standalone never take that path, so present locally instead.
	APlayerController* RecipientPC = RecipientPS->GetPlayerController();
	if (!RecipientPC)
	{
		if (CVarBwayDebugCombatNumbersRouter.GetValueOnGameThread() > 0)
		{
			UE_LOG(LogBwayCombatFeedback, Verbose,
				TEXT("Skip number pop for %s (no PlayerController — bot or disconnected)"),
				*GetNameSafe(RecipientPS));
		}
		return;
	}

	if (RecipientPC->IsLocalPlayerController())
	{
		if (UBwayCombatNumberPopComponent* NumberPop =
			RecipientPC->FindComponentByClass<UBwayCombatNumberPopComponent>())
		{
			// Use the same owner-filtered local message route as remote ClientBroadcastMessage.
			UGameplayMessageSubsystem::Get(this).BroadcastMessage(NumberTag, ClientMessage);
		}
		else
		{
			UE_LOG(LogBwayCombatFeedback, Warning,
				TEXT("Local player %s has no UBwayCombatNumberPopComponent"),
				*GetNameSafe(RecipientPC));
		}

		if (CVarBwayDebugCombatNumbersRouter.GetValueOnGameThread() > 0)
		{
			UE_LOG(LogBwayCombatFeedback, Log,
				TEXT("Local number pop %s mag=%.3f loc=%s for %s"),
				*NumberTag.ToString(), Payload.Magnitude, *WorldLocation.ToCompactString(),
				*GetNameSafe(RecipientPS));
		}
		return;
	}

	RecipientPS->ClientBroadcastMessage(ClientMessage);

	if (CVarBwayDebugCombatNumbersRouter.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogBwayCombatFeedback, Log,
			TEXT("RPC number pop %s mag=%.3f for %s"),
			*NumberTag.ToString(), Payload.Magnitude, *GetNameSafe(RecipientPS));
	}
}

void UBwayCombatFeedbackRouterComponent::RouteDamageFeedback(const FLyraVerbMessage& Payload)
{
	if (!FMath::IsFinite(Payload.Magnitude) || Payload.Magnitude <= 0.0)
	{
		return;
	}

	const FBwayDamageFeedbackRecipients Recipients = ResolveDamageRecipients(
		Payload.Instigator, Payload.Target, GetGameState<ABwayGameState>());
	ABwayPlayerState* InstigatorPS = Recipients.OutgoingPlayer;
	ABwayPlayerState* TargetPS = Recipients.IncomingPlayer;
	const FVector WorldLocation = ResolveWorldLocation(Payload.Target);

	if (InstigatorPS && InstigatorPS != TargetPS)
	{
		InstigatorPS->AddDamageDealt(FMath::RoundToInt32(FMath::Min(Payload.Magnitude, static_cast<double>(MAX_int32))));
	}

	// Incoming damage (red): owning client of the damaged player.
	if (TargetPS)
	{
		SendNumberPopToPlayer(
			TargetPS,
			BwayCombatFeedbackTags::Number_IncomingDamage,
			WorldLocation,
			Payload);
	}

	// Outgoing damage (white): owning client of the attacker, enemies only.
	if (InstigatorPS)
	{
		SendNumberPopToPlayer(
			InstigatorPS,
			BwayCombatFeedbackTags::Number_OutgoingDamage,
			WorldLocation,
			Payload);

		if (const UBwayCombatReadabilityConfig* Config =
			UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(this))
		{
			if (Config->HitConfirmationCueTag.IsValid())
			{
				if (AActor* InstigatorActor = InstigatorPS)
				{
					if (UAbilitySystemComponent* InstigatorASC =
						UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor))
					{
						FGameplayCueParameters CueParams;
						CueParams.Location = FVector(WorldLocation);
						CueParams.NormalizedMagnitude = static_cast<float>(Payload.Magnitude);
						// Damage confirmation is emitted only by authority. It was not
						// predicted by the attacker, even when damage happens inside a
						// predicted ability's scope (for example a projectile spawn hit).
						// Do not let GAS discard it as an owner-predicted duplicate.
#if !UE_BUILD_SHIPPING
						const bool bPriorPrediction = InstigatorASC->ScopedPredictionKey.IsValidKey();
#endif
						FScopedPredictionWindow ConfirmedHit(InstigatorASC, FPredictionKey(), false);
#if !UE_BUILD_SHIPPING
						const IConsoleVariable* DebugCue = IConsoleManager::Get().FindConsoleVariable(TEXT("bway.Combat.DebugCueFeedback"));
						if (DebugCue && DebugCue->GetInt() != 0)
						{
							UE_LOG(LogTemp, Display, TEXT("BwayCueFeedback: Dispatch player=%d priorPrediction=%d appliedPrediction=%d"),
								InstigatorPS->GetPlayerId(), bPriorPrediction, InstigatorASC->ScopedPredictionKey.IsValidKey());
						}
#endif
						InstigatorASC->ExecuteGameplayCue(Config->HitConfirmationCueTag, CueParams);
					}
				}
			}
		}
	}
}

void UBwayCombatFeedbackRouterComponent::RouteHealFeedback(const FLyraVerbMessage& Payload)
{
	if (!FMath::IsFinite(Payload.Magnitude) || Payload.Magnitude <= 0.0)
	{
		return;
	}

	ABwayPlayerState* InstigatorPS = UBwayCombatReadabilityLibrary::ResolveFeedbackInstigator(Payload.Instigator);
	ABwayPlayerState* TargetPS = UBwayCombatReadabilityLibrary::ResolveFeedbackPlayerTarget(Payload.Target);
	const FVector WorldLocation = ResolveWorldLocation(Payload.Target);

	if (InstigatorPS)
	{
		InstigatorPS->AddHealingDone(FMath::RoundToInt32(FMath::Min(Payload.Magnitude, static_cast<double>(MAX_int32))));
	}

	// Healing numbers only for the healed player (never enemies).
	if (TargetPS)
	{
		SendNumberPopToPlayer(
			TargetPS,
			BwayCombatFeedbackTags::Number_IncomingHeal,
			WorldLocation,
			Payload);
	}
}
