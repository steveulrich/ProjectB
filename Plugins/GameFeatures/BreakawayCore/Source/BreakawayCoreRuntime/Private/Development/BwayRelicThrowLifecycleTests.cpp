#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Abilities/BwayGameplayAbility_RelicThrow.h"
#include "Development/BwayRelicLaunchObservation.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "LyraGameplayTags.h"
#include "Character/LyraHeroComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "Input/LyraInputConfig.h"
#include "Input/LyraInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "UObject/UnrealType.h"

namespace BwayRelicThrowTest
{
const TCHAR* CarrierSetPath = TEXT("/BreakawayCore/GameModes/BallMode/Abilities/BallHandlingAbilities/AS_BW_BallAbilities.AS_BW_BallAbilities");
const TCHAR* InputConfigPath = TEXT("/BreakawayCore/Input/DA_BW_InputData_Humanoid.DA_BW_InputData_Humanoid");

class FHeroInputAccess : public ULyraHeroComponent
{
public:
	using ULyraHeroComponent::Input_AbilityInputTagPressed;
	using ULyraHeroComponent::Input_AbilityInputTagReleased;
};

class FFixture
{
public:
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture()
	{
		if (ASC) ASC->ClearAllAbilities();
		if (World) World->DestroyWorld(false);
	}

	bool Setup()
	{
		const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
		if (!Test.TestNotNull(TEXT("Throw fixture world"), World)) return false;
		ABwayGameState* GameState = World->SpawnActor<ABwayGameState>();
		ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
		Controller = World->SpawnActor<APlayerController>();
		if (!GameState || !Player || !Controller) return false;
		World->SetGameState(GameState);
		if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator, Spawn);
		Relic = World->SpawnActor<ARelicActor>(FVector(5000.f, 0.f, 1000.f), FRotator::ZeroRotator, Spawn);
		if (!Test.TestNotNull(TEXT("Throw pawn"), Pawn) || !Test.TestNotNull(TEXT("Throw relic"), Relic)) return false;
		Controller->PlayerState = Player;
		Controller->Possess(Pawn);
		Pawn->SetPlayerState(Player);
		ASC = Player->GetLyraAbilitySystemComponent();
		ASC->InitAbilityActorInfo(Player, Pawn);
		Settings = NewObject<URelicSettings>(World);
		Settings->RelicMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Settings->MinThrowSpeed = 400.f;
		Settings->MaxThrowSpeed = 1800.f;
		Settings->MaxChargeTime = 1.2f;
		Relic->InitializeRelicData(Settings);
		ULyraAbilitySet* CarrierSet = LoadObject<ULyraAbilitySet>(nullptr, CarrierSetPath);
		FObjectPropertyBase* LoadedSet = FindFProperty<FObjectPropertyBase>(ARelicActor::StaticClass(), TEXT("LoadedRelicAbilitySet"));
		if (!Test.TestNotNull(TEXT("Production carrier set"), CarrierSet) || !Test.TestNotNull(TEXT("Carrier grant property"), LoadedSet)) return false;
		LoadedSet->SetObjectPropertyValue_InContainer(Relic, CarrierSet);
		GameState->RelicManagerComponent->SetActiveRelic(Relic);
		Relic->OnPickedUp(Pawn);
		InputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Relic.Throw"));
		Mesh = Cast<UStaticMeshComponent>(Relic->GetRootComponent());
		if (!Test.TestNotNull(TEXT("Relic physics mesh"), Mesh)) return false;
		Mesh->SetEnableGravity(false);
		Mesh->SetLinearDamping(0.f);
		Mesh->SetAngularDamping(0.f);
		return FindThrow() && SetupInput();
	}

