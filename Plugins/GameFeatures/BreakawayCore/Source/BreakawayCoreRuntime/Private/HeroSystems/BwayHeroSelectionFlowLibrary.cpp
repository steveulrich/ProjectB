#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

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
	return GameMode && UGameplayStatics::HasOption(GameMode->OptionsString, OptionName.ToString());
}

bool UBwayHeroSelectionFlowLibrary::HasUrlOption(const UObject* WorldContextObject, FName OptionName)
{
	return HasUrlOptionForGameMode(GetAuthGameMode(WorldContextObject), OptionName);
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

bool UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayWithoutHeroSelectFlowForGameMode(const AGameModeBase* GameMode)
{
#if WITH_EDITOR
	if (!GIsEditor || !GameMode)
	{
		return false;
	}

	return !IsHeroSelectStagingGameMode(GameMode) && !ShouldSkipHeroSelectionGameMode(GameMode);
#else
	return false;
#endif
}

bool UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayWithoutHeroSelectFlow(const UObject* WorldContextObject)
{
	return IsDirectEditorPlayWithoutHeroSelectFlowForGameMode(GetAuthGameMode(WorldContextObject));
}
