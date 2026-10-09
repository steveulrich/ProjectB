#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Combat/BwayLifeLeechComponent.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "LyraGameplayTags.h"
#include "Messages/LyraVerbMessage.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLifeLeechTest, "Breakaway.Economy.LifeLeech",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayLifeLeechTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		Instance->Shutdown();
		GEngine->DestroyWorldContext(World);
	};
	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	World->SetGameState(GS);
	FArrayProperty* TeamsProperty = FindFProperty<FArrayProperty>(ABwayGameState::StaticClass(), TEXT("Teams"));
	if (!TestNotNull(TEXT("Team fixture property"), TeamsProperty)) return false;
	TArray<FTeamInfo>* Teams = TeamsProperty->ContainerPtrToValuePtr<TArray<FTeamInfo>>(GS);
	Teams->Add(FTeamInfo(0));
	Teams->Add(FTeamInfo(1));
	FEnumProperty* Phase = FindFProperty<FEnumProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentMatchPhase"));
	if (!TestNotNull(TEXT("Phase"), Phase)) return false;
	Phase->GetUnderlyingProperty()->SetIntPropertyValue(Phase->ContainerPtrToValuePtr<void>(GS->GetRoundManagement()), static_cast<uint64>(EBwayMatchPhase::Warmup));
	auto SpawnPlayer = [World, GS](int32 Team)
	{
		ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
		if (!Player->IsActorInitialized()) static_cast<AActor*>(Player)->PostInitializeComponents();
		ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
		UBwayHeroStatsLibrary::EnsureHeroAttributeSet(ASC);
		if (!ASC->GetSet<ULyraHealthSet>()) ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(Player));
		APawn* Avatar = World->SpawnActor<APawn>();
		Avatar->SetPlayerState(Player);
		ASC->InitAbilityActorInfo(Player, Avatar);
		ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 500.0f);
		ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 200.0f);
		ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 500.0f);
		GS->AddPlayerToTeam(Player, Team);
		Player->DispatchBeginPlay(); // Registers the production damage listener without starting match timers.
		return Player;
	};
	ABwayPlayerState* Source = SpawnPlayer(0);
	ABwayPlayerState* Target = SpawnPlayer(1);
	ULyraAbilitySystemComponent* SourceASC = Source->GetLyraAbilitySystemComponent();
	ULyraAbilitySystemComponent* TargetASC = Target->GetLyraAbilitySystemComponent();
	const ULyraHealthSet* SourceHealth = SourceASC->GetSet<ULyraHealthSet>();
	const ULyraHealthSet* TargetHealth = TargetASC->GetSet<ULyraHealthSet>();
	const UBwayHeroAttributeSet* Hero = SourceASC->GetSet<UBwayHeroAttributeSet>();
	UBwayUpgradeComponent* Shop = Source->UpgradeComponent;
	UBwayUpgradeCatalog* Catalog = NewObject<UBwayUpgradeCatalog>(Source);
	FBwayUpgradeDefinition& Leech = Catalog->Upgrades.AddDefaulted_GetRef();
	Leech.Id = TEXT("LifeLeech");
	Leech.EffectClass = UBwayUpgradeEffect_LifeLeech::StaticClass();
	Leech.Ranks.Add({30, 0.10f});
	Leech.Ranks.Add({40, 0.25f});
	const TSubclassOf<UBwayUpgradeEffect> Others[] = { UBwayUpgradeEffect_AttackStrength::StaticClass(),
		UBwayUpgradeEffect_Armor::StaticClass(), UBwayUpgradeEffect_MoveSpeed::StaticClass(), UBwayUpgradeEffect_CooldownReduction::StaticClass() };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Others); ++Index)
	{
		FBwayUpgradeDefinition& Entry = Catalog->Upgrades.AddDefaulted_GetRef();
		Entry.Id = FName(*FString::Printf(TEXT("Other%d"), Index));
		Entry.EffectClass = Others[Index];
		Entry.Ranks.Add({10, 0.01f});
	}
	FString Error;
	TestTrue(TEXT("Reversible leech catalog validates"), Catalog->ValidateCatalog(Error));
	TestTrue(TEXT("Catalog configured"), Shop->ConfigureCatalog(Catalog));
	TestTrue(TEXT("Rank one bought"), Shop->TryPurchaseUpgrade(TEXT("LifeLeech"), 0));
	TestEqual(TEXT("Price debited"), SourceASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 470.0f);
	TestEqual(TEXT("Purchased fraction"), Hero->GetLifeLeech(), 0.10f);

	int32 HealEvents = 0;
	double ReportedHealing = 0.0;
	UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(World);
	FGameplayMessageListenerHandle HealListener = Messages.RegisterListener<FLyraVerbMessage>(TAG_Lyra_Heal_Message,
		[&](FGameplayTag, const FLyraVerbMessage& Message)
		{
			if (Message.Target == Source) { ++HealEvents; ReportedHealing += Message.Magnitude; }
		});
	ON_SCOPE_EXIT { HealListener.Unregister(); };
	auto ResetHealth = [&]()
	{
		SourceASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 200.0f);
		TargetASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 200.0f);
		TargetASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 0.0f);
	};
	auto Hit = [&](float Damage)
	{
		return UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, Target, Damage, SourceASC->GetAvatarActor(), SourceASC->GetAvatarActor());
	};
	TargetASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 20.0f);
	TestTrue(TEXT("Fractional damage applied"), Hit(2.5f));
	TestEqual(TEXT("Armor mitigates before leech"), TargetHealth->GetHealth(), 198.0f);
	TestTrue(TEXT("Fractional health preserved"), FMath::IsNearlyEqual(SourceHealth->GetHealth(), 200.2f, 0.001f));
	TestEqual(TEXT("One heal event for one damage application"), HealEvents, 1);
	TestTrue(TEXT("Healing feedback uses actual fractional gain"), FMath::IsNearlyEqual(ReportedHealing, 0.2, 0.001));

	ResetHealth();
	TargetASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 3.0f);
	Hit(1000.0f);
	TestTrue(TEXT("Overkill leeches only three removed health"), FMath::IsNearlyEqual(SourceHealth->GetHealth(), 200.3f, 0.001f));
	Hit(1000.0f);
	TestTrue(TEXT("Already dead target cannot supply more health"), FMath::IsNearlyEqual(SourceHealth->GetHealth(), 200.3f, 0.001f));
	ResetHealth();
	TargetASC->AddLooseGameplayTag(TAG_Gameplay_DamageImmunity);
	Hit(50.0f);
	TestEqual(TEXT("Immune target takes no damage"), TargetHealth->GetHealth(), 200.0f);
	TestEqual(TEXT("Immune target grants no healing"), SourceHealth->GetHealth(), 200.0f);
	TargetASC->RemoveLooseGameplayTag(TAG_Gameplay_DamageImmunity);
	Hit(0.0f);
	Hit(-10.0f);
	TestEqual(TEXT("Zero/negative damage grants no healing"), SourceHealth->GetHealth(), 200.0f);

	GS->AddPlayerToTeam(Target, 0);
	Hit(10.0f);
	TestEqual(TEXT("Friendly health loss grants no healing"), SourceHealth->GetHealth(), 200.0f);
	GS->RemovePlayerFromTeam(Target);
	Hit(10.0f);
	TestEqual(TEXT("Unassigned target grants no healing"), SourceHealth->GetHealth(), 200.0f);
	GS->AddPlayerToTeam(Target, 1);
	UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, Source, 10.0f, SourceASC->GetAvatarActor(), Source);
	TestEqual(TEXT("Self damage is not refunded"), SourceHealth->GetHealth(), 190.0f);
	ResetHealth();
	SourceASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 0.0f);
	Hit(10.0f);
	TestEqual(TEXT("Dead attacker is not resurrected"), SourceHealth->GetHealth(), 0.0f);
	ResetHealth();
	SourceASC->AddLooseGameplayTag(LyraGameplayTags::Status_Death);
	Hit(10.0f);
	TestEqual(TEXT("Death transition blocks healing even with positive health"), SourceHealth->GetHealth(), 200.0f);
	SourceASC->RemoveLooseGameplayTag(LyraGameplayTags::Status_Death);
	ResetHealth();
	SourceASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 499.9f);
	Hit(100.0f);
	TestEqual(TEXT("Healing capped at max health"), SourceHealth->GetHealth(), 500.0f);
	const int32 FullHealthEvents = HealEvents;
	Hit(10.0f);
	TestEqual(TEXT("Full health emits no zero-value heal"), HealEvents, FullHealthEvents);

	ResetHealth();
	FLyraVerbMessage Invalid;
	Invalid.Verb = TAG_Lyra_Damage_Message;
	Invalid.Instigator = Source;
	Invalid.Target = World->SpawnActor<AActor>();
	Invalid.Magnitude = 100;
	Messages.BroadcastMessage(TAG_Lyra_Damage_Message, Invalid);
	TestEqual(TEXT("Non-player health loss grants no healing"), SourceHealth->GetHealth(), 200.0f);
	Invalid.Target = Target;
	Invalid.Magnitude = std::numeric_limits<double>::infinity();
	Messages.BroadcastMessage(TAG_Lyra_Damage_Message, Invalid);
	Invalid.Magnitude = std::numeric_limits<double>::quiet_NaN();
	Messages.BroadcastMessage(TAG_Lyra_Damage_Message, Invalid);
	TestEqual(TEXT("Nonfinite events cannot heal"), SourceHealth->GetHealth(), 200.0f);

	TestFalse(TEXT("Stale purchase rejected"), Shop->TryPurchaseUpgrade(TEXT("LifeLeech"), 0));
	TestTrue(TEXT("Rank two replaces prior effect"), Shop->TryPurchaseUpgrade(TEXT("LifeLeech"), 1));
	TestEqual(TEXT("Rank two total fraction, not stacked ranks"), Hero->GetLifeLeech(), 0.25f);
	TestEqual(TEXT("Second exact debit"), SourceASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), 430.0f);
	APawn* NewAvatar = World->SpawnActor<APawn>();
	NewAvatar->SetPlayerState(Source);
	SourceASC->InitAbilityActorInfo(Source, NewAvatar);
	Hit(10.0f);
	TestEqual(TEXT("Persistent upgrade works after avatar replacement"), SourceHealth->GetHealth(), 202.5f);
	for (int32 Index = 0; Index < 3; ++Index)
		TestTrue(TEXT("Alternate four-item loadout fills available slot"), Shop->TryPurchaseUpgrade(FName(*FString::Printf(TEXT("Other%d"), Index)), 0));
	const float FullWallet = SourceASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute());
	TestFalse(TEXT("Fifth distinct item rejected"), Shop->TryPurchaseUpgrade(TEXT("Other3"), 0));
	TestEqual(TEXT("Rejected fifth purchase keeps wallet"), SourceASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), FullWallet);
	Shop->ResetUpgrades();
	TestEqual(TEXT("Reset removes life leech"), Hero->GetLifeLeech(), 0.0f);
	ResetHealth();
	Hit(10.0f);
	TestEqual(TEXT("Reset stops healing"), SourceHealth->GetHealth(), 200.0f);
	TestTrue(TEXT("Purchase in new loadout"), Shop->TryPurchaseUpgrade(TEXT("LifeLeech"), 0));
	Source->LifeLeechComponent->DestroyComponent();
	Hit(10.0f);
	TestEqual(TEXT("Component end removes listener"), SourceHealth->GetHealth(), 200.0f);
	SourceASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), -1.0f);
	Shop->ResetUpgrades();
	TestEqual(TEXT("Negative base bounded"), Hero->GetLifeLeech(), 0.0f);
	SourceASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), 2.0f);
	TestEqual(TEXT("Base ratio capped at one"), Hero->GetLifeLeech(), 1.0f);
	SourceASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), std::numeric_limits<float>::quiet_NaN());
	TestEqual(TEXT("Nonfinite ratio bounded"), Hero->GetLifeLeech(), 0.0f);
	return true;
}
#endif
