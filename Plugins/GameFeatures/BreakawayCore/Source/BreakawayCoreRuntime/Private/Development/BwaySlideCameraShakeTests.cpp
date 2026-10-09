#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwaySlideCameraShakeTestTypes.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/LyraHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/GarbageCollection.h"

namespace BwaySlideCameraShakeTest
{
// Real character presentation and camera-modifier ownership in a disposable
// local authority world. No viewport, physical-input, GAS or replication claim.
struct FFixture
{
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture() { if (World) World->DestroyWorld(false); }
	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	APlayerController* Controller = nullptr;
	APlayerCameraManager* Manager = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	UBwayCharacterMovementComponent* Movement = nullptr;
	UCameraShakeBase* Unrelated = nullptr;

	APlayerController* CreateLocalController()
	{
		auto* Result = World->SpawnActor<APlayerController>();
		// No GameMode login runs in this isolated world. Use the same engine
		// initialization as SetPlayer(ULocalPlayer), without viewport/input setup.
		if (Result) Result->SetAsLocalPlayerController();
		return Result;
	}

	APlayerCameraManager* CreateManager(APlayerController* Owner)
	{
		auto* Result = World->SpawnActor<APlayerCameraManager>();
		if (!Result) return nullptr;
		if (!Result->IsActorInitialized()) static_cast<AActor*>(Result)->PostInitializeComponents();
		Owner->PlayerCameraManager = Result;
		Result->InitializeFor(Owner);
		return Result;
	}

