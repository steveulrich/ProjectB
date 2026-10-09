#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Economy/BwayEconomyLibrary.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "LyraGameplayTags.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRespawnUpgradeTest, "Breakaway.Economy.RespawnUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRespawnUpgradeTest::RunTest(const FString&)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("World"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	World->SetGameState(GS);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
	if (!TestNotNull(TEXT("Game state"), GS) || !TestNotNull(TEXT("Controller"), Controller)
		|| !TestNotNull(TEXT("Player"), Player)) return false;
	Controller->PlayerState = Player;
	Player->SetOwner(Controller);
	GS->PlayerArray.AddUnique(Player);
	if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
	ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
	UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
	const UBwayHeroAttributeSet* Hero = ASC->GetSet<UBwayHeroAttributeSet>();
	UBwayUpgradeComponent* Shop = Player->UpgradeComponent;
	UBwayRoundManagementComponent* Rounds = GS->GetRoundManagement();
	ON_SCOPE_EXIT { Rounds->ClearRespawnTimers(); ASC->GameplayEffectApplicationQueries.Reset(); };
	Rounds->CurrentMatchPhase = EBwayMatchPhase::Playing;
	Rounds->CurrentRoundState = ERoundState::RoundActive;
	Rounds->CurrentRoundNumber = 1;
	ASC->AddLooseGameplayTag(LyraGameplayTags::Status_Death);
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 200.f);
	UBwayUpgradeCatalog* Catalog = NewObject<UBwayUpgradeCatalog>(Player);
	FBwayUpgradeDefinition& Entry = Catalog->Upgrades.AddDefaulted_GetRef();
	Entry.Id = TEXT("RespawnTimeReduction");
	Entry.EffectClass = UBwayUpgradeEffect_RespawnTimeReduction::StaticClass();
	Entry.Ranks.Add({20, 0.2f});
	Entry.Ranks.Add({30, 0.4f});
	TestTrue(TEXT("Supported respawn catalog"), Shop->ConfigureCatalog(Catalog));
	auto& Pending = Rounds->RespawnTimers.Add(Controller);
	Pending.StartTime = GS->GetServerWorldTimeSeconds();
	Pending.BaseDelay = 10.f;
	Pending.Round = 1;
	Rounds->RefreshRespawnSchedule(Player);
	auto Remaining = [&]() { return World->GetTimerManager().GetTimerRemaining(Pending.Timer); };
	TestEqual(TEXT("Unmodified timer uses death baseline"), Remaining(), 10.f);
	TestTrue(TEXT("Deadline matches native timer"), FMath::IsNearlyEqual(Player->GetRespawnSchedule().ServerEndTime - GS->GetServerWorldTimeSeconds(), 10.0, 0.001));
	TestTrue(TEXT("Dead player can buy rank one"), Shop->TryPurchaseUpgrade(Entry.Id, 0));
	TestEqual(TEXT("Purchase shortens existing timer"), Remaining(), 8.f);
	TestEqual(TEXT("Exact first debit"), ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 180.f);
	const double FirstDeadline = Player->GetRespawnSchedule().ServerEndTime;
	TestFalse(TEXT("Stale request rejected"), Shop->TryPurchaseUpgrade(Entry.Id, 0));
	TestEqual(TEXT("Stale request preserves deadline"), Player->GetRespawnSchedule().ServerEndTime, FirstDeadline);
	ASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
		[](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec) { return !Spec.Def->IsA<UBwayGameplayEffect_SpendGold>(); }));
	TestFalse(TEXT("Rejected debit rolls back temporary effect"), Shop->TryPurchaseUpgrade(Entry.Id, 1));
	TestEqual(TEXT("Rollback preserves modifier"), Hero->GetRespawnTimeReduction(), 0.2f);
	TestEqual(TEXT("Rollback cannot shorten deadline"), Player->GetRespawnSchedule().ServerEndTime, FirstDeadline);
	TestEqual(TEXT("Rollback preserves wallet"), ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 180.f);
	ASC->GameplayEffectApplicationQueries.Reset();
	// Advance the recorded death age independently of the timer to test elapsed-time
	// arithmetic. Actual death/restart and peer convergence are separate runtime gates.
	Pending.StartTime -= 2.0;
	TestTrue(TEXT("Second rank commits"), Shop->TryPurchaseUpgrade(Entry.Id, 1));
	TestEqual(TEXT("Rank replaces instead of stacking"), Hero->GetRespawnTimeReduction(), 0.4f);
	TestTrue(TEXT("Elapsed death time retained"), FMath::IsNearlyEqual(Remaining(), 4.f, 0.001f));
	TestEqual(TEXT("Exact second debit"), ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 150.f);
	const double SecondDeadline = Player->GetRespawnSchedule().ServerEndTime;
	Rounds->RefreshRespawnSchedule(Player);
	TestEqual(TEXT("Unchanged modifier does not restart countdown"), Player->GetRespawnSchedule().ServerEndTime, SecondDeadline);
	ASC->InitAbilityActorInfo(Player, World->SpawnActor<APawn>());
	TestEqual(TEXT("Avatar replacement retains purchased effect"), Hero->GetRespawnTimeReduction(), 0.4f);
	Shop->ResetUpgrades();
	TestEqual(TEXT("Reset removes modifier"), Hero->GetRespawnTimeReduction(), 0.f);
	TestEqual(TEXT("Reset clears ownership"), Shop->GetOwnedRank(Entry.Id), 0);
	TestEqual(TEXT("Removal restores baseline minus elapsed time"), Remaining(), 8.f);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetRespawnTimeReductionAttribute(), 2.f);
	TestEqual(TEXT("Attribute upper clamp"), Hero->GetRespawnTimeReduction(), 1.f);
	Rounds->RefreshRespawnSchedule(Player);
	TestTrue(TEXT("Full reduction keeps a positive asynchronous timer"), Remaining() > 0.f && Remaining() <= 0.051f);
	TestTrue(TEXT("Overdue purchase still publishes a deadline"), Player->GetRespawnSchedule().ServerEndTime > GS->GetServerWorldTimeSeconds());
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetRespawnTimeReductionAttribute(), -1.f);
	TestEqual(TEXT("Negative attribute clamp"), Hero->GetRespawnTimeReduction(), 0.f);
	float NonFinite = std::numeric_limits<float>::quiet_NaN();
	Hero->PreAttributeBaseChange(UBwayHeroAttributeSet::GetRespawnTimeReductionAttribute(), NonFinite);
	TestEqual(TEXT("Nonfinite modifier rejected at clamp"), NonFinite, 0.f);
	Rounds->CurrentRoundState = ERoundState::RoundEnding;
	const double EndingDeadline = Player->GetRespawnSchedule().ServerEndTime;
	Rounds->RefreshRespawnSchedule(Player);
	TestEqual(TEXT("Ending round cannot reschedule death"), Player->GetRespawnSchedule().ServerEndTime, EndingDeadline);
	Rounds->ClearRespawnTimers();
	TestTrue(TEXT("Round cleanup clears timer ownership"), Rounds->RespawnTimers.IsEmpty());
	TestEqual(TEXT("Round cleanup clears replicated deadline"), Player->GetRespawnSchedule().ServerEndTime, 0.0);
	Rounds->RefreshRespawnSchedule(Player);
	TestTrue(TEXT("No purchase can invent a pending death"), Rounds->RespawnTimers.IsEmpty());
	return true;
}
#endif
