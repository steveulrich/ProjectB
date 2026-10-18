#include "Misc/AutomationTest.h"

#include "Abilities/BwayGameplayAbility_RelicPass.h"
#include "Abilities/BwayGameplayAbility_RelicRequest.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BrainComponent.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Economy/BwayBaseZone.h"
#include "Economy/BwayEconomyLibrary.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "LyraGameplayTags.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicBallisticsAndInterceptionTest,
	"Breakaway.Relic.BallisticsAndInterception",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicBallisticsAndInterceptionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldSettings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(true).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &WorldSettings);
	if (!TestNotNull(TEXT("Ballistics fixture world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };

	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	if (!TestNotNull(TEXT("Ballistics GameState"), GS))
	{
		return false;
	}
	World->SetGameState(GS);

	FArrayProperty* TeamsProperty = FindFProperty<FArrayProperty>(ABwayGameState::StaticClass(), TEXT("Teams"));
	if (!TestNotNull(TEXT("Teams property on GameState"), TeamsProperty))
	{
		return false;
	}
	TArray<FTeamInfo>* Teams = TeamsProperty->ContainerPtrToValuePtr<TArray<FTeamInfo>>(GS);
	Teams->Add(FTeamInfo(0));
	Teams->Add(FTeamInfo(1));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn 4 participants: [0]=Team0 Carrier, [1]=Team0 Receiver, [2]=Team1 Defender/Interceptor, [3]=Team1 Attacker
	ABwayPlayerState* Players[4] = {};
	ULyraAbilitySystemComponent* ASCs[4] = {};
	ABwayCharacterWithAbilities* Pawns[4] = {};
	const int32 TeamIndices[4] = { 0, 0, 1, 1 };

	for (int32 Index = 0; Index < 4; ++Index)
	{
		Players[Index] = World->SpawnActor<ABwayPlayerState>(SpawnParams);
		if (!TestNotNull(TEXT("Participant PlayerState"), Players[Index]))
		{
			return false;
		}
		if (!Players[Index]->IsActorInitialized())
		{
			static_cast<AActor*>(Players[Index])->PostInitializeComponents();
		}
		if (!GS->PlayerArray.Contains(Players[Index]))
		{
			GS->AddPlayerState(Players[Index]);
		}
		GS->AddPlayerToTeam(Players[Index], TeamIndices[Index]);
		Players[Index]->SetGenericTeamId(FGenericTeamId(TeamIndices[Index] + 1));

		Pawns[Index] = World->SpawnActor<ABwayCharacterWithAbilities>(
			ABwayCharacterWithAbilities::StaticClass(),
			FTransform(FVector(Index * 600.0f, 0.0f, 300.0f)),
			SpawnParams);
		if (!TestNotNull(TEXT("Participant Pawn"), Pawns[Index]))
		{
			return false;
		}
		Pawns[Index]->SetPlayerState(Players[Index]);
		Pawns[Index]->SetGenericTeamId(FGenericTeamId(TeamIndices[Index] + 1));

		ASCs[Index] = Players[Index]->GetLyraAbilitySystemComponent();
		if (!TestNotNull(TEXT("Participant ASC"), ASCs[Index]))
		{
			return false;
		}
		ASCs[Index]->InitAbilityActorInfo(Players[Index], Pawns[Index]);
	}

	URelicSettings* Settings = NewObject<URelicSettings>(World);
	Settings->RelicMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));

	// 1. Audit URelicSettings defaults
	TestEqual(TEXT("Default MinThrowSpeed is 800"), Settings->MinThrowSpeed, 800.0f);
	TestEqual(TEXT("Default MaxThrowSpeed is 2000"), Settings->MaxThrowSpeed, 2000.0f);
	TestEqual(TEXT("Default MaxChargeTime is 1.5s"), Settings->MaxChargeTime, 1.5f);
	TestEqual(TEXT("Default PassSpeed is 1500"), Settings->PassSpeed, 1500.0f);
	TestEqual(TEXT("Default PickupCooldownAfterDrop is 0.5s"), Settings->PickupCooldownAfterDrop, 0.5f);
	TestEqual(TEXT("Default FumbleDamageThreshold is 50.0"), Settings->FumbleDamageThreshold, 50.0f);
	TestEqual(TEXT("Default FumbleImpulseStrength is 600.0"), Settings->FumbleImpulseStrength, 600.0f);
	TestEqual(TEXT("Uncharged throw speed (0.0s) == 800"), Settings->ComputeChargedThrowSpeed(0.0f), 800.0f);
	TestEqual(TEXT("Half-charged throw speed (0.75s) == 1400"), Settings->ComputeChargedThrowSpeed(0.75f), 1400.0f);
	TestEqual(TEXT("Full-charged throw speed (1.5s) == 2000"), Settings->ComputeChargedThrowSpeed(1.5f), 2000.0f);
	TestEqual(TEXT("Over-charged throw speed clamped (3.0s) == 2000"), Settings->ComputeChargedThrowSpeed(3.0f), 2000.0f);

	ARelicActor* Relic = World->SpawnActor<ARelicActor>(ARelicActor::StaticClass(), FTransform(FVector(0.0f, 0.0f, 300.0f)), SpawnParams);
	ULyraAbilitySet* CarrierSet = LoadObject<ULyraAbilitySet>(nullptr,
		TEXT("/BreakawayCore/GameModes/BallMode/Abilities/BallHandlingAbilities/AS_BW_BallAbilities.AS_BW_BallAbilities"));
	FObjectPropertyBase* LoadedSetProp = FindFProperty<FObjectPropertyBase>(ARelicActor::StaticClass(), TEXT("LoadedRelicAbilitySet"));
	if (!TestNotNull(TEXT("RelicActor"), Relic) || !TestNotNull(TEXT("CarrierSet"), CarrierSet) || !TestNotNull(TEXT("LoadedSetProp"), LoadedSetProp))
	{
		return false;
	}
	Relic->InitializeRelicData(Settings);
	LoadedSetProp->SetObjectPropertyValue_InContainer(Relic, CarrierSet);
	if (UBwayRelicManagerComponent* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>())
	{
		RelicMgr->SetActiveRelic(Relic);
	}

	auto CountRelicGrants = [Relic](const ULyraAbilitySystemComponent* ASC)
	{
		int32 Count = 0;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.SourceObject.Get() == Relic)
			{
				++Count;
			}
		}
		return Count;
	};

	// Determine expected grant count from CarrierSet + 1 native pass ability
	FLyraAbilitySet_GrantedHandles ProbeHandles;
	CarrierSet->GiveToAbilitySystem(ASCs[3], &ProbeHandles, Relic);
	const int32 ExpectedGrants = CountRelicGrants(ASCs[3]) + 1;
	ProbeHandles.TakeFromAbilitySystem(ASCs[3]);

	// 2. Verify Charged Ballistic Throws (MinThrowSpeed=800, MaxThrowSpeed=2000) & RMB Prepared Catch (State.Relic.PreparingCatch)
	Relic->OnPickedUp(Pawns[0]);
	TestEqual(TEXT("Player 0 carries Relic"), Relic->CurrentCarrier.Get(), Pawns[0]);
	TestTrue(TEXT("Player 0 has State.Relic.Carrying tag"), ASCs[0]->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_Carrying));

	const FVector UnchargedVel = Relic->ComputeChargedThrowVelocity(FVector::ForwardVector, 0.0f);
	const FVector MidChargedVel = Relic->ComputeChargedThrowVelocity(FVector::ForwardVector, 0.75f);
	const FVector MaxChargedVel = Relic->ComputeChargedThrowVelocity(FVector::ForwardVector, 1.5f);
	const FVector OverChargedVel = Relic->ComputeChargedThrowVelocity(FVector::ForwardVector, 3.0f);
	TestTrue(TEXT("Uncharged velocity magnitude is 800"), FMath::IsNearlyEqual(UnchargedVel.Size(), 800.0f, 0.1f));
	TestTrue(TEXT("Intermediate 0.75s velocity magnitude is 1400"), FMath::IsNearlyEqual(MidChargedVel.Size(), 1400.0f, 0.1f));
	TestTrue(TEXT("Max-charged velocity magnitude is 2000"), FMath::IsNearlyEqual(MaxChargedVel.Size(), 2000.0f, 0.1f));
	TestTrue(TEXT("Over-charged 3.0s velocity is capped at 2000"), FMath::IsNearlyEqual(OverChargedVel.Size(), 2000.0f, 0.1f));
	TestTrue(TEXT("Server_ThrowRelic_Validate accepts MaxThrowSpeed (2000)"), Relic->Server_ThrowRelic_Validate(MaxChargedVel));

	Relic->ThrowRelicCharged(1.5f, FVector::ForwardVector);
	TestEqual(TEXT("Charged throw transitions Relic to Thrown"), Relic->GetCurrentState(), ERelicState::Thrown);
	TestNull(TEXT("Charged throw clears CurrentCarrier"), Relic->CurrentCarrier.Get());
	TestFalse(TEXT("Thrown Relic is not flagged as pass-in-flight"), Relic->bIsPassInFlight);

	// Teammate (Pawns[1]) without Prepared Catch cannot auto-catch the thrown Relic
	TestFalse(TEXT("Teammate without State.Relic.PreparingCatch cannot catch"), Relic->CanBePickedUpBy(Pawns[1]));

	// Indicator tag Gameplay.Relic.Requesting alone (after RMB release) does NOT grant catch eligibility
	if (Settings->RequestingTag.IsValid())
	{
		ASCs[1]->SetLooseGameplayTagCount(Settings->RequestingTag, 1);
		TestFalse(TEXT("Call-for-relic indicator tag alone without State.Relic.PreparingCatch does not allow catch"), Relic->CanBePickedUpBy(Pawns[1]));
		ASCs[1]->SetLooseGameplayTagCount(Settings->RequestingTag, 0);
	}

	// Teammate activates RMB Prepared Catch (State.Relic.PreparingCatch)
	ASCs[1]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
	TestTrue(TEXT("Teammate holding State.Relic.PreparingCatch satisfies CanBePickedUpBy"), Relic->CanBePickedUpBy(Pawns[1]));

	// High-velocity overlap reception on InteractionSphere secures Relic without bounce-off or duplicate grants
	FHitResult DummySweep;
	Relic->OnInteractionSphereOverlap( nullptr, Pawns[1], nullptr, 0, false, DummySweep);
	TestEqual(TEXT("Teammate holding Prepared Catch secures high-velocity throw on overlap"), Relic->CurrentCarrier.Get(), Pawns[1]);
	TestEqual(TEXT("Relic transitions to Carried on Prepared Catch reception"), Relic->GetCurrentState(), ERelicState::Carried);
	TestFalse(TEXT("Acquiring Relic clears State.Relic.PreparingCatch on catcher"), ASCs[1]->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
	TestEqual(TEXT("Prepared Catch grants exact ability set once"), CountRelicGrants(ASCs[1]), ExpectedGrants);
	// Repeated overlap and RepNotify cannot duplicate ability grants
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[1], nullptr, 0, false, DummySweep);
	Relic->OnRep_CurrentCarrier();
	TestEqual(TEXT("Repeated overlap/RepNotify does not duplicate grants"), CountRelicGrants(ASCs[1]), ExpectedGrants);

	// 3. Verify PickupCooldownAfterDrop (0.5s) prevents immediate self-reacquisition by dropping/fumbling carrier
	World->TimeSeconds = 10.0f;
	Relic->OnDropped();
	TestNull(TEXT("Relic dropped by Pawns[1]"), Relic->CurrentCarrier.Get());
	TestEqual(TEXT("Relic state is Dropped"), Relic->GetCurrentState(), ERelicState::Dropped);
	TestTrue(TEXT("Drop cooldown active for dropping carrier at t=10.0s"), Relic->IsDropPickupCooldownActiveFor(Pawns[1]));
	ASCs[1]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
	TestFalse(TEXT("Dropping carrier blocked by PickupCooldownAfterDrop at +0.1s"),
		(World->TimeSeconds = 10.1f, Relic->CanBePickedUpBy(Pawns[1])));
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[1], nullptr, 0, false, DummySweep);
	TestNull(TEXT("Dropping carrier overlap rejected during 0.5s cooldown"), Relic->CurrentCarrier.Get());

	// Advance time past PickupCooldownAfterDrop (0.5s -> t=10.6s) and verify reacquisition is allowed
	World->TimeSeconds = 10.6f;
	TestFalse(TEXT("Drop cooldown expired at +0.6s"), Relic->IsDropPickupCooldownActiveFor(Pawns[1]));
	TestTrue(TEXT("Carrier can re-acquire Relic after 0.5s cooldown"), Relic->CanBePickedUpBy(Pawns[1]));
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[1], nullptr, 0, false, DummySweep);
	TestEqual(TEXT("Carrier re-acquired Relic after cooldown"), Relic->CurrentCarrier.Get(), Pawns[1]);
	ASCs[1]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);

	// 4. Verify Directed Pass (PassSpeed=1500), Negative Controls, Missed-Pass Landing, & Mid-Air Pass Interception
	const FVector PassVel = Relic->ComputeDirectedPassVelocity(Pawns[0]->GetActorLocation());
	TestTrue(TEXT("Directed pass speed is PassSpeed (1500)"), FMath::IsNearlyEqual(PassVel.Size(), 1500.0f, 0.1f));
	TestTrue(TEXT("Server_PassRelic_Validate accepts PassSpeed (1500)"), Relic->Server_PassRelic_Validate(PassVel));

	// 4a. Negative control: Teammate catching a pass does NOT count as an interception
	Relic->Server_PassRelic_Implementation(PassVel);
	TestEqual(TEXT("Pass enters BeingPassed state"), Relic->GetCurrentState(), ERelicState::BeingPassed);
	TestTrue(TEXT("bIsPassInFlight is true during pass flight"), Relic->bIsPassInFlight);
	TestEqual(TEXT("PassingTeamIndex records Team 0"), Relic->PassingTeamIndex, 0);
	ASCs[0]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[0], nullptr, 0, false, DummySweep);
	ASCs[0]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);
	TestEqual(TEXT("Teammate caught pass"), Relic->CurrentCarrier.Get(), Pawns[0]);
	TestEqual(TEXT("Teammate pass reception does not increment Interceptions"), Players[0]->GetInterceptions(), 0);

	// 4b. Negative control: Ordinary throw caught by opponent does NOT count as an interception
	Relic->ThrowRelicCharged(0.75f, FVector::ForwardVector);
	TestEqual(TEXT("Ordinary throw state is Thrown"), Relic->GetCurrentState(), ERelicState::Thrown);
	TestFalse(TEXT("Ordinary throw does not set bIsPassInFlight"), Relic->bIsPassInFlight);
	ASCs[2]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[2], nullptr, 0, false, DummySweep);
	ASCs[2]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);
	TestEqual(TEXT("Opponent caught ordinary throw"), Relic->CurrentCarrier.Get(), Pawns[2]);
	TestEqual(TEXT("Opponent catching an ordinary throw does not increment Interceptions"), Players[2]->GetInterceptions(), 0);

	// 4c. Negative control: Missed pass landing clears pass-in-flight and PassingTeamIndex; later opponent pickup is not an interception
	Relic->OnPickedUp(Pawns[1]);
	Relic->OnDropped();
	World->TimeSeconds = 12.0f;
	Relic->OnPickedUp(Pawns[1]);
	Relic->Server_PassRelic_Implementation(PassVel);
	TestTrue(TEXT("Pass is in flight before landing"), Relic->bIsPassInFlight && Relic->PassingTeamIndex == 0);
	AActor* FloorActor = World->SpawnActor<AActor>();
	UBoxComponent* FloorBox = NewObject<UBoxComponent>(FloorActor);
	FloorActor->AddInstanceComponent(FloorBox);
	FloorActor->SetRootComponent(FloorBox);
	FloorBox->RegisterComponent();
	FHitResult LandingHit;
	LandingHit.bBlockingHit = true;
	LandingHit.ImpactPoint = Relic->GetActorLocation();
	LandingHit.ImpactNormal = FVector::UpVector;
	Relic->HandleRelicMeshHit(Cast<UPrimitiveComponent>(Relic->GetRootComponent()), FloorActor, FloorBox, FVector::UpVector * 100.0f, LandingHit);
	TestEqual(TEXT("Missed pass landing transitions Relic to Dropped"), Relic->GetCurrentState(), ERelicState::Dropped);
	TestFalse(TEXT("Missed pass landing clears bIsPassInFlight"), Relic->bIsPassInFlight);
	TestEqual(TEXT("Missed pass landing clears PassingTeamIndex"), Relic->PassingTeamIndex, INDEX_NONE);
	TestFalse(TEXT("Opposing defender cannot auto-intercept a landed pass without Prepared Catch"), Relic->CanBePickedUpBy(Pawns[3]));
	ASCs[3]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[3], nullptr, 0, false, DummySweep);
	ASCs[3]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);
	TestEqual(TEXT("Opponent picked up landed pass from ground"), Relic->CurrentCarrier.Get(), Pawns[3]);
	TestEqual(TEXT("Ground pickup after missed pass landing does not increment Interceptions"), Players[3]->GetInterceptions(), 0);

	// 4d. Negative control: Round reset during pass flight clears pass state without awarding interception
	Relic->Server_PassRelic_Implementation(PassVel);
	TestTrue(TEXT("Pass in flight from Team 1"), Relic->bIsPassInFlight && Relic->PassingTeamIndex == 1);
	Relic->BeginResettingState();
	Relic->OnDropped();
	TestFalse(TEXT("Round reset clears bIsPassInFlight"), Relic->bIsPassInFlight);
	TestEqual(TEXT("Round reset clears PassingTeamIndex"), Relic->PassingTeamIndex, INDEX_NONE);
	ASCs[1]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[1], nullptr, 0, false, DummySweep);
	ASCs[1]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);
	TestEqual(TEXT("Player 1 picked up reset Relic"), Relic->CurrentCarrier.Get(), Pawns[1]);
	TestEqual(TEXT("Pickup after round reset does not increment Interceptions"), Players[1]->GetInterceptions(), 0);

	// 4e. Positive mid-air pass interception by opposing defender (with duplicate-overlap guard)
	Relic->Server_PassRelic_Implementation(PassVel);
	TestEqual(TEXT("Pass enters BeingPassed state"), Relic->GetCurrentState(), ERelicState::BeingPassed);
	TestTrue(TEXT("bIsPassInFlight is true during pass flight"), Relic->bIsPassInFlight);
	TestEqual(TEXT("PassingTeamIndex records Team 0"), Relic->PassingTeamIndex, 0);

	// Also verify ERelicState::Passed alias preserves pass-in-flight state
	Relic->CurrentState = ERelicState::Passed;
	TestTrue(TEXT("Opposing defender (Team 1) can intercept pass in flight"), Relic->CanBePickedUpBy(Pawns[2]));

	const int32 InterceptionsBefore = Players[2]->GetInterceptions();
	ASCs[2]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[2], nullptr, 0, false, DummySweep);
	Relic->OnInteractionSphereOverlap(nullptr, Pawns[2], nullptr, 0, false, DummySweep); // Duplicate overlap negative control
	ASCs[2]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);

	TestEqual(TEXT("Defender awarded Relic possession on mid-air interception"), Relic->CurrentCarrier.Get(), Pawns[2]);
	TestEqual(TEXT("Relic state is Carried after interception"), Relic->GetCurrentState(), ERelicState::Carried);
	TestFalse(TEXT("bIsPassInFlight cleared on interception"), Relic->bIsPassInFlight);
	TestEqual(TEXT("PassingTeamIndex cleared on interception"), Relic->PassingTeamIndex, INDEX_NONE);
	TestEqual(TEXT("LastPossessingTeam updated to Defender Team 1"), Relic->LastPossessingTeam, 1);
	TestEqual(TEXT("Defender ABwayPlayerState::Interceptions incremented by exactly +1"), Players[2]->GetInterceptions(), InterceptionsBefore + 1);
	TestEqual(TEXT("Defender MatchStatsSnapshot includes Interception"), Players[2]->GetMatchStatsSnapshot().Interceptions, InterceptionsBefore + 1);

	const FBwayMatchSummary SummaryAfterIntercept = UBwayMatchStatsLibrary::BuildPostMatchSummaryData(World, 1, 1, 5.0f);
	TestEqual(TEXT("FBwayMatchSummary Team 1 aggregate includes Interception"), SummaryAfterIntercept.Team1MatchStats.Stats.Interceptions, 1);

	// 5. Verify Combat Fumble Thresholds (FumbleDamageThreshold=50.0) & Hard CC Forced Fumbles (State.Status.Silenced, State.Status.Jailed)
	// 5a. Negative control: Self-damage and teammate damage never credit ForcedFumbles to victim or teammate
	TestTrue(TEXT("Teammate damage >= 50 forces drop but never credits teammate ForcedFumbles"), Relic->HandleCarrierDamage(60.0f, Pawns[3]));
	TestEqual(TEXT("Teammate ForcedFumbles remains 0"), Players[3]->GetForcedFumbles(), 0);
	TestEqual(TEXT("Victim ForcedFumbles remains 0"), Players[2]->GetForcedFumbles(), 0);
	World->TimeSeconds = 18.0f;
	Relic->OnPickedUp(Pawns[2]);

	// 5b. Negative control: Chip damage resets when carrier passes/throws and re-acquires, preventing stale attacker credit
	TestFalse(TEXT("20.0 chip damage from Player 0 does not fumble"), Relic->HandleCarrierDamage(20.0f, Pawns[0]));
	Relic->OnDropped();
	World->TimeSeconds = 19.0f;
	Relic->OnPickedUp(Pawns[2]);
	TestEqual(TEXT("AccumulatedCarrierDamage reset to 0 on re-pickup"), Relic->GetAccumulatedCarrierDamage(), 0.0f);

	// 5c. Sub-threshold hit (30.0 < 50.0) does NOT force a fumble; cumulative hit (30.0 + 25.0 = 55.0 >= 50.0) forces immediate fumble
	const int32 Attacker0FumblesBefore = Players[0]->GetForcedFumbles();
	const int32 Carrier2FumblesBefore = Players[2]->GetForcedFumbles();
	TestFalse(TEXT("Sub-threshold 30.0 damage does not force fumble"), Relic->HandleCarrierDamage(30.0f, Pawns[0]));
	TestEqual(TEXT("Carrier retains Relic after 30.0 sub-threshold damage"), Relic->CurrentCarrier.Get(), Pawns[2]);
	TestEqual(TEXT("AccumulatedCarrierDamage tracks 30.0"), Relic->GetAccumulatedCarrierDamage(), 30.0f);
	TestEqual(TEXT("Attacker ForcedFumbles unchanged below threshold"), Players[0]->GetForcedFumbles(), Attacker0FumblesBefore);

	World->TimeSeconds = 20.0f;
	TestTrue(TEXT("Cumulative +25.0 damage (55.0 >= 50.0) forces immediate fumble"), Relic->HandleCarrierDamage(25.0f, Pawns[0]));
	TestNull(TEXT("Cumulative threshold damage clears CurrentCarrier"), Relic->CurrentCarrier.Get());
	TestEqual(TEXT("Cumulative threshold damage puts Relic in Dropped state"), Relic->GetCurrentState(), ERelicState::Dropped);
	TestEqual(TEXT("Attacker ForcedFumbles incremented by +1 on cumulative threshold fumble"), Players[0]->GetForcedFumbles(), Attacker0FumblesBefore + 1);
	TestEqual(TEXT("Losing possession does not award the carrier a forced fumble"), Players[2]->GetForcedFumbles(), Carrier2FumblesBefore);

	// Verify immediate self-reacquisition by fumbled carrier (Pawns[2]) is blocked by PickupCooldownAfterDrop (0.5s)
	World->TimeSeconds = 20.2f;
	ASCs[2]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
	TestFalse(TEXT("Fumbling carrier blocked from immediate self-reacquisition"), Relic->CanBePickedUpBy(Pawns[2]));
	ASCs[2]->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);

	// 5d. Single hit >= 50.0 damage forces immediate fumble
	Relic->OnPickedUp(Pawns[1]);
	TestEqual(TEXT("Player 1 picked up Relic"), Relic->CurrentCarrier.Get(), Pawns[1]);
	const int32 Attacker3FumblesBefore = Players[3]->GetForcedFumbles();
	const int32 Carrier1FumblesBefore = Players[1]->GetForcedFumbles();
	TestTrue(TEXT("Single 55.0 hit (>= 50.0) forces immediate fumble"), Relic->HandleCarrierDamage(55.0f, Pawns[3]));
	TestNull(TEXT("Single hit fumble clears CurrentCarrier"), Relic->CurrentCarrier.Get());
	TestEqual(TEXT("Attacker 3 ForcedFumbles incremented by +1"), Players[3]->GetForcedFumbles(), Attacker3FumblesBefore + 1);
	TestEqual(TEXT("Single-hit fumble does not credit its victim"), Players[1]->GetForcedFumbles(), Carrier1FumblesBefore);

	// 5e. Hard CC tag State.Status.Silenced forces immediate fumble
	World->TimeSeconds = 22.0f;
	Relic->OnPickedUp(Pawns[1]);
	TestEqual(TEXT("Player 1 picked up Relic for Silence test"), Relic->CurrentCarrier.Get(), Pawns[1]);
	Relic->HandleCarrierDamage(10.0f, Pawns[3]); // Attribute attacker context without reaching 50.0 damage
	ASCs[1]->SetLooseGameplayTagCount(BwayGameplayTags::State_Status_Silenced, 1);
	TestNull(TEXT("State.Status.Silenced forces immediate Relic fumble"), Relic->CurrentCarrier.Get());
	TestEqual(TEXT("Relic state is Dropped after Silence fumble"), Relic->GetCurrentState(), ERelicState::Dropped);
	TestEqual(TEXT("Attacker 3 ForcedFumbles incremented by +1 on Silence CC fumble"), Players[3]->GetForcedFumbles(), Attacker3FumblesBefore + 2);
	ASCs[1]->SetLooseGameplayTagCount(BwayGameplayTags::State_Status_Silenced, 0);

	// 5f. Hard CC tag State.Status.Jailed forces immediate fumble and prioritizes CC source over earlier chip damage
	World->TimeSeconds = 25.0f;
	Relic->OnPickedUp(Pawns[2]);
	TestEqual(TEXT("Player 2 picked up Relic for Jail test"), Relic->CurrentCarrier.Get(), Pawns[2]);
	Relic->HandleCarrierDamage(5.0f, Pawns[0]); // Attribute attacker context without reaching 50.0 damage
	UGameplayEffect* JailEffect = NewObject<UGameplayEffect>(World);
	JailEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayEffectContextHandle JailContext = ASCs[1]->MakeEffectContext();
	JailContext.AddInstigator(Pawns[1], Pawns[1]);
	FGameplayEffectSpec JailSpec(JailEffect, JailContext, 1.0f);
	JailSpec.DynamicGrantedTags.AddTag(BwayGameplayTags::State_Status_Jailed);
	const FActiveGameplayEffectHandle JailHandle = ASCs[2]->ApplyGameplayEffectSpecToSelf(JailSpec);
	TestNull(TEXT("State.Status.Jailed forces immediate Relic fumble"), Relic->CurrentCarrier.Get());
	TestEqual(TEXT("Relic state is Dropped after Jail fumble"), Relic->GetCurrentState(), ERelicState::Dropped);
	TestEqual(TEXT("Jail effect instigator receives the forced fumble"), Players[1]->GetForcedFumbles(), Carrier1FumblesBefore + 1);
	TestEqual(TEXT("Earlier chip-damage attacker does not receive Jail credit"), Players[0]->GetForcedFumbles(), Attacker0FumblesBefore + 1);
	TestEqual(TEXT("Jailed victim does not receive a forced fumble"), Players[2]->GetForcedFumbles(), Carrier2FumblesBefore);
	ASCs[2]->RemoveActiveGameplayEffect(JailHandle);

	const FBwayMatchSummary FinalSummary = UBwayMatchStatsLibrary::BuildPostMatchSummaryData(World, 0, 1, 5.0f);
	TestEqual(TEXT("FBwayMatchSummary Team 0 records its two forced fumbles"), FinalSummary.Team0MatchStats.Stats.ForcedFumbles, 2);
	TestEqual(TEXT("FBwayMatchSummary Team 1 records its two forced fumbles"), FinalSummary.Team1MatchStats.Stats.ForcedFumbles, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayEconomyBaseHealingShopWindowTest,
	"Breakaway.Economy.BaseHealingShopWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayEconomyBaseHealingShopWindowTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
	GameInstance->InitializeStandalone();
	UWorld* World = GameInstance->GetWorld();
	if (!TestNotNull(TEXT("BaseHealing fixture world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
		if (GameInstance)
		{
			GameInstance->Shutdown();
		}
		if (World && GEngine)
		{
			GEngine->DestroyWorldContext(World);
		}
	};
	World->InitializeActorsForPlay(FURL());

	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	if (!TestNotNull(TEXT("BaseHealing GameState"), GS))
	{
		return false;
	}
	World->SetGameState(GS);

	FEnumProperty* PhaseProperty = FindFProperty<FEnumProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentMatchPhase"));
	FArrayProperty* TeamsProperty = FindFProperty<FArrayProperty>(ABwayGameState::StaticClass(), TEXT("Teams"));
	FObjectProperty* PawnStateProperty = FindFProperty<FObjectProperty>(APawn::StaticClass(), TEXT("PlayerState"));
	if (!TestNotNull(TEXT("PhaseProperty"), PhaseProperty) ||
		!TestNotNull(TEXT("TeamsProperty"), TeamsProperty) ||
		!TestNotNull(TEXT("PawnStateProperty"), PawnStateProperty))
	{
		return false;
	}

	auto SetPhase = [GS, PhaseProperty](EBwayMatchPhase Phase)
	{
		PhaseProperty->GetUnderlyingProperty()->SetIntPropertyValue(
			PhaseProperty->ContainerPtrToValuePtr<void>(GS->GetRoundManagement()), static_cast<uint64>(Phase));
	};

	TArray<FTeamInfo>* Teams = TeamsProperty->ContainerPtrToValuePtr<TArray<FTeamInfo>>(GS);
	Teams->Add(FTeamInfo(0));
	Teams->Add(FTeamInfo(1));

	ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
	if (!TestNotNull(TEXT("PlayerState"), Player))
	{
		return false;
	}
	if (!Player->IsActorInitialized())
	{
		static_cast<AActor*>(Player)->PostInitializeComponents();
	}
	if (!GS->PlayerArray.Contains(Player))
	{
		GS->AddPlayerState(Player);
	}
	GS->AddPlayerToTeam(Player, 0);

	ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
	if (!ASC->GetSet<ULyraHealthSet>())
	{
		ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(Player));
	}
	if (!ASC->GetSet<UBwayHeroAttributeSet>())
	{
		ASC->AddSpawnedAttribute(NewObject<UBwayHeroAttributeSet>(Player));
	}
	const ULyraHealthSet* Health = ASC->GetSet<ULyraHealthSet>();
	const UBwayGoldAttributeSet* Gold = ASC->GetSet<UBwayGoldAttributeSet>();
	if (!TestNotNull(TEXT("HealthAttributeSet"), Health) || !TestNotNull(TEXT("GoldAttributeSet"), Gold))
	{
		return false;
	}

	ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 300.0f);
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 200.0f);
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 200.0f);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetAttackStrengthAttribute(), 40.0f);

	APawn* Pawn = World->SpawnActor<APawn>();
	USceneComponent* PawnRoot = NewObject<USceneComponent>(Pawn);
	Pawn->AddInstanceComponent(PawnRoot);
	Pawn->SetRootComponent(PawnRoot);
	PawnRoot->RegisterComponent();
	PawnStateProperty->SetObjectPropertyValue_InContainer(Pawn, Player);
	ASC->InitAbilityActorInfo(Player, Pawn);

	UBwayUpgradeComponent* Shop = Player->UpgradeComponent;
	if (!TestNotNull(TEXT("UpgradeComponent"), Shop))
	{
		return false;
	}

	UBwayUpgradeCatalog* Catalog = NewObject<UBwayUpgradeCatalog>(Player);
	Catalog->MaxOwnedUpgrades = 4;
	FBwayUpgradeDefinition& Attack = Catalog->Upgrades.AddDefaulted_GetRef();
	Attack.Id = TEXT("Attack");
	Attack.EffectClass = UBwayUpgradeEffect_AttackStrength::StaticClass();
	for (int32 RankIdx = 0; RankIdx < 3; ++RankIdx)
	{
		FBwayUpgradeRank& Rank = Attack.Ranks.AddDefaulted_GetRef();
		Rank.Cost = 50;
		Rank.Magnitude = (RankIdx + 1) * 5;
	}
	TestTrue(TEXT("ConfigureCatalog succeeds"), Shop->ConfigureCatalog(Catalog));

	// Spawn Team 0 base at (0,0,0), a second overlapping Team 0 base at (0,0,0), and Team 1 enemy base at (5000,0,0)
	ABwayBaseZone* Team0Base = World->SpawnActor<ABwayBaseZone>();
	Team0Base->TeamIndex = 0;
	Team0Base->HealingPerSecond = 25.0f;

	ABwayBaseZone* Team0BaseOverlap = World->SpawnActor<ABwayBaseZone>();
	Team0BaseOverlap->TeamIndex = 0;
	Team0BaseOverlap->HealingPerSecond = 25.0f;

	ABwayBaseZone* Team1Base = World->SpawnActor<ABwayBaseZone>();
	Team1Base->TeamIndex = 1;
	Team1Base->HealingPerSecond = 25.0f;
	Team1Base->SetActorLocation(FVector(5000.0f, 0.0f, 0.0f));

	// 1. Prematch window: valid even outside base
	Pawn->SetActorLocation(FVector(2000.0f, 0.0f, 0.0f));
	SetPhase(EBwayMatchPhase::Prematch);
	TestTrue(TEXT("Prematch window satisfies CanPurchaseUpgrades outside base"), Shop->CanPurchaseUpgrades());
	Shop->ServerTryPurchaseUpgrade_Implementation(TEXT("Attack"), 0);
	TestEqual(TEXT("Prematch purchase increments owned rank to 1"), Shop->GetOwnedRank(TEXT("Attack")), 1);
	TestEqual(TEXT("Prematch purchase debits 50 gold (200 -> 150)"), Gold->GetCurrentGold(), 150.0f);

	// 2. Playing phase inside friendly Team 0 Base Healing Zone: regenerates health without duplicate healing & satisfies CanPurchaseUpgrades
	SetPhase(EBwayMatchPhase::Playing);
	Pawn->SetActorLocation(FVector::ZeroVector);
	Shop->SetInHealingZone(true, 0);
	Shop->SetInHealingZone(true, 0); // Enter second overlapping friendly zone
	TestTrue(TEXT("Friendly player inside Team 0 base satisfies CanUseBase"), Team0Base->CanUseBase(Player));
	TestTrue(TEXT("Friendly player inside Team 0 base satisfies IsPlayerInHealingZone"), Shop->IsPlayerInHealingZone());
	TestTrue(TEXT("Friendly player inside Team 0 base satisfies CanPurchaseUpgrades"), Shop->CanPurchaseUpgrades());

	// Entering/exiting an overlapping enemy zone while inside a friendly zone does not clobber friendly eligibility
	Shop->SetInHealingZone(true, 1);
	Shop->SetInHealingZone(false, 1);
	TestTrue(TEXT("Overlapping enemy zone enter/exit preserves friendly IsPlayerInHealingZone"), Shop->IsPlayerInHealingZone());

	// Exiting one of two overlapping friendly zones preserves explicit zone presence until the second exit
	Shop->SetInHealingZone(false, 0);
	TestTrue(TEXT("Exiting one overlapping friendly zone preserves IsPlayerInHealingZone"), Shop->IsPlayerInHealingZone());

	// Tick BOTH overlapping friendly base zones for 4 * 0.25s = 1.0s: must heal exactly +25.0 HP (no duplicate healing)
	for (int32 TickIdx = 0; TickIdx < 4; ++TickIdx)
	{
		Team0Base->Tick(0.25f);
		Team0BaseOverlap->Tick(0.25f);
	}
	TestTrue(TEXT("Overlapping friendly base zones deduplicate healing (200 -> 225, not 250)"),
		FMath::IsNearlyEqual(Health->GetHealth(), 225.0f, 0.1f));

	// Live purchase while alive inside friendly base healing zone succeeds
	Shop->ServerTryPurchaseUpgrade_Implementation(TEXT("Attack"), 1);
	TestEqual(TEXT("Live base purchase increments owned rank to 2"), Shop->GetOwnedRank(TEXT("Attack")), 2);
	TestEqual(TEXT("Live base purchase debits 50 gold (150 -> 100)"), Gold->GetCurrentGold(), 100.0f);

	// 3. Stepping outside friendly base healing zone during Playing while alive immediately revokes eligibility
	Pawn->SetActorLocation(FVector(1200.0f, 0.0f, 0.0f));
	Shop->SetInHealingZone(false, 0);
	TestFalse(TEXT("Stepping outside base revokes IsPlayerInHealingZone"), Shop->IsPlayerInHealingZone());
	TestFalse(TEXT("Stepping outside base during Playing while alive revokes CanPurchaseUpgrades"), Shop->CanPurchaseUpgrades());

	Shop->ServerTryPurchaseUpgrade_Implementation(TEXT("Attack"), 2);
	TestEqual(TEXT("Rejected outside-base purchase preserves owned rank at 2"), Shop->GetOwnedRank(TEXT("Attack")), 2);
	TestEqual(TEXT("Rejected outside-base purchase has zero gold debit (remains 100)"), Gold->GetCurrentGold(), 100.0f);

	// 4. Stepping into enemy Team 1 base zone denies healing and shop eligibility
	Pawn->SetActorLocation(FVector(5000.0f, 0.0f, 0.0f));
	Shop->SetInHealingZone(true, 1);
	TestFalse(TEXT("Enemy base zone denies CanUseBase"), Team1Base->CanUseBase(Player));
	TestFalse(TEXT("Enemy base zone denies IsPlayerInHealingZone"), Shop->IsPlayerInHealingZone());
	TestFalse(TEXT("Enemy base zone denies CanPurchaseUpgrades"), Shop->CanPurchaseUpgrades());
	Shop->ServerTryPurchaseUpgrade_Implementation(TEXT("Attack"), 2);
	TestEqual(TEXT("Enemy base purchase rejected with zero gold debit"), Gold->GetCurrentGold(), 100.0f);

	// 5. Death inside base -> Dead window valid -> Respawn on new pawn outside base clears stale zone eligibility -> Reconnect clears stale zone state
	Pawn->SetActorLocation(FVector::ZeroVector);
	Shop->SetInHealingZone(true, 0);
	TestTrue(TEXT("Alive at base before death"), Shop->IsPlayerInHealingZone());
	ASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_Death, 1);
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
	TestFalse(TEXT("Dead player in base volume does not satisfy IsPlayerInHealingZone"), Shop->IsPlayerInHealingZone());
	TestFalse(TEXT("Dead player in base volume does not satisfy CanUseBase"), Team0Base->CanUseBase(Player));
	TestTrue(TEXT("Dead window during Playing satisfies CanPurchaseUpgrades"), Shop->CanPurchaseUpgrades());
	Shop->ServerTryPurchaseUpgrade_Implementation(TEXT("Attack"), 2);
	TestEqual(TEXT("Dead window purchase increments owned rank to 3"), Shop->GetOwnedRank(TEXT("Attack")), 3);
	TestEqual(TEXT("Dead window purchase debits 50 gold (100 -> 50)"), Gold->GetCurrentGold(), 50.0f);

	// Respawn onto a new pawn outside the base
	ASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_Death, 0);
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 300.0f);
	APawn* RespawnedPawn = World->SpawnActor<APawn>();
	USceneComponent* RespawnRoot = NewObject<USceneComponent>(RespawnedPawn);
	RespawnedPawn->AddInstanceComponent(RespawnRoot);
	RespawnedPawn->SetRootComponent(RespawnRoot);
	RespawnRoot->RegisterComponent();
	RespawnedPawn->SetActorLocation(FVector(2500.0f, 0.0f, 0.0f));
	PawnStateProperty->SetObjectPropertyValue_InContainer(RespawnedPawn, Player);
	ASC->InitAbilityActorInfo(Player, RespawnedPawn);
	TestFalse(TEXT("Respawning onto a new pawn outside base clears stale IsPlayerInHealingZone"), Shop->IsPlayerInHealingZone());
	TestFalse(TEXT("Respawning onto a new pawn outside base revokes CanPurchaseUpgrades"), Shop->CanPurchaseUpgrades());

	// Disconnect/inactive and reconnect/CopyProperties clear stale zone state
	RespawnedPawn->SetActorLocation(FVector::ZeroVector);
	Shop->SetInHealingZone(true, 0);
	Player->SetIsInactive(true);
	TestFalse(TEXT("Inactive/disconnected player denies CanUseBase"), Team0Base->CanUseBase(Player));
	TestFalse(TEXT("Inactive/disconnected player denies IsPlayerInHealingZone"), Shop->IsPlayerInHealingZone());
	TestFalse(TEXT("Inactive/disconnected player denies CanPurchaseUpgrades"), Shop->CanPurchaseUpgrades());
	Player->SetIsInactive(false);

	ABwayPlayerState* ReconnectedPlayer = World->SpawnActor<ABwayPlayerState>();
	if (!ReconnectedPlayer->IsActorInitialized())
	{
		static_cast<AActor*>(ReconnectedPlayer)->PostInitializeComponents();
	}
	ReconnectedPlayer->UpgradeComponent->SetInHealingZone(true, 0);
	Player->CopyProperties(ReconnectedPlayer);
	TestFalse(TEXT("CopyProperties on reconnect clears stale explicit base zone state"), ReconnectedPlayer->UpgradeComponent->IsPlayerInHealingZone());

	return true;
}

