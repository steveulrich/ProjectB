#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace BwayFlockLifecycleTest
{
// This isolated authority test exercises actual activation and the production
// end timer. It does not stand in for prediction, movement simulation or HUD proof.
class FRun : public IAutomationLatentCommand
{
public:
	explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual ~FRun() override
	{
		if (ASC) ASC->ClearAllAbilities();
		if (World) World->DestroyWorld(false);
	}

	virtual bool Update() override
	{
		if (!World && !Setup()) return true;
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 20.0) { Test->AddError(TEXT("Flock lifecycle timeout")); return true; }
		if (!bRunning)
		{
			if (Cycle == 4) return true;
			// Clear only the preceding case's cooldown in this isolated test world.
			// Natural cooldown expiry remains a separate multiplayer acceptance gate.
			for (const auto Handle : ASC->GetActiveEffects(FGameplayEffectQuery())) ASC->RemoveActiveGameplayEffect(Handle);
			Walk = 413.f + Cycle * 37.f;
			Fly = 321.f + Cycle * 113.f;
			Gravity = 0.63f + Cycle * 0.11f;
			Mode = Cycle < 2 ? MOVE_Falling : MOVE_Custom;
			CustomMode = Mode == MOVE_Custom ? 7 : 0;
			Response = Cycle < 2 ? ECR_Block : ECR_Overlap;
			Move->SetMovementMode(Mode, CustomMode);
			Move->MaxWalkSpeed = Walk;
			Move->MaxFlySpeed = Fly;
			Move->GravityScale = Gravity;
			Move->bOrientRotationToMovement = (Cycle % 2 == 0);
			Pawn->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, Response);
			if (!Test->TestTrue(TEXT("Real Flock activation accepted"), ASC->TryActivateAbility(AbilityHandle))) return true;
			FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
			if (!Test->TestTrue(TEXT("Flock remains active during its window"), Spec && Spec->IsActive())) return true;
			Test->TestEqual(TEXT("Hover flight applied"), Move->MovementMode.GetValue(), MOVE_Flying);
			Test->TestEqual(TEXT("Configured hover speed applied"), Move->MaxFlySpeed, Hover);
			Test->TestEqual(TEXT("Hover gravity applied"), Move->GravityScale, 0.f);
			Test->TestEqual(TEXT("Pawn pass-through applied"), Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
			Test->TestTrue(TEXT("Ethereal tag applied"), ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal));
			Test->TestTrue(TEXT("Immunity tag applied"), ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
			LastTick = Now;
			Elapsed = 0.f;
			bRunning = true;
			return false;
		}
		// Advance only this test world's timers once per engine frame; no sleeps,
		// global frame-counter edits, private EndAbility calls or CDO mutations.
		const float Delta = float(Now - LastTick);
		LastTick = Now;
		Elapsed += Delta;
		World->GetTimerManager().Tick(Delta);
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
		if (!Spec) { Test->AddError(TEXT("Flock spec disappeared")); return true; }
		const bool bCancel = Cycle % 2 == 0;
		if (bCancel && Elapsed >= 0.15f && Spec->IsActive()) ASC->CancelAbilityHandle(AbilityHandle);
		if (Spec->IsActive()) return false;
		if (!bCancel) Test->TestTrue(TEXT("Natural end occurred at configured duration"), Elapsed >= Duration - 0.05f && Elapsed <= Duration + 0.5f);
		Test->TestEqual(TEXT("Pre-activation fly speed restored"), Move->MaxFlySpeed, Fly);
		Test->TestEqual(TEXT("Pre-activation walk speed restored"), Move->MaxWalkSpeed, Walk);
		Test->TestEqual(TEXT("Pre-activation gravity restored"), Move->GravityScale, Gravity);
		Test->TestEqual(TEXT("Pre-activation movement mode restored"), Move->MovementMode.GetValue(), Mode);
		Test->TestEqual(TEXT("Pre-activation custom mode restored"), Move->CustomMovementMode, CustomMode);
		Test->TestEqual(TEXT("Pre-activation orientation restored"), bool(Move->bOrientRotationToMovement), Cycle % 2 == 0);
		Test->TestEqual(TEXT("Pre-activation pawn collision restored"), Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), Response);
		Test->TestFalse(TEXT("Ethereal tag released"), ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal));
		Test->TestFalse(TEXT("Immunity tag released"), ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
		Test->AddInfo(FString::Printf(TEXT("Flock case=%d exit=%s elapsed=%.3f duration=%.3f savedFly=%.3f restoredFly=%.3f mode=%d custom=%d"),
			Cycle, bCancel ? TEXT("cancel") : TEXT("timer"), Elapsed, Duration, Fly, Move->MaxFlySpeed, int32(Mode), CustomMode));
		++Cycle;
		bRunning = false;
		return false;
	}

