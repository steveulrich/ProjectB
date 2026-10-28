#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayCueManager.h"
#include "GameplayEffect.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayStatusPresentationLifetimeTest,
	"Breakaway.CombatPolish.Status.EffectPresentationLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayStatusPresentationLifetimeTest::RunTest(const FString&)
{
	struct FCase
	{
		const TCHAR* ClassPath;
		FGameplayTag Cue;
	};
	const FCase Cases[] = {
		{TEXT("/Script/HeroMorganRuntime.GE_Bway_KorrynBurdenSlow"), BwayGameplayCueTags::StatusSlowed},
		{TEXT("/Script/HeroMorganRuntime.GE_Bway_KorrynCircleSlow"), BwayGameplayCueTags::StatusSlowed},
		{TEXT("/Script/HeroMorganRuntime.GE_Bway_KorrynCursedWardSlow"), BwayGameplayCueTags::StatusSlowed},
		{TEXT("/Script/HeroMorganRuntime.GE_Bway_KorrynSilence"), BwayGameplayCueTags::StatusSilenced},
		{TEXT("/Script/HeroMorganRuntime.GE_Bway_KorrynArmorShred"), BwayGameplayCueTags::StatusArmorShred},
		{TEXT("/Script/HeroMorganRuntime.GE_Bway_KorrynCircleDamageAmp"), BwayGameplayCueTags::StatusDamageAmplified},
		{TEXT("/Script/HeroRawlinsRuntime.GE_Bway_RawlinsJailed"), BwayGameplayCueTags::StatusRooted}
	};
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	if (!TestNotNull(TEXT("Status presentation world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	struct FWorldLifetime
	{
		UWorld* World = nullptr;
		~FWorldLifetime() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } }
	};
	const TSharedRef<FWorldLifetime> Lifetime = MakeShared<FWorldLifetime>();
	Lifetime->World = World;
	World->InitializeActorsForPlay(FURL());
	AActor* Avatar = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Status target"), Avatar)) return false;
	USceneComponent* Root = NewObject<USceneComponent>(Avatar);
	Avatar->AddInstanceComponent(Root);
	Avatar->SetRootComponent(Root);
	Root->RegisterComponent();
	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Avatar);
	Avatar->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Avatar, Avatar);
	ASC->AddSpawnedAttribute(NewObject<UBwayHeroAttributeSet>(Avatar));
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1.f);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute(), 1.f);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 20.f);
	UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!TestNotNull(TEXT("Gameplay cue manager"), Manager)) return false;
	TMap<FGameplayTag, int32> ActiveRoutes, RemovedRoutes;
	const FDelegateHandle Listener = Manager->OnGameplayCueRouted().AddLambda(
		[&](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event, const FGameplayCueParameters&, EGameplayCueExecutionOptions)
		{
			if (Target != Avatar) return;
			if (Event == EGameplayCueEvent::WhileActive) ++ActiveRoutes.FindOrAdd(Tag);
			if (Event == EGameplayCueEvent::Removed) ++RemovedRoutes.FindOrAdd(Tag);
		});
	ON_SCOPE_EXIT { Manager->OnGameplayCueRouted().Remove(Listener); };
	auto Apply = [&](const TCHAR* Path, bool bShortDuration = false)
	{
		UClass* Class = LoadClass<UGameplayEffect>(nullptr, Path);
		if (!TestNotNull(FString::Printf(TEXT("Native status class %s"), Path), Class)) return FActiveGameplayEffectHandle();
		FGameplayEffectSpec Spec(Class->GetDefaultObject<UGameplayEffect>(), ASC->MakeEffectContext(), 1.f);
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.SetByCaller.Korryn.MoveSpeedMultiplier")), .75f);
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.SetByCaller.Korryn.IncomingDamageMultiplier")), 1.25f);
		if (bShortDuration) Spec.SetDuration(.03f, true);
		return ASC->ApplyGameplayEffectSpecToSelf(Spec);
	};
	for (const FCase& Entry : Cases)
	{
		UClass* Class = LoadClass<UGameplayEffect>(nullptr, Entry.ClassPath);
		if (!TestNotNull(TEXT("Native status definition"), Class)) return false;
		const UGameplayEffect* Definition = Class->GetDefaultObject<UGameplayEffect>();
		TestEqual(TEXT("Native status has exactly one cosmetic cue"), Definition->GameplayCues.Num(), 1);
		if (Definition->GameplayCues.Num() != 1) return false;
		TestTrue(TEXT("Native status binds the intended cue"), Definition->GameplayCues[0].GameplayCueTags.HasTagExact(Entry.Cue));
		const int32 ActiveBefore = ActiveRoutes.FindRef(Entry.Cue);
		const int32 RemovedBefore = RemovedRoutes.FindRef(Entry.Cue);
		const FActiveGameplayEffectHandle Handle = Apply(Entry.ClassPath);
		TestTrue(TEXT("Existing status effect applies"), Handle.IsValid());
		TestEqual(TEXT("GAS owns one active cosmetic tag"), ASC->GetTagCount(Entry.Cue), 1);
		TestEqual(TEXT("Application dispatches the persistent cue"), ActiveRoutes.FindRef(Entry.Cue), ActiveBefore + 1);
		ASC->RemoveActiveGameplayEffect(Handle);
		TestEqual(TEXT("Removing effect removes its cosmetic state"), ASC->GetTagCount(Entry.Cue), 0);
		TestEqual(TEXT("Removal dispatches cleanup once"), RemovedRoutes.FindRef(Entry.Cue), RemovedBefore + 1);
		ASC->RemoveActiveGameplayEffect(Handle);
		TestEqual(TEXT("Repeated removal does not replay cleanup"), RemovedRoutes.FindRef(Entry.Cue), RemovedBefore + 1);
	}
	const auto First = Apply(Cases[1].ClassPath);
	const auto Second = Apply(Cases[2].ClassPath);
	TestEqual(TEXT("Independent slow sources share counted cosmetic state"), ASC->GetTagCount(BwayGameplayCueTags::StatusSlowed), 2);
	ASC->RemoveActiveGameplayEffect(First);
	TestEqual(TEXT("Removing one slow preserves remaining presentation"), ASC->GetTagCount(BwayGameplayCueTags::StatusSlowed), 1);
	ASC->RemoveActiveGameplayEffect(Second);
	TestEqual(TEXT("Last slow removes presentation state"), ASC->GetTagCount(BwayGameplayCueTags::StatusSlowed), 0);
	const auto Expiring = Apply(Cases[0].ClassPath, true);
	TestTrue(TEXT("Finite slow has an active handle before expiry"), Expiring.IsValid());
	// TimerManager ticks once per engine frame. Yield between fixture ticks instead
	// of changing GFrameCounter or manually invoking GAS's expiry implementation.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Lifetime, ASC, Expiring, Ticks = 0]() mutable
	{
		Lifetime->World->Tick(LEVELTICK_All, .02f);
		if (++Ticks < 8) return false;
		TestNull(TEXT("GAS duration timer expires the effect"), ASC->GetActiveGameplayEffect(Expiring));
		TestEqual(TEXT("Expiry removes cosmetic state without manual cue calls"), ASC->GetTagCount(BwayGameplayCueTags::StatusSlowed), 0);
		TestEqual(TEXT("Presentation did not retain a gameplay slow after cleanup"), ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()), 1.f);
		return true;
	}));
	return true;
}
#endif
