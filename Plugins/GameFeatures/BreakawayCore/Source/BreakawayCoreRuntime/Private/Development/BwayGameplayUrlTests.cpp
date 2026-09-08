#include "Misc/AutomationTest.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineBaseTypes.h"

#if WITH_DEV_AUTOMATION_TESTS
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
#endif
