#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Character/LyraPawnData.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace BwayHeroBaseSpeedTest
{
// Exercises the production ASC attach/detach and attribute notification consumers.
// OnRep is invoked with received property data; this is not a network transport test.
struct FFixture
{
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture()
	{
		if (Extension) Extension->UninitializeAbilitySystem();
		if (World) World->DestroyWorld(false);
	}
	FAutomationTestBase& Test;
	UWorld* World = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	UBwayCharacterMovementComponent* Movement = nullptr;
	ULyraPawnExtensionComponent* Extension = nullptr;

	bool Setup()
	{
		const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test.TestNotNull(TEXT("Speed fixture world"), World)) return false;
		auto* GameState = World->SpawnActor<ABwayGameState>();
		if (!Test.TestNotNull(TEXT("Speed fixture game state"), GameState)) return false;
		World->SetGameState(GameState);
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>();
		if (!Test.TestNotNull(TEXT("Speed fixture pawn"), Pawn)) return false;
		Movement = Pawn->GetBwayCharacterMovement();
		Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
		if (!Test.TestNotNull(TEXT("Speed movement"), Movement) || !Test.TestNotNull(TEXT("Speed pawn extension"), Extension)) return false;
		Extension->SetPawnData(NewObject<ULyraPawnData>(World));
		Movement->SetMovementMode(MOVE_Walking);
		return true;
	}

	ABwayPlayerState* CreatePlayer(float Rating)
	{
		auto* Player = World->SpawnActor<ABwayPlayerState>();
		if (!Test.TestNotNull(TEXT("Speed fixture player"), Player)) return nullptr;
		if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
		auto* ASC = Player->GetLyraAbilitySystemComponent();
		if (!Test.TestNotNull(TEXT("Speed fixture ASC"), ASC)) return nullptr;
		UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
		if (!ASC->GetSet<ULyraHealthSet>()) ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(Player));
		ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedRatingAttribute(), Rating);
		return Player;
	}

	void Attach(ABwayPlayerState* Player)
	{
		Extension->InitializeAbilitySystem(Player->GetLyraAbilitySystemComponent(), Player);
	}

	bool ReceiveRating(ULyraAbilitySystemComponent* ASC, float Rating)
	{
		auto* Set = const_cast<UBwayHeroAttributeSet*>(ASC->GetSet<UBwayHeroAttributeSet>());
		auto* Property = FindFProperty<FStructProperty>(UBwayHeroAttributeSet::StaticClass(), TEXT("MoveSpeedRating"));
		auto* Notify = Set ? Set->FindFunction(TEXT("OnRep_MoveSpeedRating")) : nullptr;
		if (!Test.TestNotNull(TEXT("Replicated rating property"), Property) || !Test.TestNotNull(TEXT("Real rating RepNotify"), Notify)) return false;
		auto* Value = Property->ContainerPtrToValuePtr<FGameplayAttributeData>(Set);
		struct FNotifyParameters { FGameplayAttributeData OldValue; } Parameters{*Value};
		if (!Test.TestEqual(TEXT("RepNotify parameter layout"), int32(Notify->ParmsSize), int32(sizeof(Parameters)))) return false;
		Value->SetBaseValue(Rating);
		Value->SetCurrentValue(Rating);
		Set->ProcessEvent(Notify, &Parameters);
		return true;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayHeroBaseSpeedBindingTest,
	"Breakaway.Movement.HeroBaseSpeed.BindingLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayHeroBaseSpeedBindingTest::RunTest(const FString&)
{
	using namespace BwayHeroBaseSpeedTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	auto* First = F.CreatePlayer(10.5f);
	auto* Second = F.CreatePlayer(7.5f);
	if (!First || !Second) return false;
	auto* FirstASC = First->GetLyraAbilitySystemComponent();
	auto* SecondASC = Second->GetLyraAbilitySystemComponent();
	const auto Rating = UBwayHeroAttributeSet::GetMoveSpeedRatingAttribute();
	const auto Multiplier = UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute();

	F.Movement->MaxWalkSpeed = 600.f;
	F.Attach(First);
	TestEqual(TEXT("Rating received before avatar attach seeds the real movement component"), F.Movement->MaxWalkSpeed, 630.f);
	FirstASC->SetNumericAttributeBase(Rating, 12.f);
	TestEqual(TEXT("Later authoritative rating change reaches movement"), F.Movement->MaxWalkSpeed, 720.f);
	F.Movement->BindAbilitySystem(FirstASC);
	TestEqual(TEXT("Repeated bind preserves current rating"), F.Movement->MaxWalkSpeed, 720.f);

	F.Attach(Second);
	TestEqual(TEXT("Replacement ASC seeds its own current rating"), F.Movement->MaxWalkSpeed, 450.f);
	TestFalse(TEXT("Old rating delegate detached"), FirstASC->GetGameplayAttributeValueChangeDelegate(Rating).IsBoundToObject(F.Movement));
	TestFalse(TEXT("Old multiplier delegate detached"), FirstASC->GetGameplayAttributeValueChangeDelegate(Multiplier).IsBoundToObject(F.Movement));
	FirstASC->SetNumericAttributeBase(Rating, 15.f);
	TestEqual(TEXT("Old ASC cannot mutate replacement movement"), F.Movement->MaxWalkSpeed, 450.f);
	SecondASC->SetNumericAttributeBase(Rating, 11.f);
	TestEqual(TEXT("Replacement ASC remains observed"), F.Movement->MaxWalkSpeed, 660.f);

	F.Extension->UninitializeAbilitySystem();
	TestFalse(TEXT("Uninitialized rating delegate detached"), SecondASC->GetGameplayAttributeValueChangeDelegate(Rating).IsBoundToObject(F.Movement));
	TestFalse(TEXT("Uninitialized multiplier delegate detached"), SecondASC->GetGameplayAttributeValueChangeDelegate(Multiplier).IsBoundToObject(F.Movement));
	SecondASC->SetNumericAttributeBase(Rating, 8.f);
	TestEqual(TEXT("Detached ASC cannot alter the old pawn"), F.Movement->MaxWalkSpeed, 660.f);
	F.Movement->BindAbilitySystem(nullptr);
	TestEqual(TEXT("Null rebind preserves fallback"), F.Movement->MaxWalkSpeed, 660.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayHeroBaseSpeedRepNotifyTest,
	"Breakaway.Movement.HeroBaseSpeed.RepNotifyAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayHeroBaseSpeedRepNotifyTest::RunTest(const FString&)
{
	using namespace BwayHeroBaseSpeedTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	auto* Player = F.CreatePlayer(0.f);
	if (!Player) return false;
	auto* ASC = Player->GetLyraAbilitySystemComponent();
	F.Movement->MaxWalkSpeed = 725.f;
	F.Attach(Player);
	TestEqual(TEXT("Uninitialized zero rating preserves authored fallback"), F.Movement->MaxWalkSpeed, 725.f);
	if (!F.ReceiveRating(ASC, 10.5f)) return false;
	TestEqual(TEXT("Actual GAS RepNotify updates client movement base"), F.Movement->MaxWalkSpeed, 630.f);
	if (!F.ReceiveRating(ASC, 850.f)) return false;
	TestEqual(TEXT("Direct cm/s ratings retain existing conversion semantics"), F.Movement->MaxWalkSpeed, 850.f);
	if (!F.ReceiveRating(ASC, 0.f)) return false;
	TestEqual(TEXT("Uninitialized replication cannot replace valid base with minimum speed"), F.Movement->MaxWalkSpeed, 850.f);

	auto* Set = const_cast<UBwayHeroAttributeSet*>(ASC->GetSet<UBwayHeroAttributeSet>());
	for (float Invalid : {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		// Seed invalid data before attachment without passing it through GAS math.
		Set->InitMoveSpeedRating(Invalid);
		F.Movement->BindAbilitySystem(ASC);
		TestEqual(TEXT("Invalid pre-bind rating cannot corrupt movement"), F.Movement->MaxWalkSpeed, 850.f);
	}
	Set->InitMoveSpeedRating(10.f);
	F.Movement->BindAbilitySystem(ASC);
	TestEqual(TEXT("Valid subsequent attachment still seeds movement"), F.Movement->MaxWalkSpeed, 600.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayHeroBaseSpeedMultiplierTest,
	"Breakaway.Movement.HeroBaseSpeed.MultiplierComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayHeroBaseSpeedMultiplierTest::RunTest(const FString&)
{
	using namespace BwayHeroBaseSpeedTest;
	FFixture F(*this);
	if (!F.Setup()) return false;
	auto* Player = F.CreatePlayer(10.5f);
	if (!Player) return false;
	auto* ASC = Player->GetLyraAbilitySystemComponent();
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1.25f);
	F.Attach(Player);
	TestEqual(TEXT("Attach keeps unmultiplied hero base"), F.Movement->MaxWalkSpeed, 630.f);
	TestEqual(TEXT("Persistent multiplier applied exactly once"), F.Movement->GetMaxSpeed(), 787.5f);

	UGameplayEffect* Slow = NewObject<UGameplayEffect>();
	Slow->DurationPolicy = EGameplayEffectDurationType::Infinite;
	auto& Modifier = Slow->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute();
	Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
	Modifier.ModifierMagnitude = FScalableFloat(0.5f);
	const auto SlowHandle = ASC->ApplyGameplayEffectToSelf(Slow, 1.f, ASC->MakeEffectContext());
	TestTrue(TEXT("Real temporary slow applies"), SlowHandle.IsValid());
	TestEqual(TEXT("Slow does not bake into base speed"), F.Movement->MaxWalkSpeed, 630.f);
	TestEqual(TEXT("Slow composes once with persistent multiplier"), F.Movement->GetMaxSpeed(), 393.75f);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedRatingAttribute(), 12.f);
	TestEqual(TEXT("Hero base update while slowed remains unmultiplied"), F.Movement->MaxWalkSpeed, 720.f);
	TestEqual(TEXT("Updated hero base uses current modifiers once"), F.Movement->GetMaxSpeed(), 450.f);
	ASC->RemoveActiveGameplayEffect(SlowHandle);
	TestEqual(TEXT("Removing slow preserves multiplier and hero base"), F.Movement->GetMaxSpeed(), 900.f);

	// GetMaxSpeed's slide branch has an independent authored cap. Inspect the getter
	// without entering collision physics in this disposable non-simulating world.
	F.Movement->MovementMode = MOVE_Custom;
	F.Movement->CustomMovementMode = uint8(ECustomMovementMode::CMOVE_Slide);
	TestEqual(TEXT("Hero walk rating does not replace the authored slide speed cap"),
		F.Movement->GetMaxSpeed(), F.Movement->BaseSlideSpeed * F.Movement->SlideMaxSpeedFactor * 1.25f);
	F.Movement->MovementMode = MOVE_Walking;
	F.Movement->CustomMovementMode = 0;
	return true;
}
#endif
