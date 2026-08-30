// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayRelicManagerComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "SpawnSystem/BwaySpawnPoint.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRelicManagerComponent)

DEFINE_LOG_CATEGORY_STATIC(LogBwayRelic, Log, All);

UBwayRelicManagerComponent::UBwayRelicManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UBwayRelicManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache the spawn point manager
	if (AActor* Owner = GetOwner())
	{
		CachedSpawnPointManager = Owner->FindComponentByClass<UBwaySpawnPointManagerComponent>();
	}

	// Initialize relic spawn tag if not set
	if (!RelicSpawnTag.IsValid())
	{
		RelicSpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"));
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
		DamageListenerHandle = MessageSubsystem.RegisterListener(
			TAG_Lyra_Damage_Message, this, &ThisClass::OnCarrierDamageMessage);
	}
}

void UBwayRelicManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.UnregisterListener(DamageListenerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UBwayRelicManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBwayRelicManagerComponent, ActiveRelic);
	DOREPLIFETIME(UBwayRelicManagerComponent, RelicPossessingTeam);
}

const URelicSettings* UBwayRelicManagerComponent::GetRelicSettings() const
{
	if (ActiveRelic)
	{
		if (const URelicSettings* RelicOwnedSettings = ActiveRelic->GetRelicSettings())
		{
			return RelicOwnedSettings;
		}
	}

	if (!RelicSettingsAsset.IsNull())
	{
		if (const URelicSettings* Loaded = RelicSettingsAsset.Get())
		{
			return Loaded;
		}

		return RelicSettingsAsset.LoadSynchronous();
	}

	return nullptr;
}

bool UBwayRelicManagerComponent::GetRelicSpawnLocationForPosition(const FVector& WorldPosition, FVector& OutSpawnLocation) const
{
	UBwaySpawnPointManagerComponent* SpawnManager = CachedSpawnPointManager;
	if (!SpawnManager)
	{
		if (const AActor* Owner = GetOwner())
		{
			SpawnManager = Owner->FindComponentByClass<UBwaySpawnPointManagerComponent>();
		}
	}

	if (!SpawnManager)
	{
		return false;
	}

	FGameplayTag SpawnTag = RelicSpawnTag;
	if (!SpawnTag.IsValid())
	{
		SpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"), /*ErrorIfNotFound*/ false);
	}

	SpawnManager->DiscoverSpawnPoints();

	if (const ABwaySpawnPoint* SpawnPoint = SpawnManager->GetClosestSpawnPoint(WorldPosition, SpawnTag))
	{
		OutSpawnLocation = SpawnPoint->GetActorLocation();
		return true;
	}

	return false;
}

ARelicActor* UBwayRelicManagerComponent::SpawnRelic()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return nullptr;
	}

	if (ActiveRelic)
	{
		return ActiveRelic;
	}

	if (!RelicClass)
	{
		return nullptr;
	}

	if (!CachedSpawnPointManager)
	{
		if (AActor* Owner = GetOwner())
		{
			CachedSpawnPointManager = Owner->FindComponentByClass<UBwaySpawnPointManagerComponent>();
		}
	}

	if (!CachedSpawnPointManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: No SpawnPointManager found on %s"), *GetNameSafe(GetOwner()));
		return nullptr;
	}

	if (!RelicSpawnTag.IsValid())
	{
		RelicSpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"), /*ErrorIfNotFound*/ false);
	}

	CachedSpawnPointManager->DiscoverSpawnPoints();

	TArray<ABwaySpawnPoint*> RelicSpawnPoints = CachedSpawnPointManager->GetSpawnPointsByTag(RelicSpawnTag);
	if (RelicSpawnPoints.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: No registered spawn points found for tag %s"), *RelicSpawnTag.ToString());
		return nullptr;
	}

	for (ABwaySpawnPoint* SpawnPoint : RelicSpawnPoints)
	{
		if (!SpawnPoint)
		{
			continue;
		}

		AActor* SpawnedActor = SpawnPoint->GetSpawnedObject();
		if (!SpawnedActor)
		{
			SpawnedActor = SpawnPoint->SpawnObject();
		}

		if (!SpawnedActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: Spawn point %s did not spawn an actor"), *GetNameSafe(SpawnPoint));
			continue;
		}

		ActiveRelic = Cast<ARelicActor>(SpawnedActor);
		if (!ActiveRelic)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: Spawn point %s spawned %s, which is not an ARelicActor"),
				*GetNameSafe(SpawnPoint), *GetNameSafe(SpawnedActor));
			continue;
		}

		if (!RelicSettingsAsset.IsNull())
		{
			URelicSettings* Settings = RelicSettingsAsset.LoadSynchronous();
			if (Settings)
			{
				ActiveRelic->InitializeRelicData(Settings);
				UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Relic initialized with RelicSettings"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: Failed to load RelicSettingsAsset"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: No RelicSettingsAsset configured — relic spawned without settings!"));
		}

		UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Spawned relic %s via spawn point %s"),
			*GetNameSafe(ActiveRelic), *GetNameSafe(SpawnPoint));
		return ActiveRelic;
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: Could not spawn a valid relic from %d registered relic spawn point(s)"), RelicSpawnPoints.Num());
	return nullptr;
}