namespace BwayRelicPassImpactTest
{
class FRun : public IAutomationLatentCommand
{
public:
	explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual ~FRun() override
	{
		if (Relic && Relic->CurrentCarrier) Relic->OnDropped();
		if (World) World->DestroyWorld(false);
		if (Instance) Instance->Shutdown();
		if (World && GEngine) GEngine->DestroyWorldContext(World);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 20.0)
		{
			Test->AddError(TEXT("Relic pass-impact physics fixture exceeded its timeout"));
			return true;
		}
		if (!World)
		{
			if (!Setup()) return true;
			StartPass();
			return false;
		}

		World->Tick(LEVELTICK_All, 1.0f / 60.0f);
		Elapsed += 1.0f / 60.0f;
		if (CaseIndex < 2)
		{
			if (Relic->GetCurrentState() != ERelicState::Dropped)
			{
				if (Elapsed < 3.0f) return false;
				Test->AddError(FString::Printf(TEXT("%s pass never ended on physical contact; location=%s velocity=%s"),
					CaseIndex == 0 ? TEXT("Floor") : TEXT("Wall"), *Relic->GetActorLocation().ToCompactString(),
					*Mesh->GetPhysicsLinearVelocity().ToCompactString()));
				return true;
			}

			Test->TestFalse(TEXT("World impact clears pass-flight flag"), Relic->bIsPassInFlight);
			Test->TestEqual(TEXT("World impact clears passing team"), Relic->PassingTeamIndex, INDEX_NONE);
			Test->TestEqual(TEXT("World impact preserves last possessing team"), Relic->LastPossessingTeam, 0);
			Test->TestTrue(TEXT("World impact keeps loose-ball physics enabled"), Mesh->IsSimulatingPhysics());
			Test->TestFalse(TEXT("Unprepared defender cannot auto-collect a grounded pass"), Relic->CanBePickedUpBy(Defender));
			const int32 InterceptionsBefore = DefenderState->GetInterceptions();
			DefenderASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1);
			Defender->SetActorLocation(Relic->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			Defender->UpdateOverlaps();
			Test->TestEqual(TEXT("Prepared defender recovers impacted pass through overlap"), Relic->CurrentCarrier.Get(), Defender);
			Test->TestEqual(TEXT("Impacted pass recovery does not award an interception"), DefenderState->GetInterceptions(), InterceptionsBefore);
			DefenderASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);
			UE_LOG(LogTemp, Display, TEXT("BwayRelicPassImpactTest: Verify case=%s flightCleared=%d groundedInterceptions=%d"),
				CaseIndex == 0 ? TEXT("floor") : TEXT("wall"), !Relic->bIsPassInFlight, DefenderState->GetInterceptions() - InterceptionsBefore);
			++CaseIndex;
			StartPass();
			return false;
		}

