#include "Misc/AutomationTest.h"
#include "Economy/BwayEconomyLibrary.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "UObject/UnrealType.h"

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayUpgradePurchaseTest,
	"Breakaway.Economy.UpgradePurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayUpgradePurchaseTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	APlayerState* Player = World->SpawnActor<APlayerState>();
	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	World->SetGameState(GS);
	// Set a controlled phase without starting the experience, timers, or pawn spawning.
	FEnumProperty* PhaseProperty = FindFProperty<FEnumProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentMatchPhase"));
	if (!TestNotNull(TEXT("Phase fixture property"), PhaseProperty))
	{
		World->DestroyWorld(false);
		return false;
	}
	auto SetPhase = [GS, PhaseProperty](EBwayMatchPhase Phase)
	{
		PhaseProperty->GetUnderlyingProperty()->SetIntPropertyValue(
			PhaseProperty->ContainerPtrToValuePtr<void>(GS->GetRoundManagement()), static_cast<uint64>(Phase));
	};
	SetPhase(EBwayMatchPhase::Warmup);
	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Player);
	Player->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Player, Player);
	UBwayGoldAttributeSet* Gold = NewObject<UBwayGoldAttributeSet>(Player);
	UBwayHeroAttributeSet* Hero = NewObject<UBwayHeroAttributeSet>(Player);
	ASC->AddSpawnedAttribute(Gold);
	ASC->AddSpawnedAttribute(Hero);
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetAttackStrengthAttribute(), 20.0f);
	UBwayUpgradeComponent* Shop = NewObject<UBwayUpgradeComponent>(Player);
	Player->AddInstanceComponent(Shop);
	Shop->RegisterComponent();
	UBwayUpgradeCatalog* Catalog = NewObject<UBwayUpgradeCatalog>(Player);
	Catalog->MaxOwnedUpgrades = 1;
	FBwayUpgradeDefinition& Attack = Catalog->Upgrades.AddDefaulted_GetRef();
	Attack.Id = TEXT("Attack");
	Attack.EffectClass = UBwayUpgradeEffect_AttackStrength::StaticClass();
	FBwayUpgradeRank& First = Attack.Ranks.AddDefaulted_GetRef();
	First.Cost = 30;
	First.Magnitude = 5;
	FBwayUpgradeRank& Second = Attack.Ranks.AddDefaulted_GetRef();
	Second.Cost = 40;
	Second.Magnitude = 12;
	FBwayUpgradeDefinition Other = Attack;
	Other.Id = TEXT("Other");
	Catalog->Upgrades.Add(Other);
	TestTrue(TEXT("Valid catalog configures"), Shop->ConfigureCatalog(Catalog));
	TestFalse(TEXT("Unknown ID rejected"), Shop->TryPurchaseUpgrade(TEXT("Missing"), 0));
	Player->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client native call rejected"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 0));
	Player->SetRole(ROLE_Authority);
	SetPhase(EBwayMatchPhase::Playing);
	TestFalse(TEXT("Living player cannot shop in combat"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 0));
	SetPhase(EBwayMatchPhase::Warmup);
	TestEqual(TEXT("Rejected attempts preserve gold"), Gold->GetCurrentGold(), 100.0f);
	TestEqual(TEXT("Rejected attempts preserve attributes"), Hero->GetAttackStrength(), 20.0f);
	TestEqual(TEXT("Rejected attempts preserve ownership"), Shop->GetOwnedRank(TEXT("Attack")), 0);
	TestTrue(TEXT("First rank purchase succeeds"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 0));
	TestEqual(TEXT("First price paid"), Gold->GetCurrentGold(), 70.0f);
	TestEqual(TEXT("First bonus applied"), Hero->GetAttackStrength(), 25.0f);
	TestFalse(TEXT("Stale request cannot buy another rank"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 0));
	TestFalse(TEXT("Slot cap enforced"), Shop->TryPurchaseUpgrade(TEXT("Other"), 0));
	TestFalse(TEXT("Catalog cannot change while owned"), Shop->ConfigureCatalog(Catalog));
	TestTrue(TEXT("Second rank succeeds"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 1));
	TestEqual(TEXT("Second price paid"), Gold->GetCurrentGold(), 30.0f);
	TestEqual(TEXT("Rank bonus replaces previous bonus"), Hero->GetAttackStrength(), 32.0f);
	TestFalse(TEXT("Rank cap enforced"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 2));
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetAttackStrengthAttribute(), 40.0f);
	TestEqual(TEXT("Base reinitialization preserves owned bonus"), Hero->GetAttackStrength(), 52.0f);
	Shop->ResetUpgrades();
	TestEqual(TEXT("Reset clears owned rank"), Shop->GetOwnedRank(TEXT("Attack")), 0);
	TestEqual(TEXT("Reset removes bonus"), Hero->GetAttackStrength(), 40.0f);
	TestEqual(TEXT("Reset does not refund spent gold"), Gold->GetCurrentGold(), 30.0f);
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 29.0f);
	TestFalse(TEXT("Unaffordable purchase rejected"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 0));
	TestEqual(TEXT("Unaffordable attempt preserves gold"), Gold->GetCurrentGold(), 29.0f);
	TestEqual(TEXT("Unaffordable attempt preserves bonus"), Hero->GetAttackStrength(), 40.0f);
	TestEqual(TEXT("Unaffordable attempt preserves ownership"), Shop->GetOwnedRank(TEXT("Attack")), 0);
	World->DestroyWorld(false);
	return true;
}
#endif
