#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Abilities/BwayGameplayAbility_Base.h"
#include "Abilities/BwayGameplayAbility_ShieldBash.h"
#include "Abilities/BwayGameplayEffect_ArgusCooldowns.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Economy/BwayEconomyLibrary.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/World.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCooldownUpgradeTest, "Breakaway.Economy.CooldownUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCooldownUpgradeTest::RunTest(const FString&)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("World"), World)) return false;
	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	World->SetGameState(GS);
	ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
	FEnumProperty* Phase = FindFProperty<FEnumProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentMatchPhase"));
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Phase"), Phase)) { World->DestroyWorld(false); return false; }
	Phase->GetUnderlyingProperty()->SetIntPropertyValue(Phase->ContainerPtrToValuePtr<void>(GS->GetRoundManagement()), static_cast<uint64>(EBwayMatchPhase::Warmup));
	if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
	ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
	UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
	UBwayUpgradeComponent* Shop = Player->UpgradeComponent;
	UBwayUpgradeCatalog* Catalog = NewObject<UBwayUpgradeCatalog>(Player);
	FBwayUpgradeDefinition& Entry = Catalog->Upgrades.AddDefaulted_GetRef();
	Entry.Id = TEXT("CooldownReduction");
	Entry.EffectClass = UBwayUpgradeEffect_CooldownReduction::StaticClass();
	FBwayUpgradeRank& First = Entry.Ranks.AddDefaulted_GetRef(); First.Cost = 20; First.Magnitude = 0.2f;
	FBwayUpgradeRank& Second = Entry.Ranks.AddDefaulted_GetRef(); Second.Cost = 30; Second.Magnitude = 0.4f;
	TestTrue(TEXT("Supported cooldown catalog"), Shop->ConfigureCatalog(Catalog));
	ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 200.f);

	struct FCase { FGameplayAbilitySpecHandle Handle; UGameplayAbility* Ability; float Baseline = 0; };
	TArray<FCase> Cases;
	const TCHAR* SetPaths[] = {
		TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_AbilitySet_Argus.DA_BW_AbilitySet_Argus"),
		TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_AbilitySet_Gunslinger.DA_BW_AbilitySet_Gunslinger"),
		TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_AbilitySet_Alona.DA_BW_AbilitySet_Alona"),
		TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver.DA_BW_AbilitySet_Hexweaver")
	};
	FArrayProperty* GrantsProperty = FindFProperty<FArrayProperty>(ULyraAbilitySet::StaticClass(), TEXT("GrantedGameplayAbilities"));
	if (!TestNotNull(TEXT("Ability grants property"), GrantsProperty)) { World->DestroyWorld(false); return false; }
	int32 NoCooldownCount = 0;
	for (const TCHAR* Path : SetPaths)
	{
		const ULyraAbilitySet* Set = LoadObject<ULyraAbilitySet>(nullptr, Path);
		if (!TestNotNull(Path, Set)) continue;
		const auto* Grants = GrantsProperty->ContainerPtrToValuePtr<TArray<FLyraAbilitySet_GameplayAbility>>(Set);
		int32 KitCooldowns = 0;
		for (const auto& Grant : *Grants)
		{
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(Grant.Ability, Grant.AbilityLevel));
			FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			UGameplayAbility* Ability = Spec ? Spec->GetPrimaryInstance() : nullptr;
			if (!TestNotNull(TEXT("Granted ability instance"), Ability)) continue;
			if (!Ability->GetCooldownGameplayEffect())
			{
				const int32 Before = ASC->GetActiveEffects(FGameplayEffectQuery()).Num();
				Ability->ApplyCooldown(Handle, ASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo());
				TestEqual(TEXT("No cooldown is invented for primary/placement"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), Before);
				++NoCooldownCount;
				continue;
			}
			TestTrue(TEXT("Configured cooldown uses shared ability base"), Ability->IsA<UBwayGameplayAbility_Base>());
			Cases.Add({Handle, Ability});
			++KitCooldowns;
		}
		TestEqual(FString::Printf(TEXT("Five configured cooldown abilities in %s"), Path), KitCooldowns, 5);
	}
	TestEqual(TEXT("All four kits covered"), Cases.Num(), 20);
	TestEqual(TEXT("Four placement abilities have no cooldown"), NoCooldownCount, 4);

	// A concrete fixture ability without a kit override exercises the GE-defined
	// fallback. Modify only this granted instance, never a shared class default.
	const auto FallbackHandle = ASC->GiveAbility(FGameplayAbilitySpec(UBwayGameplayAbility_ShieldBash::StaticClass(), 1));
	UGameplayAbility* Fallback = ASC->FindAbilitySpecFromHandle(FallbackHandle)->GetPrimaryInstance();
	FClassProperty* CooldownClass = FindFProperty<FClassProperty>(UGameplayAbility::StaticClass(), TEXT("CooldownGameplayEffectClass"));
	if (TestNotNull(TEXT("Fallback instance"), Fallback) && TestNotNull(TEXT("Cooldown property"), CooldownClass))
	{
		CooldownClass->SetObjectPropertyValue_InContainer(Fallback, UGE_Bway_Cooldown_NoRetreat::StaticClass());
		Cases.Add({FallbackHandle, Fallback});
	}
	auto Apply = [ASC](const FCase& Case)
	{
		Case.Ability->ApplyCooldown(Case.Handle, ASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo());
		FGameplayEffectQuery Query;
		Query.EffectDefinition = Case.Ability->GetCooldownGameplayEffect()->GetClass();
		return ASC->GetActiveEffects(Query);
	};
	auto Measure = [this, ASC, &Cases, &Apply](float Factor, bool bBaseline)
	{
		for (FCase& Case : Cases)
		{
			const FString Name = Case.Ability->GetClass()->GetName();
			TestTrue(Name + TEXT(" ready before cooldown"), Case.Ability->CheckCooldown(Case.Handle, ASC->AbilityActorInfo.Get()));
			const auto Handles = Apply(Case);
			if (!TestEqual(Name + TEXT(" one active cooldown"), Handles.Num(), 1)) continue;
			const float Duration = ASC->GetActiveGameplayEffect(Handles[0])->Spec.GetDuration();
			if (bBaseline) Case.Baseline = Duration;
			TestTrue(Name + TEXT(" positive finite duration scaled once"), FMath::IsFinite(Duration) && Duration > 0 && FMath::IsNearlyEqual(Duration, Case.Baseline * Factor, 0.001f));
			TestFalse(Name + TEXT(" cooldown tags block reuse"), Case.Ability->CheckCooldown(Case.Handle, ASC->AbilityActorInfo.Get()));
			ASC->RemoveActiveGameplayEffect(Handles[0]);
			TestTrue(Name + TEXT(" removal clears cooldown gate"), Case.Ability->CheckCooldown(Case.Handle, ASC->AbilityActorInfo.Get()));
		}
	};
	Measure(1.f, true);
	TestTrue(TEXT("First rank purchased"), Shop->TryPurchaseUpgrade(Entry.Id, 0));
	Measure(0.8f, false);
	TestEqual(TEXT("Exact first debit"), ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 180.f);
	TestFalse(TEXT("Stale rank rejected"), Shop->TryPurchaseUpgrade(Entry.Id, 0));
	ASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
		[](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec) { return !Spec.Def->IsA<UBwayGameplayEffect_SpendGold>(); }));
	TestFalse(TEXT("Rejected debit rolls back cooldown rank"), Shop->TryPurchaseUpgrade(Entry.Id, 1));
	TestEqual(TEXT("Rollback preserves reduction"), ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()), 0.2f);
	ASC->GameplayEffectApplicationQueries.Reset();
	TestTrue(TEXT("Second rank replaces first"), Shop->TryPurchaseUpgrade(Entry.Id, 1));
	Measure(0.6f, false);
	TestEqual(TEXT("Exact second debit"), ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 150.f);
	if (!Cases.IsEmpty())
	{
		const auto Handles = Apply(Cases[0]);
		Shop->ResetUpgrades();
		if (Handles.Num() == 1)
		{
			TestTrue(TEXT("Reset does not rewrite an already active cooldown"), FMath::IsNearlyEqual(ASC->GetActiveGameplayEffect(Handles[0])->Spec.GetDuration(), Cases[0].Baseline * 0.6f, 0.001f));
			ASC->RemoveActiveGameplayEffect(Handles[0]);
		}
	}
	TestEqual(TEXT("Reset removes reduction"), ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()), 0.f);
	Measure(1.f, false);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetCooldownReductionAttribute(), 2.f);
	TestEqual(TEXT("Reduction clamped to one"), ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()), 1.f);
	for (const FCase& Case : Cases) TestEqual(TEXT("Full reduction creates no cooldown"), Apply(Case).Num(), 0);
	ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetCooldownReductionAttribute(), -1.f);
	TestEqual(TEXT("Negative reduction clamped"), ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()), 0.f);

	// Verify variable kit-driven PrimaryCooldown across all four hero primary attack abilities.
	for (const FCase& Case : Cases)
	{
		if (!Case.Ability->GetClass()->GetName().Contains(TEXT("Primary"))) continue;
		const FSoftObjectProperty* ConfigProp = FindFProperty<FSoftObjectProperty>(Case.Ability->GetClass(), TEXT("KitConfig"));
		UObject* Config = ConfigProp ? ConfigProp->GetPropertyValue_InContainer(Case.Ability).LoadSynchronous() : nullptr;
		FFloatProperty* PrimaryCooldownProp = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), TEXT("PrimaryCooldown")) : nullptr;
		if (!TestNotNull(TEXT("Primary ability KitConfig loaded"), Config) || !TestNotNull(TEXT("KitConfig PrimaryCooldown property"), PrimaryCooldownProp)) continue;
		const float OriginalCooldown = PrimaryCooldownProp->GetPropertyValue_InContainer(Config);
		PrimaryCooldownProp->SetPropertyValue_InContainer(Config, 1.25f);
		const auto CustomHandles = Apply(Case);
		if (TestEqual(Case.Ability->GetClass()->GetName() + TEXT(" custom kit PrimaryCooldown applied"), CustomHandles.Num(), 1))
		{
			TestTrue(Case.Ability->GetClass()->GetName() + TEXT(" uses variable KitConfig.PrimaryCooldown"),
				FMath::IsNearlyEqual(ASC->GetActiveGameplayEffect(CustomHandles[0])->Spec.GetDuration(), 1.25f, 0.001f));
			ASC->RemoveActiveGameplayEffect(CustomHandles[0]);
		}
		PrimaryCooldownProp->SetPropertyValue_InContainer(Config, OriginalCooldown);
	}

	ASC->ClearAllAbilities();
	World->DestroyWorld(false);
	return true;
}
#endif