		if (Relic->CurrentCarrier != Defender)
		{
			if (Elapsed < 3.0f) return false;
			Test->AddError(TEXT("Airborne pass did not reach the defender's overlap"));
			return true;
		}
		Test->TestEqual(TEXT("Airborne opposing catch earns exactly one interception"), DefenderState->GetInterceptions(), 1);
		Test->TestEqual(TEXT("Airborne catch enters Carried"), Relic->GetCurrentState(), ERelicState::Carried);
		Test->TestFalse(TEXT("Airborne catch clears flight metadata"), Relic->bIsPassInFlight);
		FHitResult DelayedHit;
		DelayedHit.bBlockingHit = true;
		Mesh->OnComponentHit.Broadcast(Mesh, Floor->GetOwner(), Floor, FVector::ZeroVector, DelayedHit);
		Test->TestEqual(TEXT("Delayed impact cannot detach a caught relic"), Relic->CurrentCarrier.Get(), Defender);
		Test->TestEqual(TEXT("Delayed impact cannot change Carried state"), Relic->GetCurrentState(), ERelicState::Carried);
		Test->TestEqual(TEXT("Delayed impact cannot duplicate interception credit"), DefenderState->GetInterceptions(), 1);
		UE_LOG(LogTemp, Display, TEXT("BwayRelicPassImpactTest: Verify case=airborne interceptions=%d delayedImpactIgnored=%d"),
			DefenderState->GetInterceptions(), Relic->CurrentCarrier == Defender && Relic->GetCurrentState() == ERelicState::Carried);
		return true;
	}

