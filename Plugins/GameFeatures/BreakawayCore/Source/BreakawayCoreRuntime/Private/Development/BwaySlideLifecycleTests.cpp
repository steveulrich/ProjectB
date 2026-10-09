#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayAbility_Slide.h"
#include "BwayPlayerState.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTask.h"
#include "GameplayCueManager.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/UnrealType.h"

namespace BwaySlideLifecycleTest
{
// Real GAS and movement lifecycle in an isolated authority world. No physical
// input, movement trajectory, prediction or multiplayer convergence claim.
class FFixture
{
public:
	explicit FFixture(FAutomationTestBase* InTest) : Test(InTest) {}
	~FFixture()
	{
		if (ASC)
		{
			ASC->OnAbilityEnded.Remove(EndedHandle);
			ASC->ClearAllAbilities();
		}
		if (Extension) Extension->UninitializeAbilitySystem();
		if (World) World->DestroyWorld(false);
	}

	bool Setup()
	{
		const UWorld::InitializationValues Settings = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test->TestNotNull(TEXT("Slide fixture world"), World)) return false;
		ABwayGameState* GameState = World->SpawnActor<ABwayGameState>();
		if (!Test->TestNotNull(TEXT("Slide game state"), GameState)) return false;
		World->SetGameState(GameState);
		Player = World->SpawnActor<ABwayPlayerState>();
		Controller = World->SpawnActor<APlayerController>();
		if (!Test->TestNotNull(TEXT("Slide PlayerState"), Player) || !Test->TestNotNull(TEXT("Slide controller"), Controller)) return false;
		if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
		ASC = Player->GetLyraAbilitySystemComponent();
		if (!Test->TestNotNull(TEXT("Slide ability system"), ASC)) return false;
		UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
		if (!ASC->GetSet<ULyraHealthSet>()) ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(Player));
		PawnData = NewObject<ULyraPawnData>(World);
		if (!ReplaceAvatar()) return false;

