#include "Misc/AutomationTest.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "Kismet/GameplayStatics.h"

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
#endif