private:
	UBoxComponent* AddWorldBox(const FVector& Location, const FVector& Extent)
	{
		AActor* Surface = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Surface);
		Surface->SetRootComponent(Box);
		Box->SetMobility(EComponentMobility::Static);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetWorldLocation(Location);
		Box->RegisterComponent();
		return Box;
	}

	ABwayCharacterWithAbilities* SpawnPlayer(int32 TeamIndex, ABwayPlayerState*& OutState)
	{
		OutState = World->SpawnActor<ABwayPlayerState>();
		OutState->SetGenericTeamId(FGenericTeamId(TeamIndex + 1));
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABwayCharacterWithAbilities* Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(
			FVector(-5000.0f - TeamIndex * 1000.0f, 0.0f, 1000.0f), FRotator::ZeroRotator, Spawn);
		Pawn->SetPlayerState(OutState);
		Pawn->SetGenericTeamId(FGenericTeamId(TeamIndex + 1));
		Pawn->GetCharacterMovement()->DisableMovement();
		OutState->GetLyraAbilitySystemComponent()->InitAbilityActorInfo(OutState, Pawn);
		return Pawn;
	}

	bool Setup()
	{
		Instance.Reset(NewObject<UGameInstance>(GEngine));
		Instance->InitializeStandalone();
		World = Instance->GetWorld();
		if (!Test->TestNotNull(TEXT("Relic impact fixture world"), World)) return false;
		World->InitializeActorsForPlay(FURL());
		World->SetGameState(World->SpawnActor<ABwayGameState>());
		World->SetBegunPlay(true);
		Floor = AddWorldBox(FVector(0.0f, 0.0f, -25.0f), FVector(5000.0f, 5000.0f, 25.0f));
		AddWorldBox(FVector(300.0f, 0.0f, 1000.0f), FVector(25.0f, 400.0f, 1000.0f));
		ABwayPlayerState* PasserState = nullptr;
		Passer = SpawnPlayer(0, PasserState);
		Defender = SpawnPlayer(1, DefenderState);
		DefenderASC = DefenderState->GetLyraAbilitySystemComponent();
		URelicSettings* Settings = NewObject<URelicSettings>(World);
		Settings->RelicMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Settings->RelicAbilitySet = TSoftObjectPtr<ULyraAbilitySet>(FSoftObjectPath(
			TEXT("/BreakawayCore/GameModes/BallMode/Abilities/BallHandlingAbilities/AS_BW_BallAbilities.AS_BW_BallAbilities")));
		Settings->RelicSocket = TEXT("");
		const FTransform SpawnTransform(FVector(0.0f, 1200.0f, 500.0f));
		Relic = World->SpawnActorDeferred<ARelicActor>(ARelicActor::StaticClass(), SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Test->TestNotNull(TEXT("Relic impact actor"), Relic)) return false;
		Relic->InitializeRelicData(Settings);
		Relic->FinishSpawning(SpawnTransform);
		Mesh = Cast<UStaticMeshComponent>(Relic->GetRootComponent());
		if (!Test->TestNotNull(TEXT("Relic physics root"), Mesh)) return false;
		Mesh->SetCollisionObjectType(ECC_PhysicsBody);
		Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Mesh->SetEnableGravity(true);
		return true;
	}

	void StartPass()
	{
		Relic->BeginResettingState();
		Relic->ClearScoringFlag();
		Relic->OnDropped();
		Defender->SetActorLocation(FVector(-6000.0f, 0.0f, 1000.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Relic->OnPickedUp(Passer);
		Relic->Server_PassRelic_Implementation(FVector(1500.0f, 0.0f, 0.0f));
		const FVector LaunchLocation = CaseIndex == 0 ? FVector(0.0f, 1200.0f, 500.0f)
			: CaseIndex == 1 ? FVector(0.0f, 0.0f, 500.0f) : FVector(0.0f, 2400.0f, 1500.0f);
		Relic->SetActorLocation(LaunchLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Mesh->SetPhysicsLinearVelocity(FVector(1500.0f, 0.0f, 0.0f));
		Mesh->WakeAllRigidBodies();
		if (CaseIndex == 2)
		{
			Defender->SetActorLocation(LaunchLocation + FVector(500.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		Test->TestEqual(TEXT("Pass starts in BeingPassed state"), Relic->GetCurrentState(), ERelicState::BeingPassed);
		Test->TestTrue(TEXT("Pass starts with live flight metadata"), Relic->bIsPassInFlight && Relic->PassingTeamIndex == 0);
		Elapsed = 0.0f;
	}

	FAutomationTestBase* Test = nullptr;
	TStrongObjectPtr<UGameInstance> Instance;
	UWorld* World = nullptr;
	ARelicActor* Relic = nullptr;
	UStaticMeshComponent* Mesh = nullptr;
	UBoxComponent* Floor = nullptr;
	ABwayCharacterWithAbilities* Passer = nullptr;
	ABwayCharacterWithAbilities* Defender = nullptr;
	ABwayPlayerState* DefenderState = nullptr;
	ULyraAbilitySystemComponent* DefenderASC = nullptr;
	int32 CaseIndex = 0;
	float Elapsed = 0.0f;
	double Started = FPlatformTime::Seconds();
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicPassImpactTest, "Breakaway.Relic.PassImpactLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicPassImpactTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(BwayRelicPassImpactTest::FRun(this));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#if !UE_BUILD_SHIPPING

namespace BwayGoal2GateRuntime
{
const TCHAR* HeroNames[] = { TEXT("Argus"), TEXT("Rawlins"), TEXT("Alona"), TEXT("Korryn") };

struct FHeldPawn
{
	TWeakObjectPtr<ACharacter> Pawn;
	FTransform Transform;
	EMovementMode Mode = MOVE_Walking;
	uint8 CustomMode = 0;
	TWeakObjectPtr<UBrainComponent> Brain;
	bool bBrainRunning = false;
};

struct FPlayerBackup
{
	TWeakObjectPtr<ABwayPlayerState> PS;
	float Health = 0.0f;
	float Gold = 0.0f;
};

struct FGoal2Probe
{
	bool bClient = false;
	bool bCheckAllPieWorlds = false;
	bool bSelectedStaging = false;
	bool bLoggedClientConnected = false;
	int32 Step = 0;
	double Started = FPlatformTime::Seconds();
	double StableSince = 0.0;
	FString LastSummary;
	TArray<FHeldPawn> Held;
	TArray<FPlayerBackup> Backups;
	TMap<TWeakObjectPtr<ABwayBaseZone>, float> BaseRates;
	TWeakObjectPtr<ABwayBaseZone> SpawnedTeam0Base;
};

TSharedPtr<FGoal2Probe> GActiveGoal2Probe;

UWorld* FindPrimaryWorld(bool bClient)
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* W = Context.World();
		if (!W || !W->IsGameWorld())
		{
			continue;
		}
		if (bClient && W->GetNetMode() == NM_Client)
		{
			return W;
		}
		if (!bClient && W->GetNetMode() != NM_Client)
		{
			return W;
		}
	}
	return nullptr;
}

bool GatherSortedHumanPlayers(ABwayGameState* GS, TArray<ABwayPlayerState*>& OutPlayers)
{
	OutPlayers.Reset();
	if (!GS)
	{
		return false;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
		ULyraAbilitySystemComponent* ASC = BwayPS ? BwayPS->GetLyraAbilitySystemComponent() : nullptr;
		ABwayCharacterWithAbilities* Pawn = BwayPS ? Cast<ABwayCharacterWithAbilities>(BwayPS->GetPawn()) : nullptr;
		if (!BwayPS || BwayPS->IsABot() || !Pawn || !ASC || !ASC->GetSet<ULyraHealthSet>() ||
			!ASC->GetSet<UBwayGoldAttributeSet>() || GS->GetPlayerTeam(BwayPS) == INDEX_NONE)
		{
			continue;
		}
		OutPlayers.Add(BwayPS);
	}
	OutPlayers.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return OutPlayers.Num() == 4;
}

void CleanupGoal2Probe(const TSharedPtr<FGoal2Probe>& Probe)
{
	if (!Probe || Probe->bClient)
	{
		return;
	}
	if (ABwayBaseZone* TempBase = Probe->SpawnedTeam0Base.Get())
	{
		TempBase->Destroy();
	}
	for (const FHeldPawn& H : Probe->Held)
	{
		if (ACharacter* P = H.Pawn.Get())
		{
			P->SetActorTransform(H.Transform, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* M = P->GetCharacterMovement())
			{
				M->SetMovementMode(H.Mode, H.CustomMode);
			}
		}
		if (H.bBrainRunning)
		{
			if (UBrainComponent* B = H.Brain.Get())
			{
				B->RestartLogic();
			}
		}
	}
	for (const FPlayerBackup& B : Probe->Backups)
	{
		if (ABwayPlayerState* PS = B.PS.Get())
		{
			PS->SetInHealingZone(false);
			if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
			{
				ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), B.Health);
				ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), B.Gold);
				ASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0);
				ASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Status_Silenced, 0);
				ASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Status_Jailed, 0);
			}
		}
	}
	for (const auto& Pair : Probe->BaseRates)
	{
		if (ABwayBaseZone* Base = Pair.Key.Get())
		{
			Base->HealingPerSecond = Pair.Value;
		}
	}
}

