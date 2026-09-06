// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/BwayGameplayUrlLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UnrealEdGlobals.h"
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
		TEXT("ForceHumanoid"),
		TEXT("Hero"),
		TEXT("Experience"),
		TEXT("MatchFlowConfig"),
		TEXT("PrematchDuration"),
		TEXT("WarmupDuration"),
		TEXT("PostRoundDuration"),
		TEXT("PostMatchSummaryDuration"),
		TEXT("RoundDuration"),
		TEXT("DisableRelicBotAI"),
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

/** Extract ?Key=Value&... from map?options, bare options, or ?options fragments. */
FString ExtractOptionsQuery(const FString& Value)
{
	if (Value.IsEmpty())
	{
		return FString();
	}

	const FString Trimmed = Value.TrimStartAndEnd();

	const int32 QueryStart = Trimmed.Find(TEXT("?"));
	if (QueryStart != INDEX_NONE)
	{
		return Trimmed.Mid(QueryStart);
	}

	if (Trimmed.Contains(TEXT("=")))
	{
		return FString::Printf(TEXT("?%s"), *Trimmed);
	}

	return Trimmed;
}

FString NormalizeOptionsFragment(const FString& Value)
{
	return ExtractOptionsQuery(Value);
}

/** Parse key=value pairs from a URL fragment, ?options, or bare options. */
void ParseOptionsStringToMap(const FString& OptionsFragment, TMap<FString, FString>& OutMap)
{
	if (OptionsFragment.IsEmpty())
	{
		return;
	}

	FString OptionsPart = OptionsFragment;

	const int32 QueryStart = OptionsFragment.Find(TEXT("?"));
	if (QueryStart != INDEX_NONE)
	{
		OptionsPart = OptionsFragment.Mid(QueryStart + 1);
	}
	else if (!OptionsFragment.Contains(TEXT("=")))
	{
		return;
	}

	// PIE URLs sometimes use multiple ? separators (e.g. ?PktEmulationProfile=Average?Listen).
	OptionsPart.ReplaceInline(TEXT("?"), TEXT("&"));

	TArray<FString> Tokens;
	OptionsPart.ParseIntoArray(Tokens, TEXT("&"), true);

	for (const FString& Token : Tokens)
	{
		if (Token.IsEmpty())
		{
			continue;
		}

		FString Key;
		FString Value;
		if (Token.Split(TEXT("="), &Key, &Value))
		{
			OutMap.Add(Key, Value);
		}
		else
		{
			OutMap.Add(Token, TEXT("1"));
		}
	}
}

FString BuildOptionsStringFromMap(const TMap<FString, FString>& OptionsMap)
{
	if (OptionsMap.Num() == 0)
	{
		return FString();
	}

	FString Result;
	for (const TPair<FString, FString>& Pair : OptionsMap)
	{
		// Engine travel and UGameplayStatics option readers require '?' per option.
		// Accept '&' on input for convenience, but never write it into OptionsString.
		Result += TEXT("?");

		Result += FString::Printf(TEXT("%s=%s"), *Pair.Key, *Pair.Value);
	}

	return Result;
}

bool TryFindOptionInSource(const FNamedOptionSource& Source, const FString& Key, FString& OutValue)
{
	TMap<FString, FString> SourceMap;
	ParseOptionsStringToMap(Source.Value, SourceMap);
	if (const FString* Found = SourceMap.Find(Key))
	{
		OutValue = *Found;
		return !OutValue.IsEmpty();
	}

	return false;
}

