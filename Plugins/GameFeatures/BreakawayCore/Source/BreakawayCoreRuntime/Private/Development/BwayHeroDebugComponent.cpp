// Copyright Epic Games, Inc. All Rights Reserved.

#include "Development/BwayHeroDebugComponent.h"
#include "BwayPlayerState.h"
#include "BwayCharacterWithAbilities.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogBwayHeroDebug, Log, All);

// Static member initialization
TArray<FString> UBwayHeroDebugComponent::AbilityActivationLog;
TWeakObjectPtr<UBwayHeroDebugComponent> UBwayHeroDebugComponent::LocalPlayerInstance;

UBwayHeroDebugComponent::UBwayHeroDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bShowDebugInfo = false;
}

void UBwayHeroDebugComponent::BeginPlay()
{
	Super::BeginPlay();

	// Register as local player instance if this is the local player's character
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (Pawn->IsLocallyControlled())
		{
			LocalPlayerInstance = this;
			UE_LOG(LogBwayHeroDebug, Log, TEXT("BwayHeroDebugComponent registered for local player"));
		}
	}
}

void UBwayHeroDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bShowDebugInfo)
	{
		DrawDebugInfo();
	}
}

void UBwayHeroDebugComponent::ToggleDebugDisplay()
{
	bShowDebugInfo = !bShowDebugInfo;
	UE_LOG(LogBwayHeroDebug, Log, TEXT("Hero Debug Display: %s"), bShowDebugInfo ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void UBwayHeroDebugComponent::SetDebugDisplayEnabled(bool bEnabled)
{
	bShowDebugInfo = bEnabled;
	UE_LOG(LogBwayHeroDebug, Log, TEXT("Hero Debug Display: %s"), bShowDebugInfo ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void UBwayHeroDebugComponent::LogAbilityActivation(const FString& AbilityName, bool bSuccess, const FString& FailReason)
{
	FString LogEntry = FString::Printf(TEXT("[%s] %s: %s%s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")),
		*AbilityName,
		bSuccess ? TEXT("ACTIVATED") : TEXT("FAILED"),
		FailReason.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *FailReason));

	AbilityActivationLog.Insert(LogEntry, 0);

	// Keep log size bounded
	while (AbilityActivationLog.Num() > MaxLogEntries)
	{
		AbilityActivationLog.RemoveAt(AbilityActivationLog.Num() - 1);
	}

	// Also log to output log
	if (bSuccess)
	{
		UE_LOG(LogBwayHeroDebug, Log, TEXT("Ability %s ACTIVATED"), *AbilityName);
	}
	else
	{
		UE_LOG(LogBwayHeroDebug, Warning, TEXT("Ability %s FAILED: %s"), *AbilityName, *FailReason);
	}
}

UBwayHeroDebugComponent* UBwayHeroDebugComponent::GetLocalPlayerDebugComponent()
{
	return LocalPlayerInstance.Get();
}

void UBwayHeroDebugComponent::DrawDebugInfo()
{
	if (!GEngine)
	{
		return;
	}

	// Get hero info
	FString HeroName = GetCurrentHeroName();
	int32 AbilityCount = 0;
	TArray<FString> AbilityNames;
	GetGrantedAbilitiesInfo(AbilityCount, AbilityNames);

	// Build debug string
	FString DebugText = TEXT("=== HERO DEBUG ===\n");
	DebugText += FString::Printf(TEXT("Hero: %s\n"), *HeroName);
	DebugText += FString::Printf(TEXT("Granted Abilities: %d\n"), AbilityCount);

	// List abilities
	if (AbilityNames.Num() > 0)
	{
		DebugText += TEXT("--- Abilities ---\n");
		for (const FString& Name : AbilityNames)
		{
			DebugText += FString::Printf(TEXT("  - %s\n"), *Name);
		}
	}

	// Show activation log
	if (AbilityActivationLog.Num() > 0)
	{
		DebugText += TEXT("--- Recent Activations ---\n");
		for (int32 i = 0; i < FMath::Min(5, AbilityActivationLog.Num()); ++i)
		{
			DebugText += FString::Printf(TEXT("  %s\n"), *AbilityActivationLog[i]);
		}
	}

	// Display on screen
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, DebugText);
}

FString UBwayHeroDebugComponent::GetCurrentHeroName() const
{
	ABwayPlayerState* PS = GetBwayPlayerState();
	if (!PS)
	{
		return TEXT("No PlayerState");
	}

	FPrimaryAssetId HeroId = PS->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		return TEXT("No Hero Selected");
	}

	// Try to get the display name from the hero data asset
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
	if (HeroData)
	{
		return HeroData->DisplayName.ToString();
	}

	return HeroId.ToString();
}

void UBwayHeroDebugComponent::GetGrantedAbilitiesInfo(int32& OutCount, TArray<FString>& OutAbilityNames) const
{
	OutCount = 0;
	OutAbilityNames.Empty();

	ULyraAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();
	OutCount = ActivatableAbilities.Num();

	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities)
	{
		if (Spec.Ability)
		{
			FString AbilityName = Spec.Ability->GetClass()->GetName();
			
			// Get input tag if available
			FGameplayTagContainer DynamicTags = Spec.GetDynamicSpecSourceTags();
			if (!DynamicTags.IsEmpty())
			{
				AbilityName += TEXT(" [");
				bool bFirst = true;
				for (const FGameplayTag& Tag : DynamicTags)
				{
					if (!bFirst) AbilityName += TEXT(", ");
					AbilityName += Tag.ToString();
					bFirst = false;
				}
				AbilityName += TEXT("]");
			}

			OutAbilityNames.Add(AbilityName);
		}
	}
}

ULyraAbilitySystemComponent* UBwayHeroDebugComponent::GetAbilitySystemComponent() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// First try to get ASC from pawn's player state (Lyra pattern)
	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		if (APlayerState* PS = Pawn->GetPlayerState())
		{
			return Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS));
		}
	}

	// Fallback to owner actor
	return Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner));
}

ABwayPlayerState* UBwayHeroDebugComponent::GetBwayPlayerState() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		return Cast<ABwayPlayerState>(Pawn->GetPlayerState());
	}

	return nullptr;
}