bool FinishGoal2Probe(const TSharedPtr<FGoal2Probe>& Probe, const TCHAR* Error = nullptr)
{
	if (Error)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayGoal2GateTest: FAIL role=%s step=%d reason=%s elapsed=%.2f summary=[%s]"),
			Probe->bClient ? TEXT("Client") : (Probe->bCheckAllPieWorlds ? TEXT("PIE") : TEXT("Host")),
			Probe->Step, Error, FPlatformTime::Seconds() - Probe->Started, *Probe->LastSummary);
	}
	CleanupGoal2Probe(Probe);
	GActiveGoal2Probe.Reset();
	if (!Error && Probe->bClient && FParse::Param(FCommandLine::Get(), TEXT("Goal2AutoExit")))
	{
		FPlatformMisc::RequestExit(false);
	}
	return false;
}

bool VerifyGoal2WorldConvergence(UWorld* World, FString& OutSummary)
{
	ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	TArray<ABwayPlayerState*> Players;
	if (!GatherSortedHumanPlayers(GS, Players))
	{
		return false;
	}

	// Players: [0]=Team 0, [1]=Team 1 (Interceptor), [2]=Team 0, [3]=Team 1
	const int32 InterceptsP1 = Players[1]->GetInterceptions();
	const int32 FumblesP0 = Players[0]->GetForcedFumbles();
	const int32 FumblesP2 = Players[2]->GetForcedFumbles();
	const int32 TotalFumbles = FumblesP0 + Players[1]->GetForcedFumbles() + FumblesP2 + Players[3]->GetForcedFumbles();

	ULyraAbilitySystemComponent* ASC0 = Players[0]->GetLyraAbilitySystemComponent();
	const float P0Health = ASC0 ? ASC0->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : 0.0f;
	const bool bInBase0 = Players[0]->IsPlayerInHealingZone();
	const bool bCanShop0 = Players[0]->UpgradeComponent && Players[0]->UpgradeComponent->CanPurchaseUpgrades();

	const FBwayMatchSummary MatchSummary = UBwayMatchStatsLibrary::BuildPostMatchSummaryData(World, 0, 1, 5.0f);
	const int32 SummaryIntercepts = MatchSummary.Team0MatchStats.Stats.Interceptions + MatchSummary.Team1MatchStats.Stats.Interceptions;
	const int32 SummaryFumbles = MatchSummary.Team0MatchStats.Stats.ForcedFumbles + MatchSummary.Team1MatchStats.Stats.ForcedFumbles;

	OutSummary = FString::Printf(
		TEXT("interceptsP1=%d summaryIntercepts=%d fumblesP0=%d fumblesP2=%d totalFumbles=%d summaryFumbles=%d p0HP=%.1f inBase0=%d canShop0=%d"),
		InterceptsP1, SummaryIntercepts, FumblesP0, FumblesP2, TotalFumbles, SummaryFumbles,
		P0Health, bInBase0 ? 1 : 0, bCanShop0 ? 1 : 0);

	return InterceptsP1 >= 1 && SummaryIntercepts >= 1 &&
		FumblesP0 >= 1 && FumblesP2 >= 1 && TotalFumbles >= 2 && SummaryFumbles == TotalFumbles &&
		P0Health >= 220.0f && bInBase0 && bCanShop0;
}

