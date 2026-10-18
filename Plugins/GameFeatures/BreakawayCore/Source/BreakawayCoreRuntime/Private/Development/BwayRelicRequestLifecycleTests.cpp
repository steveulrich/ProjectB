#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Abilities/BwayGameplayAbility_RelicRequest.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "TimerManager.h"

namespace BwayRelicRequestLifecycleTest
{
class FRun : public IAutomationLatentCommand
{
public:
	explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual ~FRun() override
	{
		if (AbilitySystem) AbilitySystem->ClearAllAbilities();
		if (World) World->DestroyWorld(false);
	}

	virtual bool Update() override
	{
		if (!World)
		{
			return !Setup();
		}

		World->TimeSeconds += 0.1f;
		World->GetTimerManager().Tick(0.1f);
		Elapsed += 0.1f;
		if (AbilitySystem->HasMatchingGameplayTag(RequestTag) && Elapsed < RequestDuration + 1.0f)
		{
			return false;
		}

		Test->TestFalse(TEXT("Authored request effect expires"), AbilitySystem->HasMatchingGameplayTag(RequestTag));
		Test->TestFalse(TEXT("Prepared catch expires with the request window"), AbilitySystem->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
		Test->TestFalse(TEXT("Expired requester cannot pick up a neutral relic"), Relic->CanBePickedUpBy(Pawn));
		return true;
	}

private:
	bool Setup()
	{
		const UWorld::InitializationValues Initialization = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
			true, ERHIFeatureLevel::Num, &Initialization);
		if (!Test->TestNotNull(TEXT("Request fixture world"), World)) return false;
		ABwayGameState* GameState = World->SpawnActor<ABwayGameState>();
		ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		if (!Test->TestNotNull(TEXT("Request GameState"), GameState)
			|| !Test->TestNotNull(TEXT("Request PlayerState"), Player)
			|| !Test->TestNotNull(TEXT("Request controller"), Controller)) return false;
		World->SetGameState(GameState);
		if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(FVector(0.0f, 0.0f, 1000.0f), FRotator::ZeroRotator, Spawn);
		Relic = World->SpawnActor<ARelicActor>(FVector(5000.0f, 0.0f, 1000.0f), FRotator::ZeroRotator, Spawn);
		if (!Test->TestNotNull(TEXT("Request pawn"), Pawn) || !Test->TestNotNull(TEXT("Request relic"), Relic)) return false;
		Controller->Possess(Pawn);
		Pawn->SetPlayerState(Player);
		AbilitySystem = Player->GetLyraAbilitySystemComponent();
		if (!Test->TestNotNull(TEXT("Request ability system"), AbilitySystem)) return false;
		AbilitySystem->InitAbilityActorInfo(Player, Pawn);

		const URelicSettings* Settings = LoadObject<URelicSettings>(nullptr,
			TEXT("/BreakawayCore/GameModes/BallMode/Relic/DA_BW_RelicSettings_Default.DA_BW_RelicSettings_Default"));
		UClass* RequestClass = LoadClass<UBwayGameplayAbility_RelicRequest>(nullptr,
			TEXT("/BreakawayCore/GameModes/BallMode/Abilities/GA_BW_Relic_Request.GA_BW_Relic_Request_C"));
		if (!Test->TestNotNull(TEXT("Authored relic settings"), Settings)
			|| !Test->TestNotNull(TEXT("Authored request ability"), RequestClass)
			|| !Test->TestNotNull(TEXT("Relic manager"), GameState->RelicManagerComponent.Get())) return false;
		Relic->InitializeRelicData(Settings);
		GameState->RelicManagerComponent->SetActiveRelic(Relic);
		RequestTag = Settings->RequestingTag;
		if (!Test->TestTrue(TEXT("Authored request tag"), RequestTag.IsValid())) return false;
		const FGameplayTag RequestInputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.RelicRequest"));
		FGameplayAbilitySpec RequestSpec(RequestClass, 1);
		RequestSpec.GetDynamicSpecSourceTags().AddTag(RequestInputTag);
		const FGameplayAbilitySpecHandle AbilityHandle = AbilitySystem->GiveAbility(RequestSpec);
		Test->TestFalse(TEXT("Fresh pawn is not prepared to catch"), Relic->CanBePickedUpBy(Pawn));

		// 1. Held input press/release path: releasing RMB clears State.Relic.PreparingCatch immediately
		// while preserving the 3.0s Gameplay.Relic.Requesting indicator effect.
		AbilitySystem->AbilityInputTagPressed(RequestInputTag);
		AbilitySystem->ProcessAbilityInput(1.0f / 60.0f, false);
		Test->TestTrue(TEXT("Held RMB press activates request indicator tag"), AbilitySystem->HasMatchingGameplayTag(RequestTag));
		Test->TestTrue(TEXT("Held RMB press grants State.Relic.PreparingCatch"), AbilitySystem->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
		Test->TestTrue(TEXT("Held RMB requester can pick up the relic"), Relic->CanBePickedUpBy(Pawn));

		AbilitySystem->AbilityInputTagReleased(RequestInputTag);
		AbilitySystem->ProcessAbilityInput(1.0f / 60.0f, false);
		Test->TestFalse(TEXT("Releasing RMB clears State.Relic.PreparingCatch immediately"), AbilitySystem->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
		Test->TestFalse(TEXT("Releasing RMB revokes catch eligibility"), Relic->CanBePickedUpBy(Pawn));
		Test->TestTrue(TEXT("Releasing RMB preserves the 3.0s call-for-relic indicator tag"), AbilitySystem->HasMatchingGameplayTag(RequestTag));

		// 2. Re-pressing RMB while the 3.0s indicator is still active re-enters prepared catch cleanly without duplicate indicator GEs,
		// and acquiring the Relic clears both states.
		AbilitySystem->AbilityInputTagPressed(RequestInputTag);
		AbilitySystem->ProcessAbilityInput(1.0f / 60.0f, false);
		Test->TestTrue(TEXT("Re-pressing RMB restores State.Relic.PreparingCatch"), AbilitySystem->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
		Relic->OnPickedUp(Pawn);
		Test->TestEqual(TEXT("Prepared catcher acquires the relic"), Relic->CurrentCarrier.Get(), Pawn);
		Test->TestFalse(TEXT("Acquiring the relic clears State.Relic.PreparingCatch"), AbilitySystem->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
		Test->TestFalse(TEXT("Acquiring the relic clears Gameplay.Relic.Requesting"), AbilitySystem->HasMatchingGameplayTag(RequestTag));
		AbilitySystem->AbilityInputTagReleased(RequestInputTag);
		AbilitySystem->ProcessAbilityInput(1.0f / 60.0f, false);
		Relic->OnDropped();
		Relic->SetActorLocation(FVector(5000.0f, 0.0f, 1000.0f));
		World->TimeSeconds += Settings->PickupCooldownAfterDrop + 0.1f;

		// 3. Direct activation and early effect removal path.
		if (!Test->TestTrue(TEXT("Production request ability activates"), AbilitySystem->TryActivateAbility(AbilityHandle, false))) return false;
		Test->TestTrue(TEXT("Active request grants prepared catch"), AbilitySystem->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
		Test->TestTrue(TEXT("Active requester can pick up the relic"), Relic->CanBePickedUpBy(Pawn));
		AbilitySystem->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(RequestTag));
		Test->TestFalse(TEXT("Removing request effect also removes prepared catch"), AbilitySystem->HasMatchingGameplayTag(BwayGameplayTags::State_Relic_PreparingCatch));
		Test->TestFalse(TEXT("Removed requester cannot pick up the relic"), Relic->CanBePickedUpBy(Pawn));

		for (const FActiveGameplayEffectHandle EffectHandle : AbilitySystem->GetActiveEffects(FGameplayEffectQuery()))
		{
			AbilitySystem->RemoveActiveGameplayEffect(EffectHandle);
		}
		if (!Test->TestTrue(TEXT("Request reactivation succeeds"), AbilitySystem->TryActivateAbility(AbilityHandle, false))) return false;
		const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(RequestTag));
		const TArray<FActiveGameplayEffectHandle> RequestEffects = AbilitySystem->GetActiveEffects(Query);
		if (!Test->TestEqual(TEXT("One active request effect"), RequestEffects.Num(), 1)) return false;
		RequestDuration = AbilitySystem->GetActiveGameplayEffect(RequestEffects[0])->Spec.GetDuration();
		return Test->TestTrue(TEXT("Request duration is finite and bounded"), RequestDuration > 0.0f && RequestDuration <= 10.0f);
	}

	FAutomationTestBase* Test = nullptr;
	UWorld* World = nullptr;
	ULyraAbilitySystemComponent* AbilitySystem = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	ARelicActor* Relic = nullptr;
	FGameplayTag RequestTag;
	float RequestDuration = 0.0f;
	float Elapsed = 0.0f;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayRelicRequestLifecycleTest,
	"Breakaway.Relic.RequestLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayRelicRequestLifecycleTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(BwayRelicRequestLifecycleTest::FRun(this));
	return true;
}
#endif