	bool SetupInput()
	{
		const ULyraInputConfig* InputConfig = LoadObject<ULyraInputConfig>(nullptr, InputConfigPath);
		if (!Test.TestNotNull(TEXT("Production input config"), InputConfig)) return false;
		ThrowAction = InputConfig->FindAbilityInputActionForTag(InputTag);
		if (!Test.TestNotNull(TEXT("Production throw action"), ThrowAction)) return false;
		ULyraPawnExtensionComponent* PawnExtension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
		FObjectPropertyBase* CachedASC = FindFProperty<FObjectPropertyBase>(ULyraPawnExtensionComponent::StaticClass(), TEXT("AbilitySystemComponent"));
		if (!Test.TestNotNull(TEXT("Production pawn extension"), PawnExtension) || !Test.TestNotNull(TEXT("Pawn ASC cache property"), CachedASC)) return false;
		CachedASC->SetObjectPropertyValue_InContainer(PawnExtension, ASC);
		ULyraHeroComponent* HeroInput = NewObject<ULyraHeroComponent>(Pawn);
		Pawn->AddInstanceComponent(HeroInput);
		InputComponent = NewObject<ULyraInputComponent>(Controller);
		PlayerInput = NewObject<UEnhancedPlayerInput>(Controller);
		TArray<uint32> BindHandles;
		InputComponent->BindAbilityActions(InputConfig, HeroInput, &FHeroInputAccess::Input_AbilityInputTagPressed,
			&FHeroInputAccess::Input_AbilityInputTagReleased, BindHandles);
		return true;
	}

