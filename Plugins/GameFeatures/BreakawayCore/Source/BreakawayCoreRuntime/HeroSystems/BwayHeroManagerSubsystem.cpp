// Fill out your copyright notice in the Description page of Project Settings.


// BwayHeroManagerSubsystem.cpp
#include "BwayHeroManagerSubsystem.h"
#include "BwayHeroRegistry.h"
#include "BwayGameFeatureData.h" // Your custom GameFeatureData
#include "BwayHeroDataAsset.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureData.h"
#include "GameFeatureTypes.h" // For FGameFeatureStateChangeContext
#include "Engine/AssetManager.h"

void UBwayHeroManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Collection.InitializeDependency<UGameFeaturesSubsystem>();
    UGameFeaturesSubsystem& GFS = UGameFeaturesSubsystem::Get();

    // Register this subsystem as an observer
    GFS.AddObserver(this); // <--- Key change: Registering the observer

    UE_LOG(LogTemp, Log, TEXT("UBwayHeroManagerSubsystem Initialized and registered as Game Feature observer."));
}

void UBwayHeroManagerSubsystem::Deinitialize()
{
    UGameFeaturesSubsystem::Get().RemoveObserver(this); // Unregister the observer

    // Unregister all heroes if the subsystem is shutting down
    UBwayHeroRegistry* HeroRegistry = UBwayHeroRegistry::Get(this); // Assuming Get() can take a UObject for context
    if (HeroRegistry)
    {
        for (auto const& Pair : RegisteredHeroesByPlugin)
        {
            for (const UBwayHeroDataAsset* HeroData : Pair.Value)
            {
                if (HeroData) // Ensure hero data is still valid
                {
                    HeroRegistry->UnregisterHero(HeroData);
                }
            }
        }
    }
    RegisteredHeroesByPlugin.Empty();

    UE_LOG(LogTemp, Log, TEXT("UBwayHeroManagerSubsystem Deinitialized and unregistered as Game Feature observer."));
    Super::Deinitialize();
}

void UBwayHeroManagerSubsystem::OnGameFeatureActivated(const UGameFeatureData* GameFeatureData, const FString& PluginName)
{
    UE_LOG(LogTemp, Log, TEXT("UBwayHeroManagerSubsystem: Game Feature '%s' is now ACTIVE. GFD: %s"),
        *PluginName, GameFeatureData ? *GameFeatureData->GetName() : TEXT("None"));

    if (GameFeatureData)
    {
        ProcessGameFeatureDataForRegistration(GameFeatureData, PluginName);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UBwayHeroManagerSubsystem: Game Feature '%s' activated without GameFeatureData."), *PluginName);
    }
}

void UBwayHeroManagerSubsystem::OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, FGameFeatureDeactivatingContext& Context, const FString& PluginURL)
{
    UE_LOG(LogTemp, Log, TEXT("UBwayHeroManagerSubsystem: Game Feature '%s' is DEACTIVATING. GFD: %s"),
        *PluginURL, GameFeatureData ? *GameFeatureData->GetName() : TEXT("None"));

    // GameFeatureData might be null here if the plugin is being forcefully removed or had issues.
    // We primarily use PluginName to look up what to unregister.
    ProcessGameFeatureDataForUnregistration(GameFeatureData, PluginURL);
}