void UBwayRelicManagerComponent::ResetRelic()
{
	if (GetOwnerRole() != ROLE_Authority || !ActiveRelic)
	{
		return;
	}

	ActiveRelic->BeginResettingState();
	ActiveRelic->ClearScoringFlag();

	if (CachedSpawnPointManager)
	{
		ABwaySpawnPoint* RelicSpawn = CachedSpawnPointManager->GetRandomSpawnPoint(RelicSpawnTag);
		if (RelicSpawn)
		{
			ActiveRelic->SetActorLocation(RelicSpawn->GetActorLocation());
			ActiveRelic->SetActorRotation(RelicSpawn->GetActorRotation());
			ActiveRelic->OnDropped();
			
			OnRelicResetEvent.Broadcast(ActiveRelic);
			UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Relic reset to spawn point: %s"), *RelicSpawn->GetName());
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayRelicManager: No relic spawn points found for reset"));
}

void UBwayRelicManagerComponent::OnCarrierDamageMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	(void)Channel;

	if (GetOwnerRole() != ROLE_Authority || !ActiveRelic)
	{
		return;
	}

	if (ActiveRelic->GetCurrentState() != ERelicState::Carried || !ActiveRelic->CurrentCarrier)
	{
		return;
	}

	const URelicSettings* Settings = GetRelicSettings();
	if (Settings && !Settings->bFumbleOnDamage)
	{
		return;
	}

	if (Payload.Magnitude <= 0.0)
	{
		return;
	}

	ABwayCharacterWithAbilities* Carrier = ActiveRelic->CurrentCarrier;
	APlayerState* TargetPS = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Payload.Target);
	const bool bTargetIsCarrierPawn = (Cast<AActor>(Payload.Target) == Carrier);
	const bool bTargetIsCarrierPS = (TargetPS && TargetPS == Carrier->GetPlayerState());
	if (!bTargetIsCarrierPawn && !bTargetIsCarrierPS)
	{
		return;
	}

	UE_LOG(LogBwayRelic, Log, TEXT("Fumble-on-damage: %s took %.1f damage while carrying — dropping relic"),
		*GetNameSafe(Carrier), static_cast<float>(Payload.Magnitude));

	ActiveRelic->ForceFumbleFromDamage();
}

void UBwayRelicManagerComponent::DropRelicIfCarriedBy(ABwayCharacterWithAbilities* Character)
{
	if (GetOwnerRole() != ROLE_Authority || !ActiveRelic || !Character)
	{
		return;
	}

	if (ActiveRelic->CurrentCarrier == Character && ActiveRelic->GetCurrentState() == ERelicState::Carried)
	{
		UE_LOG(LogBwayRelic, Log, TEXT("DropRelicIfCarriedBy: %s died while carrying — dropping relic (not a fumble)"),
			*GetNameSafe(Character));
		ActiveRelic->OnDropped();
	}
}

void UBwayRelicManagerComponent::OnRelicCarrierChanged(ABwayCharacterWithAbilities* NewCarrier)
{
	ABwayGameState* BwayGS = Cast<ABwayGameState>(GetOwner());

	if (NewCarrier && NewCarrier->GetPlayerState() && BwayGS)
	{
		RelicPossessingTeam = BwayGS->GetPlayerTeam(NewCarrier->GetPlayerState());
	}
	else
	{
		RelicPossessingTeam = -1; // Neutral
	}

	OnRelicCarrierChangedEvent.Broadcast(ActiveRelic, NewCarrier);
}

void UBwayRelicManagerComponent::OnRep_RelicPossessingTeam()
{
	if (RelicPossessingTeam >= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Team %d now possesses the relic"), RelicPossessingTeam + 1);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Relic is now neutral"));
	}
}

void UBwayRelicManagerComponent::SetActiveRelic(ARelicActor* NewRelic)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	ActiveRelic = NewRelic;
	UE_LOG(LogTemp, Log, TEXT("BwayRelicManager: Active relic set to %s"), *GetNameSafe(ActiveRelic));
}