	bool FindThrow()
	{
		int32 Matches = 0;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_RelicThrow>())
			{
				++Matches;
				Handle = Spec.Handle;
				Test.TestTrue(TEXT("Production throw preserves authored input tag"), Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag));
			}
		}
		return Test.TestEqual(TEXT("Exactly one native carrier throw grant"), Matches, 1);
	}

	bool Press()
	{
		PumpInput(true);
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		return Test.TestTrue(TEXT("Press starts held charge"), Spec && Spec->IsActive())
			&& Test.TestTrue(TEXT("Holding keeps the relic carried"), Relic->CurrentCarrier == Pawn && Relic->GetCurrentState() == ERelicState::Carried);
	}

	void Release()
	{
		PumpInput(false);
	}

	void HoldFrames(int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World->TimeSeconds += 1.f / 60.f;
			PumpInput(true);
		}
	}

	void PumpInput(bool bHeld)
	{
		PlayerInput->InjectInputForAction(ThrowAction, FInputActionValue(bHeld));
		const TArray<UInputComponent*> InputStack { InputComponent };
		PlayerInput->ProcessInputStack(InputStack, 1.f / 60.f, false);
		ASC->ProcessAbilityInput(1.f / 60.f, false);
	}

	bool StepLaunchPhysics()
	{
		FPhysScene* Scene = World->GetPhysicsScene();
		if (!Test.TestNotNull(TEXT("Launch physics scene"), Scene)
			|| !Test.TestTrue(TEXT("Released relic has a simulated body"), Mesh->IsSimulatingPhysics())) return false;
		const FVector Gravity = FVector::ZeroVector;
		constexpr float StepSeconds = 1.f / 120.f;
		for (int32 Step = 0; Step < 4; ++Step)
		{
			Scene->ProcessDeferredCreatePhysicsState();
			Scene->SetUpForFrame(&Gravity, StepSeconds, 0.f, StepSeconds, StepSeconds, 1, false);
			Scene->StartFrame();
			Scene->WaitPhysScenes();
			Scene->EndFrame();
		}
		return true;
	}

	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	ULyraAbilitySystemComponent* ASC = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	APlayerController* Controller = nullptr;
	ARelicActor* Relic = nullptr;
	UStaticMeshComponent* Mesh = nullptr;
	URelicSettings* Settings = nullptr;
	ULyraInputComponent* InputComponent = nullptr;
	UEnhancedPlayerInput* PlayerInput = nullptr;
	const UInputAction* ThrowAction = nullptr;
	FGameplayTag InputTag;
	FGameplayAbilitySpecHandle Handle;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicThrowInputTest, "Breakaway.Relic.ChargedThrow.InputLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicThrowInputTest::RunTest(const FString&)
{
	for (const float ChargeFraction : { 0.f, 0.5f, 1.f, 3.f })
	{
		BwayRelicThrowTest::FFixture Fixture(*this);
		if (!Fixture.Setup() || !Fixture.Press()) return false;
		const float HeldSeconds = Fixture.Settings->MaxChargeTime * ChargeFraction;
		Fixture.World->TimeSeconds += HeldSeconds;
		const FRotator ReleaseAim(20.f, 90.f, 0.f);
		Fixture.Controller->SetControlRotation(ReleaseAim);
		Fixture.Release();
		TestTrue(TEXT("Release throws and detaches the relic"), Fixture.Relic->GetCurrentState() == ERelicState::Thrown && !Fixture.Relic->CurrentCarrier);
		const FVector LaunchPosition = Fixture.Mesh->GetComponentLocation();
		if (!Fixture.StepLaunchPhysics()) return false;
		const FVector Velocity = Fixture.Mesh->GetPhysicsLinearVelocity();
		const float ExpectedSpeed = Fixture.Settings->ComputeChargedThrowSpeed(HeldSeconds);
		AddInfo(FString::Printf(TEXT("Charged throw physical sample: held=%.3f expectedSpeed=%.3f actualVelocity=%s displacement=%s"),
			HeldSeconds, ExpectedSpeed, *Velocity.ToString(), *(Fixture.Mesh->GetComponentLocation() - LaunchPosition).ToString()));
		TestTrue(FString::Printf(TEXT("Charge uses configured speed after physics: held=%.3f expected=%.3f actual=%s"), HeldSeconds, ExpectedSpeed, *Velocity.ToString()),
			FMath::IsNearlyEqual(Velocity.Size(), double(ExpectedSpeed), 0.1));
		TestTrue(FString::Printf(TEXT("Release samples current controller aim: actual=%s expected=%s"), *Velocity.ToString(), *ReleaseAim.Vector().ToString()),
			FVector::DotProduct(Velocity.GetSafeNormal(), ReleaseAim.Vector()) > 0.999);
		TestTrue(TEXT("Physics advances the relic along release aim"), FVector::DotProduct(Fixture.Mesh->GetComponentLocation() - LaunchPosition, ReleaseAim.Vector()) > 0.1);
		TestNull(TEXT("Throw removes its carrier grant"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.Handle));
		Fixture.Release();
		if (!Fixture.StepLaunchPhysics()) return false;
		TestTrue(TEXT("Duplicate release cannot throw again"), Fixture.Mesh->GetPhysicsLinearVelocity().Equals(Velocity, 0.1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicThrowCancelTest, "Breakaway.Relic.ChargedThrow.CancelAndReacquire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicThrowCancelTest::RunTest(const FString&)
{
	BwayRelicThrowTest::FFixture Fixture(*this);
	if (!Fixture.Setup() || !Fixture.Press()) return false;
	Fixture.ASC->CancelAbilityHandle(Fixture.Handle);
	Fixture.HoldFrames(12);
	TestFalse(TEXT("Sustained input cannot reactivate canceled charge"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.Handle)->IsActive());
	Fixture.Release();
	TestTrue(TEXT("Canceled charge leaves relic carried"), Fixture.Relic->CurrentCarrier == Fixture.Pawn && Fixture.Relic->GetCurrentState() == ERelicState::Carried);
	if (!Fixture.Press()) return false;
	Fixture.Relic->OnDropped();
	TestNull(TEXT("Ownership loss removes active throw grant"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.Handle));
	Fixture.World->TimeSeconds += Fixture.Settings->PickupCooldownAfterDrop + 0.1f;
	Fixture.Relic->OnPickedUp(Fixture.Pawn);
	if (!Fixture.FindThrow()) return false;
	Fixture.HoldFrames(12);
	TestFalse(TEXT("Sustained input cannot activate replacement grant"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.Handle)->IsActive());
	Fixture.Release();
	TestTrue(TEXT("Late release cannot throw a reacquired relic"), Fixture.Relic->CurrentCarrier == Fixture.Pawn && Fixture.Relic->GetCurrentState() == ERelicState::Carried);
	TestFalse(TEXT("Replacement grant remains inactive"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.Handle)->IsActive());
	if (!Fixture.Press()) return false;
	Fixture.ASC->AddLooseGameplayTag(LyraGameplayTags::Status_Death_Dying);
	TestFalse(TEXT("Death tag cancels active throw charge immediately"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.Handle)->IsActive());
	Fixture.Release();
	TestTrue(TEXT("Releasing after death cancellation does not launch the relic"), Fixture.Relic->CurrentCarrier == Fixture.Pawn && Fixture.Relic->GetCurrentState() == ERelicState::Carried);
	Fixture.ASC->RemoveLooseGameplayTag(LyraGameplayTags::Status_Death_Dying);

	if (!Fixture.Press()) return false;
	Fixture.Controller->UnPossess();
	TestFalse(TEXT("Possession change cancels active throw charge immediately"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.Handle)->IsActive());
	Fixture.Release();
	TestTrue(TEXT("Releasing after possession change does not launch the relic"), Fixture.Relic->CurrentCarrier == Fixture.Pawn && Fixture.Relic->GetCurrentState() == ERelicState::Carried);
	Fixture.Controller->Possess(Fixture.Pawn);
	Fixture.ASC->InitAbilityActorInfo(Fixture.Pawn->GetPlayerState(), Fixture.Pawn);

	if (!Fixture.Press()) return false;
	Fixture.Release();
	TestTrue(TEXT("A fresh press and release throws the reacquired relic"), Fixture.Relic->GetCurrentState() == ERelicState::Thrown && !Fixture.Relic->CurrentCarrier);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicThrowAssetInputTest, "Breakaway.Relic.ChargedThrow.AuthoredInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicThrowAssetInputTest::RunTest(const FString&)
{
	const UBwayGameplayAbility_RelicThrow* ThrowDefault = GetDefault<UBwayGameplayAbility_RelicThrow>();
	TestEqual(TEXT("Native throw preserves the authored HUD icon"), ThrowDefault->GetDisplayData().Icon.ToSoftObjectPath().ToString(),
		FString(TEXT("/BreakawayCore/GameModes/BallMode/Abilities/UI/Icons/icon_treasure_throw.icon_treasure_throw")));
	TestNotNull(TEXT("Authored throw HUD icon loads"), ThrowDefault->GetDisplayData().Icon.LoadSynchronous());
	const ULyraInputConfig* Config = LoadObject<ULyraInputConfig>(nullptr, BwayRelicThrowTest::InputConfigPath);
	if (!TestNotNull(TEXT("Humanoid input config"), Config)) return false;
	const UInputAction* Action = Config->FindAbilityInputActionForTag(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Relic.Throw")));
	if (!TestNotNull(TEXT("Authored throw action"), Action)) return false;
	TestEqual(TEXT("Throw tag resolves the intended action"), Action->GetName(), FString(TEXT("IA_BW_Relic_Throw")));
	int32 ThrowEntries = 0;
	for (const FLyraInputAction& Entry : Config->AbilityInputActions)
	{
		if (Entry.InputAction != Action) continue;
		++ThrowEntries;
		TestTrue(TEXT("Throw activates only on a fresh input edge"), Entry.bUseStartedForAbilityPress);
	}
	TestEqual(TEXT("One authored throw input entry"), ThrowEntries, 1);
	for (const UInputTrigger* Trigger : Action->Triggers)
	{
		TestTrue(TEXT("Action supports continuous held input"), Trigger && Trigger->IsA<UInputTriggerDown>());
	}
	for (const TCHAR* Path : {
		TEXT("/BreakawayCore/Input/Mappings/IMC_BW_HeroAbilities.IMC_BW_HeroAbilities"),
		TEXT("/BreakawayCore/Input/Mappings/IMC_BW_RelicAbilities.IMC_BW_RelicAbilities") })
	{
		const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, Path);
		if (!TestNotNull(TEXT("Production mapping context"), Context)) return false;
		int32 PrimaryMappings = 0;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action != Action) continue;
			PrimaryMappings += Mapping.Key == EKeys::LeftMouseButton;
			for (const UInputTrigger* Trigger : Mapping.Triggers)
			{
				TestTrue(TEXT("Throw mapping supports continuous held input"), Trigger && Trigger->IsA<UInputTriggerDown>());
			}
		}
		TestEqual(FString(Path) + TEXT(" has one primary-button throw binding"), PrimaryMappings, 1);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicLaunchObservationTest, "Breakaway.Relic.ChargedThrow.DampedLaunchObservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicLaunchObservationTest::RunTest(const FString&)
{
    // Exercise production Enhanced Input release and real Chaos integration with
    // different actual step durations. This is independent of the inverse model.
    for (const double StepSeconds : { 0.016, 0.0255, 0.033333 })
    {
        BwayRelicThrowTest::FFixture Fixture(*this);
        if (!Fixture.Setup()) return false;
        Fixture.Settings->MaxThrowSpeed = 2000.f;
        Fixture.Mesh->SetEnableGravity(true);
        Fixture.Mesh->SetLinearDamping(1.5f);
        if (!Fixture.Press()) return false;
        Fixture.World->TimeSeconds += Fixture.Settings->MaxChargeTime + 0.35f;
        const FVector Aim = FRotator(60.f, 0.f, 0.f).Vector();
        Fixture.Controller->SetControlRotation(FRotator(60.f, 0.f, 0.f));
        Fixture.Release();
        FPhysScene* Scene = Fixture.World->GetPhysicsScene();
        if (!TestNotNull(TEXT("Observed physics scene"), Scene)) return false;
        const FBodyInstance* LaunchBody = Fixture.Mesh->GetBodyInstance();
        if (!TestNotNull(TEXT("Observed real launch body"), LaunchBody)
            || !TestTrue(TEXT("Real damped launch passes the live DOF guard"), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*LaunchBody))) return false;
        BwayRelicFlight::FLaunchObservation Observation;
        const FVector Gravity(0.f, 0.f, -980.f);
        FDelegateHandle Handle = Scene->OnPhysScenePreTick.AddLambda([&](FPhysScene*, float ActualSeconds)
        { Observation.AddStep(ActualSeconds, Fixture.Mesh->GetLinearDamping(), Gravity); });
        ON_SCOPE_EXIT { Scene->OnPhysScenePreTick.Remove(Handle); };
        for (int32 Index = 0; Index < 2; ++Index)
        {
            Scene->ProcessDeferredCreatePhysicsState();
            Scene->SetUpForFrame(&Gravity, StepSeconds, 0.f, StepSeconds, StepSeconds, 1, false);
            Scene->StartFrame(); Scene->WaitPhysScenes(); Scene->EndFrame();
        }
        const FVector Observed = Fixture.Mesh->GetPhysicsLinearVelocity();
        FVector Initial;
        if (!TestTrue(TEXT("Observed real steps reconstruct a launch"), Observation.Reconstruct(Observed, Initial))) return false;
        TestEqual(TEXT("Exactly two actual solver steps observed"), Observation.Steps.Num(), 2);
        TestTrue(TEXT("Recovered actual release vector matches the2000cm/s contract"), Initial.Equals(Aim * 2000.0, 1.0));
        if (StepSeconds > 0.025)
            TestTrue(TEXT("Normal damped motion demonstrates the former fixed-tolerance rejection"), 2000.0 - Observed.Size() > 180.0);
        FVector WrongInitial;
        TestTrue(TEXT("Wrong observed velocity remains measurable"), Observation.Reconstruct(Observed * 0.5, WrongInitial));
        TestTrue(TEXT("Half-speed physical launch is not accepted by correction"), (WrongInitial - Aim * 2000.0).Size() > 180.0);
        TestFalse(TEXT("No physical motion is not launch proof"), Observation.Reconstruct(FVector::ZeroVector, WrongInitial));
    }
    BwayRelicFlight::FLaunchObservation Empty;
    FVector Initial;
    TestFalse(TEXT("Missing actual step record is rejected"), Empty.Reconstruct(FVector(1000,0,0), Initial));
    TestFalse(TEXT("Stale step is rejected"), Empty.AddStep(0.2, 1.5, FVector(0,0,-980)));
    BwayRelicFlight::FLaunchObservation Negative;
    TestFalse(TEXT("Negative damping is rejected"), Negative.AddStep(0.02, -1.0, FVector::ZeroVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicLaunchDOFTest, "Breakaway.Relic.ChargedThrow.UnconstrainedLaunchDegreesOfFreedom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicLaunchDOFTest::RunTest(const FString&)
{
    BwayRelicThrowTest::FFixture Fixture(*this);
    if (!Fixture.Setup() || !Fixture.Press()) return false;
    Fixture.Release();
    FPhysScene* Scene = Fixture.World->GetPhysicsScene();
    if (!TestNotNull(TEXT("DOF fixture physics scene"), Scene)) return false;
    Scene->ProcessDeferredCreatePhysicsState();
    FBodyInstance* Body = Fixture.Mesh->GetBodyInstance();
    if (!TestNotNull(TEXT("DOF fixture real body"), Body) || !TestTrue(TEXT("Released body is dynamic"), Body->IsDynamic())) return false;
    const auto ClearAxisLocks = [Body]()
    {
        Body->bLockXTranslation = Body->bLockYTranslation = Body->bLockZTranslation = false;
        Body->bLockXRotation = Body->bLockYRotation = Body->bLockZRotation = false;
    };
    ON_SCOPE_EXIT { ClearAxisLocks(); Body->SetDOFLock(EDOFMode::Default); };
    if (!TestTrue(TEXT("Project Default DOF is Full3D"), UPhysicsSettings::Get()->DefaultDegreesOfFreedom == ESettingsDOF::Full3D)
        || !TestTrue(TEXT("Body retains Default DOF"), Body->DOFMode == EDOFMode::Default)
        || !TestTrue(TEXT("Default generic translation lock is true"), Body->bLockTranslation)
        || !TestTrue(TEXT("Default generic rotation lock is true"), Body->bLockRotation)) return false;
    TestNull(TEXT("Engine creates no constraint for unrestricted defaults"), Body->DOFConstraint);
    TestTrue(TEXT("Dormant default plane flags do not reject a real unrestricted body"), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));

    // Real engine constraint creation is the oracle; no synthetic constraint pointer is supplied.
    for (int32 Axis = 0; Axis < 6; ++Axis)
    {
        ClearAxisLocks();
        Body->bLockXTranslation = Axis == 0; Body->bLockYTranslation = Axis == 1; Body->bLockZTranslation = Axis == 2;
        Body->bLockXRotation = Axis == 3; Body->bLockYRotation = Axis == 4; Body->bLockZRotation = Axis == 5;
        TestFalse(FString::Printf(TEXT("Pending axis constraint%d is rejected"), Axis), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));
        Body->SetDOFLock(EDOFMode::SixDOF);
        TestNotNull(FString::Printf(TEXT("Engine creates real axis constraint%d"), Axis), Body->DOFConstraint);
        TestFalse(FString::Printf(TEXT("Actual axis constraint%d is rejected"), Axis), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));
        ClearAxisLocks();
        TestFalse(TEXT("Existing constraint survives cleared flags until engine refresh and stays rejected"), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));
        Body->SetDOFLock(EDOFMode::SixDOF);
        TestNull(TEXT("Engine removes the cleared axis constraint"), Body->DOFConstraint);
        TestTrue(TEXT("Recreated unconstrained body is accepted"), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));
    }
    for (const EDOFMode::Type Mode : { EDOFMode::XYPlane, EDOFMode::XZPlane, EDOFMode::YZPlane, EDOFMode::CustomPlane })
    {
        Body->CustomDOFPlaneNormal = FVector::UpVector;
        Body->SetDOFLock(Mode);
        TestNotNull(TEXT("Engine creates a real planar constraint"), Body->DOFConstraint);
        TestFalse(TEXT("Planar and custom-plane constraints stay unsupported"), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));
    }
    Body->SetDOFLock(EDOFMode::None);
    TestNull(TEXT("None removes the actual DOF constraint"), Body->DOFConstraint);
    TestTrue(TEXT("Unconstrained None mode is supported"), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));
    Body->DOFMode = EDOFMode::XYPlane;
    TestNull(TEXT("Pending mode assignment alone creates no constraint"), Body->DOFConstraint);
    TestFalse(TEXT("Pending planar mode is still rejected"), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));
    Body->DOFMode = static_cast<EDOFMode::Type>(255);
    TestFalse(TEXT("Unknown mode stays unsupported"), BwayRelicFlight::HasSupportedLaunchDegreesOfFreedom(*Body));
    return true;
}

#endif
