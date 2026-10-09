#include "Misc/AutomationTest.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayMatchFlowConfig.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayDemoMatchRulesTest,
	"Breakaway.FrontEnd.DemoMatchRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayDemoMatchRulesTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	const FString SavedCommandLine = FCommandLine::Get();
	ON_SCOPE_EXIT
	{
		FCommandLine::Set(*SavedCommandLine);
		World->DestroyWorld(false);
	};
	FCommandLine::Set(TEXT(""));
	World->URL = FURL(nullptr, TEXT("/BreakawayCore/Maps/L_BW_Dorado"), TRAVEL_Absolute);
	auto LoadExperience = [&](const TCHAR* Path) -> const ULyraExperienceDefinition*
	{
		UClass* Class = LoadClass<ULyraExperienceDefinition>(nullptr, Path);
		return TestNotNull(Path, Class) ? Class->GetDefaultObject<ULyraExperienceDefinition>() : nullptr;
	};
	const ULyraExperienceDefinition* Demo = LoadExperience(TEXT("/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic.B_BW_Experience_CaptureTheRelic_C"));
	const ULyraExperienceDefinition* Dev = LoadExperience(TEXT("/BreakawayCore/Experiences/B_BW_Experience_Dev.B_BW_Experience_Dev_C"));
	if (!Demo || !Dev) return false;

	const FBwayResolvedMatchFlowSettings DemoRules = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(World, nullptr, Demo);
	TestEqual(TEXT("Normal experience resolves the demo asset"), DemoRules.ConfigAssetName, FName(TEXT("DA_BW_MatchFlow_Demo")));
	TestEqual(TEXT("Normal match requires three round wins"), DemoRules.PointsToWin, 3);
	TestEqual(TEXT("Normal round lasts four minutes"), DemoRules.RoundDuration, 240.0f);
	const FBwayResolvedMatchFlowSettings DevRules = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(World, nullptr, Dev);
	TestEqual(TEXT("Explicit development action keeps its asset"), DevRules.ConfigAssetName, FName(TEXT("DA_BW_MatchFlow_Dev")));
	TestEqual(TEXT("Development rounds remain short"), DevRules.RoundDuration, 90.0f);
	const FBwayResolvedMatchFlowSettings Fallback = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(World, nullptr, nullptr);
	TestEqual(TEXT("No experience uses demo rules"), Fallback.ConfigAssetName, DemoRules.ConfigAssetName);

	// Exercise the actual production resolver after staging forwards explicit test rules.
	FString Travel(TEXT("/BreakawayCore/Maps/L_BW_Dorado?Experience=B_BW_Experience_CaptureTheRelic"));
	UBwayGameplayUrlLibrary::AppendMatchRulesToTravelUrl(Travel, TEXT("?HeroSelectStaging=1?RoundDuration=12?PointsToWin=1"));
	World->URL = FURL(nullptr, *Travel, TRAVEL_Absolute);
	const FBwayResolvedMatchFlowSettings Override = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(World, nullptr, Demo);
	TestEqual(TEXT("Staged test round override wins over demo config"), Override.RoundDuration, 12.0f);
	TestEqual(TEXT("Staged test score override wins over demo config"), Override.PointsToWin, 1);
	const UBwayMatchFlowConfig* DemoAsset = LoadObject<UBwayMatchFlowConfig>(nullptr,
		TEXT("/BreakawayCore/MatchFlow/DA_BW_MatchFlow_Demo.DA_BW_MatchFlow_Demo"));
	if (TestNotNull(TEXT("Demo config asset remains loaded"), DemoAsset))
	{
		TestEqual(TEXT("Overrides do not mutate the config asset"), DemoAsset->RoundDuration, 240.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayStagingMatchRulesTest,
	"Breakaway.FrontEnd.StagingMatchRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayStagingMatchRulesTest::RunTest(const FString& Parameters)
{
	const FString Staging(TEXT("?Experience=Staging?listen=1?HeroSelectStaging=1?NoSeamlessTravel"
		"?HeroSelectTargetMap=/Arena?HeroSelectTargetExperience=Match?NumBots=0?PointsToWin=1"
		"?RoundDuration=180?WarmupDuration=3?PrematchDuration=0?PostRoundDuration=2"
		"?PostMatchSummaryDuration=9?MatchFlowConfig=/Game/Rules.Custom?DisableRelicBotAI"
		"?Hero=Argus?Name=Player?Unknown=Ignored"));
	FString Travel(TEXT("/Arena?Experience=Match?listen?SkipHeroSelection=1?SeamlessTravel"));
	UBwayGameplayUrlLibrary::AppendMatchRulesToTravelUrl(Travel, Staging);
	const FURL Destination(nullptr, *Travel, TRAVEL_Absolute);
	TestTrue(TEXT("Destination parsed by engine"), Destination.Valid != 0);
	TestEqual(TEXT("Zero bot override survives"), FString(Destination.GetOption(TEXT("NumBots="), TEXT("missing"))), FString(TEXT("0")));
	TestEqual(TEXT("Hosted win score survives"), FString(Destination.GetOption(TEXT("PointsToWin="), TEXT("missing"))), FString(TEXT("1")));
	TestEqual(TEXT("Hosted round timer survives"), FString(Destination.GetOption(TEXT("RoundDuration="), TEXT("missing"))), FString(TEXT("180")));
	for (const TCHAR* Key : {TEXT("WarmupDuration"), TEXT("PrematchDuration"), TEXT("PostRoundDuration"), TEXT("PostMatchSummaryDuration"), TEXT("MatchFlowConfig")})
	{
		TestEqual(FString::Printf(TEXT("Preserve %s"), Key), FString(Destination.GetOption(*(FString(Key) + TEXT("=")), TEXT(""))), UGameplayStatics::ParseOption(Staging, Key));
	}
	TestTrue(TEXT("Valueless bot-AI flag retained"), Destination.HasOption(TEXT("DisableRelicBotAI")));
	TestEqual(TEXT("Match experience wins over staging"), FString(Destination.GetOption(TEXT("Experience="), TEXT(""))), FString(TEXT("Match")));
	TestTrue(TEXT("Bare listen marker preserved for engine client-travel stripping"), Destination.Op.Contains(TEXT("listen")));
	for (const TCHAR* Key : {TEXT("HeroSelectStaging"), TEXT("HeroSelectTargetMap"), TEXT("HeroSelectTargetExperience"), TEXT("NoSeamlessTravel"), TEXT("Hero"), TEXT("Name"), TEXT("Unknown")})
	{
		TestFalse(FString::Printf(TEXT("Do not leak %s"), Key), Destination.HasOption(Key));
	}
	const FString FirstTravel = Travel;
	UBwayGameplayUrlLibrary::AppendMatchRulesToTravelUrl(Travel, Staging);
	TestEqual(TEXT("Repeated assembly adds no duplicates"), Travel, FirstTravel);
	FString Explicit(TEXT("/Arena?RoundDuration=240?NumBots=4"));
	UBwayGameplayUrlLibrary::AppendMatchRulesToTravelUrl(Explicit, Staging);
	const FURL ExplicitDestination(nullptr, *Explicit, TRAVEL_Absolute);
	TestEqual(TEXT("Explicit destination timer wins"), FString(ExplicitDestination.GetOption(TEXT("RoundDuration="), TEXT(""))), FString(TEXT("240")));
	TestEqual(TEXT("Explicit destination bot count wins"), FString(ExplicitDestination.GetOption(TEXT("NumBots="), TEXT(""))), FString(TEXT("4")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayGameplayUrlEngineCompatibilityTest,
	"Breakaway.FrontEnd.GameplayUrlEngineCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayGameplayUrlEngineCompatibilityTest::RunTest(const FString& Parameters)
{
	for (const FString& Input : {
		FString(TEXT("?listen?Experience=Staging?HeroSelectStaging=1")),
		FString(TEXT("?listen=1&Experience=Staging&HeroSelectStaging=1"))})
	{
		FString Options = Input;
		UBwayGameplayUrlLibrary::AppendMissingOptionsFromUrlSource(Options,
			TEXT("/Map?Experience=MustNotOverride?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=Match"));
		TestTrue(TEXT("Engine recognizes listen"), UGameplayStatics::HasOption(Options, TEXT("listen")));
		TestTrue(TEXT("Engine recognizes staging"), UGameplayStatics::HasOption(Options, TEXT("HeroSelectStaging")));
		TestEqual(TEXT("Existing experience preserved"), UGameplayStatics::ParseOption(Options, TEXT("Experience")), FString(TEXT("Staging")));
		TestEqual(TEXT("Target map readable by travel code"), UGameplayStatics::ParseOption(Options, TEXT("HeroSelectTargetMap")), FString(TEXT("/BreakawayCore/Maps/L_BW_Dorado")));
		TestEqual(TEXT("Target experience readable by travel code"), UGameplayStatics::ParseOption(Options, TEXT("HeroSelectTargetExperience")), FString(TEXT("Match")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRematchTravelBaseTest,
	"Breakaway.FrontEnd.RematchTravelBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRematchTravelBaseTest::RunTest(const FString& Parameters)
{
	FURL Base(nullptr, TEXT("/BreakawayCore/Maps/L_BW_Dorado?listen?Experience=B_BW_Experience_CaptureTheRelic"
		"?SkipHeroSelection=1?SeamlessTravel?Restart?PointsToWin=3?RoundDuration=90?SkipHeroSelectionDebug=1"), TRAVEL_Absolute);
	Base.Port = 17777;
	// Engine ProcessServerTravel consumes removal tokens. Browse then uses LastURL again.
	const FURL LegacyFirstParse(&Base, TEXT("/BreakawayCore/Maps/L_BW_Dorado?-SkipHeroSelection?-SeamlessTravel?-Restart?NoSeamlessTravel"), TRAVEL_Relative);
	const FURL LegacySecondParse(&Base, *LegacyFirstParse.ToString(), TRAVEL_Relative);
	TestTrue(TEXT("Regression reproduced: second relative parse restores skip flag"), LegacySecondParse.HasOption(TEXT("SkipHeroSelection")));

	UBwayGameplayUrlLibrary::ResetMatchTravelOptions(Base);
	const FURL FirstParse(&Base, TEXT("/BreakawayCore/Maps/L_BW_Dorado?NoSeamlessTravel"), TRAVEL_Relative);
	const FURL SecondParse(&Base, *FirstParse.ToString(), TRAVEL_Relative);
	TestFalse(TEXT("Selection skip stays removed through both parses"), SecondParse.HasOption(TEXT("SkipHeroSelection")));
	TestFalse(TEXT("Seamless transition stays removed"), SecondParse.HasOption(TEXT("SeamlessTravel")));
	TestFalse(TEXT("Restart flag stays removed"), SecondParse.HasOption(TEXT("Restart")));
	TestTrue(TEXT("Rematch explicitly uses a fresh world"), SecondParse.HasOption(TEXT("NoSeamlessTravel")));
	TestTrue(TEXT("Longer unrelated key is preserved"), SecondParse.HasOption(TEXT("SkipHeroSelectionDebug")));
	TestTrue(TEXT("Listen session is preserved"), SecondParse.HasOption(TEXT("listen")));
	TestEqual(TEXT("Actual PIE listen port is preserved"), SecondParse.Port, 17777);
	TestEqual(TEXT("Match rule survives"), FString(SecondParse.GetOption(TEXT("PointsToWin="), TEXT(""))), FString(TEXT("3")));
	TestEqual(TEXT("Experience survives"), FString(SecondParse.GetOption(TEXT("Experience="), TEXT(""))), FString(TEXT("B_BW_Experience_CaptureTheRelic")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRematchStartupOptionsTest,
	"Breakaway.FrontEnd.RematchStartupOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRematchStartupOptionsTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	const FString SavedCommandLine = FCommandLine::Get();
	ON_SCOPE_EXIT
	{
		FCommandLine::Set(*SavedCommandLine);
		World->DestroyWorld(false);
	};
	AGameModeBase* GameMode = World->SpawnActor<AGameModeBase>();
	if (!TestNotNull(TEXT("Fixture game mode"), GameMode)) return false;
	FCommandLine::Set(TEXT("/BreakawayCore/Maps/L_BW_Dorado?SkipHeroSelection=1?Hero=Argus?PointsToWin=5"));
	const FString SelectionOptions(TEXT("?listen?HeroSelectStaging=1?NoSeamlessTravel?Experience=Match?PointsToWin=3"));
	World->URL = FURL(nullptr, *(TEXT("/BreakawayCore/Maps/L_BW_HeroSelect_Staging") + SelectionOptions), TRAVEL_Absolute);
	GameMode->OptionsString = SelectionOptions;
	UBwayGameplayUrlLibrary::AugmentGameModeOptionsString(GameMode);
	TestFalse(TEXT("Explicit staging does not inherit the launch skip"), UGameplayStatics::HasOption(GameMode->OptionsString, TEXT("SkipHeroSelection")));
	TestTrue(TEXT("Staging route preserved"), UGameplayStatics::HasOption(GameMode->OptionsString, TEXT("HeroSelectStaging")));
	TestEqual(TEXT("Current match rule preserved"), UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("PointsToWin"), 0), 3);
	TestFalse(TEXT("Presence query cannot recover the launch skip"), UBwayGameplayUrlLibrary::HasGameplayUrlOption(World, TEXT("SkipHeroSelection")));
	int32 SkipInt = 0;
	float SkipFloat = 0.0f;
	FString SkipString;
	FString Source;
	TestFalse(TEXT("Integer query respects selection"), UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionInt(World, TEXT("SkipHeroSelection"), SkipInt));
	TestFalse(TEXT("Sourced integer query respects selection"), UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionIntWithSource(World, TEXT("SkipHeroSelection"), SkipInt, Source));
	TestFalse(TEXT("Float query respects selection"), UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionFloatWithSource(World, TEXT("SkipHeroSelection"), SkipFloat, Source));
	TestFalse(TEXT("String query respects selection"), UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionString(World, TEXT("SkipHeroSelection"), SkipString));

	FString Appended = SelectionOptions;
	UBwayGameplayUrlLibrary::AppendMissingOptionsFromUrlSource(Appended, FCommandLine::Get());
	TestFalse(TEXT("Fragment merge respects explicit selection"), UGameplayStatics::HasOption(Appended, TEXT("SkipHeroSelection")));

	// The same process may still start an explicitly requested direct match.
	World->URL = FURL(nullptr, TEXT("/BreakawayCore/Maps/L_BW_Dorado?SkipHeroSelection=1?Experience=Match"), TRAVEL_Absolute);
	GameMode->OptionsString = TEXT("?SkipHeroSelection=1?Experience=Match");
	UBwayGameplayUrlLibrary::AugmentGameModeOptionsString(GameMode);
	TestEqual(TEXT("Initial direct match retains its explicit skip"), UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("SkipHeroSelection"), 0), 1);
	TestFalse(TEXT("Direct match does not inherit stale staging routing"), UGameplayStatics::HasOption(GameMode->OptionsString, TEXT("HeroSelectStaging")));
	TestTrue(TEXT("Direct match can query the skip option"), UBwayGameplayUrlLibrary::HasGameplayUrlOption(World, TEXT("SkipHeroSelection")));
	return true;
}
#endif