bool TickGoal2Probe(const TSharedPtr<FGoal2Probe>& Probe)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - Probe->Started > 120.0)
	{
		return FinishGoal2Probe(Probe, TEXT("timeout"));
	}

	UWorld* World = FindPrimaryWorld(Probe->bClient);
	if (!World)
	{
		return true;
	}
	ABwayGameState* GS = World->GetGameState<ABwayGameState>();
	if (!GS)
	{
		return true;
	}

	if (Probe->bClient && !Probe->bLoggedClientConnected)
	{
		if (const APlayerController* LocalPC = World->GetFirstPlayerController())
		{
			if (LocalPC->PlayerState)
			{
				Probe->bLoggedClientConnected = true;
				UE_LOG(LogTemp, Display, TEXT("BwayGoal2GateTest: ClientConnected player=%d"), LocalPC->PlayerState->GetPlayerId());
			}
		}
	}

	// Auto-assign and lock heroes in HeroSelect_Staging on Host
	if (!Probe->bClient && World->GetMapName().Contains(TEXT("HeroSelect_Staging")))
	{
		if (!Probe->bSelectedStaging && GS->PlayerArray.Num() == 4 && GS->HeroSelectionManager && GS->HeroSelectionManager->IsSelectionActive())
		{
			TArray<ABwayPlayerState*> Roster;
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS); BwayPS && !BwayPS->IsABot())
				{
					Roster.Add(BwayPS);
				}
			}
			if (Roster.Num() == 4)
			{
				Roster.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
				Probe->bSelectedStaging = true;
				for (int32 I = 0; I < 4; ++I)
				{
					const FPrimaryAssetId HeroId = UBwayHeroRegistry::ResolveHeroIdByName(HeroNames[I]);
					if (!GS->HeroSelectionManager->AssignHeroToPlayer(Roster[I], HeroId, true))
					{
						return FinishGoal2Probe(Probe, TEXT("staging-hero-assign-failed"));
					}
					UE_LOG(LogTemp, Display, TEXT("BwayGoal2GateTest: Locked hero=%s player=%d"), HeroNames[I], Roster[I]->GetPlayerId());
				}
			}
		}
		return true;
	}

	if (!World->GetMapName().Contains(TEXT("L_BW_Dorado")))
	{
		return true;
	}

	UBwayRoundManagementComponent* Round = GS->GetRoundManagement();
	if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return true;
	}

	TArray<ABwayPlayerState*> Players;
	if (!GatherSortedHumanPlayers(GS, Players))
	{
		return true;
	}

	if (Probe->bClient)
	{
		FString Summary;
		const bool bConverged = VerifyGoal2WorldConvergence(World, Summary);
		Probe->LastSummary = Summary;
		if (!bConverged)
		{
			Probe->StableSince = 0.0;
			return true;
		}
		if (Probe->StableSince == 0.0)
		{
			Probe->StableSince = Now;
		}
		if (Now - Probe->StableSince < 0.5)
		{
			return true;
		}
		const APlayerController* LocalPC = World->GetFirstPlayerController();
		const int32 ObserverId = (LocalPC && LocalPC->PlayerState) ? LocalPC->PlayerState->GetPlayerId() : INDEX_NONE;
		UE_LOG(LogTemp, Display, TEXT("BwayGoal2GateTest: Client PASS observer=%d %s"), ObserverId, *Summary);
		return FinishGoal2Probe(Probe, nullptr);
	}

	auto PlacePawn = [](ABwayCharacterWithAbilities* P, const FVector& Loc, const FRotator& Rot)
	{
		if (!P)
		{
			return;
		}
		P->SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* C = P->GetController())
		{
			C->SetControlRotation(Rot);
		}
		if (UCharacterMovementComponent* M = P->GetCharacterMovement())
		{
			M->SetMovementMode(MOVE_Flying);
			M->StopMovementImmediately();
		}
	};

	ABwayCharacterWithAbilities* Pawn0 = Cast<ABwayCharacterWithAbilities>(Players[0]->GetPawn());
	ABwayCharacterWithAbilities* Pawn1 = Cast<ABwayCharacterWithAbilities>(Players[1]->GetPawn());
	ABwayCharacterWithAbilities* Pawn2 = Cast<ABwayCharacterWithAbilities>(Players[2]->GetPawn());
	ABwayCharacterWithAbilities* Pawn3 = Cast<ABwayCharacterWithAbilities>(Players[3]->GetPawn());

	if (Probe->Step == 0)
	{
		for (TActorIterator<ABwayBaseZone> It(World); It; ++It)
		{
			Probe->BaseRates.Add(*It, It->HealingPerSecond);
		}
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* C = *It;
			if (!C)
			{
				continue;
			}
			FHeldPawn H;
			H.Pawn = C;
			H.Transform = C->GetActorTransform();
			if (UCharacterMovementComponent* M = C->GetCharacterMovement())
			{
				H.Mode = M->MovementMode;
				H.CustomMode = M->CustomMovementMode;
				M->StopMovementImmediately();
			}
			if (AAIController* AI = Cast<AAIController>(C->GetController()))
			{
				if (UBrainComponent* B = AI->GetBrainComponent())
				{
					H.Brain = B;
					H.bBrainRunning = B->IsRunning();
					B->StopLogic(TEXT("Goal2GateTest"));
				}
			}
			Probe->Held.Add(H);
			if (!Players.Contains(C->GetPlayerState<ABwayPlayerState>()))
			{
				C->SetActorLocation(FVector(0.0f, -9000.0f, 4000.0f), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		const int32 DesiredTeams[4] = { 0, 1, 0, 1 };
		for (int32 I = 0; I < 4; ++I)
		{
			if (GS->GetPlayerTeam(Players[I]) != DesiredTeams[I])
			{
				GS->RemovePlayerFromTeam(Players[I]);
				GS->AddPlayerToTeam(Players[I], DesiredTeams[I]);
				Players[I]->SetGenericTeamId(FGenericTeamId(DesiredTeams[I] + 1));
				if (IGenericTeamAgentInterface* TeamPawn = Cast<IGenericTeamAgentInterface>(Players[I]->GetPawn()))
				{
					TeamPawn->SetGenericTeamId(FGenericTeamId(DesiredTeams[I] + 1));
				}
			}
			ULyraAbilitySystemComponent* ASC = Players[I]->GetLyraAbilitySystemComponent();
			Probe->Backups.Add({
				Players[I],
				ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()),
				ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
			});
		}

		UBwayRelicManagerComponent* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>();
		ARelicActor* Relic = RelicMgr ? RelicMgr->GetRelicActor() : nullptr;
		if (!Relic)
		{
			for (TActorIterator<ARelicActor> It(World); It; ++It)
			{
				Relic = *It;
				break;
			}
		}
		if (!Relic)
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Relic = World->SpawnActor<ARelicActor>(ARelicActor::StaticClass(), FTransform(FVector(0.0f, 0.0f, 4000.0f)), Spawn);
			if (Relic && RelicMgr)
			{
				RelicMgr->SetActiveRelic(Relic);
			}
		}
		if (!Relic)
		{
			return FinishGoal2Probe(Probe, TEXT("missing-relic-actor"));
		}
		if (!Relic->GetRelicSettings())
		{
			URelicSettings* DefaultSettings = NewObject<URelicSettings>(World);
			Relic->InitializeRelicData(DefaultSettings);
		}

		// 1. Charged Ballistic Throw (MaxThrowSpeed=2000) & RMB Prepared Catch (State.Relic.PreparingCatch)
		if (Relic->CurrentCarrier)
		{
			Relic->BeginResettingState();
			Relic->ClearScoringFlag();
			Relic->OnDropped();
		}

		ULyraAbilitySystemComponent* ASC0 = Players[0]->GetLyraAbilitySystemComponent();
		ULyraAbilitySystemComponent* ASC1 = Players[1]->GetLyraAbilitySystemComponent();
		ULyraAbilitySystemComponent* ASC2 = Players[2]->GetLyraAbilitySystemComponent();
		ULyraAbilitySystemComponent* ASC3 = Players[3]->GetLyraAbilitySystemComponent();

		// Verify RMB held prepared catch vs 3.0s call-for-relic indicator decoupling on locally-controlled Host player (ASC0) before pickup
		const FGameplayTag RequestInputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.RelicRequest"), false);
		const FGameplayTag RequestIndicatorTag = Relic->GetRelicSettings() ? Relic->GetRelicSettings()->RequestingTag : FGameplayTag();
		if (RequestInputTag.IsValid() && RequestIndicatorTag.IsValid())
		{
			ASC0->AbilityInputTagPressed(RequestInputTag);
			ASC0->ProcessAbilityInput(1.0f / 60.0f, false);
			if (!ASC0->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch)
				|| !ASC0->HasMatchingGameplayTag(RequestIndicatorTag))
			{
				return FinishGoal2Probe(Probe, TEXT("prepared-catch-press-tags-missing"));
			}
			ASC0->AbilityInputTagReleased(RequestInputTag);
			ASC0->ProcessAbilityInput(1.0f / 60.0f, false);
			if (ASC0->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch)
				|| !ASC0->HasMatchingGameplayTag(RequestIndicatorTag))
			{
				return FinishGoal2Probe(Probe, TEXT("prepared-catch-release-decouple-failed"));
			}
		}

		Relic->OnPickedUp(Pawn0);
		if (Relic->CurrentCarrier != Pawn0)
		{
			return FinishGoal2Probe(Probe, TEXT("initial-pickup-failed"));
		}

		// Verify minimum (0.0s -> 800), intermediate (0.75s -> 1400), and capped (1.5s/3.0s -> 2000) charged throw speeds
		if (!FMath::IsNearlyEqual(Relic->ComputeChargedThrowVelocity(FVector::ForwardVector, 0.0f).Size(), 800.0f, 1.0f)
			|| !FMath::IsNearlyEqual(Relic->ComputeChargedThrowVelocity(FVector::ForwardVector, 0.75f).Size(), 1400.0f, 1.0f)
			|| !FMath::IsNearlyEqual(Relic->ComputeChargedThrowVelocity(FVector::ForwardVector, 1.5f).Size(), 2000.0f, 1.0f)
			|| !FMath::IsNearlyEqual(Relic->ComputeChargedThrowVelocity(FVector::ForwardVector, 3.0f).Size(), 2000.0f, 1.0f))
		{
			return FinishGoal2Probe(Probe, TEXT("charged-throw-curve-mismatch"));
		}

		ASC2->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1, EGameplayTagReplicationState::TagAndCountToAll);
		Relic->ThrowRelicCharged(1.5f, FVector::ForwardVector);
		if (Relic->GetCurrentState() != ERelicState::Thrown)
		{
			return FinishGoal2Probe(Probe, TEXT("charged-throw-state-failed"));
		}

		FHitResult Sweep;
		Relic->OnInteractionSphereOverlap(nullptr, Pawn2, nullptr, 0, false, Sweep);
		ASC2->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0, EGameplayTagReplicationState::TagAndCountToAll);
		if (Relic->CurrentCarrier != Pawn2 || Relic->GetCurrentState() != ERelicState::Carried)
		{
			return FinishGoal2Probe(Probe, TEXT("prepared-catch-overlap-failed"));
		}

		// 2. PickupCooldownAfterDrop (0.5s) & Directed Pass (PassSpeed=1500) & Mid-Air Interception (with negative controls)
		Relic->OnDropped();
		if (!Relic->IsDropPickupCooldownActiveFor(Pawn2) || Relic->CanBePickedUpBy(Pawn2))
		{
			return FinishGoal2Probe(Probe, TEXT("drop-cooldown-not-enforced"));
		}

		// Negative control: Ordinary throw caught by opponent (Pawn1) does NOT count as an interception
		Relic->OnPickedUp(Pawn0);
		Relic->ThrowRelicCharged(0.75f, FVector::ForwardVector);
		ASC1->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1, EGameplayTagReplicationState::TagAndCountToAll);
		Relic->OnInteractionSphereOverlap(nullptr, Pawn1, nullptr, 0, false, Sweep);
		ASC1->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0, EGameplayTagReplicationState::TagAndCountToAll);
		if (Relic->CurrentCarrier != Pawn1 || Players[1]->GetInterceptions() != 0)
		{
			return FinishGoal2Probe(Probe, TEXT("ordinary-throw-falsely-counted-interception"));
		}
		Relic->OnDropped();

		// Teammate Pawn0 picks up immediately and passes at PassSpeed=1500
		Relic->OnPickedUp(Pawn0);
		const FVector PassVelocity = Relic->ComputeDirectedPassVelocity(Pawn2->GetActorLocation());
		if (!FMath::IsNearlyEqual(PassVelocity.Size(), 1500.0f, 1.0f))
		{
			return FinishGoal2Probe(Probe, TEXT("pass-speed-mismatch"));
		}
		Relic->Server_PassRelic_Implementation(PassVelocity);
		if (!Relic->bIsPassInFlight || Relic->PassingTeamIndex != 0)
		{
			return FinishGoal2Probe(Probe, TEXT("pass-in-flight-state-failed"));
		}

		// Opposing Defender Pawn1 (Team 1) intercepts the pass mid-air
		ASC1->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 1, EGameplayTagReplicationState::TagAndCountToAll);
		Relic->OnInteractionSphereOverlap(nullptr, Pawn1, nullptr, 0, false, Sweep);
		Relic->OnInteractionSphereOverlap(nullptr, Pawn1, nullptr, 0, false, Sweep); // Duplicate overlap negative control
		ASC1->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, 0, EGameplayTagReplicationState::TagAndCountToAll);
		if (Relic->CurrentCarrier != Pawn1 || Players[1]->GetInterceptions() != 1)
		{
			return FinishGoal2Probe(Probe, TEXT("midair-interception-failed"));
		}

		// 3. Combat Fumble Threshold (50.0) & Hard CC (State.Status.Silenced / State.Status.Jailed) Forced Fumbles
		const int32 DamageAttackerFumblesBefore = Players[0]->GetForcedFumbles();
		const int32 DamageVictimFumblesBefore = Players[1]->GetForcedFumbles();
		if (Relic->HandleCarrierDamage(30.0f, Pawn0) || Relic->CurrentCarrier != Pawn1)
		{
			return FinishGoal2Probe(Probe, TEXT("sub-threshold-damage-fumbled-early"));
		}
		if (!Relic->HandleCarrierDamage(25.0f, Pawn0) || Relic->CurrentCarrier != nullptr
			|| Players[0]->GetForcedFumbles() != DamageAttackerFumblesBefore + 1
			|| Players[1]->GetForcedFumbles() != DamageVictimFumblesBefore)
		{
			return FinishGoal2Probe(Probe, TEXT("cumulative-threshold-fumble-failed"));
		}

		// Hard CC forced fumble on Pawn3 (Team 1) prioritizes forcing CC source (Pawn2) over earlier chip damage (Pawn0)
		const int32 CCAttackerFumblesBefore = Players[2]->GetForcedFumbles();
		const int32 ChipAttackerFumblesBefore = Players[0]->GetForcedFumbles();
		const int32 CCVictimFumblesBefore = Players[3]->GetForcedFumbles();
		Relic->OnPickedUp(Pawn3);
		Relic->HandleCarrierDamage(10.0f, Pawn0);
		UGameplayEffect* JailEffect = NewObject<UGameplayEffect>(World);
		JailEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FGameplayEffectContextHandle JailContext = ASC2->MakeEffectContext();
		JailContext.AddInstigator(Pawn2, Pawn2);
		FGameplayEffectSpec JailSpec(JailEffect, JailContext, 1.0f);
		JailSpec.DynamicGrantedTags.AddTag(BwayGameplayTags::State_Status_Jailed);
		const FActiveGameplayEffectHandle JailHandle = ASC3->ApplyGameplayEffectSpecToSelf(JailSpec);
		ASC3->RemoveActiveGameplayEffect(JailHandle);
		if (Relic->CurrentCarrier != nullptr || Players[2]->GetForcedFumbles() != CCAttackerFumblesBefore + 1
			|| Players[0]->GetForcedFumbles() != ChipAttackerFumblesBefore
			|| Players[3]->GetForcedFumbles() != CCVictimFumblesBefore)
		{
			return FinishGoal2Probe(Probe, TEXT("hard-cc-fumble-failed"));
		}

		// 4. Team Base Healing Zone & Live Upgrade Shop Eligibility
		ABwayBaseZone* Team0Base = nullptr;
		for (TActorIterator<ABwayBaseZone> It(World); It; ++It)
		{
			if (It->TeamIndex == 0 && It->bEnabled)
			{
				Team0Base = *It;
				break;
			}
		}
		if (!Team0Base)
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Team0Base = World->SpawnActor<ABwayBaseZone>(ABwayBaseZone::StaticClass(), FTransform(FVector(0.0f, 0.0f, 4000.0f)), Spawn);
			if (Team0Base)
			{
				Team0Base->TeamIndex = 0;
				Probe->SpawnedTeam0Base = Team0Base;
			}
		}
		if (!Team0Base)
		{
			return FinishGoal2Probe(Probe, TEXT("missing-team0-base-zone"));
		}
		Team0Base->HealingPerSecond = 25.0f;

		const FVector BaseCenter = Team0Base->GetActorLocation();
		const FVector OutsidePos = BaseCenter + FVector(3000.0f, 0.0f, 0.0f);

		// Outside base while alive during Playing -> CanPurchaseUpgrades must be false and purchase rejected with zero debit
		PlacePawn(Pawn0, OutsidePos, FRotator::ZeroRotator);
		Players[0]->SetInHealingZone(false);
		ASC0->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 200.0f);
		ASC0->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 500.0f);

		UBwayUpgradeComponent* Shop0 = Players[0]->UpgradeComponent;
		const UBwayUpgradeCatalog* Cat0 = Shop0 ? Shop0->GetCatalog() : nullptr;
		if (!Shop0 || !Cat0 || Cat0->Upgrades.IsEmpty())
		{
			return FinishGoal2Probe(Probe, TEXT("missing-shop-catalog"));
		}
		const FName UpgradeId = Cat0->Upgrades[0].Id;
		const int32 InitialRank = Shop0->GetOwnedRank(UpgradeId);

		if (Shop0->CanPurchaseUpgrades())
		{
			return FinishGoal2Probe(Probe, TEXT("outside-base-window-unexpectedly-open"));
		}
		Shop0->ServerTryPurchaseUpgrade_Implementation(UpgradeId, InitialRank);
		if (Shop0->GetOwnedRank(UpgradeId) != InitialRank ||
			!FMath::IsNearlyEqual(ASC0->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 500.0f, 0.1f))
		{
			return FinishGoal2Probe(Probe, TEXT("outside-base-purchase-not-rejected"));
		}

		// Step inside friendly Team 0 Base Healing Zone -> regenerates health & allows live shop purchase
		PlacePawn(Pawn0, BaseCenter, FRotator::ZeroRotator);
		Players[0]->SetInHealingZone(true, 0);
		if (!Players[0]->IsPlayerInHealingZone() || !Shop0->CanPurchaseUpgrades())
		{
			return FinishGoal2Probe(Probe, TEXT("inside-base-window-not-open"));
		}

		for (int32 TickIdx = 0; TickIdx < 4; ++TickIdx)
		{
			Team0Base->Tick(0.25f);
		}
		if (ASC0->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) < 220.0f)
		{
			return FinishGoal2Probe(Probe, TEXT("base-healing-did-not-regenerate"));
		}

		Shop0->ServerTryPurchaseUpgrade_Implementation(UpgradeId, InitialRank);
		if (Shop0->GetOwnedRank(UpgradeId) != InitialRank + 1 ||
			ASC0->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()) >= 500.0f)
		{
			return FinishGoal2Probe(Probe, TEXT("inside-base-live-purchase-failed"));
		}

		PlacePawn(Pawn1, OutsidePos + FVector(0.0f, 300.0f, 0.0f), FRotator::ZeroRotator);
		PlacePawn(Pawn2, OutsidePos + FVector(0.0f, 600.0f, 0.0f), FRotator::ZeroRotator);
		PlacePawn(Pawn3, OutsidePos + FVector(0.0f, 900.0f, 0.0f), FRotator::ZeroRotator);

		for (ABwayPlayerState* PS : Players)
		{
			PS->ForceNetUpdate();
		}

		Probe->Step = 1;
		return true;
	}

	if (Probe->Step == 1)
	{
		ABwayBaseZone* Team0Base = Probe->SpawnedTeam0Base.Get();
		if (!Team0Base)
		{
			for (TActorIterator<ABwayBaseZone> It(World); It; ++It)
			{
				if (It->TeamIndex == 0 && It->bEnabled)
				{
					Team0Base = *It;
					break;
				}
			}
		}
		if (Team0Base)
		{
			PlacePawn(Pawn0, Team0Base->GetActorLocation(), FRotator::ZeroRotator);
			Players[0]->SetInHealingZone(true, 0);
		}

		FString HostSummary;
		bool bAllConverged = VerifyGoal2WorldConvergence(World, HostSummary);
		Probe->LastSummary = HostSummary;
		int32 PieWorldsVerified = 1;

		if (bAllConverged && Probe->bCheckAllPieWorlds && GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				UWorld* ClientWorld = Ctx.World();
				if (!ClientWorld || !ClientWorld->IsGameWorld() || ClientWorld->GetNetMode() != NM_Client)
				{
					continue;
				}
				FString ClientSummary;
				if (!VerifyGoal2WorldConvergence(ClientWorld, ClientSummary))
				{
					Probe->LastSummary = FString::Printf(TEXT("Host:[%s] Client:[%s]"), *HostSummary, *ClientSummary);
					bAllConverged = false;
					break;
				}
				++PieWorldsVerified;
			}
			if (PieWorldsVerified < 4)
			{
				bAllConverged = false;
			}
		}

		if (!bAllConverged)
		{
			Probe->StableSince = 0.0;
			return true;
		}
		if (Probe->StableSince == 0.0)
		{
			Probe->StableSince = Now;
		}
		const double RequiredHold = Probe->bCheckAllPieWorlds ? 0.5 : 2.5;
		if (Now - Probe->StableSince < RequiredHold)
		{
			return true;
		}

		if (Probe->bCheckAllPieWorlds)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayGoal2GateTest: PIE PASS worlds=%d %s"), PieWorldsVerified, *HostSummary);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("BwayGoal2GateTest: Host PASS %s"), *HostSummary);
		}
		return FinishGoal2Probe(Probe, nullptr);
	}

	return true;
}

