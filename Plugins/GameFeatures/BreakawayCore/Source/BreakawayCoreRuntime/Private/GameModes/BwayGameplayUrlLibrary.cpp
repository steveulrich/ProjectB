// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/BwayGameplayUrlLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayUrlLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogBwayGameplayUrl, Log, All);

namespace BwayGameplayUrl
{
struct FNamedOptionSource
{
	FString Label;
	FString Value;
};

const TArray<FName>& GetKnownGameplayOptionKeys()
{
	static const TArray<FName> Keys = {
		TEXT("NumBots"),
		TEXT("PointsToWin"),
		TEXT("SkipHeroSelection"),
		TEXT("Experience"),
		TEXT("MatchFlowConfig"),
		TEXT("HeroSelectStaging"),
		TEXT("HeroSelectTargetMap"),
		TEXT("HeroSelectTargetExperience"),
	};
	return Keys;
}

void AddSourceIfNonEmpty(TArray<FNamedOptionSource>& OutSources, const FString& Label, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		OutSources.Add({Label, Value});
	}
}

void GatherNamedOptionSources(const UObject* WorldContextObject, TArray<FNamedOptionSource>& OutSources)
{
	OutSources.Reset();

	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	if (const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr)
	{
		AddSourceIfNonEmpty(OutSources, TEXT("GameMode.OptionsString"), GameMode->OptionsString);
	}

	if (World)
	{
		AddSourceIfNonEmpty(OutSources, TEXT("World.URL"), World->URL.ToString());

		if (GEngine)
		{
			if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World))
			{
				AddSourceIfNonEmpty(OutSources, TEXT("WorldContext.LastURL"), WorldContext->LastURL.ToString());
			}

#if WITH_EDITOR
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				if (WorldContext.WorldType != EWorldType::PIE || !WorldContext.World())
				{
					continue;
				}

				const FString PieLastUrl = WorldContext.LastURL.ToString();
				if (PieLastUrl.IsEmpty())
				{
					continue;
				}

				const FString Label = FString::Printf(
					TEXT("PIE.LastURL[%s]"),
					WorldContext.World() == World ? TEXT("Current") : *WorldContext.World()->GetName());
				AddSourceIfNonEmpty(OutSources, Label, PieLastUrl);
			}
#endif
		}
	}

#if WITH_EDITOR
	if (const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>())
	{
		AddSourceIfNonEmpty(OutSources, TEXT("Editor.AdditionalLaunchParameters"), PlaySettings->AdditionalLaunchParameters);

		FString AdditionalServerGameOptions;
		if (PlaySettings->GetAdditionalServerGameOptions(AdditionalServerGameOptions))
		{
			AddSourceIfNonEmpty(OutSources, TEXT("Editor.AdditionalServerGameOptions"), AdditionalServerGameOptions);
		}
	}
#endif

	AddSourceIfNonEmpty(OutSources, TEXT("CommandLine"), FCommandLine::Get());
}

void LogOptionSourcesDiagnostic(const UObject* WorldContextObject, const TCHAR* Context)
{
	TArray<FNamedOptionSource> Sources;
	GatherNamedOptionSources(WorldContextObject, Sources);

	UE_LOG(LogBwayGameplayUrl, Log, TEXT("=== BwayGameplayUrl diagnostic (%s) — %d source(s) ==="), Context, Sources.Num());
	for (const FNamedOptionSource& Source : Sources)
	{
		UE_LOG(LogBwayGameplayUrl, Log, TEXT("  [%s] %s"), *Source.Label, *Source.Value);
	}

	for (const FName& OptionName : GetKnownGameplayOptionKeys())
	{
		const FString Key = OptionName.ToString();
		bool bFound = false;
		for (const FNamedOptionSource& Source : Sources)
		{
			if (UGameplayStatics::HasOption(Source.Value, Key))
			{
				const FString Value = UGameplayStatics::ParseOption(Source.Value, Key);
				UE_LOG(LogBwayGameplayUrl, Log, TEXT("  -> %s=%s (from %s)"), *Key, *Value, *Source.Label);
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			UE_LOG(LogBwayGameplayUrl, Warning, TEXT("  -> %s NOT FOUND in any source"), *Key);
		}
	}
}

bool TryGetOptionFromSources(
	const TArray<FNamedOptionSource>& Sources,
	FName OptionName,
	int32& OutValue,
	FString* OutSourceLabel)
{
	const FString Key = OptionName.ToString();
	for (const FNamedOptionSource& Source : Sources)
	{
		if (UGameplayStatics::HasOption(Source.Value, Key))
		{
			OutValue = UGameplayStatics::GetIntOption(Source.Value, Key, 0);
			if (OutSourceLabel)
			{
				*OutSourceLabel = Source.Label;
			}
			return true;
		}
	}

	return false;
}

bool TryGetStringOptionFromSources(
	const TArray<FNamedOptionSource>& Sources,
	FName OptionName,
	FString& OutValue,
	FString* OutSourceLabel)
{
	const FString Key = OptionName.ToString();
	for (const FNamedOptionSource& Source : Sources)
	{
		if (UGameplayStatics::HasOption(Source.Value, Key))
		{
			OutValue = UGameplayStatics::ParseOption(Source.Value, Key);
			if (!OutValue.IsEmpty())
			{
				if (OutSourceLabel)
				{
					*OutSourceLabel = Source.Label;
				}
				return true;
			}
		}
	}

	return false;
}

void GatherOptionSources(const UObject* WorldContextObject, TArray<FString>& OutSources)
{
	TArray<FNamedOptionSource> NamedSources;
	GatherNamedOptionSources(WorldContextObject, NamedSources);
	OutSources.Reset();
	for (const FNamedOptionSource& Source : NamedSources)
	{
		OutSources.Add(Source.Value);
	}
}
}

