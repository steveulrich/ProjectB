#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwayProjectilePresentationTestTypes.h"
#include "BwaySlideCameraShakeTestTypes.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "Combat/BwayProjectilePresentationData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWave.h"

namespace BwayProjectilePresentationTest
{
struct FFixture
{
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture()
	{
		if (World)
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	}
	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	USoundBase* Sound = nullptr;
	FGameplayTag Cue;
	bool Setup()
	{
		const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test.TestNotNull(TEXT("Projectile fixture world"), World)) return false;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Sound = NewObject<USoundWave>(World);
		Cue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Breakaway.Scoring.Goal"), false);
		return Test.TestTrue(TEXT("Existing native cue is registered"), Cue.IsValid());
	}
	ABwayProjectilePresentationTestActor* Spawn(bool bCue = false, APawn* Instigator = nullptr, bool bBegin = true)
	{
		const FTransform Transform(FVector(0.f, 0.f, 10000.f));
		auto* Actor = World->SpawnActorDeferred<ABwayProjectilePresentationTestActor>(
			ABwayProjectilePresentationTestActor::StaticClass(), Transform, Instigator, Instigator,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Test.TestNotNull(TEXT("Projectile"), Actor)) return nullptr;
		Actor->SetActorEnableCollision(false);
		auto* Data = NewObject<UBwayProjectilePresentationData>(Actor);
		Data->SpawnSound = Sound;
		Data->ImpactSound = Sound;
		Data->ExpireSound = Sound;
		if (bCue) Data->SpawnCueTag = Data->ImpactCueTag = Data->ExpireCueTag = Cue;
		Actor->ApplyPresentation(Data);
		Actor->FinishSpawning(Transform);
		if (bBegin && !Actor->HasActorBegunPlay()) Actor->DispatchBeginPlay();
		return Actor;
	}
	static int32 ShakeCount(APlayerController* Controller)
	{
		TArray<FActiveCameraShakeInfo> Active;
		if (Controller && Controller->PlayerCameraManager)
		{
			if (auto* Modifier = Cast<UCameraModifier_CameraShake>(Controller->PlayerCameraManager->FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass())))
				Modifier->GetActiveCameraShakes(Active);
		}
		return Active.Num();
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayProjectilePresentationLifecycleTest,
	"Breakaway.CombatPolish.Projectile.EventLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayProjectilePresentationLifecycleTest::RunTest(const FString&)
{
	using namespace BwayProjectilePresentationTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	for (bool bCue : {false, true})
	{
		auto* Actor = F.Spawn(bCue);
		if (!Actor) return false;
		TestEqual(TEXT("Spawn chooses exactly one presentation route"), Actor->CueCalls + Actor->FallbackCalls, 1);
		Actor->RepeatSpawn();
		TestEqual(TEXT("Repeated authority spawn does not repeat presentation"), Actor->CueCalls + Actor->FallbackCalls, 1);
		Actor->bAcceptHit = false;
		Actor->Hit();
		TestFalse(TEXT("Rejected overlap preserves projectile"), Actor->IsActorBeingDestroyed());
		TestEqual(TEXT("Rejected overlap emits no impact"), Actor->CueCalls + Actor->FallbackCalls, 1);
		Actor->bAcceptHit = true;
		Actor->bTryReentrantHit = true;
		Actor->Hit();
		TestEqual(TEXT("Reentrant hit did not apply damage again"), Actor->DamageCalls, 2);
		TestTrue(TEXT("Accepted hit destroys projectile"), Actor->IsActorBeingDestroyed());
		TestEqual(TEXT("Spawn, impact and expiry each execute once"), Actor->CueCalls + Actor->FallbackCalls, 3);
		TestEqual(TEXT("Cue wins over configured fallback"), Actor->FallbackCalls, bCue ? 0 : 3);
		TestEqual(TEXT("Fallback does not also execute a cue"), Actor->CueCalls, bCue ? 3 : 0);
		Actor->Hit(true);
		TestEqual(TEXT("Late collision notification does not repeat event"), Actor->CueCalls + Actor->FallbackCalls, 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayProjectilePresentationExpiryTest,
	"Breakaway.CombatPolish.Projectile.ExpiryAndRoundCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayProjectilePresentationExpiryTest::RunTest(const FString&)
{
	using namespace BwayProjectilePresentationTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	auto* Timed = F.Spawn();
	auto* Round = F.Spawn();
	auto* ScoringHit = F.Spawn();
	auto* WorldHit = F.Spawn();
	if (!Timed || !Round || !ScoringHit || !WorldHit) return false;
	Timed->Timeout();
	TestTrue(TEXT("Lifespan expiry destroys projectile"), Timed->IsActorBeingDestroyed());
	TestEqual(TEXT("Lifespan presents spawn and expiry before destruction"), Timed->FallbackCalls, 2);
	Round->RoundCleanup();
	TestTrue(TEXT("Round cleanup destroys projectile"), Round->IsActorBeingDestroyed());
	TestEqual(TEXT("Round cleanup does not play an expiry burst"), Round->FallbackCalls, 1);
	ScoringHit->bFinishRoundDuringDamage = true;
	ScoringHit->Hit();
	TestTrue(TEXT("Damage callback defers destruction until impact dispatch"), ScoringHit->bAliveAfterRoundCallback);
	TestTrue(TEXT("Deferred round cleanup still completes"), ScoringHit->IsActorBeingDestroyed());
	TestEqual(TEXT("Round-ending hit presents impact without cleanup expiry"), ScoringHit->FallbackCalls, 2);
	WorldHit->bAcceptHit = false;
	WorldHit->Hit(true);
	TestTrue(TEXT("Blocking world collision expires without damage"), WorldHit->IsActorBeingDestroyed());
	TestEqual(TEXT("Blocking world collision presents its impact"), WorldHit->FallbackCalls, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayProjectilePresentationReceiveTest,
	"Breakaway.CombatPolish.Projectile.EventReceiptAndCameraPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayProjectilePresentationReceiveTest::RunTest(const FString&)
{
	using namespace BwayProjectilePresentationTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	APlayerController* Local = F.World->SpawnActor<APlayerController>();
	APlayerController* Remote = F.World->SpawnActor<APlayerController>();
	APawn* LocalPawn = F.World->SpawnActor<APawn>();
	APawn* RemotePawn = F.World->SpawnActor<APawn>();
	if (!Local || !Remote || !LocalPawn || !RemotePawn) return false;
	Local->SetAsLocalPlayerController();
	Local->Possess(LocalPawn);
	Remote->Possess(RemotePawn);
	if (!TestTrue(TEXT("Fixture has a local owner and a nonlocal owner"), Local->IsLocalController() && !Remote->IsLocalController())) return false;
	if (!TestNotNull(TEXT("Local camera manager"), Local->PlayerCameraManager.Get())) return false;
	// The config is restored on every exit; no authored asset is saved or dirtied.
	auto* Config = const_cast<UBwayCombatReadabilityConfig*>(UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(LocalPawn));
	if (!TestNotNull(TEXT("Production readability config"), Config)) return false;
	TGuardValue<bool> RestoreShakeSetting(Config->bReduceCameraShake, false);
	auto* Receiver = F.Spawn(false, LocalPawn, false);
	auto* RemoteReceiver = F.Spawn(false, RemotePawn, false);
	auto* ReducedReceiver = F.Spawn(false, LocalPawn, false);
	if (!Receiver || !RemoteReceiver || !ReducedReceiver) return false;
	Receiver->UseSimulatedRole();
	Receiver->DispatchBeginPlay();
	Receiver->Hit(true);
	TestEqual(TEXT("Applying replicated presentation does not replay historical events"), Receiver->FallbackCalls, 0);
	TestEqual(TEXT("Simulated collision never applies damage or emits presentation"), Receiver->DamageCalls, 0);
	Receiver->Receive(EBwayProjectilePresentationEvent::Impact, F.Cue, F.Sound, LocalPawn, UBwaySlideTestShake::StaticClass());
	Receiver->Receive(EBwayProjectilePresentationEvent::Impact, F.Cue, F.Sound, LocalPawn, UBwaySlideTestShake::StaticClass());
	TestEqual(TEXT("Receiver executes cue once"), Receiver->CueCalls, 1);
	TestEqual(TEXT("Receiver has only the new impact event, no historical spawn"), Receiver->ReceivedEvents(), uint8(1 << uint8(EBwayProjectilePresentationEvent::Impact)));
	TestEqual(TEXT("Receiver rejects fallback alongside valid cue"), Receiver->FallbackCalls, 0);
	TestEqual(TEXT("Local owner starts one shake despite repeated receipt"), FFixture::ShakeCount(Local), 1);
	RemoteReceiver->Receive(EBwayProjectilePresentationEvent::Impact, {}, F.Sound, RemotePawn, UBwaySlideTestShake::StaticClass());
	TestEqual(TEXT("Nonlocal owner never starts a camera shake"), FFixture::ShakeCount(Remote), 0);
	Config->bReduceCameraShake = true;
	ReducedReceiver->Receive(EBwayProjectilePresentationEvent::Impact, {}, F.Sound, LocalPawn, UBwaySlideTestShake::StaticClass());
	TestEqual(TEXT("Reduced shake setting prevents a second owner shake"), FFixture::ShakeCount(Local), 1);
	TestEqual(TEXT("Reduced motion preserves impact sound/VFX route"), ReducedReceiver->FallbackCalls, 1);
	return true;
}
#endif
