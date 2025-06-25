// BwayHeroPluginLoader.cpp
#include "BwayHeroPluginLoader.h"
#include "Interfaces/IPluginManager.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturePluginOperationResult.h"

using namespace UE::GameFeatures;                      // for UE::GameFeatures::FResult

/*---------------------------------------------------------------------------
 *  Static factory   (called from Blueprints)
 *--------------------------------------------------------------------------*/
UBwayHeroPluginLoader* UBwayHeroPluginLoader::LoadAllHeroPlugins(UObject* WorldContextObject)
{
    UBwayHeroPluginLoader* Node = NewObject<UBwayHeroPluginLoader>();
    Node->SetFlags(RF_StrongRefOnFrame);               // keep alive until completion
    return Node;
}

/*---------------------------------------------------------------------------
 *  UBlueprintAsyncActionBase override
 *--------------------------------------------------------------------------*/
void UBwayHeroPluginLoader::Activate()
{
    // 1) find every plugin whose content root lives under /GameFeatures/Heroes/
    for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins()) 
    {
        const EPluginType PluginType = Plugin->GetType();
        if (PluginType != EPluginType::Project)
        {
            continue;   // skip non-game-feature plugins
        }

        const FString Descriptor = Plugin->GetDescriptorFileName();
        if ( !Descriptor.Contains("Hero_") )
        {
            continue;
        }

        FString URL = UGameFeaturesSubsystem::GetPluginURL_FileProtocol(
                          Plugin->GetDescriptorFileName()); 
        PendingURLs.Add(URL);
    }

    // 2) Kick-off the chain (will recurse through PendingURLs)
    StartNext();
}

/*---------------------------------------------------------------------------
 *  PRIVATE HELPERS
 *--------------------------------------------------------------------------*/
void UBwayHeroPluginLoader::StartNext()
{
    // Remove already-active plugins to avoid redundant state changes
    while (PendingURLs.Num() > 0)
    {
        const FString URL = PendingURLs.Pop(EAllowShrinking::No);
        if (UGameFeaturesSubsystem::Get().IsGameFeaturePluginActive(URL))
        {
            continue;                    // already active
        }

        // minimal memory: mount content only, no separate modules
        UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
            URL,
            FGameFeaturePluginLoadComplete::CreateUObject(
                this, &UBwayHeroPluginLoader::HandleLoadComplete));
        return;  
    }

    /* ---------------- ALL PLUGINS DONE ---------------- */
    OnCompleted.Broadcast();     // Blueprint event
    SetReadyToDestroy();
}

void UBwayHeroPluginLoader::HandleLoadComplete(const FResult& Result)
{
    if (Result.HasError())
    {
        // Log the error and continue with the next URL
        UE_LOG(LogTemp, Error, TEXT("Failed to load hero plugin: %s"), *Result.GetError());
    }

    // Continue with next pending URL
    StartNext();
}