private:
	bool Setup()
	{
		const UWorld::InitializationValues Settings = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test->TestNotNull(TEXT("Fixture world"), World)) return false;
		ABwayGameState* GameState = World->SpawnActor<ABwayGameState>();
		if (!Test->TestNotNull(TEXT("Game state and experience manager"), GameState)) return false;
		World->SetGameState(GameState);
		ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>();
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		if (!Test->TestNotNull(TEXT("Player"), Player) || !Test->TestNotNull(TEXT("Pawn"), Pawn) || !Test->TestNotNull(TEXT("Controller"), Controller)) return false;
		if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
		Controller->Possess(Pawn);
		Pawn->SetPlayerState(Player);
		ASC = Player->GetLyraAbilitySystemComponent();
		ASC->InitAbilityActorInfo(Player, Pawn);
		UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
		Move = Pawn->GetCharacterMovement();
		const ULyraAbilitySet* Set = LoadObject<ULyraAbilitySet>(nullptr,
			TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver.DA_BW_AbilitySet_Hexweaver"));
		const FArrayProperty* GrantsProperty = FindFProperty<FArrayProperty>(ULyraAbilitySet::StaticClass(), TEXT("GrantedGameplayAbilities"));
		if (!Test->TestNotNull(TEXT("Production Korryn ability set"), Set) || !Test->TestNotNull(TEXT("Ability grants"), GrantsProperty)) return false;
		const auto* Grants = GrantsProperty->ContainerPtrToValuePtr<TArray<FLyraAbilitySet_GameplayAbility>>(Set);
		for (const auto& Grant : *Grants)
		{
			for (const UClass* Class = Grant.Ability.Get(); Class; Class = Class->GetSuperClass())
			{
				if (Class->GetName() == TEXT("BwayGameplayAbility_KorrynFlock"))
				{
					AbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(Grant.Ability, Grant.AbilityLevel));
					break;
				}
			}
		}
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
		UGameplayAbility* Ability = Spec ? Spec->GetPrimaryInstance() : nullptr;
		if (!Test->TestNotNull(TEXT("Production Flock instance"), Ability)) return false;
		const FSoftObjectProperty* Kit = FindFProperty<FSoftObjectProperty>(Ability->GetClass(), TEXT("KitConfig"));
		if (!Test->TestNotNull(TEXT("Kit reference"), Kit)) return false;
		UObject* Config = Kit->GetPropertyValue_InContainer(Ability).LoadSynchronous();
		if (!Test->TestNotNull(TEXT("Production kit config"), Config)) return false;
		const FFloatProperty* DurationProperty = FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("FlockDuration"));
		const FFloatProperty* HoverProperty = FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("FlockHoverSpeed"));
		if (!Test->TestNotNull(TEXT("Duration property"), DurationProperty) || !Test->TestNotNull(TEXT("Hover property"), HoverProperty)) return false;
		Duration = DurationProperty->GetPropertyValue_InContainer(Config);
		Hover = HoverProperty->GetPropertyValue_InContainer(Config);
		Started = FPlatformTime::Seconds();
		return Test->TestTrue(TEXT("Bounded production duration"), Duration > 0.3f && Duration < 5.f);
	}

	FAutomationTestBase* Test;
	UWorld* World = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	ULyraAbilitySystemComponent* ASC = nullptr;
	UCharacterMovementComponent* Move = nullptr;
	FGameplayAbilitySpecHandle AbilityHandle;
	double Started = 0, LastTick = 0;
	float Duration = 0, Hover = 0, Elapsed = 0, Walk = 0, Fly = 0, Gravity = 0;
	EMovementMode Mode = MOVE_Falling;
	uint8 CustomMode = 0;
	ECollisionResponse Response = ECR_Block;
	int32 Cycle = 0;
	bool bRunning = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayFlockLifecycleTest, "Breakaway.Abilities.Korryn.FlockLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayFlockLifecycleTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(BwayFlockLifecycleTest::FRun(this));
	return true;
}
#endif
