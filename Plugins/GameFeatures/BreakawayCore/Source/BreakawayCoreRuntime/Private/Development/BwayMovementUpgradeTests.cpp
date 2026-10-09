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
#include "Economy/BwayEconomyLibrary.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/World.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayMovementUpgradeTest,
	"Breakaway.Economy.MovementUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayMovementUpgradeTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	World->SetGameState(GS);
	ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
	FEnumProperty* Phase = FindFProperty<FEnumProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentMatchPhase"));
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Controlled phase"), Phase))
	{
		World->DestroyWorld(false);
		return false;
	}
	Phase->GetUnderlyingProperty()->SetIntPropertyValue(Phase->ContainerPtrToValuePtr<void>(GS->GetRoundManagement()), static_cast<uint64>(EBwayMatchPhase::Warmup));
	if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
	ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
	UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
	if (!ASC->GetSet<ULyraHealthSet>()) ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(Player));
	UBwayUpgradeComponent* Shop = Player->UpgradeComponent;
	// Values deliberately differ from catalog tuning: this checks composition,
	// transaction rollback and the actual movement consumer, not alpha balance.
	UBwayUpgradeCatalog* Catalog = NewObject<UBwayUpgradeCatalog>(Player);
	FBwayUpgradeDefinition& Speed = Catalog->Upgrades.AddDefaulted_GetRef();
	Speed.Id = TEXT("MoveSpeed");
	Speed.EffectClass = UBwayUpgradeEffect_MoveSpeed::StaticClass();
	FBwayUpgradeRank& First = Speed.Ranks.AddDefaulted_GetRef();
	First.Cost = 30;
	First.Magnitude = 0.10f;
	FBwayUpgradeRank& Second = Speed.Ranks.AddDefaulted_GetRef();
	Second.Cost = 40;
	Second.Magnitude = 0.25f;
	TestTrue(TEXT("Movement catalog is supported"), Shop->ConfigureCatalog(Catalog));
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 200.0f);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1.0f);
	ABwayCharacterWithAbilities* Pawn = World->SpawnActor<ABwayCharacterWithAbilities>();
	ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	if (!TestNotNull(TEXT("Pawn extension"), Extension)) { World->DestroyWorld(false); return false; }
	ULyraPawnData* PawnData = NewObject<ULyraPawnData>(World);
	Extension->SetPawnData(PawnData);
	Extension->InitializeAbilitySystem(ASC, Player);
	UBwayCharacterMovementComponent* Movement = Pawn->GetBwayCharacterMovement();
	Movement->MaxWalkSpeed = 600.0f;
	Movement->SetMovementMode(MOVE_Walking);
	auto SlideCap = [Movement]()
	{
		// Getter coverage only; this world does not simulate slide physics.
		const auto PreviousMode = Movement->MovementMode;
		const uint8 PreviousCustom = Movement->CustomMovementMode;
		Movement->MovementMode = MOVE_Custom;
		Movement->CustomMovementMode = static_cast<uint8>(ECustomMovementMode::CMOVE_Slide);
		const float Result = Movement->GetMaxSpeed();
		Movement->MovementMode = PreviousMode;
		Movement->CustomMovementMode = PreviousCustom;
		return Result;
	};
	const float BaselineSlideCap = SlideCap();
	auto ExpectSpeed = [this, ASC, Movement, SlideCap, BaselineSlideCap](const TCHAR* Label, float Multiplier)
	{
		TestTrue(Label, FMath::IsNearlyEqual(ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()), Multiplier, 0.001f));
		TestTrue(FString(Label) + TEXT(" reaches movement consumer"), FMath::IsNearlyEqual(Movement->GetMaxSpeed(), 600.0f * Multiplier, 0.01f));
		TestTrue(FString(Label) + TEXT(" reaches slide speed cap"), FMath::IsNearlyEqual(SlideCap(), BaselineSlideCap * Multiplier, 0.01f));
	};
	ExpectSpeed(TEXT("Baseline"), 1.0f);
	TestTrue(TEXT("First movement purchase"), Shop->TryPurchaseUpgrade(Speed.Id, 0));
	ExpectSpeed(TEXT("Purchased bonus"), 1.10f);
	TestEqual(TEXT("Exact first debit"), ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 170.0f);
	TestFalse(TEXT("Duplicate request rejected"), Shop->TryPurchaseUpgrade(Speed.Id, 0));
	ASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
		[](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec) { return !Spec.Def->IsA<UBwayGameplayEffect_SpendGold>(); }));
	TestFalse(TEXT("Rejected debit rolls back movement rank"), Shop->TryPurchaseUpgrade(Speed.Id, 1));
	ExpectSpeed(TEXT("Rejected rank preserves previous speed"), 1.10f);
	ASC->GameplayEffectApplicationQueries.Reset();
	TestTrue(TEXT("Second movement rank"), Shop->TryPurchaseUpgrade(Speed.Id, 1));
	ExpectSpeed(TEXT("Rank replaces rather than stacks"), 1.25f);
	TestEqual(TEXT("Exact second debit"), ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 130.0f);
	UGameplayEffect* Slow = NewObject<UGameplayEffect>();
	Slow->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& SlowModifier = Slow->Modifiers.AddDefaulted_GetRef();
	SlowModifier.Attribute = UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute();
	SlowModifier.ModifierOp = EGameplayModOp::Multiplicitive;
	SlowModifier.ModifierMagnitude = FScalableFloat(0.5f);
	const FActiveGameplayEffectHandle SlowHandle = ASC->ApplyGameplayEffectToSelf(Slow, 1.0f, ASC->MakeEffectContext());
	ExpectSpeed(TEXT("Slow composes with upgrade"), 0.625f);
	ASC->RemoveActiveGameplayEffect(SlowHandle);
	ExpectSpeed(TEXT("Removing slow preserves upgrade"), 1.25f);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1.0f);
	ExpectSpeed(TEXT("Hero base reinitialization preserves upgrade"), 1.25f);
	Extension->UninitializeAbilitySystem();
	ABwayCharacterWithAbilities* Replacement = World->SpawnActor<ABwayCharacterWithAbilities>();
	ULyraPawnExtensionComponent* ReplacementExtension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Replacement);
	if (!TestNotNull(TEXT("Replacement pawn extension"), ReplacementExtension)) { World->DestroyWorld(false); return false; }
	ReplacementExtension->SetPawnData(PawnData);
	ReplacementExtension->InitializeAbilitySystem(ASC, Player);
	UBwayCharacterMovementComponent* ReplacementMovement = Replacement->GetBwayCharacterMovement();
	ReplacementMovement->MaxWalkSpeed = 800.0f;
	ReplacementMovement->SetMovementMode(MOVE_Walking);
	TestTrue(TEXT("Replacement pawn consumes persistent bonus"), FMath::IsNearlyEqual(ReplacementMovement->GetMaxSpeed(), 1000.0f, 0.01f));
	TestEqual(TEXT("Replacement retains owned rank"), Shop->GetOwnedRank(Speed.Id), 2);
	Shop->ResetUpgrades();
	TestTrue(TEXT("New-match reset removes movement bonus"), FMath::IsNearlyEqual(ReplacementMovement->GetMaxSpeed(), 800.0f, 0.01f));
	TestEqual(TEXT("New-match reset clears movement rank"), Shop->GetOwnedRank(Speed.Id), 0);
	ReplacementExtension->UninitializeAbilitySystem();
	World->DestroyWorld(false);
	return true;
}
#endif