	bool Setup()
	{
		const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test.TestNotNull(TEXT("Camera fixture world"), World)) return false;
		// Native dynamic death delegates enter AActor::ProcessEvent, which does
		// not dispatch until the world has initialized its actors for play.
		World->InitializeActorsForPlay(FURL());
		if (!Test.TestTrue(TEXT("World permits real actor event dispatch"), World->AreActorsInitialized())) return false;
		Controller = CreateLocalController();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator, Spawn);
		if (!Test.TestNotNull(TEXT("Local controller"), Controller) || !Test.TestNotNull(TEXT("Character"), Pawn)) return false;
		// Initialized controllers create their camera manager during PostInitializeComponents.
		Manager = Controller->PlayerCameraManager;
		if (!Test.TestNotNull(TEXT("Camera manager"), Manager) || !Test.TestNotNull(TEXT("Shake modifier"), Modifier(Manager))) return false;
		Controller->Possess(Pawn);
		if (!Test.TestTrue(TEXT("Locally controlled character"), Pawn->IsLocallyControlled())) return false;
		Movement = Pawn->GetBwayCharacterMovement();
		Movement->SlideCameraShakeClass = UBwaySlideTestShake::StaticClass();
		Unrelated = Manager->StartCameraShake(UBwayUnrelatedTestShake::StaticClass());
		return Test.TestNotNull(TEXT("Unrelated indefinite shake"), Unrelated);
	}

	static UCameraModifier_CameraShake* Modifier(APlayerCameraManager* Camera)
	{
		return Camera ? Cast<UCameraModifier_CameraShake>(Camera->FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass())) : nullptr;
	}
	static TArray<FActiveCameraShakeInfo> Active(APlayerCameraManager* Camera)
	{
		TArray<FActiveCameraShakeInfo> Out;
		if (auto* Mod = Modifier(Camera)) Mod->GetActiveCameraShakes(Out);
		return Out;
	}
	static bool Contains(APlayerCameraManager* Camera, UCameraShakeBase* Shake)
	{
		return Active(Camera).ContainsByPredicate([Shake](const auto& Info) { return Info.ShakeInstance == Shake; });
	}
	static UCameraShakeBase* Find(APlayerCameraManager* Camera, UClass* Class)
	{
		for (const auto& Info : Active(Camera)) if (Info.ShakeInstance && Info.ShakeInstance->GetClass() == Class) return Info.ShakeInstance;
		return nullptr;
	}
	static UBwaySlideTestShakePattern* Pattern(UCameraShakeBase* Shake)
	{
		return Shake ? Cast<UBwaySlideTestShakePattern>(Shake->GetRootShakePattern()) : nullptr;
	}
	static void AdvanceCamera(APlayerCameraManager* Camera, float Seconds)
	{
		FMinimalViewInfo View;
		Modifier(Camera)->ModifyCamera(Seconds, View);
	}
	UCameraShakeBase* Enter()
	{
		Movement->Velocity = FVector(500.f, 0.f, 0.f);
		Movement->SetMovementMode(MOVE_Custom, uint8(ECustomMovementMode::CMOVE_Slide));
		Pawn->Tick(1.f / 60.f);
		UCameraShakeBase* Shake = Find(Controller->PlayerCameraManager, Movement->SlideCameraShakeClass);
		Test.TestNotNull(TEXT("Character starts configured slide shake"), Shake);
		return Shake;
	}
	void Leave()
	{
		Movement->SetMovementMode(MOVE_Walking);
		Pawn->Tick(1.f / 60.f);
	}
	void ExpectUnrelated()
	{
		Test.TestTrue(TEXT("Unrelated shake remains on original manager"), Contains(Manager, Unrelated));
		Test.TestEqual(TEXT("Unrelated shake never receives stop"), Pattern(Unrelated)->StopCount, 0);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideShakeReleaseTest, "Breakaway.CombatPolish.Slide.CameraShake.ReleaseAndReentry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideShakeReleaseTest::RunTest(const FString&)
{
	using namespace BwaySlideCameraShakeTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	for (int32 Cycle = 0; Cycle < 3; ++Cycle)
	{
		auto* Shake = F.Enter();
		if (!Shake) return false;
		auto* Pattern = F.Pattern(Shake);
		F.AdvanceCamera(F.Manager, 2.f);
		TestTrue(TEXT("Indefinite shake stays active before release"), F.Contains(F.Manager, Shake));
		F.Leave();
		TestEqual(TEXT("Release requests stop once"), Pattern->StopCount, 1);
		TestFalse(TEXT("Release preserves authored blend-out"), Pattern->bLastStopImmediate);
		F.Pawn->Tick(1.f / 60.f);
		TestEqual(TEXT("Following tick does not restart blend-out"), Pattern->StopCount, 1);
		F.AdvanceCamera(F.Manager, 0.1f);
		TestTrue(TEXT("Shake remains during authored blend-out"), F.Contains(F.Manager, Shake));
		F.AdvanceCamera(F.Manager, 0.2f);
		TestFalse(TEXT("Shake removed after blend-out"), F.Contains(F.Manager, Shake));
		F.Pawn->Tick(1.f / 60.f);
		TestEqual(TEXT("No slide-shake accumulation"), F.Active(F.Manager).Num(), 1);
		F.ExpectUnrelated();
	}
	if (!F.Enter()) return false;
	F.Leave();
	if (!F.Enter()) return false;
	TestEqual(TEXT("Rapid re-entry replaces fading shake without stacking"), F.Active(F.Manager).Num(), 2);
	F.Controller->UnPossess();
	TestEqual(TEXT("Unpossession immediately removes owned shake"), F.Active(F.Manager).Num(), 1);
	F.ExpectUnrelated();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideShakeOwnerTest, "Breakaway.CombatPolish.Slide.CameraShake.OwnerAndClassChanges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideShakeOwnerTest::RunTest(const FString&)
{
	using namespace BwaySlideCameraShakeTest;
	FFixture F(*this);
	if (!F.Setup() || !F.Enter()) return false;
	F.Movement->SlideCameraShakeClass = UBwaySlideAlternateTestShake::StaticClass();
	F.Pawn->Tick(1.f / 60.f);
	TestNull(TEXT("Original class removed"), F.Find(F.Manager, UBwaySlideTestShake::StaticClass()));
	TestNotNull(TEXT("Replacement class started"), F.Find(F.Manager, UBwaySlideAlternateTestShake::StaticClass()));
	TestEqual(TEXT("Class replacement keeps one owned shake"), F.Active(F.Manager).Num(), 2);
	auto* Replacement = F.CreateManager(F.Controller);
	if (!TestNotNull(TEXT("Replacement camera manager"), Replacement)) return false;
	F.Pawn->Tick(1.f / 60.f);
	TestEqual(TEXT("Old manager retains only unrelated effect"), F.Active(F.Manager).Num(), 1);
	TestEqual(TEXT("New manager receives one slide effect"), F.Active(Replacement).Num(), 1);
	F.Movement->SlideCameraShakeClass = nullptr;
	F.Pawn->Tick(1.f / 60.f);
	TestEqual(TEXT("Removing configured class cleans owned effect"), F.Active(Replacement).Num(), 0);
	F.Movement->SlideCameraShakeClass = UBwaySlideTestShake::StaticClass();
	if (!F.Enter()) return false;
	auto* NextController = F.CreateLocalController();
	if (!TestNotNull(TEXT("Next controller"), NextController)) return false;
	auto* NextManager = F.CreateManager(NextController);
	if (!TestNotNull(TEXT("Next local manager"), NextManager)) return false;
	NextController->Possess(F.Pawn);
	if (!TestTrue(TEXT("Replacement controller owns a locally controlled character"), F.Pawn->IsLocallyControlled())) return false;
	TestEqual(TEXT("Controller replacement removes prior local effect"), F.Active(Replacement).Num(), 0);
	F.Movement->SetMovementMode(MOVE_Custom, uint8(ECustomMovementMode::CMOVE_Slide));
	F.Pawn->Tick(1.f / 60.f);
	TestEqual(TEXT("New local owner receives one effect"), F.Active(NextManager).Num(), 1);
	NextController->UnPossess();
	F.Pawn->Tick(1.f / 60.f);
	TestEqual(TEXT("Unowned pawn never starts a new effect"), F.Active(NextManager).Num(), 0);
	F.ExpectUnrelated();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideShakeTeardownTest, "Breakaway.CombatPolish.Slide.CameraShake.DeathAndTeardown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideShakeTeardownTest::RunTest(const FString&)
{
	using namespace BwaySlideCameraShakeTest;
	{
		FFixture F(*this);
		if (!F.Setup() || !F.Enter()) return false;
		F.Leave(); // Death must also remove an already blending-out instance.
		auto* Health = F.Pawn->FindComponentByClass<ULyraHealthComponent>();
		if (!TestNotNull(TEXT("Character health component"), Health)) return false;
		Health->StartDeath();
		TestEqual(TEXT("Death removes owned blend-out immediately"), F.Active(F.Manager).Num(), 1);
		// Deliberately stale movement cannot revive a dead pawn's shake.
		F.Movement->SetMovementMode(MOVE_Custom, uint8(ECustomMovementMode::CMOVE_Slide));
		F.Pawn->Tick(1.f / 60.f);
		TestNull(TEXT("Dead pawn cannot restart slide shake"), F.Find(F.Manager, UBwaySlideTestShake::StaticClass()));
		F.ExpectUnrelated();
	}
	{
		FFixture F(*this);
		if (!F.Setup() || !F.Enter()) return false;
		// Invoke the real lifecycle callback; this isolated world intentionally
		// does not run Lyra experience/BeginPlay or viewport initialization.
		F.Pawn->EndPlay(EEndPlayReason::RemovedFromWorld);
		TestEqual(TEXT("EndPlay removes owned shake immediately"), F.Active(F.Manager).Num(), 1);
		F.ExpectUnrelated();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideShakeFiniteTest, "Breakaway.CombatPolish.Slide.CameraShake.FiniteDuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideShakeFiniteTest::RunTest(const FString&)
{
	using namespace BwaySlideCameraShakeTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	F.Movement->SlideCameraShakeClass = UBwaySlideFiniteTestShake::StaticClass();
	TWeakObjectPtr<UCameraShakeBase> FinishedShake = F.Enter();
	if (!FinishedShake.IsValid()) return false;
	F.AdvanceCamera(F.Manager, 0.5f);
	TestNull(TEXT("Finite authored shake finishes"), F.Find(F.Manager, UBwaySlideFiniteTestShake::StaticClass()));
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Character retains finite one-shot identity through GC"), FinishedShake.IsValid());
	F.Pawn->Tick(1.f / 60.f);
	TestNull(TEXT("Continuous slide does not turn finite shake into a loop"), F.Find(F.Manager, UBwaySlideFiniteTestShake::StaticClass()));
	F.Leave();
	if (!F.Enter()) return false;
	TestEqual(TEXT("Next slide may play finite shake again"), F.Active(F.Manager).Num(), 2);
	F.ExpectUnrelated();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideShakePoolTest, "Breakaway.CombatPolish.Slide.CameraShake.ExpiredInstanceOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideShakePoolTest::RunTest(const FString&)
{
	using namespace BwaySlideCameraShakeTest;
	for (const bool bFinite : {false, true})
	{
		FFixture F(*this);
		if (!F.Setup()) return false;
		F.Movement->SlideCameraShakeClass = bFinite ? UBwaySlideFiniteTestShake::StaticClass() : UBwaySlideTestShake::StaticClass();
		auto* Owned = F.Enter();
		if (!Owned) return false;
		if (!bFinite) F.Leave();
		F.AdvanceCamera(F.Manager, 0.5f);
		TestFalse(TEXT("Owned run has expired before another effect starts"), F.Contains(F.Manager, Owned));
		// Deliberately start between the modifier's expiry and the next pawn
		// tick, the window where an expired pooled UObject used to alias.
		auto* Other = F.Manager->StartCameraShake(F.Movement->SlideCameraShakeClass);
		if (!TestNotNull(TEXT("Independent same-class effect"), Other)) return false;
		TestTrue(TEXT("Expired owned instance cannot be recycled into another effect"), Other != Owned);
		if (bFinite) F.Leave();
		else F.Pawn->Tick(1.f / 60.f);
		TestTrue(TEXT("Slide release leaves same-class effect active"), F.Contains(F.Manager, Other));
		TestEqual(TEXT("Release never stops independent same-class effect"), F.Pattern(Other)->StopCount, 0);
		F.Pawn->FindComponentByClass<ULyraHealthComponent>()->StartDeath();
		TestTrue(TEXT("Death leaves independent same-class effect active"), F.Contains(F.Manager, Other));
		TestEqual(TEXT("Death never stops independent same-class effect"), F.Pattern(Other)->StopCount, 0);
		F.ExpectUnrelated();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideShakeSingleInstanceTest, "Breakaway.CombatPolish.Slide.CameraShake.AuthoredSingleInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwaySlideShakeSingleInstanceTest::RunTest(const FString&)
{
	using namespace BwaySlideCameraShakeTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	F.Movement->SlideCameraShakeClass = UBwaySlideSingleInstanceTestShake::StaticClass();
	auto* Prior = F.Manager->StartCameraShake(F.Movement->SlideCameraShakeClass);
	if (!TestNotNull(TEXT("Existing single-instance effect"), Prior) || !F.Enter()) return false;
	TestEqual(TEXT("Authored single-instance policy prevents stacking on slide start"), F.Active(F.Manager).Num(), 2);
	TestTrue(TEXT("Authored single-instance class remains single-instance"), GetDefault<UBwaySlideSingleInstanceTestShake>()->bSingleInstance);
	auto* Other = F.Manager->StartCameraShake(F.Movement->SlideCameraShakeClass);
	if (!TestNotNull(TEXT("External single-instance takeover"), Other)) return false;
	TestEqual(TEXT("External takeover also preserves one same-class effect"), F.Active(F.Manager).Num(), 2);
	F.Leave();
	F.Controller->UnPossess();
	TestTrue(TEXT("Cleanup does not stop a later single-instance takeover"), F.Contains(F.Manager, Other));
	TestEqual(TEXT("Later single-instance owner never receives slide stop"), F.Pattern(Other)->StopCount, 0);
	F.ExpectUnrelated();
	return true;
}
#endif
