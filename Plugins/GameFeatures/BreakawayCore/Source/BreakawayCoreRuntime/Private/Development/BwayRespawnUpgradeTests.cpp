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
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "GameModes/BwayMatchFlowConfig.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/LyraExperienceDefinition.h"
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

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BreakawayGameMode.h"
#include "BwayCharacterWithAbilities.h"
#include "Character/LyraHealthComponent.h"
#include "GameModes/BwayMatchFlowConfig.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRespawnEscalatingProgressionTest, "Breakaway.Respawn.EscalatingProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRespawnEscalatingProgressionTest::RunTest(const FString&)
{
	// 1. Verify UBwayMatchConfigDataAsset / UBwayMatchFlowConfig defaults and asset resolution
	const UBwayMatchConfigDataAsset* ConfigCDO = GetDefault<UBwayMatchConfigDataAsset>();
	if (!TestNotNull(TEXT("UBwayMatchConfigDataAsset CDO"), ConfigCDO)) return false;
	TestEqual(TEXT("CDO BaseRespawnDelaySeconds default"), ConfigCDO->BaseRespawnDelaySeconds, 10.0f);
	TestEqual(TEXT("CDO PerDeathRespawnIncrementSeconds default"), ConfigCDO->PerDeathRespawnIncrementSeconds, 5.0f);
	TestEqual(TEXT("CDO MaxRespawnDelaySeconds default"), ConfigCDO->MaxRespawnDelaySeconds, 25.0f);
	TestTrue(TEXT("CDO bResetRespawnEscalationEachRound default"), ConfigCDO->bResetRespawnEscalationEachRound);

	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("World"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };

	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	World->SetGameState(GS);
	if (!TestNotNull(TEXT("GameState"), GS)) return false;
	if (!GS->IsActorInitialized()) static_cast<AActor*>(GS)->PostInitializeComponents();

	UBwayRoundManagementComponent* Rounds = GS->GetRoundManagement();
	if (!TestNotNull(TEXT("RoundManagementComponent"), Rounds)) return false;

	// Verify Demo MatchFlowConfig resolution and fast Dev override capability
	const FBwayResolvedMatchFlowSettings DemoResolved = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(World, nullptr, nullptr);
	TestEqual(TEXT("Demo resolved BaseRespawnDelaySeconds"), DemoResolved.BaseRespawnDelaySeconds, 10.0f);
	TestEqual(TEXT("Demo resolved PerDeathRespawnIncrementSeconds"), DemoResolved.PerDeathRespawnIncrementSeconds, 5.0f);
	TestEqual(TEXT("Demo resolved MaxRespawnDelaySeconds"), DemoResolved.MaxRespawnDelaySeconds, 25.0f);
	TestTrue(TEXT("Demo resolved bResetRespawnEscalationEachRound"), DemoResolved.bResetRespawnEscalationEachRound);

	const UBwayMatchConfigDataAsset* FastDevConfig = LoadObject<UBwayMatchConfigDataAsset>(nullptr,
		TEXT("/BreakawayCore/MatchFlow/DA_BW_MatchFlow_Dev.DA_BW_MatchFlow_Dev"));
	if (!TestNotNull(TEXT("Authored fast development match config"), FastDevConfig)) return false;
	TestEqual(TEXT("Development asset keeps a three-second respawn"), FastDevConfig->BaseRespawnDelaySeconds, 3.0f);
	TestEqual(TEXT("Development asset disables escalation"), FastDevConfig->PerDeathRespawnIncrementSeconds, 0.0f);
	TestEqual(TEXT("Development asset caps respawns at three seconds"), FastDevConfig->MaxRespawnDelaySeconds, 3.0f);
	const FBwayResolvedMatchFlowSettings FastResolved = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(World, FastDevConfig, nullptr);
	TestEqual(TEXT("Fast dev override BaseRespawnDelaySeconds"), FastResolved.BaseRespawnDelaySeconds, 3.0f);
	TestEqual(TEXT("Fast dev override PerDeathRespawnIncrementSeconds"), FastResolved.PerDeathRespawnIncrementSeconds, 0.0f);
	TestEqual(TEXT("Fast dev override MaxRespawnDelaySeconds"), FastResolved.MaxRespawnDelaySeconds, 3.0f);
	TestEqual(TEXT("Fast dev resolution names DA_BW_MatchFlow_Dev"), FastResolved.ConfigAssetName, FName(TEXT("DA_BW_MatchFlow_Dev")));

	// 1b. The authored Demo asset itself (not only the CDO defaults) carries the provisional 10/5/25 progression.
	const UBwayMatchFlowConfig* DemoAsset = LoadObject<UBwayMatchFlowConfig>(nullptr,
		TEXT("/BreakawayCore/MatchFlow/DA_BW_MatchFlow_Demo.DA_BW_MatchFlow_Demo"));
	if (!TestNotNull(TEXT("Authored Demo match config"), DemoAsset)) return false;
	TestEqual(TEXT("Demo asset BaseRespawnDelaySeconds"), DemoAsset->BaseRespawnDelaySeconds, 10.0f);
	TestEqual(TEXT("Demo asset PerDeathRespawnIncrementSeconds"), DemoAsset->PerDeathRespawnIncrementSeconds, 5.0f);
	TestEqual(TEXT("Demo asset MaxRespawnDelaySeconds"), DemoAsset->MaxRespawnDelaySeconds, 25.0f);
	TestTrue(TEXT("Demo asset bResetRespawnEscalationEachRound"), DemoAsset->bResetRespawnEscalationEachRound);
	TestEqual(TEXT("Default resolution names DA_BW_MatchFlow_Demo"), DemoResolved.ConfigAssetName, FName(TEXT("DA_BW_MatchFlow_Demo")));

	// 1c. Selected experiences resolve the intended asset: the hosted Capture-the-Relic match (and the
	// staging/ball experiences that carry no explicit config) use Demo; only the Dev experience opts into 3/0/3.
	struct FExperienceExpectation
	{
		const TCHAR* ClassPath;
		const TCHAR* ExpectedConfig;
		float Base;
		float Increment;
		float Max;
	};
	const FExperienceExpectation ExperienceExpectations[] = {
		{ TEXT("/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic.B_BW_Experience_CaptureTheRelic_C"), TEXT("DA_BW_MatchFlow_Demo"), 10.0f, 5.0f, 25.0f },
		{ TEXT("/BreakawayCore/Experiences/B_BW_Experience_HeroSelectStaging.B_BW_Experience_HeroSelectStaging_C"), TEXT("DA_BW_MatchFlow_Demo"), 10.0f, 5.0f, 25.0f },
		{ TEXT("/BreakawayCore/Experiences/B_BW_Experience_BallMode.B_BW_Experience_BallMode_C"), TEXT("DA_BW_MatchFlow_Demo"), 10.0f, 5.0f, 25.0f },
		{ TEXT("/BreakawayCore/Experiences/B_BW_Experience_Dev.B_BW_Experience_Dev_C"), TEXT("DA_BW_MatchFlow_Dev"), 3.0f, 0.0f, 3.0f },
	};
	for (const FExperienceExpectation& Expectation : ExperienceExpectations)
	{
		UClass* ExperienceClass = LoadClass<ULyraExperienceDefinition>(nullptr, Expectation.ClassPath);
		if (!TestNotNull(*FString::Printf(TEXT("Experience class %s"), Expectation.ClassPath), ExperienceClass)) continue;
		const ULyraExperienceDefinition* Experience = GetDefault<ULyraExperienceDefinition>(ExperienceClass);
		const FBwayResolvedMatchFlowSettings ExperienceResolved = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(World, nullptr, Experience);
		const FString Label = ExperienceClass->GetName();
		TestEqual(*FString::Printf(TEXT("%s resolves %s"), *Label, Expectation.ExpectedConfig), ExperienceResolved.ConfigAssetName, FName(Expectation.ExpectedConfig));
		TestEqual(*FString::Printf(TEXT("%s base delay"), *Label), ExperienceResolved.BaseRespawnDelaySeconds, Expectation.Base);
		TestEqual(*FString::Printf(TEXT("%s increment"), *Label), ExperienceResolved.PerDeathRespawnIncrementSeconds, Expectation.Increment);
		TestEqual(*FString::Printf(TEXT("%s cap"), *Label), ExperienceResolved.MaxRespawnDelaySeconds, Expectation.Max);
	}

	// Apply Demo escalating progression rules to RoundManagementComponent
	UBwayMatchFlowLibrary::ApplyMatchRulesOnly(World, DemoResolved, Rounds, nullptr);
	TestEqual(TEXT("Applied BaseRespawnDelaySeconds"), Rounds->BaseRespawnDelaySeconds, 10.0f);
	TestEqual(TEXT("Applied PerDeathRespawnIncrementSeconds"), Rounds->PerDeathRespawnIncrementSeconds, 5.0f);
	TestEqual(TEXT("Applied MaxRespawnDelaySeconds"), Rounds->MaxRespawnDelaySeconds, 25.0f);
	TestTrue(TEXT("Applied bResetRespawnEscalationEachRound"), Rounds->bResetRespawnEscalationEachRound);

	// 2. Spawn controllers and PlayerStates on Teams 0 and 1 so single deaths do not eliminate Team 0
	APlayerController* VictimController = World->SpawnActor<APlayerController>();
	ABwayPlayerState* VictimPS = World->SpawnActor<ABwayPlayerState>();
	APlayerController* TeammateController = World->SpawnActor<APlayerController>();
	ABwayPlayerState* TeammatePS = World->SpawnActor<ABwayPlayerState>();
	APlayerController* EnemyController = World->SpawnActor<APlayerController>();
	ABwayPlayerState* EnemyPS = World->SpawnActor<ABwayPlayerState>();
	if (!TestNotNull(TEXT("VictimController"), VictimController) || !TestNotNull(TEXT("VictimPS"), VictimPS)
		|| !TestNotNull(TEXT("TeammateController"), TeammateController) || !TestNotNull(TEXT("TeammatePS"), TeammatePS)
		|| !TestNotNull(TEXT("EnemyController"), EnemyController) || !TestNotNull(TEXT("EnemyPS"), EnemyPS))
	{
		return false;
	}

	auto InitPlayer = [&](APlayerController* PC, ABwayPlayerState* PS, int32 TeamIndex)
	{
		PC->PlayerState = PS;
		PS->SetOwner(PC);
		GS->PlayerArray.AddUnique(PS);
		if (!PS->IsActorInitialized()) static_cast<AActor*>(PS)->PostInitializeComponents();
		if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
		{
			ASC->InitializeComponent();
			if (!ASC->GetSet<ULyraHealthSet>())
			{
				ASC->AddAttributeSetSubobject(NewObject<ULyraHealthSet>(PS));
			}
			UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
		}
		GS->AddPlayerToTeam(PS, TeamIndex);
	};
	InitPlayer(VictimController, VictimPS, 0);
	InitPlayer(TeammateController, TeammatePS, 0);
	InitPlayer(EnemyController, EnemyPS, 1);

	ULyraAbilitySystemComponent* VictimASC = VictimPS->GetLyraAbilitySystemComponent();
	const UBwayHeroAttributeSet* VictimHero = VictimASC ? VictimASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
	UBwayUpgradeComponent* VictimShop = VictimPS->UpgradeComponent;
	if (!TestNotNull(TEXT("VictimASC"), VictimASC) || !TestNotNull(TEXT("VictimHero"), VictimHero)
		|| !TestNotNull(TEXT("VictimShop"), VictimShop))
	{
		return false;
	}
	ON_SCOPE_EXIT { Rounds->ClearRespawnTimers(); VictimASC->GameplayEffectApplicationQueries.Reset(); };

	VictimASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 500.0f);
	UBwayUpgradeCatalog* Catalog = NewObject<UBwayUpgradeCatalog>(VictimPS);
	FBwayUpgradeDefinition& Entry = Catalog->Upgrades.AddDefaulted_GetRef();
	Entry.Id = TEXT("RespawnTimeReduction");
	Entry.EffectClass = UBwayUpgradeEffect_RespawnTimeReduction::StaticClass();
	Entry.Ranks.Add({20, 0.2f});
	Entry.Ranks.Add({30, 0.4f});
	TestTrue(TEXT("Configured RespawnTimeReduction catalog"), VictimShop->ConfigureCatalog(Catalog));

	auto SpawnLivingPawnFor = [&](APlayerController* PC, ABwayPlayerState* PS) -> ABwayCharacterWithAbilities*
	{
		if (APawn* Existing = PC->GetPawn())
		{
			if (ULyraHealthComponent* OldHealth = ULyraHealthComponent::FindHealthComponent(Existing))
			{
				OldHealth->UninitializeFromAbilitySystem();
			}
			PC->UnPossess();
			Existing->Destroy();
		}
		ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent();
		ASC->RemoveLooseGameplayTag(LyraGameplayTags::Status_Death);
		ASC->RemoveLooseGameplayTag(LyraGameplayTags::Status_Death_Dying);
		ASC->RemoveLooseGameplayTag(LyraGameplayTags::Status_Death_Dead);
		ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 100.0f);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABwayCharacterWithAbilities* Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(SpawnParams);
		if (!Pawn) return nullptr;
		PC->Possess(Pawn);
		Pawn->SetPlayerState(PS);
		ASC->InitAbilityActorInfo(PS, Pawn);
		if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn))
		{
			Health->InitializeWithAbilitySystem(ASC);
		}
		return Pawn;
	};

	SpawnLivingPawnFor(TeammateController, TeammatePS);
	SpawnLivingPawnFor(EnemyController, EnemyPS);
	GS->UpdateTeamAliveCount(0, 2);
	GS->UpdateTeamAliveCount(1, 1);

	Rounds->CurrentMatchPhase = EBwayMatchPhase::Playing;
	Rounds->CurrentRoundState = ERoundState::RoundActive;
	Rounds->CurrentRoundNumber = 1;

	auto TriggerVictimDeath = [&]() -> bool
	{
		ABwayCharacterWithAbilities* VictimPawn = SpawnLivingPawnFor(VictimController, VictimPS);
		if (!VictimPawn) return false;
		GS->UpdateTeamAliveCount(0, 2);
		ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(VictimPawn);
		if (!Health) return false;
		VictimASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
		VictimASC->AddLooseGameplayTag(LyraGameplayTags::Status_Death);
		Health->StartDeath();
		Rounds->OnPlayerDied(VictimController, EnemyController);
		return Rounds->RespawnTimers.Contains(VictimController);
	};

	auto GetPendingTimerRemaining = [&]() -> float
	{
		const auto* Pending = Rounds->RespawnTimers.Find(VictimController);
		return Pending ? World->GetTimerManager().GetTimerRemaining(Pending->Timer) : -1.0f;
	};

	// 3. Verify 4 sequential deaths within Round 1 escalate: 10s -> 15s -> 20s -> 25s (cap)
	const float ExpectedRound1Delays[] = { 10.0f, 15.0f, 20.0f, 25.0f };
	for (int32 DeathIndex = 0; DeathIndex < 4; ++DeathIndex)
	{
		TestTrue(*FString::Printf(TEXT("Round 1 Death %d scheduled"), DeathIndex + 1), TriggerVictimDeath());
		const auto* Pending = Rounds->RespawnTimers.Find(VictimController);
		if (!TestNotNull(TEXT("Pending respawn entry"), Pending)) return false;

		const float ExpectedDelay = ExpectedRound1Delays[DeathIndex];
		TestEqual(*FString::Printf(TEXT("Round 1 Death %d DeathsInCurrentRound"), DeathIndex + 1),
			Rounds->GetPlayerDeathsInCurrentRound(VictimPS), DeathIndex + 1);
		TestEqual(*FString::Printf(TEXT("Round 1 Death %d PlayerState.DeathsInCurrentRound"), DeathIndex + 1),
			VictimPS->GetDeathsInCurrentRound(), DeathIndex + 1);
		TestTrue(*FString::Printf(TEXT("Round 1 Death %d BaseDelay=%.1fs"), DeathIndex + 1, ExpectedDelay),
			FMath::IsNearlyEqual(Pending->BaseDelay, ExpectedDelay, 0.001f));
		TestTrue(*FString::Printf(TEXT("Round 1 Death %d Remaining=%.1fs"), DeathIndex + 1, ExpectedDelay),
			FMath::IsNearlyEqual(GetPendingTimerRemaining(), ExpectedDelay, 0.001f));
		TestTrue(*FString::Printf(TEXT("Round 1 Death %d Replicated Schedule matches"), DeathIndex + 1),
			FMath::IsNearlyEqual(VictimPS->GetRespawnSchedule().ServerEndTime - GS->GetServerWorldTimeSeconds(), static_cast<double>(ExpectedDelay), 0.01));
	}

	// Verify 5th death remains capped at MaxRespawnDelaySeconds (25.0s)
	TestTrue(TEXT("Round 1 Death 5 scheduled"), TriggerVictimDeath());
	TestEqual(TEXT("Round 1 Death 5 DeathsInCurrentRound == 5"), Rounds->GetPlayerDeathsInCurrentRound(VictimPS), 5);
	TestTrue(TEXT("Round 1 Death 5 capped at 25.0s"), FMath::IsNearlyEqual(GetPendingTimerRemaining(), 25.0f, 0.001f));

	// 4. Verify live pending-death timer shortening on an escalated death (25.0s base delay)
	{
		auto* Pending = Rounds->RespawnTimers.Find(VictimController);
		Pending->StartTime -= 5.0; // Simulate 5.0s elapsed into the 25.0s death
		TestTrue(TEXT("Purchase Rank 1 (20% reduction) while dead"), VictimShop->TryPurchaseUpgrade(Entry.Id, 0));
		// EffectiveDelay = 25.0 * (1.0 - 0.2) = 20.0s; minus 5.0s elapsed = 15.0s remaining
		TestTrue(TEXT("Rank 1 shortens live escalated timer to 15.0s"), FMath::IsNearlyEqual(GetPendingTimerRemaining(), 15.0f, 0.01f));

		TestTrue(TEXT("Purchase Rank 2 (40% reduction) while dead"), VictimShop->TryPurchaseUpgrade(Entry.Id, 1));
		// EffectiveDelay = 25.0 * (1.0 - 0.4) = 15.0s; minus 5.0s elapsed = 10.0s remaining
		TestTrue(TEXT("Rank 2 shortens live escalated timer to 10.0s"), FMath::IsNearlyEqual(GetPendingTimerRemaining(), 10.0f, 0.01f));
	}

	// 5. Verify round-boundary reset via StartNewRound() resets within-round death count to 0 while preserving RespawnTimeReduction
	Rounds->StartNewRound();
	TestEqual(TEXT("StartNewRound advances to Round 2"), Rounds->GetCurrentRoundNumber(), 2);
	TestEqual(TEXT("StartNewRound resets component round death count to 0"), Rounds->GetPlayerDeathsInCurrentRound(VictimPS), 0);
	TestEqual(TEXT("StartNewRound resets PlayerState DeathsInCurrentRound to 0"), VictimPS->GetDeathsInCurrentRound(), 0);
	TestEqual(TEXT("RespawnTimeReduction (0.40) persists into Round 2"), VictimHero->GetRespawnTimeReduction(), 0.4f);

	// First death in Round 2 uses 10.0s base delay composed with 0.40 reduction -> 6.0s effective delay
	TestTrue(TEXT("Round 2 Death 1 scheduled"), TriggerVictimDeath());
	{
		const auto* Pending = Rounds->RespawnTimers.Find(VictimController);
		if (!TestNotNull(TEXT("Round 2 Death 1 pending entry"), Pending)) return false;
		TestEqual(TEXT("Round 2 Death 1 count is 1"), Rounds->GetPlayerDeathsInCurrentRound(VictimPS), 1);
		TestTrue(TEXT("Round 2 Death 1 BaseDelay reset to 10.0s"), FMath::IsNearlyEqual(Pending->BaseDelay, 10.0f, 0.001f));
		TestTrue(TEXT("Round 2 Death 1 EffectiveDelay is 10.0 * (1 - 0.4) = 6.0s"), FMath::IsNearlyEqual(GetPendingTimerRemaining(), 6.0f, 0.01f));
	}

	// Verify ResetForNextMatch unconditionally clears round death counts and timers
	Rounds->bResetRespawnEscalationEachRound = false;
	Rounds->StartNewRound();
	TestEqual(TEXT("With bResetRespawnEscalationEachRound=false, round start preserves count"), Rounds->GetPlayerDeathsInCurrentRound(VictimPS), 1);
	Rounds->ResetForNextMatch();
	TestEqual(TEXT("ResetForNextMatch unconditionally resets round death count to 0"), Rounds->GetPlayerDeathsInCurrentRound(VictimPS), 0);
	TestEqual(TEXT("ResetForNextMatch resets PlayerState DeathsInCurrentRound to 0"), VictimPS->GetDeathsInCurrentRound(), 0);
	TestTrue(TEXT("ResetForNextMatch clears RespawnTimers"), Rounds->RespawnTimers.IsEmpty());

	// 6. Verify Legacy ABreakawayGameMode debt cleanup via reflection
	const UClass* GameModeClass = ABreakawayGameMode::StaticClass();
	const TCHAR* RemovedFunctions[] = {
		TEXT("StartRound"), TEXT("EndRound"), TEXT("ScoreGoal"), TEXT("SpawnRelic"), TEXT("ResetArenaForNextRound")
	};
	for (const TCHAR* FuncName : RemovedFunctions)
	{
		TestNull(*FString::Printf(TEXT("ABreakawayGameMode must not declare deprecated function %s"), FuncName),
			GameModeClass->FindFunctionByName(FName(FuncName), EIncludeSuperFlag::ExcludeSuper));
	}
	const TCHAR* RemovedProperties[] = {
		TEXT("TeamScores"), TEXT("RoundState")
	};
	for (const TCHAR* PropName : RemovedProperties)
	{
		TestNull(*FString::Printf(TEXT("ABreakawayGameMode must not declare deprecated property %s"), PropName),
			GameModeClass->FindPropertyByName(FName(PropName)));
	}
	TestNotNull(TEXT("Canonical RoundManagementComponent on GameState"), GS->FindComponentByClass<UBwayRoundManagementComponent>());
	TestNotNull(TEXT("Canonical ScoringComponent on GameState"), GS->FindComponentByClass<UBwayScoringComponent>());
	TestNotNull(TEXT("Canonical RelicManagerComponent on GameState"), GS->FindComponentByClass<UBwayRelicManagerComponent>());

	// 6b. Serialized Blueprint subclasses still load against the slimmed native GameMode and compile cleanly,
	// so no graph retains a call to a removed legacy authority function or property.
	const TCHAR* GameModeBlueprintPaths[] = {
		TEXT("/BreakawayCore/GameModes/B_BWayGameMode.B_BWayGameMode_C"),
		TEXT("/BreakawayCore/GameModes/B_BWayGameMode_Default.B_BWayGameMode_Default_C"),
	};
	for (const TCHAR* ClassPath : GameModeBlueprintPaths)
	{
		UClass* BlueprintClass = LoadClass<AGameModeBase>(nullptr, ClassPath);
		if (!TestNotNull(*FString::Printf(TEXT("GameMode Blueprint class %s"), ClassPath), BlueprintClass)) continue;
		TestTrue(*FString::Printf(TEXT("%s derives from ABreakawayGameMode"), *BlueprintClass->GetName()),
			BlueprintClass->IsChildOf(ABreakawayGameMode::StaticClass()));
		for (const TCHAR* FuncName : RemovedFunctions)
		{
			TestNull(*FString::Printf(TEXT("%s does not resurrect %s"), *BlueprintClass->GetName(), FuncName),
				BlueprintClass->FindFunctionByName(FName(FuncName), EIncludeSuperFlag::IncludeSuper));
		}
#if WITH_EDITORONLY_DATA
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
		{
			TestNotEqual(*FString::Printf(TEXT("%s compiles without errors"), *Blueprint->GetName()),
				static_cast<int32>(Blueprint->Status), static_cast<int32>(EBlueprintStatus::BS_Error));
		}
#endif
	}

	// 7. Dorado VSM: the large arena meshes that overflowed the non-Nanite marking queue are Nanite with a
	// Nanite-flagged material, and the shadow configuration is not degraded below Lyra/engine defaults.
	const TCHAR* DoradoNaniteMeshes[] = {
		TEXT("/BreakawayCore/Environments/Levels/BW_Dorado/el_dorado_fbx_Scene_rel_eldorado_main_floor_C.el_dorado_fbx_Scene_rel_eldorado_main_floor_C"),
		TEXT("/BreakawayCore/Environments/Levels/BW_Dorado/el_dorado_fbx_Scene_rel_eldorado_main_floor_D.el_dorado_fbx_Scene_rel_eldorado_main_floor_D"),
		TEXT("/BreakawayCore/Environments/Levels/BW_Dorado/el_dorado_fbx_Scene_rel_eldorado_flr_base_b.el_dorado_fbx_Scene_rel_eldorado_flr_base_b"),
		TEXT("/BreakawayCore/Environments/Levels/BW_Dorado/el_dorado_fbx_Scene_rel_eldorado_flr_foot_c.el_dorado_fbx_Scene_rel_eldorado_flr_foot_c"),
		TEXT("/BreakawayCore/Environments/Levels/BW_Dorado/el_dorado_fbx_Scene_rel_el_ArenaStartAreaWall_B_001.el_dorado_fbx_Scene_rel_el_ArenaStartAreaWall_B_001"),
		TEXT("/BreakawayCore/Environments/Levels/BW_Dorado/el_dorado_fbx_Scene_Rel_El_Dorado_Relic_Snake_B.el_dorado_fbx_Scene_Rel_El_Dorado_Relic_Snake_B"),
	};
	for (const TCHAR* MeshPath : DoradoNaniteMeshes)
	{
		const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
		if (!TestNotNull(*FString::Printf(TEXT("Dorado mesh %s"), MeshPath), Mesh)) continue;
#if WITH_EDITOR
		// The authored Nanite flag is editor-only data; cooked builds carry the built Nanite resources instead.
		TestTrue(*FString::Printf(TEXT("%s is Nanite-enabled"), *Mesh->GetName()), Mesh->IsNaniteEnabled());
#else
		TestTrue(*FString::Printf(TEXT("%s has Nanite data"), *Mesh->GetName()), Mesh->HasValidNaniteData());
#endif
		for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
		{
			const UMaterial* Base = Slot.MaterialInterface ? Slot.MaterialInterface->GetMaterial() : nullptr;
			TestTrue(*FString::Printf(TEXT("%s material %s is used with Nanite"), *Mesh->GetName(), *GetNameSafe(Base)),
				Base && Base->GetUsageByFlag(MATUSAGE_Nanite));
		}
	}
	if (IConsoleVariable* FirstLevel = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Clipmap.FirstLevel")))
	{
		TestTrue(TEXT("VSM clipmap first level is not coarsened above the engine default (6)"), FirstLevel->GetInt() <= 6);
	}
	if (IConsoleVariable* LodBias = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional")))
	{
		TestTrue(TEXT("Directional VSM resolution bias is not degraded (<= 0)"), LodBias->GetFloat() <= 0.0f);
	}

	return true;
}
#endif
