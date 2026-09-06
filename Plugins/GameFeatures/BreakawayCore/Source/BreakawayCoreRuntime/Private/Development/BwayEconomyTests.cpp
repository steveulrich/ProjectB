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
#include "GameState/BwayScoringComponent.h"
#include "UObject/UnrealType.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "LyraGameplayTags.h"
#include "Economy/BwayBaseZone.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"

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
	// Block the debit after the infinite upgrade has already applied. The failed
	// transaction must remove that effect without changing balance or ownership.
	ASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
		[](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
		{ return !Spec.Def->IsA<UBwayGameplayEffect_SpendGold>(); }));
	TestFalse(TEXT("Rejected debit fails purchase"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 0));
	TestEqual(TEXT("Rejected debit preserves gold"), Gold->GetCurrentGold(), 100.0f);
	TestEqual(TEXT("Rejected debit rolls back bonus"), Hero->GetAttackStrength(), 20.0f);
	TestEqual(TEXT("Rejected debit preserves ownership"), Shop->GetOwnedRank(TEXT("Attack")), 0);
	ASC->GameplayEffectApplicationQueries.Reset();
	bool bReentryAttempted = false;
	bool bReentrySucceeded = false;
	const FDelegateHandle ReentryHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UBwayHeroAttributeSet::GetAttackStrengthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData&)
		{
			bReentryAttempted = true;
			bReentrySucceeded |= Shop->TryPurchaseUpgrade(TEXT("Attack"), 0);
			Shop->ResetUpgrades();
		});
	TestTrue(TEXT("First rank purchase succeeds"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 0));
	ASC->GetGameplayAttributeValueChangeDelegate(UBwayHeroAttributeSet::GetAttackStrengthAttribute()).Remove(ReentryHandle);
	TestTrue(TEXT("Effect notification exercised reentry"), bReentryAttempted);
	TestFalse(TEXT("Reentrant purchase rejected"), bReentrySucceeded);
	TestEqual(TEXT("Reentrant reset cannot clear purchased rank"), Shop->GetOwnedRank(TEXT("Attack")), 1);
	TestEqual(TEXT("First price paid"), Gold->GetCurrentGold(), 70.0f);
	TestEqual(TEXT("First bonus applied"), Hero->GetAttackStrength(), 25.0f);
	TestFalse(TEXT("Stale request cannot buy another rank"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 0));
	TestFalse(TEXT("Slot cap enforced"), Shop->TryPurchaseUpgrade(TEXT("Other"), 0));
	TestFalse(TEXT("Catalog cannot change while owned"), Shop->ConfigureCatalog(Catalog));
	ASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
		[](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
		{ return !Spec.Def->IsA<UBwayGameplayEffect_SpendGold>(); }));
	TestFalse(TEXT("Failed rank increase rejects purchase"), Shop->TryPurchaseUpgrade(TEXT("Attack"), 1));
	TestEqual(TEXT("Failed rank increase preserves old bonus"), Hero->GetAttackStrength(), 25.0f);
	TestEqual(TEXT("Failed rank increase preserves old rank"), Shop->GetOwnedRank(TEXT("Attack")), 1);
	TestEqual(TEXT("Failed rank increase preserves balance"), Gold->GetCurrentGold(), 70.0f);
	ASC->GameplayEffectApplicationQueries.Reset();
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
	ULyraHealthSet* Health = NewObject<ULyraHealthSet>(Player);
	ASC->AddSpawnedAttribute(Health);
	SetPhase(EBwayMatchPhase::Playing);
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 100.0f);
	TestFalse(TEXT("Living health closes combat purchase window"), Shop->IsPurchaseWindowOpen());
	ASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_Death_Dead, 1);
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
	TestTrue(TEXT("Death opens purchase window"), Shop->IsPurchaseWindowOpen());
	ASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_Death_Dead, 0);
	TestTrue(TEXT("Pawn tag cleanup preserves death purchase window"), Shop->IsPurchaseWindowOpen());
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 100.0f);
	TestFalse(TEXT("Respawn health closes combat purchase window"), Shop->IsPurchaseWindowOpen());
	// A real pawn/ASC avatar with explicit team membership exercises spatial access.
	APawn* Pawn = World->SpawnActor<APawn>();
	USceneComponent* PawnRoot = NewObject<USceneComponent>(Pawn);
	Pawn->AddInstanceComponent(PawnRoot);
	Pawn->SetRootComponent(PawnRoot);
	PawnRoot->RegisterComponent();
	FObjectProperty* PawnStateProperty = FindFProperty<FObjectProperty>(APawn::StaticClass(), TEXT("PlayerState"));
	FArrayProperty* TeamsProperty = FindFProperty<FArrayProperty>(ABwayGameState::StaticClass(), TEXT("Teams"));
	if (!TestNotNull(TEXT("Pawn player-state fixture property"), PawnStateProperty)
		|| !TestNotNull(TEXT("Team fixture property"), TeamsProperty))
	{
		World->DestroyWorld(false);
		return false;
	}
	PawnStateProperty->SetObjectPropertyValue_InContainer(Pawn, Player);
	TArray<FTeamInfo>* Teams = TeamsProperty->ContainerPtrToValuePtr<TArray<FTeamInfo>>(GS);
	Teams->Add(FTeamInfo(0));
	Teams->Add(FTeamInfo(1));
	GS->AddPlayerToTeam(Player, 0);
	ASC->InitAbilityActorInfo(Player, Pawn);
	ABwayBaseZone* Base = World->SpawnActor<ABwayBaseZone>();
	TestTrue(TEXT("Teammate inside base has access"), Base->CanUseBase(Player));
	TestTrue(TEXT("Living player can shop in own base"), Shop->IsPurchaseWindowOpen());
	Pawn->SetActorLocation(FVector(501.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Leaving base immediately removes access"), Base->CanUseBase(Player));
	TestFalse(TEXT("Leaving base closes living-player shop"), Shop->IsPurchaseWindowOpen());
	Pawn->SetActorLocation(FVector::ZeroVector);
	GS->AddPlayerToTeam(Player, 1);
	TestFalse(TEXT("Enemy cannot use base"), Base->CanUseBase(Player));
	GS->AddPlayerToTeam(Player, 0);
	Base->bEnabled = false;
	TestFalse(TEXT("Disabled base denies access"), Base->CanUseBase(Player));
	Base->bEnabled = true;
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
	TestFalse(TEXT("Base cannot heal a dead pawn"), Base->CanUseBase(Player));
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 100.0f);
	ASC->InitAbilityActorInfo(Player, Player);
	TestFalse(TEXT("Missing pawn avatar denies base access"), Base->CanUseBase(Player));
	SetPhase(EBwayMatchPhase::PostRound);
	TestTrue(TEXT("Between-round planning opens purchase window"), Shop->IsPurchaseWindowOpen());
	// This fixture does not BeginPlay, so initialize the score array normally
	// established there before exercising the production AddScore method.
	UBwayScoringComponent* Scoring = GS->FindComponentByClass<UBwayScoringComponent>();
	FArrayProperty* ScoresProperty = FindFProperty<FArrayProperty>(UBwayScoringComponent::StaticClass(), TEXT("TeamScores"));
	if (!TestNotNull(TEXT("Score fixture property"), ScoresProperty))
	{
		World->DestroyWorld(false);
		return false;
	}
	ScoresProperty->ContainerPtrToValuePtr<TArray<int32>>(Scoring)->Init(0, 2);
	Scoring->AddScore(0, GS->GetRoundManagement()->PointsToWin);
	TestTrue(TEXT("Fixture reached winning score"), GS->GetRoundManagement()->CheckMatchEnd());
	TestFalse(TEXT("Final-round summary closes purchase window"), Shop->IsPurchaseWindowOpen());
	SetPhase(EBwayMatchPhase::PostMatch);
	TestFalse(TEXT("Postmatch closes purchase window"), Shop->IsPurchaseWindowOpen());
	World->DestroyWorld(false);
	return true;
}
#endif
