#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"

#include "GameModes/BwayGameplayUrlLibrary.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

const AGameModeBase* UBwayHeroSelectionFlowLibrary::GetAuthGameMode(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetAuthGameMode();
	}

	return nullptr;
}

bool UBwayHeroSelectionFlowLibrary::HasUrlOptionForGameMode(const AGameModeBase* GameMode, FName OptionName)
{
	return GameMode && UBwayGameplayUrlLibrary::HasGameplayUrlOption(GameMode, OptionName);
}

bool UBwayHeroSelectionFlowLibrary::HasUrlOption(const UObject* WorldContextObject, FName OptionName)
{
	return UBwayGameplayUrlLibrary::HasGameplayUrlOption(WorldContextObject, OptionName);
}

bool UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingGameMode(const AGameModeBase* GameMode)
{
	return HasUrlOptionForGameMode(GameMode, TEXT("HeroSelectStaging"));
}

bool UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(const UObject* WorldContextObject)
{
	return IsHeroSelectStagingGameMode(GetAuthGameMode(WorldContextObject));
}

bool UBwayHeroSelectionFlowLibrary::ShouldSkipHeroSelectionGameMode(const AGameModeBase* GameMode)
{
	return HasUrlOptionForGameMode(GameMode, TEXT("SkipHeroSelection"));
}

bool UBwayHeroSelectionFlowLibrary::ShouldSkipHeroSelectionWorld(const UObject* WorldContextObject)
{
	return ShouldSkipHeroSelectionGameMode(GetAuthGameMode(WorldContextObject));
}

bool UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidGameMode(const AGameModeBase* GameMode)
{
	return HasUrlOptionForGameMode(GameMode, TEXT("ForceHumanoid"));
}

bool UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidWorld(const UObject* WorldContextObject)
{
	return ShouldForceHumanoidGameMode(GetAuthGameMode(WorldContextObject));
}

bool UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayMatchMapForGameMode(const AGameModeBase* GameMode)
{
#if WITH_EDITOR
	if (!GIsEditor || !GameMode)
	{
		return false;
	}

	return !IsHeroSelectStagingGameMode(GameMode);
#else
	return false;
#endif
}

bool UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayMatchMap(const UObject* WorldContextObject)
{
	return IsDirectEditorPlayMatchMapForGameMode(GetAuthGameMode(WorldContextObject));
}

bool UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayWithoutHeroSelectFlowForGameMode(const AGameModeBase* GameMode)
{
#if WITH_EDITOR
	if (!GIsEditor || !GameMode)
	{
		return false;
	}

	return IsDirectEditorPlayMatchMapForGameMode(GameMode)
		&& !ShouldSkipHeroSelectionGameMode(GameMode)
		&& !HasUrlOptionForGameMode(GameMode, TEXT("Hero"));
#else
	return false;
#endif
}

bool UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayWithoutHeroSelectFlow(const UObject* WorldContextObject)
{
	return IsDirectEditorPlayWithoutHeroSelectFlowForGameMode(GetAuthGameMode(WorldContextObject));
}

FPrimaryAssetId UBwayHeroSelectionFlowLibrary::ResolveDirectPieBotHeroIdForGameMode(const AGameModeBase* GameMode)
{
#if WITH_EDITOR
	if (!IsDirectEditorPlayMatchMapForGameMode(GameMode) || ShouldForceHumanoidGameMode(GameMode))
	{
		return FPrimaryAssetId();
	}

	FString HeroName;
	if (UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionString(GameMode, TEXT("Hero"), HeroName))
	{
		const FPrimaryAssetId UrlHeroId = UBwayHeroRegistry::ResolveHeroIdByName(HeroName);
		if (UrlHeroId.IsValid())
		{
			return UrlHeroId;
		}
	}

	return UBwayHeroRegistry::ResolveHeroIdByName(TEXT("Argus"));
#else
	return FPrimaryAssetId();
#endif
}

FPrimaryAssetId UBwayHeroSelectionFlowLibrary::ResolveDirectPieBotHeroId(const UObject* WorldContextObject)
{
	return ResolveDirectPieBotHeroIdForGameMode(GetAuthGameMode(WorldContextObject));
}