void StartGoal2Probe(bool bClient, bool bCheckAllPieWorlds)
{
	if (GActiveGoal2Probe)
	{
		CleanupGoal2Probe(GActiveGoal2Probe);
	}
	TSharedPtr<FGoal2Probe> Probe = MakeShared<FGoal2Probe>();
	Probe->bClient = bClient;
	Probe->bCheckAllPieWorlds = bCheckAllPieWorlds;
	GActiveGoal2Probe = Probe;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float)
	{
		if (GActiveGoal2Probe != Probe)
		{
			return false;
		}
		return TickGoal2Probe(Probe);
	}), 0.05f);
}

FAutoConsoleCommand CmdGoal2PIE(
	TEXT("bway.Test.Goal2RelicBasePIE"),
	TEXT("Development 4-world PIE verification for Relic ballistics, prepared catch, interception, forced fumbles, and base healing shop eligibility."),
	FConsoleCommandDelegate::CreateLambda([]() { StartGoal2Probe(false, true); }));

FAutoConsoleCommand CmdGoal2Host(
	TEXT("bway.Test.Goal2RelicBaseHost"),
	TEXT("Development 4-process packaged authority coordinator for Goal 2 Relic ballistics, interception, fumbles, and base healing shop gate."),
	FConsoleCommandDelegate::CreateLambda([]() { StartGoal2Probe(false, false); }));

FAutoConsoleCommand CmdGoal2Client(
	TEXT("bway.Test.Goal2RelicBaseClient"),
	TEXT("Development 4-process packaged client observer for Goal 2 Relic ballistics, interception, fumbles, and base healing shop gate."),
	FConsoleCommandDelegate::CreateLambda([]() { StartGoal2Probe(true, false); }));

} // namespace BwayGoal2GateRuntime

#endif // !UE_BUILD_SHIPPING