void UBwayGameplayUrlLibrary::LogGameplayUrlDiagnostics(const UObject* WorldContextObject, const FString& Context)
{
	BwayGameplayUrl::LogOptionSourcesDiagnostic(WorldContextObject, Context.IsEmpty() ? TEXT("manual") : *Context);
}

void UBwayGameplayUrlLibrary::AppendMissingOptionsFromUrlSource(FString& OptionsString, const FString& UrlSource)
{
	if (UrlSource.IsEmpty())
	{
		return;
	}

	for (const FName& OptionName : BwayGameplayUrl::GetKnownGameplayOptionKeys())
	{
		const FString Key = OptionName.ToString();
		if (UGameplayStatics::HasOption(OptionsString, Key))
		{
			continue;
		}

		if (UGameplayStatics::HasOption(UrlSource, Key))
		{
			const FString Value = UGameplayStatics::ParseOption(UrlSource, Key);
			if (!Value.IsEmpty())
			{
				OptionsString += OptionsString.IsEmpty() ? TEXT("?") : TEXT("&");
				OptionsString += FString::Printf(TEXT("%s=%s"), *Key, *Value);
				UE_LOG(LogBwayGameplayUrl, Log, TEXT("Augment: appended %s=%s from source fragment"), *Key, *Value);
			}
		}
	}
}

void UBwayGameplayUrlLibrary::AugmentGameModeOptionsString(AGameModeBase* GameMode)
{
	if (!GameMode)
	{
		return;
	}

	const FString OptionsBefore = GameMode->OptionsString;

	TArray<BwayGameplayUrl::FNamedOptionSource> Sources;
	BwayGameplayUrl::GatherNamedOptionSources(GameMode, Sources);

	for (const BwayGameplayUrl::FNamedOptionSource& Source : Sources)
	{
		if (Source.Label != TEXT("GameMode.OptionsString"))
		{
			AppendMissingOptionsFromUrlSource(GameMode->OptionsString, Source.Value);
		}
	}

	UE_LOG(LogBwayGameplayUrl, Log,
		TEXT("AugmentGameModeOptionsString: before='%s' after='%s'"),
		*OptionsBefore,
		*GameMode->OptionsString);
}

bool UBwayGameplayUrlLibrary::HasGameplayUrlOption(const UObject* WorldContextObject, FName OptionName)
{
	TArray<FString> Sources;
	BwayGameplayUrl::GatherOptionSources(WorldContextObject, Sources);

	const FString Key = OptionName.ToString();
	for (const FString& Source : Sources)
	{
		if (UGameplayStatics::HasOption(Source, Key))
		{
			return true;
		}
	}

	return false;
}

bool UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionInt(const UObject* WorldContextObject, FName OptionName, int32& OutValue)
{
	TArray<BwayGameplayUrl::FNamedOptionSource> Sources;
	BwayGameplayUrl::GatherNamedOptionSources(WorldContextObject, Sources);
	return BwayGameplayUrl::TryGetOptionFromSources(Sources, OptionName, OutValue, nullptr);
}

bool UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionString(const UObject* WorldContextObject, FName OptionName, FString& OutValue)
{
	TArray<BwayGameplayUrl::FNamedOptionSource> Sources;
	BwayGameplayUrl::GatherNamedOptionSources(WorldContextObject, Sources);
	return BwayGameplayUrl::TryGetStringOptionFromSources(Sources, OptionName, OutValue, nullptr);
}

bool UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionIntWithSource(
	const UObject* WorldContextObject,
	FName OptionName,
	int32& OutValue,
	FString& OutSourceLabel)
{
	TArray<BwayGameplayUrl::FNamedOptionSource> Sources;
	BwayGameplayUrl::GatherNamedOptionSources(WorldContextObject, Sources);
	return BwayGameplayUrl::TryGetOptionFromSources(Sources, OptionName, OutValue, &OutSourceLabel);
}
