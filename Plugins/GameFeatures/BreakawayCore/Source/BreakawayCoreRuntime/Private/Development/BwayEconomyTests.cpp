#include "Misc/AutomationTest.h"
#include "Economy/BwayEconomyLibrary.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayGoldDebitTest,
	"Breakaway.Economy.GoldDebit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayGoldDebitTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Fixture world"), World))
	{
		return false;
	}
	APlayerState* Player = World->SpawnActor<APlayerState>();
	if (!TestNotNull(TEXT("Fixture player"), Player))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestFalse(TEXT("Missing economy rejects debit"), UBwayEconomyLibrary::TrySpendGold(Player, 10));
	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Player);
	Player->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Player, Player);
	UBwayGoldAttributeSet* Gold = NewObject<UBwayGoldAttributeSet>(Player);
	ASC->AddSpawnedAttribute(Gold);
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 100.0f);
	Player->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot debit"), UBwayEconomyLibrary::TrySpendGold(Player, 10));
	TestEqual(TEXT("Client attempt preserves balance"), Gold->GetCurrentGold(), 100.0f);
	Player->SetRole(ROLE_Authority);

	TestFalse(TEXT("Null player rejected"), UBwayEconomyLibrary::TrySpendGold(nullptr, 1));
	TestFalse(TEXT("Negative price rejected"), UBwayEconomyLibrary::TrySpendGold(Player, -10));
	TestFalse(TEXT("Unaffordable price rejected"), UBwayEconomyLibrary::TrySpendGold(Player, 101));
	TestFalse(TEXT("Oversized price rejected"), UBwayEconomyLibrary::TrySpendGold(Player, MAX_int32));
	TestEqual(TEXT("Rejections preserve balance"), Gold->GetCurrentGold(), 100.0f);
	TestTrue(TEXT("Zero price succeeds without debit"), UBwayEconomyLibrary::TrySpendGold(Player, 0));
	TestTrue(TEXT("Instant debit reports successful application"), UBwayEconomyLibrary::TrySpendGold(Player, 60));
	TestEqual(TEXT("Exact cost deducted"), Gold->GetCurrentGold(), 40.0f);
	TestFalse(TEXT("Repeated price cannot overdraw"), UBwayEconomyLibrary::TrySpendGold(Player, 60));
	TestEqual(TEXT("No partial payment"), Gold->GetCurrentGold(), 40.0f);
	TestTrue(TEXT("Exact remaining balance can be spent"), UBwayEconomyLibrary::TrySpendGold(Player, 40));
	TestEqual(TEXT("Balance reaches zero"), Gold->GetCurrentGold(), 0.0f);
	TestFalse(TEXT("Empty balance rejects debit"), UBwayEconomyLibrary::TrySpendGold(Player, 1));
	World->DestroyWorld(false);
	return true;
}
#endif