void UBwayHeroManagerSubsystem::ProcessGameFeatureDataForRegistration(const UGameFeatureData* GFD, const FString& PluginName)
{
    // This function remains largely the same as in the previous correct observer-based answer
    if (!GFD) return;

    if (const UBwayGameFeatureData* HeroFeatureData = Cast<UBwayGameFeatureData>(GFD))
    {
        UBwayHeroRegistry* HeroRegistry = UBwayHeroRegistry::Get(this);
        if (!HeroRegistry)
        {
            UE_LOG(LogTemp, Error, TEXT("UBwayHeroManagerSubsystem: UBwayHeroRegistry not found for registration from plugin '%s'!"), *PluginName);
            return;
        }

        // If this plugin was somehow already processed and not cleaned up, clear old entries.
        if (RegisteredHeroesByPlugin.Contains(PluginName))
        {
            UE_LOG(LogTemp, Warning, TEXT("UBwayHeroManagerSubsystem: Plugin '%s' is being re-registered. Clearing existing registered heroes from this plugin first."), *PluginName);
            // Call unregistration logic for this plugin before re-registering to prevent duplicates
            // This assumes ProcessGameFeatureDataForUnregistration can handle being called even if GFD is the same instance
            // and relies on PluginName primarily for cleanup.
            TArray<TObjectPtr<const UBwayHeroDataAsset>>* ExistingHeroes = RegisteredHeroesByPlugin.Find(PluginName);
            if (ExistingHeroes)
            {
                for (const UBwayHeroDataAsset* HeroData : *ExistingHeroes)
                {
                    if (HeroData) HeroRegistry->UnregisterHero(HeroData);
                }
                RegisteredHeroesByPlugin.Remove(PluginName); // Remove the old entry before adding a new one
            }
        }

        TArray<TObjectPtr<const UBwayHeroDataAsset>> NewlyRegisteredHeroes;
        for (const TSoftObjectPtr<UBwayHeroDataAsset>& HeroAssetPtr : HeroFeatureData->HeroDataAssets)
        {
            UBwayHeroDataAsset* LoadedHeroData = HeroAssetPtr.LoadSynchronous();
            if (LoadedHeroData)
            {
                HeroRegistry->RegisterHero(LoadedHeroData);
                NewlyRegisteredHeroes.Add(LoadedHeroData);
                UE_LOG(LogTemp, Log, TEXT("UBwayHeroManagerSubsystem: Registered Hero '%s' from GFP '%s'"), *LoadedHeroData->GetName(), *PluginName);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("UBwayHeroManagerSubsystem: Failed to load HeroDataAsset '%s' from GFP '%s'."), *HeroAssetPtr.ToString(), *PluginName);
            }
        }

        if (!NewlyRegisteredHeroes.IsEmpty())
        {
            RegisteredHeroesByPlugin.Add(PluginName, NewlyRegisteredHeroes);
        }
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("UBwayHeroManagerSubsystem: Game Feature Data '%s' for GFP '%s' is not a UBwayGameFeatureData_WithHeroes. Skipping hero registration."), *GFD->GetName(), *PluginName);
    }
}

void UBwayHeroManagerSubsystem::ProcessGameFeatureDataForUnregistration(const UGameFeatureData* GFD, const FString& PluginName)
{
    // This function also remains largely the same, relying on PluginName.
    // GFD is passed for logging or potential future use but isn't strictly needed if PluginName is the key.
    UBwayHeroRegistry* HeroRegistry = UBwayHeroRegistry::Get(this);
    if (!HeroRegistry)
    {
        UE_LOG(LogTemp, Error, TEXT("UBwayHeroManagerSubsystem: UBwayHeroRegistry not found for unregistration from plugin '%s'!"), *PluginName);
        return;
    }

    if (TArray<TObjectPtr<const UBwayHeroDataAsset>>* HeroesFromThisPlugin = RegisteredHeroesByPlugin.Find(PluginName))
    {
        UE_LOG(LogTemp, Log, TEXT("UBwayHeroManagerSubsystem: Unregistering %d heroes from plugin '%s'."), HeroesFromThisPlugin->Num(), *PluginName);
        for (const UBwayHeroDataAsset* HeroData : *HeroesFromThisPlugin)
        {
            if (HeroData) // Check if it's still valid
            {
                HeroRegistry->UnregisterHero(HeroData);
                UE_LOG(LogTemp, Log, TEXT("UBwayHeroManagerSubsystem: Unregistered Hero '%s' from GFP '%s'"), *HeroData->GetName(), *PluginName);
            }
        }
        RegisteredHeroesByPlugin.Remove(PluginName);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UBwayHeroManagerSubsystem: Attempting to unregister heroes for plugin '%s', but no registration record found."), *PluginName);
    }
}