void GatherNamedOptionSources(const UObject* WorldContextObject, TArray<FNamedOptionSource>& OutSources)
{
	OutSources.Reset();

	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

#if WITH_EDITOR
	// Highest priority: toolbar / play URL the user typed in the editor.
	if (GEditor && !GEditor->UserEditedPlayWorldURL.IsEmpty())
	{
		const FString Normalized = NormalizeOptionsFragment(GEditor->UserEditedPlayWorldURL);
		AddSourceIfNonEmpty(OutSources, TEXT("Editor.UserEditedPlayWorldURL"), Normalized);
	}

	if (const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>())
	{
		FString AdditionalServerGameOptions;
		if (PlaySettings->GetAdditionalServerGameOptions(AdditionalServerGameOptions))
		{
			const FString Normalized = NormalizeOptionsFragment(AdditionalServerGameOptions);
			if (!AdditionalServerGameOptions.Equals(Normalized, ESearchCase::CaseSensitive))
			{
				UE_LOG(LogBwayGameplayUrl, Log,
					TEXT("Normalized Editor.AdditionalServerGameOptions: raw='%s' -> '%s' (use ?Options only; map belongs in Server Map Name Override)"),
					*AdditionalServerGameOptions, *Normalized);
			}
			AddSourceIfNonEmpty(OutSources, TEXT("Editor.AdditionalServerGameOptions"), Normalized);
		}

		AddSourceIfNonEmpty(OutSources, TEXT("Editor.AdditionalLaunchParameters"), NormalizeOptionsFragment(PlaySettings->AdditionalLaunchParameters));
	}

#endif

	if (World)
	{
		AddSourceIfNonEmpty(OutSources, TEXT("World.URL"), World->URL.ToString());
		if (GEngine)
		{
			if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World))
			{
				// LastURL can still describe the source map during seamless travel.
				if (WorldContext->LastURL.Map == World->URL.Map)
				{
					AddSourceIfNonEmpty(OutSources, TEXT("WorldContext.LastURL"), WorldContext->LastURL.ToString());
				}
			}
		}
	}

	AddSourceIfNonEmpty(OutSources, TEXT("CommandLine"), FCommandLine::Get());

	// Lowest priority: whatever InitGame already captured (often empty/partial on listen-server authority).
	if (const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr)
	{
		AddSourceIfNonEmpty(OutSources, TEXT("GameMode.OptionsString"), GameMode->OptionsString);
	}
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
			FString Value;
			if (TryFindOptionInSource(Source, Key, Value))
			{
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

void ApplyPriorityGameplayOptions(FString& OptionsString, const TArray<FNamedOptionSource>& Sources)
{
	TMap<FString, FString> MergedOptions;
	ParseOptionsStringToMap(OptionsString, MergedOptions);

	TMap<FString, FString> GameplayOverrides;

	for (const FNamedOptionSource& Source : Sources)
	{
		if (Source.Label == TEXT("GameMode.OptionsString"))
		{
			continue;
		}

		TMap<FString, FString> SourceMap;
		ParseOptionsStringToMap(Source.Value, SourceMap);

		for (const FName& OptionName : GetKnownGameplayOptionKeys())
		{
			const FString Key = OptionName.ToString();
			if (GameplayOverrides.Contains(Key))
			{
				continue;
			}

			if (const FString* FoundValue = SourceMap.Find(Key))
			{
				const FString ResolvedValue = *FoundValue;
				GameplayOverrides.Add(Key, ResolvedValue);
				UE_LOG(LogBwayGameplayUrl, Log, TEXT("Augment: applied %s=%s from %s"), *Key, *ResolvedValue, *Source.Label);
			}
		}
	}

	for (const TPair<FString, FString>& Pair : GameplayOverrides)
	{
		MergedOptions.Add(Pair.Key, Pair.Value);
	}

	OptionsString = BuildOptionsStringFromMap(MergedOptions);
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
		FString Value;
		if (TryFindOptionInSource(Source, Key, Value))
		{
			OutValue = FCString::Atoi(*Value);
			if (OutSourceLabel)
			{
				*OutSourceLabel = Source.Label;
			}
			return true;
		}
	}

	return false;
}

bool TryGetFloatOptionFromSources(
	const TArray<FNamedOptionSource>& Sources,
	FName OptionName,
	float& OutValue,
	FString* OutSourceLabel)
{
	const FString Key = OptionName.ToString();
	for (const FNamedOptionSource& Source : Sources)
	{
		FString Value;
		if (TryFindOptionInSource(Source, Key, Value) && LexTryParseString(OutValue, *Value))
		{
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
		FString Value;
		if (TryFindOptionInSource(Source, Key, Value))
		{
			OutValue = Value;
			if (OutSourceLabel)
			{
				*OutSourceLabel = Source.Label;
			}
			return true;
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

	TMap<FString, FString> MergedOptions;
	BwayGameplayUrl::ParseOptionsStringToMap(OptionsString, MergedOptions);

	TMap<FString, FString> SourceMap;
	BwayGameplayUrl::ParseOptionsStringToMap(UrlSource, SourceMap);

	for (const FName& OptionName : BwayGameplayUrl::GetKnownGameplayOptionKeys())
	{
		const FString Key = OptionName.ToString();
		if (MergedOptions.Contains(Key))
		{
			continue;
		}

		if (const FString* FoundValue = SourceMap.Find(Key))
		{
			const FString ResolvedValue = *FoundValue;
			MergedOptions.Add(Key, ResolvedValue);
			UE_LOG(LogBwayGameplayUrl, Log, TEXT("Augment: appended %s=%s from source fragment"), *Key, *ResolvedValue);
		}
	}

	OptionsString = BwayGameplayUrl::BuildOptionsStringFromMap(MergedOptions);
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
	BwayGameplayUrl::ApplyPriorityGameplayOptions(GameMode->OptionsString, Sources);

	UE_LOG(LogBwayGameplayUrl, Log,
		TEXT("AugmentGameModeOptionsString: before='%s' after='%s'"),
		*OptionsBefore,
		*GameMode->OptionsString);
}

bool UBwayGameplayUrlLibrary::HasGameplayUrlOption(const UObject* WorldContextObject, FName OptionName)
{
	TArray<BwayGameplayUrl::FNamedOptionSource> Sources;
	BwayGameplayUrl::GatherNamedOptionSources(WorldContextObject, Sources);

	FString Value;
	const FString Key = OptionName.ToString();
	for (const BwayGameplayUrl::FNamedOptionSource& Source : Sources)
	{
		if (BwayGameplayUrl::TryFindOptionInSource(Source, Key, Value))
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

bool UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionFloat(const UObject* WorldContextObject, FName OptionName, float& OutValue)
{
	TArray<BwayGameplayUrl::FNamedOptionSource> Sources;
	BwayGameplayUrl::GatherNamedOptionSources(WorldContextObject, Sources);
	return BwayGameplayUrl::TryGetFloatOptionFromSources(Sources, OptionName, OutValue, nullptr);
}

bool UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionFloatWithSource(
	const UObject* WorldContextObject,
	FName OptionName,
	float& OutValue,
	FString& OutSourceLabel)
{
	TArray<BwayGameplayUrl::FNamedOptionSource> Sources;
	BwayGameplayUrl::GatherNamedOptionSources(WorldContextObject, Sources);
	return BwayGameplayUrl::TryGetFloatOptionFromSources(Sources, OptionName, OutValue, &OutSourceLabel);
}