		const ULyraAbilitySet* Set = LoadObject<ULyraAbilitySet>(nullptr,
			TEXT("/BreakawayCore/Characters/DA_BW_AbilitySet_Humanoid.DA_BW_AbilitySet_Humanoid"));
		const FArrayProperty* GrantsProperty = FindFProperty<FArrayProperty>(ULyraAbilitySet::StaticClass(), TEXT("GrantedGameplayAbilities"));
		if (!Test->TestNotNull(TEXT("Production humanoid ability set"), Set) || !Test->TestNotNull(TEXT("Ability grants property"), GrantsProperty)) return false;
		const auto* Grants = GrantsProperty->ContainerPtrToValuePtr<TArray<FLyraAbilitySet_GameplayAbility>>(Set);
		int32 SlideGrants = 0;
		for (const auto& Grant : *Grants)
		{
			if (Grant.Ability && Grant.Ability->IsChildOf(UBwayGameplayAbility_Slide::StaticClass()))
			{
				++SlideGrants;
				InputTag = Grant.InputTag;
				if (!Test->TestTrue(TEXT("Granted slide input tag is valid"), InputTag.IsValid())) return false;
				FGameplayAbilitySpec Spec(Grant.Ability, Grant.AbilityLevel);
				Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
				AbilityHandle = ASC->GiveAbility(Spec);
			}
		}
		if (!Test->TestEqual(TEXT("Exactly one production common slide grant"), SlideGrants, 1)
			|| !Test->TestTrue(TEXT("Production slide input tag"), InputTag.IsValid())) return false;
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
		Ability = Spec ? Spec->GetPrimaryInstance() : nullptr;
		TasksProperty = FindFProperty<FArrayProperty>(UGameplayAbility::StaticClass(), TEXT("ActiveTasks"));
		CacheProperty = FindFProperty<FObjectPropertyBase>(UBwayGameplayAbility_Slide::StaticClass(), TEXT("CachedBwayMoveComp"));
		SlidingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sliding"));
		if (!Test->TestNotNull(TEXT("Production slide instance"), Ability)
			|| !Test->TestNotNull(TEXT("Task inspection property"), TasksProperty)
			|| !Test->TestNotNull(TEXT("Activation cache inspection property"), CacheProperty)) return false;
		EndedHandle = ASC->OnAbilityEnded.AddLambda([this](const FAbilityEndedData& Data)
		{
			if (Data.AbilitySpecHandle == AbilityHandle) ++EndCount;
		});
		return Test->TestTrue(TEXT("Fixture is an authoritative locally controlled ability owner"),
			Pawn->HasAuthority() && ASC->AbilityActorInfo->IsLocallyControlled());
	}

	bool ReplaceAvatar()
	{
		if (Extension) Extension->UninitializeAbilitySystem();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(FVector(1000.f * ++AvatarCount, 0.f, 1000.f), FRotator::ZeroRotator, Spawn);
		if (!Test->TestNotNull(TEXT("Slide avatar"), Pawn)) return false;
		Controller->Possess(Pawn);
		Pawn->SetPlayerState(Player);
		Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
		if (!Test->TestNotNull(TEXT("Slide pawn extension"), Extension)) return false;
		Extension->SetPawnData(PawnData);
		Extension->InitializeAbilitySystem(ASC, Player);
		Movement = Pawn->GetBwayCharacterMovement();
		Movement->SetMovementMode(MOVE_Walking);
		return true;
	}

	bool Begin(const TCHAR* Label)
	{
		// Each isolated case tests termination, not natural cooldown expiry.
		for (const auto Handle : ASC->GetActiveEffects(FGameplayEffectQuery())) ASC->RemoveActiveGameplayEffect(Handle);
		ASC->ClearAbilityInput();
		Movement->SetMovementMode(MOVE_Walking);
		Movement->Velocity = FVector(FMath::Max(500.f, Movement->MinSlideSpeed + 100.f), 0.f, 0.f);
		StartEndCount = EndCount;
		ASC->AbilityInputTagPressed(InputTag);
		ASC->ProcessAbilityInput(1.f / 60.f, false);
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
		if (!Test->TestTrue(FString(Label) + TEXT(": real input activation"), Spec && Spec->IsActive())) return false;
		Test->TestTrue(FString(Label) + TEXT(": movement enters slide"), Movement->IsSliding());
		Test->TestEqual(FString(Label) + TEXT(": one owned sliding tag"), ASC->GetTagCount(SlidingTag), 1);
		PriorTasks.Reset();
		int32 ReleaseTasks = 0;
		for (UGameplayTask* Task : Tasks())
		{
			PriorTasks.Add(Task);
			if (Task && Task->IsA<UAbilityTask_WaitInputRelease>()) ++ReleaseTasks;
		}
		Test->TestEqual(FString(Label) + TEXT(": one input-release task"), ReleaseTasks, 1);
		Test->TestTrue(FString(Label) + TEXT(": observer bound during activation"), Pawn->MovementModeChangedDelegate.GetAllObjects().Contains(Ability));
		return true;
	}

	void Release()
	{
		ASC->AbilityInputTagReleased(InputTag);
		ASC->ProcessAbilityInput(1.f / 60.f, false);
	}

	void ExpectEnded(const TCHAR* Label, EMovementMode ExpectedMode, uint8 ExpectedCustomMode = 0, int32 ExpectedEndEvents = 1)
	{
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
		Test->TestTrue(FString(Label) + TEXT(": spec ended"), Spec && !Spec->IsActive());
		Test->TestEqual(FString(Label) + TEXT(": exact activation end count"), EndCount, StartEndCount + ExpectedEndEvents);
		Test->TestEqual(FString(Label) + TEXT(": mode preserved"), Movement->MovementMode.GetValue(), ExpectedMode);
		Test->TestEqual(FString(Label) + TEXT(": custom mode preserved"), Movement->CustomMovementMode, ExpectedCustomMode);
		Test->TestEqual(FString(Label) + TEXT(": sliding tag removed"), ASC->GetTagCount(SlidingTag), 0);
		Test->TestEqual(FString(Label) + TEXT(": active tasks cleared"), Tasks().Num(), 0);
		Test->TestNull(FString(Label) + TEXT(": execution cache cleared"), CacheProperty->GetObjectPropertyValue_InContainer(Ability));
		Test->TestFalse(FString(Label) + TEXT(": movement observer unbound"), Pawn->MovementModeChangedDelegate.GetAllObjects().Contains(Ability));
		for (const auto& Task : PriorTasks)
		{
			Test->TestTrue(FString(Label) + TEXT(": prior task finished"), !Task.IsValid() || Task->IsFinished());
		}
		Test->AddInfo(FString::Printf(TEXT("SlideLifecycle: case=%s endCount=%d mode=%d custom=%d tagCount=%d tasks=%d"),
			Label, EndCount, int32(Movement->MovementMode), Movement->CustomMovementMode, ASC->GetTagCount(SlidingTag), Tasks().Num()));
	}

	const TArray<TObjectPtr<UGameplayTask>>& Tasks() const
	{
		return *TasksProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<UGameplayTask>>>(Ability);
	}

	FAutomationTestBase* Test;
	UWorld* World = nullptr;
	ABwayPlayerState* Player = nullptr;
	APlayerController* Controller = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	ULyraPawnData* PawnData = nullptr;
	ULyraPawnExtensionComponent* Extension = nullptr;
	ULyraAbilitySystemComponent* ASC = nullptr;
	UBwayCharacterMovementComponent* Movement = nullptr;
	UGameplayAbility* Ability = nullptr;
	FGameplayAbilitySpecHandle AbilityHandle;
	FGameplayTag InputTag;
	FGameplayTag SlidingTag;
	FDelegateHandle EndedHandle;
	const FArrayProperty* TasksProperty = nullptr;
	const FObjectPropertyBase* CacheProperty = nullptr;
	TArray<TWeakObjectPtr<UGameplayTask>> PriorTasks;
	int32 EndCount = 0;
	int32 StartEndCount = 0;
	int32 AvatarCount = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySlideLifecycleTest,
	"Breakaway.CombatPolish.Slide.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwaySlideLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	BwaySlideLifecycleTest::FFixture Fixture(this);
	if (!Fixture.Setup()) return false;
	TestFalse(TEXT("Null actor info rejects activation safely"), Fixture.Ability->CanActivateAbility(Fixture.AbilityHandle, nullptr, nullptr, nullptr, nullptr));

	if (!Fixture.Begin(TEXT("input-release"))) return false;
	Fixture.Release();
	Fixture.ExpectEnded(TEXT("input-release"), MOVE_Walking);
	Fixture.Release();
	TestEqual(TEXT("Repeated release cannot end the activation twice"), Fixture.EndCount, Fixture.StartEndCount + 1);

	{
		// Zero is an authored hold policy, not an invalid configuration. Exercise
		// the real pre-movement exit check without introducing trajectory tuning.
		TGuardValue<float> ZeroThreshold(Fixture.Movement->MinSlideSpeed, 0.f);
		if (!Fixture.Begin(TEXT("held-zero-threshold"))) return false;
		Fixture.Movement->Velocity = FVector::ZeroVector;
		bool bRemainedActive = true;
		for (int32 Check = 0; Check < 30; ++Check)
		{
			Fixture.Movement->UpdateCharacterStateBeforeMovement(1.f / 60.f);
			const auto* Spec = Fixture.ASC->FindAbilitySpecFromHandle(Fixture.AbilityHandle);
			bRemainedActive &= Spec && Spec->InputPressed && Spec->IsActive() && Fixture.Movement->IsSliding()
				&& Fixture.Movement->Velocity.IsZero() && Fixture.ASC->GetTagCount(Fixture.SlidingTag) == 1
				&& Fixture.Tasks().Num() == 1 && Fixture.EndCount == Fixture.StartEndCount;
		}
		TestTrue(TEXT("Zero threshold retains held stationary slide through 30 real pre-movement checks"), bRemainedActive);
		Fixture.Release();
		Fixture.ExpectEnded(TEXT("held-zero-threshold"), MOVE_Walking);
	}
	{
		// Optional automatic exit has its own disposable positive threshold.
		TGuardValue<float> PositiveThreshold(Fixture.Movement->MinSlideSpeed, 100.f);
		if (!Fixture.Begin(TEXT("low-speed"))) return false;
		Fixture.Movement->Velocity = FVector::ZeroVector;
		Fixture.Movement->UpdateCharacterStateBeforeMovement(1.f / 60.f);
		Fixture.ExpectEnded(TEXT("low-speed"), MOVE_Walking);
		Fixture.Release();
		TestEqual(TEXT("Release after low-speed exit is harmless"), Fixture.EndCount, Fixture.StartEndCount + 1);
	}

	if (!Fixture.Begin(TEXT("slide-jump"))) return false;
	// The project override narrows access; dispatch through the public CMC API.
	TestTrue(TEXT("Production slide jump succeeds"), static_cast<UCharacterMovementComponent*>(Fixture.Movement)->DoJump(false, 1.f / 60.f));
	Fixture.ExpectEnded(TEXT("slide-jump"), MOVE_Falling);
	TestTrue(TEXT("Jump keeps its aerial state after slide ability cleanup"), Fixture.Movement->IsSlideJumping());
	TestTrue(TEXT("Jump retains upward velocity"), Fixture.Movement->Velocity.Z > 0.f);
	Fixture.Release();

	if (!Fixture.Begin(TEXT("external-fall"))) return false;
	Fixture.Movement->SetMovementMode(MOVE_Falling);
	Fixture.ExpectEnded(TEXT("external-fall"), MOVE_Falling);
	Fixture.Release();
	if (!Fixture.Begin(TEXT("external-custom-mode"))) return false;
	Fixture.Movement->SetMovementMode(MOVE_Custom, uint8(ECustomMovementMode::CMOVE_WallRun));
	Fixture.ExpectEnded(TEXT("external-custom-mode"), MOVE_Custom, uint8(ECustomMovementMode::CMOVE_WallRun));
	Fixture.Release();

	for (int32 Cycle = 0; Cycle < 3; ++Cycle)
	{
		if (!Fixture.Begin(TEXT("cancel-repeat"))) return false;
		Fixture.ASC->CancelAbilityHandle(Fixture.AbilityHandle);
		Fixture.ExpectEnded(TEXT("cancel-repeat"), MOVE_Walking);
		Fixture.Release();
	}

	// GAS publishes its ended callback after releasing the old active spec.
	// A new activation ending inside that callback must not inherit the old guard.
	if (!Fixture.Begin(TEXT("restart-from-ended"))) return false;
	bool bEndRestartAttempted = false;
	bool bEndRestartAccepted = false;
	const FDelegateHandle RestartEndedHandle = Fixture.ASC->OnAbilityEnded.AddLambda([&](const FAbilityEndedData& Data)
	{
		if (Data.AbilitySpecHandle != Fixture.AbilityHandle || bEndRestartAttempted) return;
		bEndRestartAttempted = true;
		bEndRestartAccepted = Fixture.ASC->TryActivateAbility(Fixture.AbilityHandle, false);
		for (UGameplayTask* Task : Fixture.Tasks()) Fixture.PriorTasks.Add(Task);
		Fixture.Release();
	});
	Fixture.ASC->CancelAbilityHandle(Fixture.AbilityHandle);
	Fixture.ASC->OnAbilityEnded.Remove(RestartEndedHandle);
	TestTrue(TEXT("Ended callback attempted a fresh activation"), bEndRestartAttempted);
	TestTrue(TEXT("Ended callback activation accepted"), bEndRestartAccepted);
	Fixture.ExpectEnded(TEXT("restart-from-ended"), MOVE_Walking, 0, 2);

	// The real slide cue is a synchronous external callback boundary inside
	// ActivateAbility. Restart there, then prove the old stack cannot add a second
	// input-release task to the new activation on this same instanced ability.
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!TestNotNull(TEXT("Slide cue manager"), CueManager)) return false;
	bool bCueRestartAttempted = false;
	bool bCueRestartAccepted = false;
	const FDelegateHandle RestartCueHandle = CueManager->OnGameplayCueRouted().AddLambda(
		[&](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event, const FGameplayCueParameters&, EGameplayCueExecutionOptions)
		{
			if (Target != Fixture.Pawn || Tag != BwayGameplayCueTags::MovementSlide || Event != EGameplayCueEvent::Executed || bCueRestartAttempted) return;
			bCueRestartAttempted = true;
			Fixture.ASC->CancelAbilityHandle(Fixture.AbilityHandle);
			bCueRestartAccepted = Fixture.ASC->TryActivateAbility(Fixture.AbilityHandle, false);
		});
	const bool bCueActivationStarted = Fixture.Begin(TEXT("restart-from-cue"));
	CueManager->OnGameplayCueRouted().Remove(RestartCueHandle);
	if (!bCueActivationStarted) return false;
	TestTrue(TEXT("Production cue callback attempted a fresh activation"), bCueRestartAttempted);
	TestTrue(TEXT("Production cue callback activation accepted"), bCueRestartAccepted);
	TestEqual(TEXT("Only the old cue activation ended before input release"), Fixture.EndCount, Fixture.StartEndCount + 1);
	Fixture.Release();
	Fixture.ExpectEnded(TEXT("restart-from-cue"), MOVE_Walking, 0, 2);

	// A rejected availability query must not keep the old avatar's movement
	// component. The PlayerState ASC and its inactive ability survive replacement.
	Fixture.Movement->SetMovementMode(MOVE_Custom, uint8(ECustomMovementMode::CMOVE_Slide));
	TestFalse(TEXT("An already sliding avatar rejects another slide activation"), Fixture.ASC->TryActivateAbility(Fixture.AbilityHandle, false));
	TestNull(TEXT("Rejected activation does not cache an avatar"), Fixture.CacheProperty->GetObjectPropertyValue_InContainer(Fixture.Ability));
	ABwayCharacterWithAbilities* RejectedPawn = Fixture.Pawn;
	UBwayCharacterMovementComponent* RejectedMovement = Fixture.Movement;
	if (!Fixture.ReplaceAvatar()) return false;
	TestTrue(TEXT("Old avatar deliberately still sliding in rejection fixture"), RejectedMovement->IsSliding());
	if (!Fixture.Begin(TEXT("replacement-after-rejection"))) return false;
	TestFalse(TEXT("Rejected old avatar has no ability observer"), RejectedPawn->MovementModeChangedDelegate.GetAllObjects().Contains(Fixture.Ability));
	RejectedMovement->SetMovementMode(MOVE_Falling);
	TestTrue(TEXT("Old avatar mode change cannot stop replacement activation"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.AbilityHandle)->IsActive());
	Fixture.Release();
	Fixture.ExpectEnded(TEXT("replacement-after-rejection"), MOVE_Walking);

	if (!Fixture.Begin(TEXT("avatar-teardown"))) return false;
	ABwayCharacterWithAbilities* ActiveOldPawn = Fixture.Pawn;
	UBwayCharacterMovementComponent* ActiveOldMovement = Fixture.Movement;
	Fixture.Extension->UninitializeAbilitySystem();
	Fixture.ExpectEnded(TEXT("avatar-teardown"), MOVE_Walking);
	if (!Fixture.ReplaceAvatar()) return false;
	if (!Fixture.Begin(TEXT("replacement-after-teardown"))) return false;
	ActiveOldMovement->SetMovementMode(MOVE_Falling);
	TestFalse(TEXT("Torn-down avatar has no observer"), ActiveOldPawn->MovementModeChangedDelegate.GetAllObjects().Contains(Fixture.Ability));
	TestTrue(TEXT("Torn-down avatar cannot end the next activation"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.AbilityHandle)->IsActive());
	Fixture.Release();
	Fixture.ExpectEnded(TEXT("replacement-after-teardown"), MOVE_Walking);
	return true;
}
#endif
