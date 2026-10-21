#include "Misc/AutomationTest.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/BwayGameplayAbility_RelicPass.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BrainComponent.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "Combat/BwayHeroProjectileBase.h"
#include "Combat/BwayLifeLeechComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Containers/Ticker.h"
#include "Economy/BwayBaseZone.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "LyraGameplayTags.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Relic/RelicActor.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BwayAllKitCombatEffectsTest
{
struct FParticipant
{
	ABwayPlayerState* PS = nullptr;
	ABwayCharacterWithAbilities* Pawn = nullptr;
	APlayerController* PC = nullptr;
	ULyraAbilitySystemComponent* ASC = nullptr;
	UBwayCharacterMovementComponent* Move = nullptr;
};

class FRun : public IAutomationLatentCommand
{
public:
	explicit FRun(FAutomationTestBase* InTest, bool bInRestrictionsOnly = false, int32 InBlessingCase = INDEX_NONE,
		int32 InReviewGroup = 0, int32 InReviewCase = 0)
		: Test(InTest), bRestrictionsOnly(bInRestrictionsOnly), BlessingCase(InBlessingCase), ReviewGroup(InReviewGroup), ReviewCase(InReviewCase) {}

	virtual ~FRun() override
	{
		if (Ally.ASC) Ally.ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(BlessingHealObserver);
		if (Enemy.ASC) Enemy.ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(ReviewEffectObserver);
		if (bRestrictionsOnly)
		{
			if (Relic && Relic->CurrentCarrier) Relic->OnDropped();
			if (Source.ASC)
			{
				Source.ASC->CancelAllAbilities();
				if (Source.PS) Source.PS->GetHeroAbilityGrantedHandles().TakeFromAbilitySystem(Source.ASC);
			}
		}
		if (Source.ASC) Source.ASC->ClearAllAbilities();
		if (Enemy.ASC) Enemy.ASC->ClearAllAbilities();
		if (World) World->DestroyWorld(false);
		if (Instance) Instance->Shutdown();
		if (World && GEngine) GEngine->DestroyWorldContext(World);
	}

	float Health(const FParticipant& P) const
	{
		return P.ASC ? P.ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.f;
	}

	float Armor(const FParticipant& P) const
	{
		return P.ASC ? P.ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetArmorAttribute()) : 0.f;
	}

	float MoveMult(const FParticipant& P) const
	{
		return P.ASC ? P.ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()) : 0.f;
	}

	float DamageMult(const FParticipant& P) const
	{
		return P.ASC ? P.ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute()) : 0.f;
	}

	static void EnsureBegunPlay(AActor* Actor)
	{
		if (!Actor) return;
		if (!Actor->IsActorInitialized())
		{
			Actor->InitializeComponents();
			Actor->PostInitializeComponents();
		}
		Actor->UpdateOverlaps();
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
	}

	void EnsureZonesAndProjectilesBegunPlay()
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!IsValid(A) || A->IsActorBeingDestroyed()) continue;
			const FString Name = A->GetClass()->GetName();
			if (A->IsA<ABwayHeroProjectileBase>() || Name == TEXT("BwayAlonaBlessingZone") || Name == TEXT("BwayKorrynSpiteZone"))
			{
				EnsureBegunPlay(A);
			}
		}
	}

	FVector EffectiveVelocity(const FParticipant& P) const
	{
		if (!P.Move) return FVector::ZeroVector;
		P.Move->HandlePendingLaunch();
		return P.Move->Velocity;
	}

	bool SpawnParticipant(FParticipant& Out, int32 Team, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Out.PS = World->SpawnActor<ABwayPlayerState>(Params);
		Out.PC = World->SpawnActor<APlayerController>(Params);
		Out.Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(ABwayCharacterWithAbilities::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (!Out.PS || !Out.PC || !Out.Pawn) return false;
		EnsureBegunPlay(Out.PS);
		EnsureBegunPlay(Out.PC);
		if (!GS->PlayerArray.Contains(Out.PS)) GS->AddPlayerState(Out.PS);
		Out.PS->SetGenericTeamId(FGenericTeamId(Team + 1));
		Out.Pawn->SetGenericTeamId(FGenericTeamId(Team + 1));
		GS->AddPlayerToTeam(Out.PS, Team);
		Out.PC->Possess(Out.Pawn);
		Out.Pawn->SetPlayerState(Out.PS);
		Out.ASC = Out.PS->GetLyraAbilitySystemComponent();
		UBwayHeroStatsLibrary::EnsureHeroAttributeSet(Out.ASC);
		if (!Out.ASC->GetSet<ULyraHealthSet>()) Out.ASC->AddSpawnedAttribute(NewObject<ULyraHealthSet>(Out.PS));
		if (!Out.ASC->GetSet<UBwayGoldAttributeSet>()) Out.ASC->AddSpawnedAttribute(NewObject<UBwayGoldAttributeSet>(Out.PS));
		ULyraPawnExtensionComponent* Ext = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Out.Pawn);
		Ext->SetPawnData(bRestrictionsOnly ? RestrictionPawnData : NewObject<ULyraPawnData>(World));
		Ext->InitializeAbilitySystem(Out.ASC, Out.PS);
		EnsureBegunPlay(Out.Pawn);
		Out.Move = Out.Pawn->GetBwayCharacterMovement();
		Out.Move->bRunPhysicsWithNoController = true;
		Out.Move->SetMovementMode(MOVE_Walking);
		return true;
	}

	bool RespawnEnemyPawn()
	{
		if (Enemy.Pawn)
		{
			if (ULyraPawnExtensionComponent* OldExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Enemy.Pawn))
			{
				OldExt->UninitializeAbilitySystem();
			}
			Enemy.PC->UnPossess();
			Enemy.Pawn->Destroy();
			Enemy.Pawn = nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Enemy.Pawn = World->SpawnActor<ABwayCharacterWithAbilities>(ABwayCharacterWithAbilities::StaticClass(), FVector(160.f, 0.f, 200.f), FRotator::ZeroRotator, Params);
		if (!Enemy.Pawn) return false;
		Enemy.Pawn->SetGenericTeamId(FGenericTeamId(2));
		Enemy.PC->Possess(Enemy.Pawn);
		Enemy.Pawn->SetPlayerState(Enemy.PS);
		ULyraPawnExtensionComponent* Ext = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Enemy.Pawn);
		Ext->SetPawnData(NewObject<ULyraPawnData>(World));
		Ext->InitializeAbilitySystem(Enemy.ASC, Enemy.PS);
		EnsureBegunPlay(Enemy.Pawn);
		Enemy.Move = Enemy.Pawn->GetBwayCharacterMovement();
		Enemy.Move->bRunPhysicsWithNoController = true;
		Enemy.Move->SetMovementMode(MOVE_Walking);
		return true;
	}

	bool ConfigureRoundActive()
	{
		UBwayRoundManagementComponent* Round = GS ? GS->GetRoundManagement() : nullptr;
		FEnumProperty* PhaseProp = FindFProperty<FEnumProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentMatchPhase"));
		FEnumProperty* StateProp = FindFProperty<FEnumProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentRoundState"));
		FIntProperty* NumProp = FindFProperty<FIntProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentRoundNumber"));
		if (!Round || !PhaseProp || !StateProp || !NumProp) return false;
		PhaseProp->GetUnderlyingProperty()->SetIntPropertyValue(PhaseProp->ContainerPtrToValuePtr<void>(Round), static_cast<uint64>(EBwayMatchPhase::Playing));
		StateProp->GetUnderlyingProperty()->SetIntPropertyValue(StateProp->ContainerPtrToValuePtr<void>(Round), static_cast<uint64>(ERoundState::RoundActive));
		NumProp->SetPropertyValue_InContainer(Round, 1);
		GS->UpdateTeamAliveCount(0, 2);
		GS->UpdateTeamAliveCount(1, 2);
		return true;
	}

	bool Setup()
	{
		Instance.Reset(NewObject<UGameInstance>(GEngine));
		Instance->InitializeStandalone();
		World = Instance->GetWorld();
		if (!Test->TestNotNull(TEXT("All-kit fixture world"), World)) return false;
		World->InitializeActorsForPlay(FURL());
		GS = World->SpawnActor<ABwayGameState>();
		if (!Test->TestNotNull(TEXT("All-kit GameState"), GS)) return false;
		World->SetGameState(GS);
		World->SetBegunPlay(true);
		EnsureBegunPlay(GS);
		FArrayProperty* TeamsProp = FindFProperty<FArrayProperty>(ABwayGameState::StaticClass(), TEXT("Teams"));
		if (!Test->TestNotNull(TEXT("Teams property"), TeamsProp)) return false;
		TArray<FTeamInfo>* Teams = TeamsProp->ContainerPtrToValuePtr<TArray<FTeamInfo>>(GS);
		Teams->Add(FTeamInfo(0));
		Teams->Add(FTeamInfo(1));
		if (!ConfigureRoundActive()) return false;
		if (bRestrictionsOnly)
		{
			RestrictionPawnData = LoadObject<ULyraPawnData>(nullptr,
				TEXT("/BreakawayCore/Characters/DA_BW_PawnData_Humanoid.DA_BW_PawnData_Humanoid"));
			if (!Test->TestNotNull(TEXT("Authored restriction PawnData"), RestrictionPawnData)
				|| !Test->TestNotNull(TEXT("Authored ability tag relationships"), RestrictionPawnData->TagRelationshipMapping.Get())) return false;
		}

		if (!SpawnParticipant(Source, 0, FVector(0.f, 0.f, 200.f))
			|| !SpawnParticipant(Ally, 0, FVector(0.f, 200.f, 200.f))
			|| !SpawnParticipant(Enemy, 1, FVector(160.f, 0.f, 200.f))
			|| !SpawnParticipant(Assister, 0, FVector(-300.f, 0.f, 200.f)))
		{
			Test->AddError(TEXT("Failed to spawn combat participants"));
			return false;
		}

		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Relic = World->SpawnActor<ARelicActor>(ARelicActor::StaticClass(), FTransform(FVector(0.f, 0.f, 3000.f)), Spawn);
		ULyraAbilitySet* CarrierSet = LoadObject<ULyraAbilitySet>(nullptr,
			TEXT("/BreakawayCore/GameModes/BallMode/Abilities/BallHandlingAbilities/AS_BW_BallAbilities.AS_BW_BallAbilities"));
		FObjectPropertyBase* LoadedSetProp = FindFProperty<FObjectPropertyBase>(ARelicActor::StaticClass(), TEXT("LoadedRelicAbilitySet"));
		if (!Test->TestNotNull(TEXT("Fixture Relic"), Relic)
			|| !Test->TestNotNull(TEXT("Carrier AbilitySet"), CarrierSet)
			|| !Test->TestNotNull(TEXT("LoadedRelicAbilitySet property"), LoadedSetProp))
		{
			return false;
		}
		EnsureBegunPlay(Relic);
		LoadedSetProp->SetObjectPropertyValue_InContainer(Relic, CarrierSet);
		if (UBwayRelicManagerComponent* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			RelicMgr->SetActiveRelic(Relic);
		}
		if (!bRestrictionsOnly)
		{
			HumanoidSet = LoadObject<ULyraAbilitySet>(nullptr, TEXT("/BreakawayCore/Characters/DA_BW_AbilitySet_Humanoid.DA_BW_AbilitySet_Humanoid"));
			if (!Test->TestNotNull(TEXT("Humanoid ability set"), HumanoidSet)) return false;
		}
		Started = FPlatformTime::Seconds();
		return true;
	}

	bool GrantAbilitySet(ULyraAbilitySystemComponent* ASC, const ULyraAbilitySet* Set)
	{
		const FArrayProperty* GrantsProp = FindFProperty<FArrayProperty>(ULyraAbilitySet::StaticClass(), TEXT("GrantedGameplayAbilities"));
		if (!ASC || !Set || !GrantsProp) return false;
		const auto* Grants = GrantsProp->ContainerPtrToValuePtr<TArray<FLyraAbilitySet_GameplayAbility>>(Set);
		for (const auto& Grant : *Grants)
		{
			if (!Grant.Ability) continue;
			FGameplayAbilitySpec Spec(Grant.Ability, Grant.AbilityLevel);
			if (Grant.InputTag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
			ASC->GiveAbility(Spec);
		}
		return true;
	}

	FGameplayAbilitySpecHandle FindAbilityHandle(ULyraAbilitySystemComponent* ASC, const TCHAR* ClassName) const
	{
		if (!ASC) return FGameplayAbilitySpecHandle();
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			for (const UClass* Cls = Spec.Ability ? Spec.Ability->GetClass() : nullptr; Cls; Cls = Cls->GetSuperClass())
			{
				if (Cls->GetName() == ClassName) return Spec.Handle;
			}
		}
		return FGameplayAbilitySpecHandle();
	}

	bool ConfigureHero(const TCHAR* HeroDataPath, const TCHAR* AbilitySetPath)
	{
		CurrentHeroData = LoadObject<UBwayHeroDataAsset>(nullptr, HeroDataPath);
		const ULyraAbilitySet* HeroSet = LoadObject<ULyraAbilitySet>(nullptr, AbilitySetPath);
		if (!Test->TestNotNull(HeroDataPath, CurrentHeroData) || !Test->TestNotNull(AbilitySetPath, HeroSet)) return false;
		Source.ASC->ClearAllAbilities();
		UBwayHeroStatsLibrary::ApplyHeroStatsFromDataAsset(Source.Pawn, CurrentHeroData);
		return GrantAbilitySet(Source.ASC, HumanoidSet) && GrantAbilitySet(Source.ASC, HeroSet);
	}

	// Immediate authority eligibility only. No successful activation or world/timer
	// tick is used here; the original effects test and owned-client gate remain separate.
	TArray<FString> RestrictionFootprint() const
	{
		TArray<FString> Rows;
		int32 Index = 0;
		for (const FParticipant* P : {&Source, &Ally, &Enemy, &Assister})
		{
			const FString Prefix = FString::Printf(TEXT("P%d:"), Index++);
			TArray<FGameplayAttribute> Attrs; P->ASC->GetAllAttributes(Attrs);
			for (const auto& Attr : Attrs) Rows.Add(Prefix + FString::Printf(TEXT("attr:%s:%.17g:%.17g"),
				*Attr.GetName(), double(P->ASC->GetNumericAttributeBase(Attr)), double(P->ASC->GetNumericAttribute(Attr))));
			FGameplayTagContainer Tags; P->ASC->GetOwnedGameplayTags(Tags);
			for (const auto& Tag : Tags) Rows.Add(Prefix + FString::Printf(TEXT("tag:%s:%d"), *Tag.ToString(), P->ASC->GetTagCount(Tag)));
			for (const auto H : P->ASC->GetActiveEffects(FGameplayEffectQuery()))
			{
				const FActiveGameplayEffect* Effect = P->ASC->GetActiveGameplayEffect(H);
				Rows.Add(Prefix + FString::Printf(TEXT("effect:%s:%s:%.17g:%.17g:%d"), *H.ToString(),
					Effect ? *GetPathNameSafe(Effect->Spec.Def) : TEXT("missing"), Effect ? double(Effect->Spec.GetLevel()) : -1.,
					Effect ? double(Effect->Spec.GetDuration()) : -1., Effect ? Effect->Spec.GetStackCount() : -1));
			}
			for (const auto& Spec : P->ASC->GetActivatableAbilities()) Rows.Add(Prefix + FString::Printf(
				TEXT("grant:%s:%s:%d:%d:%d:%s:%s"), *Spec.Handle.ToString(), *GetPathNameSafe(Spec.Ability), Spec.Level,
				Spec.IsActive(), Spec.InputPressed, *Spec.GetDynamicSpecSourceTags().ToStringSimple(), *GetPathNameSafe(Spec.SourceObject.Get())));
			for (const FName Name : {TEXT("Kills"), TEXT("Deaths"), TEXT("Assists"), TEXT("ObjectiveScore"), TEXT("DamageDealt"),
				TEXT("HealingDone"), TEXT("ForcedFumbles"), TEXT("Interceptions"), TEXT("DeathsInCurrentRound")})
			{
				const auto* Prop = FindFProperty<FIntProperty>(P->PS->GetClass(), Name);
				Test->TestNotNull(TEXT("Restriction stat property"), Prop);
				Rows.Add(Prefix + FString::Printf(TEXT("stat:%s:%d"), *Name.ToString(), Prop ? Prop->GetPropertyValue_InContainer(P->PS) : -1));
			}
			const auto* Gold = FindFProperty<FDoubleProperty>(P->PS->GetClass(), TEXT("MatchGoldEarned"));
			Test->TestNotNull(TEXT("Restriction earned-gold property"), Gold);
			Rows.Add(Prefix + FString::Printf(TEXT("gold:%.17g:carry:%d"), Gold ? Gold->GetPropertyValue_InContainer(P->PS) : -1., P->PS->bHasRelic));
#if !UE_BUILD_SHIPPING
			Rows.Add(Prefix + FString::Printf(TEXT("credit:%d:%d"), P->Pawn->GetCombatContributionCountForAudit(), P->Pawn->GetConsumedAssisterCountForAudit()));
#endif
			const FTransform Transform = P->Pawn->GetActorTransform();
			const FQuat Rotation = Transform.GetRotation();
			const FRotator Control = P->PC->GetControlRotation();
			int32 VectorIndex = 0;
			for (const FVector V : {Transform.GetLocation(), Transform.GetScale3D(), P->Move->Velocity, P->Move->PendingLaunchVelocity,
				P->Move->GetCurrentAcceleration(), FVector(Control.Pitch, Control.Yaw, Control.Roll)})
				Rows.Add(Prefix + FString::Printf(TEXT("vector:%d:%.17g:%.17g:%.17g"), VectorIndex++, V.X, V.Y, V.Z));
			Rows.Add(Prefix + FString::Printf(TEXT("rotation:%.17g:%.17g:%.17g:%.17g:move:%d:%d:%.17g:%.17g:collision:%d"),
				Rotation.X, Rotation.Y, Rotation.Z, Rotation.W, int32(P->Move->MovementMode), int32(P->Move->CustomMovementMode),
				double(P->Move->MaxWalkSpeed), double(P->Move->MaxFlySpeed), int32(P->Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn))));
		}
		for (TActorIterator<AActor> It(World); It; ++It) if (IsValid(*It) && !It->IsActorBeingDestroyed())
			Rows.Add(FString::Printf(TEXT("actor:%u:%s"), It->GetUniqueID(), *It->GetPathName()));
		Rows.Sort();
		return Rows;
	}

	TArray<FString> PrimaryNegativeOutcomes() const
	{
		TArray<FString> Outcomes;
		for (const FString& Row : RestrictionFootprint())
		{
			// A successful swing may legitimately change source cooldown/spec state.
			// Compare combat outcomes, not the rejection-only full footprint.
			if (Row.Contains(TEXT(":attr:")) || Row.Contains(TEXT(":stat:")) || Row.Contains(TEXT(":gold:")) || Row.Contains(TEXT(":credit:"))
				|| (!Row.StartsWith(TEXT("P0:")) && (Row.Contains(TEXT(":tag:")) || Row.Contains(TEXT(":effect:"))))) Outcomes.Add(Row);
		}
		FGameplayTagContainer SourceTags; Source.ASC->GetOwnedGameplayTags(SourceTags);
		const auto StatusRoot = FGameplayTag::RequestGameplayTag(TEXT("State.Status"));
		for (const auto& Tag : SourceTags) if (Tag.MatchesTag(StatusRoot) || Tag == TAG_Gameplay_DamageImmunity)
			Outcomes.Add(FString::Printf(TEXT("sourceStatus:%s:%d"), *Tag.ToString(), Source.ASC->GetTagCount(Tag)));
		Outcomes.Sort();
		return Outcomes;
	}

	bool RestrictionReady(FGameplayAbilitySpecHandle Handle, const TCHAR* Context)
	{
		const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(Handle);
		if (!Test->TestNotNull(Context, Spec) || !Test->TestFalse(TEXT("Restriction spec is inactive"), Spec->IsActive())) return false;
		const UGameplayAbility* Ability = Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get();
		FGameplayTagContainer Failure;
		return Test->TestTrue(Context, Ability->CanActivateAbility(Handle, Source.ASC->AbilityActorInfo.Get(), nullptr, nullptr, &Failure))
			&& Test->TestTrue(TEXT("Ready spec has no failure tags"), Failure.IsEmpty());
	}

	bool RestrictionRejected(FGameplayAbilitySpecHandle Handle, const TCHAR* StateName)
	{
		const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(Handle);
		if (!Test->TestNotNull(TEXT("Same restricted authored spec"), Spec)) return false;
		const UClass* AbilityClass = Spec->Ability->GetClass();
		const auto Before = RestrictionFootprint();
		int32 Failures = 0;
		FGameplayTagContainer Failure;
		const FDelegateHandle Observer = Source.ASC->AbilityFailedCallbacks.AddLambda(
			[&](const UGameplayAbility* Failed, const FGameplayTagContainer& Reasons)
			{
				if (Failed && Failed->GetClass() == AbilityClass) { ++Failures; Failure.AppendTags(Reasons); }
			});
		const bool bActivated = Source.ASC->TryActivateAbility(Handle);
		Source.ASC->AbilityFailedCallbacks.Remove(Observer);
		const FGameplayTag Blocked = UAbilitySystemGlobals::Get().ActivateFailTagsBlockedTag;
		bool bOk = Test->TestFalse(TEXT("Restricted production activation rejected"), bActivated);
		bOk &= Test->TestEqual(TEXT("Exactly one production failure callback"), Failures, 1);
		bOk &= Test->TestTrue(TEXT("Production failure is blocked-tag rejection"), Blocked.IsValid() && Failure.HasTagExact(Blocked));
		bOk &= Test->TestTrue(TEXT("Rejected call leaves immediate fixture footprint unchanged"), Before == RestrictionFootprint());
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitRestrictions: Rejected state=%s class=%s handle=%s activated=%d failures=%d blocked=%d unchanged=%d"),
			StateName, *AbilityClass->GetPathName(), *Handle.ToString(), bActivated, Failures, Failure.HasTagExact(Blocked), Before == RestrictionFootprint());
		return bOk;
	}

	bool RestrictionCarryState(bool bCarried) const
	{
		const auto CarrierTag = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.RelicCarrier"));
		bool bOk = Test->TestTrue(TEXT("Production carrier pointer"), Relic->CurrentCarrier.Get() == (bCarried ? Source.Pawn : nullptr));
		bOk &= Test->TestEqual(TEXT("Production carried/dropped state"), Relic->GetCurrentState(), bCarried ? ERelicState::Carried : ERelicState::Dropped);
		bOk &= Test->TestEqual(TEXT("Production possession flag"), Source.PS->bHasRelic, bCarried);
		bOk &= Test->TestEqual(TEXT("Production carrier tag count"), Source.ASC->GetTagCount(CarrierTag), bCarried ? 1 : 0);
		bOk &= Test->TestEqual(TEXT("Production carrying tag count"), Source.ASC->GetTagCount(BwayGameplayTags::State_Relic_Carrying), bCarried ? 1 : 0);
		bOk &= Test->TestTrue(TEXT("Production relic attachment"), Relic->GetAttachParentActor() == (bCarried ? Source.Pawn : nullptr));
		return bOk;
	}

	bool RestrictionCarrierGrants(bool bCarried) const
	{
		TArray<FString> Actual, Expected;
		const auto Signature = [](const UClass* Class, int32 Level, const FGameplayTagContainer& Tags)
			{ return FString::Printf(TEXT("%s:%d:%s"), *GetPathNameSafe(Class), Level, *Tags.ToStringSimple()); };
		for (const auto& Spec : Source.ASC->GetActivatableAbilities()) if (Spec.SourceObject.Get() == Relic)
			Actual.Add(Signature(Spec.Ability->GetClass(), Spec.Level, Spec.GetDynamicSpecSourceTags()));
		if (bCarried)
		{
			const auto* Loaded = FindFProperty<FObjectPropertyBase>(ARelicActor::StaticClass(), TEXT("LoadedRelicAbilitySet"));
			const auto* GrantsProp = FindFProperty<FArrayProperty>(ULyraAbilitySet::StaticClass(), TEXT("GrantedGameplayAbilities"));
			const auto* Set = Loaded ? Cast<ULyraAbilitySet>(Loaded->GetObjectPropertyValue_InContainer(Relic)) : nullptr;
			if (!Test->TestNotNull(TEXT("Actual carrier set"), Set) || !Test->TestNotNull(TEXT("Authored carrier grants"), GrantsProp)) return false;
			for (const auto& Grant : *GrantsProp->ContainerPtrToValuePtr<TArray<FLyraAbilitySet_GameplayAbility>>(Set))
			{
				if (!Test->TestNotNull(TEXT("Authored carrier grant class"), Grant.Ability.Get())) return false;
				FGameplayTagContainer Tags; if (Grant.InputTag.IsValid()) Tags.AddTag(Grant.InputTag);
				Expected.Add(Signature(Grant.Ability.Get(), Grant.AbilityLevel, Tags));
			}
			FGameplayTagContainer PassTags; PassTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Relic.Pass")));
			Expected.Add(Signature(UBwayGameplayAbility_RelicPass::StaticClass(), 1, PassTags));
		}
		Actual.Sort(); Expected.Sort();
		return Test->TestTrue(TEXT("Exact production Relic-sourced grants and input tags"), Actual == Expected);
	}

	bool FreshRestrictionRelic()
	{
		// Each hero gets one real pickup on a fresh isolated relic. No ticking or
		// writes to production repick cooldown state are needed between heroes.
		if (Relic) { if (Relic->CurrentCarrier) Relic->OnDropped(); Relic->Destroy(); }
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Relic = World->SpawnActor<ARelicActor>(ARelicActor::StaticClass(), FTransform(FVector(0.f, 0.f, 3000.f)), Params);
		auto* Set = LoadObject<ULyraAbilitySet>(nullptr,
			TEXT("/BreakawayCore/GameModes/BallMode/Abilities/BallHandlingAbilities/AS_BW_BallAbilities.AS_BW_BallAbilities"));
		auto* Loaded = FindFProperty<FObjectPropertyBase>(ARelicActor::StaticClass(), TEXT("LoadedRelicAbilitySet"));
		if (!Test->TestNotNull(TEXT("Fresh restriction relic"), Relic) || !Test->TestNotNull(TEXT("Authored carrier set"), Set)
			|| !Test->TestNotNull(TEXT("Carrier set property"), Loaded)) return false;
		EnsureBegunPlay(Relic); Loaded->SetObjectPropertyValue_InContainer(Relic, Set);
		auto* Manager = GS->FindComponentByClass<UBwayRelicManagerComponent>();
		if (!Test->TestNotNull(TEXT("Restriction relic manager"), Manager)) return false;
		Manager->SetActiveRelic(Relic);
		return true;
	}

	bool CheckAuthoredRestrictions()
	{
		struct FHero { const TCHAR* Path; const TCHAR* Families[5]; };
		const FHero Heroes[] = {
			{TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus"),
				{TEXT("BwayGameplayAbility_MeleePrimary"), TEXT("BwayGameplayAbility_ArgusSlide"), TEXT("BwayGameplayAbility_NoRetreat"), TEXT("BwayGameplayAbility_ForGlory"), TEXT("BwayGameplayAbility_Retribution")}},
			{TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona"),
				{TEXT("BwayGameplayAbility_AlonaPrimary"), TEXT("BwayGameplayAbility_AlonaSunsGrace"), TEXT("BwayGameplayAbility_AlonaRadiance"), TEXT("BwayGameplayAbility_AlonaSunBurst"), TEXT("BwayGameplayAbility_AlonaBlessing")}},
			{TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver"),
				{TEXT("BwayGameplayAbility_KorrynPrimary"), TEXT("BwayGameplayAbility_KorrynFlock"), TEXT("BwayGameplayAbility_KorrynBurdenOfSin"), TEXT("BwayGameplayAbility_KorrynCircleOfSpite"), TEXT("BwayGameplayAbility_KorrynAuraOfSilence")}},
			{TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger"),
				{TEXT("BwayGameplayAbility_RawlinsPrimary"), TEXT("BwayGameplayAbility_RawlinsDoubleDown"), TEXT("BwayGameplayAbility_RawlinsPowerShot"), TEXT("BwayGameplayAbility_RawlinsSlideShot"), TEXT("BwayGameplayAbility_RawlinsBlazingBarrage")}}
		};
		const FName Inputs[] = {TEXT("InputTag.Ability.Primary"), TEXT("InputTag.Ability.Ability4"), TEXT("InputTag.Ability.Ability1"), TEXT("InputTag.Ability.Ability2"), TEXT("InputTag.Ability.Ability3")};
		int32 Completed = 0;
		for (const FHero& Hero : Heroes)
		{
			CurrentHeroData = LoadObject<UBwayHeroDataAsset>(nullptr, Hero.Path);
			if (!Test->TestNotNull(TEXT("Authored hero data"), CurrentHeroData)
				|| !Test->TestTrue(TEXT("Authored hero sets exist"), !CurrentHeroData->AbilitySets.IsEmpty()) || !FreshRestrictionRelic()) return false;
			// HeroData owns the real grant path. Common abilities are outside this
			// matrix; granting the Humanoid set would duplicate PlayerState's Gold set.
			Source.Pawn->InitializeHeroData(CurrentHeroData);
			ResetArena(); // Before restriction/snapshots only; never between rejection and recovery.
			TSet<const UClass*> AttributeClasses;
			for (const UAttributeSet* Attrs : Source.ASC->GetSpawnedAttributes())
			{
				if (!Test->TestTrue(TEXT("No duplicate attribute-set class"), Attrs && !AttributeClasses.Contains(Attrs->GetClass()))) return false;
				AttributeClasses.Add(Attrs->GetClass());
			}
			bool bInjectedSilence = false;
			ON_SCOPE_EXIT
			{
				if (bInjectedSilence) Source.ASC->RemoveLooseGameplayTag(BwayGameplayTags::State_Status_Silenced);
				if (Relic && Relic->CurrentCarrier) Relic->OnDropped();
			};
			TArray<FGameplayAbilitySpecHandle> Handles;
			for (int32 I = 0; I < 5; ++I)
			{
				const auto H = FindAbilityHandle(Source.ASC, Hero.Families[I]);
				if (!Test->TestTrue(TEXT("Five distinct actual hero grants"), H.IsValid() && !Handles.Contains(H))) return false;
				const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(H);
				if (!Test->TestTrue(TEXT("Authored hero input tag"), Spec->GetDynamicSpecSourceTags().HasTagExact(FGameplayTag::RequestGameplayTag(Inputs[I])))
					|| !RestrictionReady(H, TEXT("Clean authored CanActivate true"))) return false;
				Handles.Add(H);
			}
			const auto Clean = RestrictionFootprint();
			if (!Test->TestEqual(TEXT("Clean silence count"), Source.ASC->GetTagCount(BwayGameplayTags::State_Status_Silenced), 0)) return false;
			Source.ASC->AddLooseGameplayTag(BwayGameplayTags::State_Status_Silenced); bInjectedSilence = true;
			if (!Test->TestEqual(TEXT("Explicit silence fixture count"), Source.ASC->GetTagCount(BwayGameplayTags::State_Status_Silenced), 1)) return false;
			for (const auto H : Handles) if (!RestrictionRejected(H, TEXT("silenced"))) return false;
			Source.ASC->RemoveLooseGameplayTag(BwayGameplayTags::State_Status_Silenced); bInjectedSilence = false;
			if (!Test->TestTrue(TEXT("Exact footprint restored by removing only fixture silence"), Clean == RestrictionFootprint())) return false;
			for (const auto H : Handles) if (!RestrictionReady(H, TEXT("Same handle recovers after silence removal"))) return false;
			Relic->OnPickedUp(Source.Pawn);
			if (!RestrictionCarryState(true) || !RestrictionCarrierGrants(true)) return false;
			for (const auto H : Handles) if (!RestrictionRejected(H, TEXT("carrier"))) return false;
			if (!RestrictionCarryState(true) || !RestrictionCarrierGrants(true)) return false;
			Relic->OnDropped();
			if (!RestrictionCarryState(false) || !RestrictionCarrierGrants(false)
				|| !Test->TestTrue(TEXT("Exact participant footprint restored after production drop"), Clean == RestrictionFootprint())) return false;
			for (const auto H : Handles) if (!RestrictionReady(H, TEXT("Same handle recovers after production drop"))) return false;
			Completed += Handles.Num();
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitRestrictions: HeroPASS hero=%s actions=5 rejected=10 recoveries=10 cleanup=1 scope=headless-authority-immediate"), Hero.Path);
		}
		return Test->TestEqual(TEXT("All authored actions checked"), Completed, 20);
	}

	void ResetArena(float SourceHP = 200.f, float EnemyHP = 500.f, float AllyHP = 200.f,
		const FVector& EnemyLoc = FVector(160.f, 0.f, 200.f), const FVector& AllyLoc = FVector(0.f, 200.f, 200.f))
	{
		if (Relic && Relic->CurrentCarrier) Relic->OnDropped();
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FString Name = It->GetClass()->GetName();
			if (It->IsA<ABwayHeroProjectileBase>() || Name == TEXT("BwayAlonaBlessingZone") || Name == TEXT("BwayKorrynSpiteZone"))
			{
				ToDestroy.Add(*It);
			}
		}
		for (AActor* A : ToDestroy) A->Destroy();

		const FGameplayTag StatusTags[] = {
			BwayGameplayTags::State_Status_Silenced,
			BwayGameplayTags::State_Status_Ethereal,
			BwayGameplayTags::State_Status_ArmorShred,
			BwayGameplayTags::State_Status_Slowed,
			BwayGameplayTags::State_Status_DamageAmplified,
			TAG_Gameplay_DamageImmunity,
			FGameplayTag::RequestGameplayTag(TEXT("State.Dashing"), false),
			FGameplayTag::RequestGameplayTag(TEXT("State.Unstoppable"), false),
			FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sliding"), false)
		};

		for (FParticipant* P : {&Source, &Ally, &Enemy, &Assister})
		{
			if (!P->ASC) continue;
			P->ASC->CancelAllAbilities();
			for (const FActiveGameplayEffectHandle H : P->ASC->GetActiveEffects(FGameplayEffectQuery()))
			{
				P->ASC->RemoveActiveGameplayEffect(H);
			}
			for (const FGameplayTag& Tag : StatusTags)
			{
				if (Tag.IsValid() && P->ASC->GetTagCount(Tag) > 0)
				{
					P->ASC->SetLooseGameplayTagCount(Tag, 0);
				}
			}
		}

		Source.Pawn->SetActorLocationAndRotation(FVector(0.f, 0.f, 200.f), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Source.PC->SetControlRotation(FRotator::ZeroRotator);
		Source.Move->SetMovementMode(MOVE_Walking);
		Source.Move->Velocity = FVector::ZeroVector;
		Source.Move->PendingLaunchVelocity = FVector::ZeroVector;
		Source.Pawn->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

		Ally.Pawn->SetActorLocationAndRotation(AllyLoc, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Ally.Move->SetMovementMode(MOVE_Walking);
		Ally.Move->Velocity = FVector::ZeroVector;
		Ally.Move->PendingLaunchVelocity = FVector::ZeroVector;

		Enemy.Pawn->SetActorLocationAndRotation(EnemyLoc, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Enemy.Move->SetMovementMode(MOVE_Walking);
		Enemy.Move->Velocity = FVector::ZeroVector;
		Enemy.Move->PendingLaunchVelocity = FVector::ZeroVector;

		Assister.Pawn->SetActorLocationAndRotation(FVector(-300.f, 0.f, 200.f), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Assister.Move->SetMovementMode(MOVE_Walking);
		Assister.Move->Velocity = FVector::ZeroVector;
		Assister.Move->PendingLaunchVelocity = FVector::ZeroVector;

		if (CurrentHeroData) UBwayHeroStatsLibrary::ApplyHeroStatsFromDataAsset(Source.Pawn, CurrentHeroData);
		Source.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), 0.10f);
		Source.ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), SourceHP);

		Ally.ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 500.f);
		Ally.ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), AllyHP);
		Ally.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1.f);

		Enemy.ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 1000.f);
		Enemy.ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), EnemyHP);
		Enemy.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 0.f);
		Enemy.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute(), 1.f);
		Enemy.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1.f);

		Assister.ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 500.f);
		Assister.ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 500.f);
		Assister.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetAttackStrengthAttribute(), 50.f);

		PhaseElapsed = 0.f;
	}

	void TickWorldStep(float StepDelta)
	{
		PhaseElapsed += StepDelta;
		if (Source.Move) Source.Move->HandlePendingLaunch();
		if (Enemy.Move) Enemy.Move->HandlePendingLaunch();
		World->GetTimerManager().Tick(StepDelta);
		if (Source.Move) Source.Move->HandlePendingLaunch();
		if (Enemy.Move) Enemy.Move->HandlePendingLaunch();
		EnsureZonesAndProjectilesBegunPlay();
		TArray<ABwayHeroProjectileBase*> Projectiles;
		for (TActorIterator<ABwayHeroProjectileBase> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed()) Projectiles.Add(*It);
		}
		for (ABwayHeroProjectileBase* Proj : Projectiles)
		{
			if (!IsValid(Proj) || Proj->IsActorBeingDestroyed()) continue;
			if (UProjectileMovementComponent* PM = Proj->FindComponentByClass<UProjectileMovementComponent>())
			{
				PM->TickComponent(StepDelta, LEVELTICK_All, nullptr);
			}
		}
		if (Enemy.Move) Enemy.Move->HandlePendingLaunch();
	}

	void AdvancePhase()
	{
		++Phase;
		bPhaseInitialized = false;
		PhaseElapsed = 0.f;
	}

	bool UpdateBlessingLifecycle()
	{
		if (FPlatformTime::Seconds() - Started > 20.)
		{
			Test->AddError(TEXT("Blessing lifecycle fixture timed out"));
			return true;
		}
		if (!bPhaseInitialized)
		{
			if (!ConfigureHero(TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona"),
				TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_AbilitySet_Alona.DA_BW_AbilitySet_Alona"))) return true;
			ResetArena(200.f, 100.f, 50.f, FVector(160.f, 100.f, 200.f), FVector(160.f, 0.f, 200.f));
			Source.ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Source.ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()));
			// Real duplicate component overlaps must still produce one initial heal.
			USphereComponent* ExtraOverlap = NewObject<USphereComponent>(Ally.Pawn);
			Ally.Pawn->AddInstanceComponent(ExtraOverlap);
			ExtraOverlap->SetupAttachment(Ally.Pawn->GetRootComponent());
			ExtraOverlap->SetSphereRadius(30.f);
			ExtraOverlap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			ExtraOverlap->SetCollisionObjectType(ECC_Pawn);
			ExtraOverlap->SetCollisionResponseToAllChannels(ECR_Ignore);
			ExtraOverlap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			ExtraOverlap->RegisterComponent();
			TArray<FOverlapResult> OverlapControl;
			World->OverlapMultiByChannel(OverlapControl, Ally.Pawn->GetActorLocation(), FQuat::Identity,
				ECC_Pawn, FCollisionShape::MakeSphere(100.f));
			int32 AllyOverlapCount = 0;
			for (const auto& Hit : OverlapControl) if (Hit.GetActor() == Ally.Pawn) ++AllyOverlapCount;
			if (!Test->TestTrue(TEXT("Duplicate-component overlap control is exercised"), AllyOverlapCount >= 2)) return true;
			BlessingHealObserver = Ally.ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
				[this](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
				{
					if (Spec.GetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Heal, false, 0.f) <= 0.f) return;
					++BlessingHealApplications;
					const auto& Context = Spec.GetEffectContext();
					bBlessingAttributionValid &= Context.GetOriginalInstigator() == Source.PS
						&& Context.GetOriginalInstigatorAbilitySystemComponent() == Source.ASC;
					AActor* Causer = Context.GetEffectCauser();
					bBlessingAttributionValid &= Causer && Causer->GetClass()->GetName().Contains(TEXT("BlessingZone"));
				});
			const auto Handle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_AlonaBlessing"));
			if (!Test->TestTrue(TEXT("Production Blessing cast accepted"), Source.ASC->TryActivateAbility(Handle))) return true;
			EnsureZonesAndProjectilesBegunPlay();
			int32 ZoneCount = 0;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName().Contains(TEXT("BlessingZone")) && !It->IsActorBeingDestroyed())
				{
					BlessingZone = *It;
					++ZoneCount;
				}
			}
			if (!Test->TestEqual(TEXT("Exactly one production Blessing zone"), ZoneCount, 1)) return true;
			Test->TestTrue(TEXT("Zone owned by persistent source player"), BlessingZone->GetOwner() == Source.PS);
			BlessingDuration = BlessingZone->GetLifeSpan();
			if (!Test->TestTrue(TEXT("Bounded authored Blessing duration"), BlessingDuration > 2.2f && BlessingDuration < 10.f)) return true;
			Test->TestTrue(TEXT("Initial ally heal is exactly70"), FMath::IsNearlyEqual(Health(Ally), 120.f, .1f));
			Test->TestEqual(TEXT("Initial heal applied once"), BlessingHealApplications, 1);
			Test->TestEqual(TEXT("Enemy inside circle is not healed"), Health(Enemy), 100.f);
			BlessingLastTick = FPlatformTime::Seconds();
			bPhaseInitialized = true;
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		const float Delta = FMath::Clamp(float(Now - BlessingLastTick), 0.f, .1f);
		BlessingLastTick = Now;
		// Only zone/lifecycle timers are needed here; reset may replace fixture pawns.
		PhaseElapsed += Delta;
		World->GetTimerManager().Tick(Delta);
		if (!bSubCheckDone && PhaseElapsed >= 1.1f)
		{
			Test->TestTrue(TEXT("First periodic ally heal is exactly35"), FMath::IsNearlyEqual(Health(Ally), 155.f, .1f));
			Test->TestEqual(TEXT("Initial plus one periodic heal"), BlessingHealApplications, 2);
			if (!Test->TestTrue(TEXT("Zone remains before lifecycle trigger"), BlessingZone.IsValid() && !BlessingZone->IsActorBeingDestroyed())) return true;
			if (BlessingCase == 0)
			{
				const float RemainingBeforeDeath = BlessingZone->GetLifeSpan();
				ULyraHealthComponent* SourceHealth = ULyraHealthComponent::FindHealthComponent(Source.Pawn);
				if (!Test->TestNotNull(TEXT("Source health component"), SourceHealth)) return true;
				SourceHealth->DamageSelfDestruct();
				Test->TestEqual(TEXT("Production self-destruct depleted source health"), Health(Source), 0.f);
				// Exercise the real pawn-extension teardown before removing the corpse.
				ULyraPawnExtensionComponent::FindPawnExtensionComponent(Source.Pawn)->UninitializeAbilitySystem();
				Source.PC->UnPossess();
				Source.Pawn->Destroy();
				Source.Pawn = nullptr;
				Source.Move = nullptr;
				Test->TestNull(TEXT("Dead source ASC has no avatar"), Source.ASC->GetAvatarActor());
				Test->TestTrue(TEXT("Source death and pawn removal preserve zone"), BlessingZone.IsValid() && !BlessingZone->IsActorBeingDestroyed());
				if (BlessingZone.IsValid()) Test->TestTrue(TEXT("Death preserves original expiry deadline"),
					FMath::IsNearlyEqual(BlessingZone->GetLifeSpan(), RemainingBeforeDeath, .001f));
			}
			else
			{
				if (BlessingCase == 1) GS->GetRoundManagement()->ResetRoundState();
				else if (BlessingCase == 2) GS->GetRoundManagement()->EndRound(0, EBwayWinCondition::GoalScored);
				else if (BlessingCase == 3) BlessingZone->Destroy();
				else GS->GetRoundManagement()->ResetForNextMatch();
				Test->TestTrue(TEXT("Lifecycle trigger removes zone immediately"), !BlessingZone.IsValid() || BlessingZone->IsActorBeingDestroyed());
			}
			bSubCheckDone = true;
		}
		if (BlessingCase == 0 && PhaseElapsed >= 2.1f && !bBlessingPostDeathChecked)
		{
			Test->TestTrue(TEXT("Healing continues after source pawn teardown"), FMath::IsNearlyEqual(Health(Ally), 190.f, .1f));
			Test->TestEqual(TEXT("Post-death heal applied once"), BlessingHealApplications, 3);
			bBlessingPostDeathChecked = true;
		}
		if (BlessingCase == 0 && PhaseElapsed < BlessingDuration - .01f)
		{
			if (!Test->TestTrue(TEXT("Zone survives until original deadline"), BlessingZone.IsValid() && !BlessingZone->IsActorBeingDestroyed())) return true;
		}
		if (PhaseElapsed < BlessingDuration + .3f) return false;
		const int32 ExpectedHeals = BlessingCase == 0 ? FMath::CeilToInt(BlessingDuration) : 2;
		Test->TestEqual(TEXT("No duplicate or post-cleanup healing applications"), BlessingHealApplications, ExpectedHeals);
		if (BlessingCase == 0 || BlessingCase == 3)
		{
			Test->TestTrue(TEXT("Exact remaining heal budget"), FMath::IsNearlyEqual(Health(Ally), 120.f + (ExpectedHeals - 1) * 35.f, .1f));
			Test->TestEqual(TEXT("Enemy never healed"), Health(Enemy), 100.f);
		}
		Test->TestTrue(TEXT("Source player attribution survives teardown"), bBlessingAttributionValid);
		Test->TestTrue(TEXT("Original expiry/cleanup leaves no zone"), !BlessingZone.IsValid() || BlessingZone->IsActorBeingDestroyed());
		Test->AddInfo(FString::Printf(TEXT("Blessing lifecycle case=%d heals=%d elapsed=%.3f duration=%.3f scope=isolated-authority-timers"), BlessingCase, BlessingHealApplications, PhaseElapsed, BlessingDuration));
		return true;
	}

	bool ReviewHero(int32 Hero)
	{
		static const TCHAR* Data[] = {
			TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus"),
			TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona"),
			TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver"),
			TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger")};
		static const TCHAR* Sets[] = {
			TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_AbilitySet_Argus.DA_BW_AbilitySet_Argus"),
			TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_AbilitySet_Alona.DA_BW_AbilitySet_Alona"),
			TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver.DA_BW_AbilitySet_Hexweaver"),
			TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_AbilitySet_Gunslinger.DA_BW_AbilitySet_Gunslinger")};
		return Hero >= 0 && Hero < 4 && ConfigureHero(Data[Hero], Sets[Hero]);
	}

	TArray<AActor*> ReviewActors(const TCHAR* ClassFragment) const
	{
		TArray<AActor*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
			if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->GetClass()->GetName().Contains(ClassFragment)) Result.Add(*It);
		return Result;
	}

	TArray<FActiveGameplayEffectHandle> ReviewEffectsFrom(AActor* EffectSource) const
	{
		TArray<FActiveGameplayEffectHandle> Result;
		for (const auto H : Enemy.ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			const FActiveGameplayEffect* Effect = Enemy.ASC->GetActiveGameplayEffect(H);
			if (Effect && Effect->Spec.GetEffectContext().GetSourceObject() == EffectSource) Result.Add(H);
		}
		return Result;
	}

	bool ReviewHandlesGone(const TArray<FActiveGameplayEffectHandle>& Handles)
	{
		bool bOk = true;
		for (const auto H : Handles) bOk &= Test->TestNull(TEXT("Exact owned effect handle removed before fixture purge"), Enemy.ASC->GetActiveGameplayEffect(H));
		return bOk;
	}

	void ReviewStep()
	{
		// This isolated fixture manually advances timers/projectiles. GAS duration
		// callbacks independently read UWorld::TimeSeconds; advance that same clock.
		// One latent update per engine frame avoids TimerManager's per-frame guard.
		constexpr float Delta = .025f;
		if (World->GetTimerManager().HasBeenTickedThisFrame()) return;
		World->TimeSeconds += Delta;
		World->UnpausedTimeSeconds += Delta;
		TickWorldStep(Delta);
	}

	bool ReviewKillSource(bool bDestroyPawn)
	{
		ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(Source.Pawn);
		if (!Test->TestNotNull(TEXT("Production source health component"), HC)) return false;
		HC->DamageSelfDestruct();
		if (!Test->TestEqual(TEXT("Production self-destruct depletes source health"), Health(Source), 0.f)) return false;
		if (bDestroyPawn)
		{
			ULyraPawnExtensionComponent::FindPawnExtensionComponent(Source.Pawn)->UninitializeAbilitySystem();
			Source.PC->UnPossess();
			Source.Pawn->Destroy();
			Source.Pawn = nullptr;
			Source.Move = nullptr;
			Test->TestNull(TEXT("Source ASC avatar removed by production pawn extension"), Source.ASC->GetAvatarActor());
		}
		return true;
	}

	void ReviewRoundTrigger(int32 Trigger)
	{
		if (Trigger == 0) GS->GetRoundManagement()->ResetRoundState();
		else if (Trigger == 1) GS->GetRoundManagement()->EndRound(0, EBwayWinCondition::GoalScored);
		else GS->GetRoundManagement()->ResetForNextMatch();
	}

	bool UpdateCircleLifecycle()
	{
		// 0 retained dead caster; 1 removed caster; 2 target teardown;
		// 3 direct reset; 4 round end; 5 rematch reset. Each owns a fresh world.
		if (!bPhaseInitialized)
		{
			if (!ReviewHero(2)) return true;
			ResetArena(200.f, 500.f, 200.f, ReviewCase <= 1 ? FVector(-2000.f, 0.f, 200.f) : FVector(160.f, 0.f, 200.f));
			const auto H = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynCircleOfSpite"));
			if (!Test->TestTrue(TEXT("Real Circle activation accepted"), Source.ASC->TryActivateAbility(H))) return true;
			EnsureZonesAndProjectilesBegunPlay();
			const auto Zones = ReviewActors(TEXT("BwayKorrynSpiteZone"));
			if (!Test->TestEqual(TEXT("Exactly one production Circle"), Zones.Num(), 1)) return true;
			ReviewActor = Zones[0];
			ReviewDuration = ReviewActor->GetLifeSpan();
			Test->TestTrue(TEXT("Circle duration remains authored five seconds"), FMath::IsNearlyEqual(ReviewDuration, 5.f, .01f));
			Test->TestTrue(TEXT("Circle preserves production pawn ownership"), ReviewActor->GetOwner() == Source.Pawn);
			if (ReviewCase <= 1)
			{
				Test->TestFalse(TEXT("Outside target starts unaffected"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed));
			}
			else
			{
				ReviewEffectHandles = ReviewEffectsFrom(ReviewActor.Get());
				if (!Test->TestEqual(TEXT("Circle owns exactly slow and amp effects"), ReviewEffectHandles.Num(), 2)) return true;
			}
			bPhaseInitialized = true;
			return false;
		}
		ReviewStep();
		if (!bSubCheckDone && PhaseElapsed >= .25f)
		{
			if (!Test->TestTrue(TEXT("Circle alive before trigger"), ReviewActor.IsValid() && !ReviewActor->IsActorBeingDestroyed())) return true;
			const float Remaining = ReviewActor->GetLifeSpan();
			if (ReviewCase <= 1)
			{
				if (!ReviewKillSource(ReviewCase == 1)) return true;
				if (!Test->TestTrue(TEXT("Caster death preserves Circle"), ReviewActor.IsValid() && !ReviewActor->IsActorBeingDestroyed())) return true;
				Test->TestTrue(TEXT("Caster death does not restart original lifespan"), FMath::IsNearlyEqual(Remaining, ReviewActor->GetLifeSpan(), .001f));
				Enemy.Pawn->SetActorLocation(FVector(160.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
				Enemy.Pawn->UpdateOverlaps();
				ReviewEffectHandles = ReviewEffectsFrom(ReviewActor.Get());
				Test->TestEqual(TEXT("Post-death entrant receives exactly two owned effects"), ReviewEffectHandles.Num(), 2);
				Test->TestTrue(TEXT("Post-death Circle slows exact15 percent"), FMath::IsNearlyEqual(MoveMult(Enemy), .85f, .001f));
				Test->TestTrue(TEXT("Post-death Circle amplifies exact35 percent"), FMath::IsNearlyEqual(DamageMult(Enemy), 1.35f, .001f));
				for (const auto EffectH : ReviewEffectHandles)
				{
					const auto* Effect = Enemy.ASC->GetActiveGameplayEffect(EffectH);
					if (Effect) Test->TestTrue(TEXT("Circle context retains original player and ASC"),
						Effect->Spec.GetEffectContext().GetOriginalInstigator() == Source.PS && Effect->Spec.GetEffectContext().GetOriginalInstigatorAbilitySystemComponent() == Source.ASC);
				}
			}
			else if (ReviewCase == 2)
			{
				ULyraPawnExtensionComponent::FindPawnExtensionComponent(Enemy.Pawn)->UninitializeAbilitySystem();
				Enemy.PC->UnPossess();
				Enemy.Pawn->Destroy(); Enemy.Pawn = nullptr; Enemy.Move = nullptr;
				ReviewActor->Destroy();
				ReviewHandlesGone(ReviewEffectHandles);
			}
			else ReviewRoundTrigger(ReviewCase - 3);
			bSubCheckDone = true;
		}
		if (ReviewCase <= 1 && PhaseElapsed >= ReviewDuration - .1f && PhaseElapsed < ReviewDuration - .04f)
		{
			bReviewDeadlineSample = true;
			Test->TestTrue(TEXT("Circle and owned effects persist immediately before original deadline"), ReviewActor.IsValid()
				&& !ReviewActor->IsActorBeingDestroyed() && ReviewEffectsFrom(ReviewActor.Get()).Num() == 2);
		}
		if (PhaseElapsed < (ReviewCase <= 1 ? ReviewDuration + .15f : .5f)) return false;
		if (ReviewCase <= 1) Test->TestTrue(TEXT("Observed pre-expiry boundary"), bReviewDeadlineSample);
		Test->TestTrue(TEXT("Circle removed on original expiry or production transition"), !ReviewActor.IsValid() || ReviewActor->IsActorBeingDestroyed());
		ReviewHandlesGone(ReviewEffectHandles);
		Test->TestTrue(TEXT("Target persistent ASC speed restored"), FMath::IsNearlyEqual(MoveMult(Enemy), 1.f, .001f));
		Test->TestTrue(TEXT("Target persistent ASC amplification restored"), FMath::IsNearlyEqual(DamageMult(Enemy), 1.f, .001f));
		Test->TestFalse(TEXT("No stale Circle slow tag"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed));
		Test->TestFalse(TEXT("No stale Circle amp tag"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_DamageAmplified));
		Test->AddInfo(FString::Printf(TEXT("CircleLifecycle case=%d duration=%.3f elapsed=%.3f scope=authority-lifecycle-no-respawn"), ReviewCase, ReviewDuration, PhaseElapsed));
		return true;
	}

	bool UpdateProjectileLifecycle()
	{
		// Three real primaries x seven cases: retained/deleted dead-source hit,
		// deleted-source expiry, reset/end/rematch, and actual lethal credit.
		const int32 Hero = ReviewCase / 7 + 1;
		const int32 Scenario = ReviewCase % 7;
		if (!bPhaseInitialized)
		{
			if (!ReviewHero(Hero)) return true;
			ResetArena(200.f, Scenario == 6 ? 5.f : 500.f, 200.f, FVector(1200.f, 0.f, 200.f), FVector(-2000.f, 2000.f, 200.f));
			Assister.Pawn->SetActorLocation(FVector(-2000.f, -2000.f, 200.f));
			static const TCHAR* Classes[] = {TEXT("BwayGameplayAbility_AlonaPrimary"), TEXT("BwayGameplayAbility_KorrynPrimary"), TEXT("BwayGameplayAbility_RawlinsPrimary")};
			ActiveHandle = FindAbilityHandle(Source.ASC, Classes[Hero - 1]);
			ReviewEffectObserver = Enemy.ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
				[this](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
				{
					if (Spec.GetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, false, 0.f) <= 0.f) return;
					++ReviewDamageEffects;
					const auto& Context = Spec.GetEffectContext();
					bReviewContextValid &= Context.GetOriginalInstigator() == Source.PS && Context.GetOriginalInstigatorAbilitySystemComponent() == Source.ASC
						&& Context.GetEffectCauser() == ReviewActor.Get();
				});
			if (!Test->TestTrue(TEXT("Real primary activates before shooter death"), Source.ASC->TryActivateAbility(ActiveHandle))) return true;
			EnsureZonesAndProjectilesBegunPlay();
			TArray<AActor*> Projectiles;
			for (TActorIterator<ABwayHeroProjectileBase> It(World); It; ++It) if (IsValid(*It) && !It->IsActorBeingDestroyed()) Projectiles.Add(*It);
			if (!Test->TestEqual(TEXT("Exactly one already-fired projectile before timer advancement"), Projectiles.Num(), 1)) return true;
			ReviewActor = Projectiles[0];
			ReviewDuration = ReviewActor->GetLifeSpan();
			if (!Test->TestTrue(TEXT("Projectile has bounded authored lifespan"), ReviewDuration > .1f && ReviewDuration < 10.f)) return true;
			if (!ReviewKillSource(Scenario != 0)) return true;
			Test->TestTrue(TEXT("Already-fired projectile survives source death"), ReviewActor.IsValid() && !ReviewActor->IsActorBeingDestroyed());
			Test->TestTrue(TEXT("Death preserves projectile original expiry"), FMath::IsNearlyEqual(ReviewDuration, ReviewActor->GetLifeSpan(), .001f));
			// Rawlins' delayed second shot must be cancelled through actual teardown;
			// retained-corpse case uses real GAS cancel because this isolated fixture
			// has no authored automatic death-ability grant.
			if (Scenario == 0) Source.ASC->CancelAbilityHandle(ActiveHandle);
			if (Scenario <= 1 || Scenario == 6)
			{
				Enemy.Pawn->SetActorLocation(FVector(160.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			else Enemy.Pawn->SetActorLocation(FVector(-2000.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (Scenario >= 3 && Scenario <= 5) ReviewRoundTrigger(Scenario - 3);
			bPhaseInitialized = true;
			return false;
		}
		ReviewStep();
		if ((Scenario <= 1 || Scenario == 6) && !bSubCheckDone && ReviewDamageEffects > 0)
		{
			static const float ExpectedDamage[] = {38.f, 34.f, 19.f};
			ReviewHitHealth = Health(Enemy);
			Test->TestTrue(TEXT("Exactly one original projectile deals expected damage"), FMath::IsNearlyEqual(ReviewHitHealth, Scenario == 6 ? 0.f : 500.f - ExpectedDamage[Hero - 1], .05f));
			if (Scenario != 6) Test->TestEqual(TEXT("Projectile creates exactly one credit contribution"), Enemy.Pawn->GetCombatContributionCountForAudit(), 1);
			Test->TestTrue(TEXT("Original persistent source context used after death"), bReviewContextValid);
			Test->TestEqual(TEXT("One damage application from the fired projectile"), ReviewDamageEffects, 1);
			if (Scenario != 6) Test->TestTrue(TEXT("Only Korryn retains its independent armor shred"), FMath::IsNearlyEqual(Armor(Enemy), Hero == 2 ? -2.f : 0.f, .001f));
			if (Scenario == 6)
			{
				ULyraHealthComponent* TargetHealth = ULyraHealthComponent::FindHealthComponent(Enemy.Pawn);
				if (!Test->TestNotNull(TEXT("Lethal target health component"), TargetHealth)) return true;
				// Existing standalone credit fixture's production death route. This
				// supplies controller ownership, not a fabricated contribution.
				Enemy.PC->PlayerState = Enemy.PS;
				if (!TargetHealth->IsDeadOrDying()) TargetHealth->StartDeath();
				Enemy.PC->PlayerState = nullptr;
				Test->TestEqual(TEXT("Post-mortem projectile credits original dead shooter"), Source.PS->GetKills(), 1);
				Test->TestEqual(TEXT("Post-mortem projectile records target death"), Enemy.PS->GetDeaths(), 1);
				Test->TestEqual(TEXT("Lethal projectile contribution consumed exactly once"), Enemy.Pawn->GetCombatContributionCountForAudit(), 0);
			}
			bSubCheckDone = true;
		}
		if (Scenario == 2 && PhaseElapsed >= ReviewDuration - .1f && PhaseElapsed < ReviewDuration - .04f)
		{
			bReviewDeadlineSample = true;
			Test->TestTrue(TEXT("Missed projectile remains immediately before original expiry"), ReviewActor.IsValid() && !ReviewActor->IsActorBeingDestroyed());
		}
		if (PhaseElapsed < (Scenario == 2 ? ReviewDuration + .15f : .5f)) return false;
		if (Scenario <= 1 || Scenario == 6)
		{
			Test->TestTrue(TEXT("Post-mortem projectile actually hit through production collision"), bSubCheckDone);
			Test->TestTrue(TEXT("No delayed second hit after teardown/cancel"), FMath::IsNearlyEqual(Health(Enemy), ReviewHitHealth, .001f));
			Test->TestEqual(TEXT("No extra delayed damage applications"), ReviewDamageEffects, 1);
		}
		else
		{
			Test->TestEqual(TEXT("Miss/transition causes no target damage"), Health(Enemy), 500.f);
			Test->TestEqual(TEXT("Miss/transition creates no contribution"), Enemy.Pawn->GetCombatContributionCountForAudit(), 0);
			Test->TestEqual(TEXT("Miss/transition creates no damage application"), ReviewDamageEffects, 0);
		}
		if (Scenario == 2) Test->TestTrue(TEXT("Observed projectile pre-expiry boundary"), bReviewDeadlineSample);
		Test->TestEqual(TEXT("Dead source receives no life-leech healing"), Health(Source), 0.f);
		Test->TestTrue(TEXT("Projectile removed on hit, original expiry or transition"), !ReviewActor.IsValid() || ReviewActor->IsActorBeingDestroyed());
		int32 LiveProjectiles = 0;
		for (TActorIterator<ABwayHeroProjectileBase> It(World); It; ++It) if (IsValid(*It) && !It->IsActorBeingDestroyed()) ++LiveProjectiles;
		Test->TestEqual(TEXT("No stale or delayed-shot projectile"), LiveProjectiles, 0);
		Test->AddInfo(FString::Printf(TEXT("ProjectileLifecycle hero=%d scenario=%d elapsed=%.3f scope=authority-real-projectile-no-owner"), Hero, Scenario, PhaseElapsed));
		return true;
	}

	static const TCHAR* ReviewActionClass(int32 Action)
	{
		static const TCHAR* Classes[] = {
			TEXT("BwayGameplayAbility_MeleePrimary"), TEXT("BwayGameplayAbility_ArgusSlide"), TEXT("BwayGameplayAbility_NoRetreat"), TEXT("BwayGameplayAbility_ForGlory"), TEXT("BwayGameplayAbility_Retribution"),
			TEXT("BwayGameplayAbility_AlonaPrimary"), TEXT("BwayGameplayAbility_AlonaSunsGrace"), TEXT("BwayGameplayAbility_AlonaRadiance"), TEXT("BwayGameplayAbility_AlonaSunBurst"), TEXT("BwayGameplayAbility_AlonaBlessing"),
			TEXT("BwayGameplayAbility_KorrynPrimary"), TEXT("BwayGameplayAbility_KorrynFlock"), TEXT("BwayGameplayAbility_KorrynBurdenOfSin"), TEXT("BwayGameplayAbility_KorrynCircleOfSpite"), TEXT("BwayGameplayAbility_KorrynAuraOfSilence"),
			TEXT("BwayGameplayAbility_RawlinsPrimary"), TEXT("BwayGameplayAbility_RawlinsDoubleDown"), TEXT("BwayGameplayAbility_RawlinsPowerShot"), TEXT("BwayGameplayAbility_RawlinsSlideShot"), TEXT("BwayGameplayAbility_RawlinsBlazingBarrage")};
		return Classes[Action];
	}

	bool ReviewEnemyStrike()
	{
		Enemy.Pawn->SetActorLocationAndRotation(Source.Pawn->GetActorLocation() - FVector(160.f, 0.f, 0.f), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Enemy.PC->SetControlRotation(FRotator::ZeroRotator);
		return Test->TestTrue(TEXT("Real opposing primary strike accepted"), Enemy.ASC->TryActivateAbility(ReviewStrikeHandle));
	}

	bool ReviewRadianceParameters()
	{
		const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
		const auto* Soft = Spec && Spec->Ability ? FindFProperty<FSoftObjectProperty>(Spec->Ability->GetClass(), TEXT("KitConfig")) : nullptr;
		UObject* Config = Soft ? Soft->GetPropertyValue_InContainer(Spec->Ability).LoadSynchronous() : nullptr;
		if (!Test->TestNotNull(TEXT("Actual Radiance grant's loaded kit config"), Config)) return false;
		const auto Read = [&](const TCHAR* Name, float& Out)
		{
			const auto* Prop = FindFProperty<FFloatProperty>(Config->GetClass(), Name);
			if (!Test->TestNotNull(Name, Prop)) return false;
			Out = Prop->GetPropertyValue_InContainer(Config);
			return FMath::IsFinite(Out);
		};
		float Base = 0.f, Divisor = 0.f, Cap = 0.f, Interval = 0.f;
		if (!Read(TEXT("RadianceHealPerTick"), Base) || !Read(TEXT("RadianceStrengthDivisor"), Divisor)
			|| !Read(TEXT("RadianceMaxHealMultiplier"), Cap) || !Read(TEXT("RadianceActiveDuration"), ReviewDuration)
			|| !Read(TEXT("RadianceHealTickInterval"), Interval)) return false;
		if (!Test->TestTrue(TEXT("Radiance authored schedule is finite and bounded"), Divisor > 0.f && Cap > 0.f && Interval > 0.f && ReviewDuration >= Interval && ReviewDuration < 10.f)) return false;
		ReviewExpectedHeal = Base * FMath::Clamp(Source.ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetAttackStrengthAttribute()) / Divisor, 0.f, Cap);
		ReviewExpectedHealCount = FMath::FloorToInt(ReviewDuration / Interval + KINDA_SMALL_NUMBER);
		return Test->TestTrue(TEXT("Radiance independent expected heal is positive"), ReviewExpectedHeal > 0.f && ReviewExpectedHealCount > 0);
	}

	bool UpdateTargeting()
	{
		const int32 Action = ReviewCase / 4;
		const int32 Variant = ReviewCase % 4; // enemy positive, friendly, miss, raw immunity
		const bool bMovement = Action == 1 || Action == 6 || Action == 11 || Action == 16;
		const bool bHealing = Action == 7 || Action == 9;
		if ((bMovement && Variant != 0) || (bHealing && Variant == 3))
		{
			Test->AddInfo(FString::Printf(TEXT("All20Applicability action=%d variant=%d result=NA reason=%s"), Action, Variant,
				bMovement ? TEXT("self-movement; real protection covered by variant0") : TEXT("healing has no damage-immunity rejection requirement")));
			return true;
		}
		if (!bPhaseInitialized)
		{
			if (!ReviewHero(Action / 5)) return true;
			const FVector Far(-5000.f, 3000.f, 200.f);
			const bool bAllyInLane = bHealing ? Variant == 0 : Variant == 1;
			const bool bEnemyInLane = bHealing ? Variant == 1 : Variant == 0 || Variant == 3;
			ResetArena(200.f, 500.f, 100.f, bEnemyInLane ? FVector(160.f, 0.f, 200.f) : Far,
				bAllyInLane ? FVector(160.f, 0.f, 200.f) : Far + FVector(0.f, 1000.f, 0.f));
			Assister.Pawn->SetActorLocation(Far + FVector(0.f, 2000.f, 0.f));
			Test->TestTrue(TEXT("Targeting control has distinct enemy and friendly teams"), !GS->AreOnSameTeam(Source.Pawn, Enemy.Pawn) && GS->AreOnSameTeam(Source.Pawn, Ally.Pawn));
			ActiveHandle = FindAbilityHandle(Source.ASC, ReviewActionClass(Action));
			if (!Test->TestTrue(TEXT("Requested production action grant exists"), ActiveHandle.IsValid())) return true;
			if (Action == 7 && !ReviewRadianceParameters()) return true;
			if (bMovement)
			{
				Source.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 0.f);
				const auto* Set = LoadObject<ULyraAbilitySet>(nullptr, TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_AbilitySet_Argus.DA_BW_AbilitySet_Argus"));
				if (!Test->TestNotNull(TEXT("Opposing strike authored set"), Set) || !GrantAbilitySet(Enemy.ASC, Set)) return true;
				Enemy.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetAttackStrengthAttribute(), 50.f);
				Enemy.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), 0.f);
				ReviewStrikeHandle = FindAbilityHandle(Enemy.ASC, TEXT("BwayGameplayAbility_MeleePrimary"));
				if (!ReviewEnemyStrike()) return true;
				if (!Test->TestTrue(TEXT("Vulnerable lane actually takes30 damage before window"), FMath::IsNearlyEqual(Health(Source), 170.f, .05f))) return true;
				Source.ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 200.f);
			}
			if (Variant == 3) Enemy.ASC->AddLooseGameplayTag(TAG_Gameplay_DamageImmunity);
			ReviewOutcomes = PrimaryNegativeOutcomes();
			if (!Test->TestTrue(TEXT("Real targeting action accepted"), Source.ASC->TryActivateAbility(ActiveHandle))) return true;
			EnsureZonesAndProjectilesBegunPlay();
			if (bMovement)
			{
				if (!Test->TestTrue(TEXT("Actual ability window grants damage immunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity))) return true;
				if (Action == 11) Test->TestEqual(TEXT("Actual Flock window ignores Pawn collisions"), Source.Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
				if (!ReviewEnemyStrike()) return true;
				Test->TestEqual(TEXT("Actual opposing hit cannot damage active immunity window"), Health(Source), 200.f);
			}
			bPhaseInitialized = true;
			return false;
		}
		ReviewStep();
		const float ObserveAt = bMovement ? 2.f : bHealing ? .15f : Action == 19 ? 1.4f : .8f;
		if (PhaseElapsed < ObserveAt) return false;
		if (bMovement)
		{
			const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			Test->TestFalse(TEXT("Movement action ended naturally"), Spec && Spec->IsActive());
			Test->TestFalse(TEXT("Movement action removed its immunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
			if (!ReviewEnemyStrike()) return true;
			Test->TestTrue(TEXT("Same opposing strike deals30 damage after window"), FMath::IsNearlyEqual(Health(Source), 170.f, .05f));
		}
		else if (bHealing)
		{
			Test->TestEqual(TEXT("Healing never changes enemy health"), Health(Enemy), 500.f);
			Test->TestEqual(TEXT("Healing creates no enemy contribution"), Enemy.Pawn->GetCombatContributionCountForAudit(), 0);
			if (Action == 7)
			{
				Test->TestTrue(TEXT("Radiance heals selected ally or defined self fallback by exact configured scaling"), FMath::IsNearlyEqual(Health(Ally), 100.f + (Variant == 0 ? ReviewExpectedHeal : 0.f), .05f)
					&& FMath::IsNearlyEqual(Health(Source), 200.f + (Variant == 0 ? 0.f : ReviewExpectedHeal), .05f));
			}
			else Test->TestTrue(TEXT("Blessing initial70 reaches only in-zone ally"), FMath::IsNearlyEqual(Health(Ally), Variant == 0 ? 170.f : 100.f, .05f));
		}
		else if (Variant == 1 || Variant == 2)
		{
			Test->TestTrue(TEXT("Friendly/miss activation leaves exact combat outcomes unchanged"), ReviewOutcomes == PrimaryNegativeOutcomes());
			Test->TestTrue(TEXT("Friendly/miss target has no pending launch"), EffectiveVelocity(Enemy).IsNearlyZero() && EffectiveVelocity(Ally).IsNearlyZero());
		}
		else
		{
			static const float Damage[] = {30.f, 0.f, 22.f, 22.f, 80.f, 38.f, 0.f, 0.f, 46.f, 0.f, 34.f, 0.f, 45.f, 0.f, 33.f, 38.f, 0.f, 69.f, 46.f, 264.f};
			const float ExpectedDamage = Variant == 3 ? 0.f : Damage[Action];
			Test->TestTrue(TEXT("Positive/immunity action has exact health damage"), FMath::IsNearlyEqual(Health(Enemy), 500.f - ExpectedDamage, .1f));
			Test->TestTrue(TEXT("Positive/immunity action has exact actual-damage leech"), FMath::IsNearlyEqual(Health(Source), 200.f + ExpectedDamage * .1f, .1f));
			Test->TestEqual(TEXT("Blocked/zero damage cannot create combat contribution"), Enemy.Pawn->GetCombatContributionCountForAudit(), ExpectedDamage > 0.f ? 1 : 0);
			Test->TestTrue(TEXT("Independent armor shred follows primary hit even under raw immunity"), FMath::IsNearlyEqual(Armor(Enemy), Action == 10 ? -2.f : 0.f, .001f));
			Test->TestEqual(TEXT("Independent silence follows Aura hit even under raw immunity"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced), Action == 14);
			Test->TestEqual(TEXT("Slow applies only for the two configured actions"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed), Action == 12 || Action == 13);
			Test->TestTrue(TEXT("Independent slow has exact action magnitude"), FMath::IsNearlyEqual(MoveMult(Enemy), Action == 12 ? .5f : Action == 13 ? .85f : 1.f, .001f));
			Test->TestTrue(TEXT("Circle amplification has exact action magnitude"), FMath::IsNearlyEqual(DamageMult(Enemy), Action == 13 ? 1.35f : 1.f, .001f));
		}
		Test->AddInfo(FString::Printf(TEXT("All20Targeting action=%d variant=%d elapsed=%.3f scope=authority-real-action"), Action, Variant, PhaseElapsed));
		return true;
	}

	bool ReviewInterruptAsync(int32 Action, int32 Trigger)
	{
		const auto* BeforeSpec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
		if (!Test->TestTrue(TEXT("Exact async instance still active at interruption"), BeforeSpec && BeforeSpec->IsActive())) return false;
		ReviewHitHealth = Health(Enemy);
		ReviewAllyHealth = Health(Ally);
		ReviewContributionCount = Enemy.Pawn->GetCombatContributionCountForAudit();
		ReviewSourceHealth = Trigger == 1 ? 0.f : Health(Source);
		if (Trigger == 0) Source.ASC->CancelAbilityHandle(ActiveHandle);
		else
		{
			if (Trigger == 1 && !ReviewKillSource(false)) return false;
			ULyraPawnExtensionComponent::FindPawnExtensionComponent(Source.Pawn)->UninitializeAbilitySystem();
			Test->TestNull(TEXT("Production avatar teardown unbinds ASC"), Source.ASC->GetAvatarActor());
		}
		Test->TestTrue(TEXT("Interruption itself cannot heal the source"), FMath::IsNearlyEqual(Health(Source), ReviewSourceHealth, .001f));
		Test->TestTrue(TEXT("Interruption itself leaves target and ally health unchanged"), FMath::IsNearlyEqual(Health(Enemy), ReviewHitHealth, .001f)
			&& FMath::IsNearlyEqual(Health(Ally), ReviewAllyHealth, .001f));
		Test->TestEqual(TEXT("Interruption itself preserves earned contribution"), Enemy.Pawn->GetCombatContributionCountForAudit(), ReviewContributionCount);
		const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
		Test->TestFalse(TEXT("Exact interrupted spec immediately inactive"), Spec && Spec->IsActive());
		Test->TestFalse(TEXT("Interrupted action immediately clears owned immunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
		Test->TestFalse(TEXT("Interrupted action immediately clears owned ethereal"), Source.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal));
		if (Action == 11)
		{
			Test->TestEqual(TEXT("Interrupted Flock restores prior fly speed"), Source.Move->MaxFlySpeed, 345.f);
			Test->TestEqual(TEXT("Interrupted Flock restores prior gravity"), Source.Move->GravityScale, 1.2f);
			if (Trigger == 1)
			{
				// Death cancels Flock first, then Lyra's OnDeathStarted disables
				// movement/collision. That terminal policy supersedes living restoration.
				const auto* HealthComponent = ULyraHealthComponent::FindHealthComponent(Source.Pawn);
				Test->TestTrue(TEXT("Interrupted Flock source entered production death"), HealthComponent && HealthComponent->IsDeadOrDying());
				Test->TestEqual(TEXT("Dead Flock source keeps capsule collision disabled"), Source.Pawn->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
				Test->TestEqual(TEXT("Dead Flock source keeps pawn response ignored"), Source.Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
				Test->TestEqual(TEXT("Dead Flock source keeps movement disabled"), Source.Move->MovementMode.GetValue(), MOVE_None);
			}
			else
			{
				Test->TestEqual(TEXT("Living interrupted Flock restores prior pawn response"), Source.Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
			}
		}
		bSubCheckDone = true;
		return true;
	}

	bool UpdateAsyncLifecycle()
	{
		static const int32 Actions[] = {1, 2, 4, 6, 7, 11, 15, 16, 18};
		const bool bBetweenRetributionHits = ReviewCase >= 27;
		const int32 Action = bBetweenRetributionHits ? 4 : Actions[ReviewCase / 3];
		const int32 Trigger = bBetweenRetributionHits ? ReviewCase - 27 : ReviewCase % 3; // GAS cancel, death+uninitialize, healthy uninitialize
		if (!bPhaseInitialized)
		{
			if (!ReviewHero(Action / 5)) return true;
			ResetArena(200.f, 500.f, 100.f, FVector(160.f, 0.f, 200.f), FVector(160.f, 100.f, 200.f));
			Source.Move->MaxFlySpeed = 345.f;
			Source.Move->GravityScale = 1.2f;
			ActiveHandle = FindAbilityHandle(Source.ASC, ReviewActionClass(Action));
			if (!Test->TestTrue(TEXT("Real async action activation accepted"), Source.ASC->TryActivateAbility(ActiveHandle))) return true;
			const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			if (!Test->TestTrue(TEXT("Async action demonstrably active before interruption"), Spec && Spec->IsActive())) return true;
			EnsureZonesAndProjectilesBegunPlay();
			if (!bBetweenRetributionHits && !ReviewInterruptAsync(Action, Trigger)) return true;
			bPhaseInitialized = true;
			return false;
		}
		ReviewStep();
		if (bBetweenRetributionHits && !bSubCheckDone)
		{
			if (PhaseElapsed < .15f) return false;
			if (!Test->TestTrue(TEXT("Real Retribution uppercut landed before interruption"), FMath::IsNearlyEqual(Health(Enemy), 465.f, .05f)
				&& EffectiveVelocity(Enemy).Z >= 500.f)) return true;
			if (!ReviewInterruptAsync(Action, Trigger)) return true;
		}
		if (PhaseElapsed < (Action == 7 ? 4.3f : 2.f)) return false;
		const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
		Test->TestFalse(TEXT("Interrupted action stays inactive beyond original callbacks"), Spec && Spec->IsActive());
		// The approved policy preserves a bullet already fired before interruption.
		const float AlreadyFiredDamage = Action == 15 ? 19.f : 0.f;
		Test->TestTrue(TEXT("No late damage other than the single already-fired bullet"), FMath::IsNearlyEqual(Health(Enemy), ReviewHitHealth - AlreadyFiredDamage, .05f));
		Test->TestEqual(TEXT("Interruption preserves legitimate earned contributions without adding late hits"), Enemy.Pawn->GetCombatContributionCountForAudit(),
			Action == 15 ? 1 : ReviewContributionCount);
		Test->TestTrue(TEXT("Interrupted Radiance produces no later heal tick"), FMath::IsNearlyEqual(Health(Ally), ReviewAllyHealth, .05f));
		const float ExpectedSource = ReviewSourceHealth + (Trigger == 0 ? AlreadyFiredDamage * .1f : 0.f);
		Test->TestTrue(TEXT("No dead/detached source leech or unexpected self-heal"), FMath::IsNearlyEqual(Health(Source), ExpectedSource, .05f));
		Test->TestFalse(TEXT("No delayed restoration of immunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
		Test->TestFalse(TEXT("No delayed restoration of ethereal"), Source.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal));
		int32 LiveProjectiles = 0;
		for (TActorIterator<ABwayHeroProjectileBase> It(World); It; ++It) if (IsValid(*It) && !It->IsActorBeingDestroyed()) ++LiveProjectiles;
		Test->TestEqual(TEXT("No unbounded delayed projectile after interruption"), LiveProjectiles, 0);
		Test->AddInfo(FString::Printf(TEXT("AsyncLifecycle action=%d trigger=%d betweenHits=%d elapsed=%.3f scope=authority-cancel-death-avatar-teardown-not-round-respawn"), Action, Trigger, bBetweenRetributionHits, PhaseElapsed));
		return true;
	}

	TArray<FActiveGameplayEffectHandle> ReviewEffectFamily(const TCHAR* Fragment) const
	{
		TArray<FActiveGameplayEffectHandle> Result;
		for (const auto H : Enemy.ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			const auto* Effect = Enemy.ASC->GetActiveGameplayEffect(H);
			if (Effect && Effect->Spec.Def && Effect->Spec.Def->GetClass()->GetName().Contains(Fragment)) Result.Add(H);
		}
		return Result;
	}

	bool UpdateStatusOwnership()
	{
		// 0/1 independent Circle removal orders; 2 Burden+Circle expiry;
		// 3 primary shred cap/refresh/expiry; 4 silence expiry; 5/6 full/clamped Radiance.
		if (!bPhaseInitialized)
		{
			if (!ReviewHero(ReviewCase >= 5 ? 1 : 2)) return true;
			ResetArena(200.f, 1000.f, ReviewCase == 6 ? 490.f : 100.f, FVector(160.f, 0.f, 200.f), FVector(160.f, 100.f, 200.f));
			if (ReviewCase <= 2)
			{
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynCircleOfSpite"));
				if (!Test->TestTrue(TEXT("First real Circle accepted"), Source.ASC->TryActivateAbility(ActiveHandle))) return true;
				EnsureZonesAndProjectilesBegunPlay();
				const auto First = ReviewActors(TEXT("BwayKorrynSpiteZone"));
				if (!Test->TestEqual(TEXT("One first Circle"), First.Num(), 1)) return true;
				ReviewActor = First[0];
				ReviewEffectHandles = ReviewEffectsFrom(ReviewActor.Get());
				if (!Test->TestEqual(TEXT("First zone owns exactly two handles"), ReviewEffectHandles.Num(), 2)) return true;
				if (ReviewCase <= 1)
				{
					const auto* Set = LoadObject<ULyraAbilitySet>(nullptr, TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver.DA_BW_AbilitySet_Hexweaver"));
					if (!Test->TestNotNull(TEXT("Second source authored Korryn grant"), Set)) return true;
					UBwayHeroStatsLibrary::ApplyHeroStatsFromDataAsset(Ally.Pawn, CurrentHeroData);
					if (!GrantAbilitySet(Ally.ASC, Set)) return true;
					const auto SecondH = FindAbilityHandle(Ally.ASC, TEXT("BwayGameplayAbility_KorrynCircleOfSpite"));
					if (!Test->TestTrue(TEXT("Distinct source real Circle accepted"), Ally.ASC->TryActivateAbility(SecondH))) return true;
					EnsureZonesAndProjectilesBegunPlay();
					const auto Both = ReviewActors(TEXT("BwayKorrynSpiteZone"));
					if (!Test->TestEqual(TEXT("Two separate production circles"), Both.Num(), 2)) return true;
					AActor* Second = Both[0] == ReviewActor.Get() ? Both[1] : Both[0];
					const auto SecondHandles = ReviewEffectsFrom(Second);
					if (!Test->TestEqual(TEXT("Second zone owns independent two handles"), SecondHandles.Num(), 2)) return true;
					for (const auto H : SecondHandles)
					{
						Test->TestFalse(TEXT("Sources do not share target effect handles"), ReviewEffectHandles.Contains(H));
						const auto* Effect = Enemy.ASC->GetActiveGameplayEffect(H);
						if (Effect) Test->TestTrue(TEXT("Second zone effect retains second player's ASC"), Effect->Spec.GetEffectContext().GetOriginalInstigatorAbilitySystemComponent() == Ally.ASC);
					}
					Test->TestTrue(TEXT("Two Circle slows compound while amp remains override35percent"), FMath::IsNearlyEqual(MoveMult(Enemy), .85f * .85f, .001f) && FMath::IsNearlyEqual(DamageMult(Enemy), 1.35f, .001f));
					AActor* Removed = ReviewCase == 0 ? ReviewActor.Get() : Second;
					AActor* Surviving = ReviewCase == 0 ? Second : ReviewActor.Get();
					const auto RemovedHandles = ReviewEffectsFrom(Removed);
					const auto SurvivingHandles = ReviewEffectsFrom(Surviving);
					Removed->Destroy();
					ReviewHandlesGone(RemovedHandles);
					for (const auto H : SurvivingHandles) Test->TestNotNull(TEXT("Other source's exact handle survives removal"), Enemy.ASC->GetActiveGameplayEffect(H));
					Test->TestTrue(TEXT("Removing one zone preserves remaining15percent slow and35percent amp"), FMath::IsNearlyEqual(MoveMult(Enemy), .85f, .001f) && FMath::IsNearlyEqual(DamageMult(Enemy), 1.35f, .001f));
					Enemy.Pawn->SetActorLocation(FVector(-2000.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
					ReviewHandlesGone(SurvivingHandles);
					Enemy.Pawn->SetActorLocation(FVector(160.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
					const auto ReentryHandles = ReviewEffectsFrom(Surviving);
					Test->TestEqual(TEXT("Reentry creates one fresh pair, never duplicate effects"), ReentryHandles.Num(), 2);
					Surviving->Destroy();
					ReviewHandlesGone(ReentryHandles);
					Test->TestTrue(TEXT("Removing final zone restores baseline attributes"), FMath::IsNearlyEqual(MoveMult(Enemy), 1.f, .001f) && FMath::IsNearlyEqual(DamageMult(Enemy), 1.f, .001f));
					Test->TestFalse(TEXT("No final slowed tag"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed));
					Test->TestFalse(TEXT("No final amplified tag"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_DamageAmplified));
					Test->AddInfo(FString::Printf(TEXT("StatusOwnership removalOrder=%d twoSources=1 reentry=1"), ReviewCase));
					return true;
				}
				if (!Test->TestTrue(TEXT("Real Burden overlaps Circle"), Source.ASC->TryActivateAbility(FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynBurdenOfSin"))))) return true;
				Test->TestTrue(TEXT("Burden and Circle compound exact slow"), FMath::IsNearlyEqual(MoveMult(Enemy), .425f, .001f));
				ReviewDuration = 5.f;
			}
			else if (ReviewCase == 3)
			{
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynPrimary"));
				ReviewEffectObserver = Enemy.ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda([this](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
				{
					if (Spec.GetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, false, 0.f) > 0.f) ++ReviewDamageEffects;
				});
				if (!Test->TestTrue(TEXT("First real shred projectile accepted"), Source.ASC->TryActivateAbility(ActiveHandle))) return true;
			}
			else if (ReviewCase == 4)
			{
				if (!Test->TestTrue(TEXT("Real Aura applies timed silence"), Source.ASC->TryActivateAbility(FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynAuraOfSilence"))))) return true;
				ReviewEffectHandles = ReviewEffectFamily(TEXT("KorrynSilence"));
				if (!Test->TestEqual(TEXT("One real silence effect"), ReviewEffectHandles.Num(), 1)) return true;
				ReviewDuration = 5.f;
			}
			else
			{
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_AlonaRadiance"));
				if (!ReviewRadianceParameters()) return true;
				BlessingHealObserver = Ally.ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda([this](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
				{
					const float Heal = Spec.GetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Heal, false, 0.f);
					if (Heal <= 0.f) return;
					++ReviewHealApplications;
					bReviewContextValid &= FMath::IsNearlyEqual(Heal, ReviewExpectedHeal, .01f) && Spec.GetEffectContext().GetOriginalInstigatorAbilitySystemComponent() == Source.ASC;
				});
				if (!Test->TestTrue(TEXT("Real Radiance full schedule accepted"), Source.ASC->TryActivateAbility(ActiveHandle))) return true;
				Test->TestEqual(TEXT("Exactly one initial heal application"), ReviewHealApplications, 1);
			}
			bPhaseInitialized = true;
			return false;
		}
		ReviewStep();
		if (ReviewCase == 3)
		{
			if (ReviewDamageEffects > ReviewObservedHits)
			{
				++ReviewObservedHits;
				if (!Test->TestEqual(TEXT("One hit per real primary activation"), ReviewDamageEffects, ReviewObservedHits)) return true;
				const auto Effects = ReviewEffectFamily(TEXT("KorrynArmorShred"));
				if (!Test->TestEqual(TEXT("Same source retains one shred effect"), Effects.Num(), 1)) return true;
				const auto* Effect = Enemy.ASC->GetActiveGameplayEffect(Effects[0]);
				Test->TestEqual(TEXT("Shred stacks cap at five"), Effect->Spec.GetStackCount(), FMath::Min(ReviewObservedHits, 5));
				Test->TestTrue(TEXT("Armor follows exact -2 increments and max -10"), FMath::IsNearlyEqual(Armor(Enemy), -2.f * FMath::Min(ReviewObservedHits, 5), .001f));
				Test->TestTrue(TEXT("Authored shred duration is exactly two seconds"), FMath::IsNearlyEqual(Effect->GetDuration(), 2.f, .001f));
				if (ReviewObservedHits == 5) ReviewRefreshAt = PhaseElapsed + 1.f;
				if (ReviewObservedHits == 6)
				{
					Test->TestTrue(TEXT("Sixth successful hit refreshes capped stack duration"), Effect->StartWorldTime > ReviewOldStart + .9f);
					ReviewDuration = float(Effect->StartWorldTime + Effect->GetDuration());
					ReviewEffectHandles = Effects;
				}
				else ReviewOldStart = Effect->StartWorldTime;
				if (ReviewObservedHits < 5 && !Test->TestTrue(TEXT("Next real shred primary activates"), Source.ASC->TryActivateAbility(ActiveHandle))) return true;
			}
			if (ReviewObservedHits == 5 && PhaseElapsed >= ReviewRefreshAt && !bSubCheckDone)
			{
				if (!Test->TestTrue(TEXT("Sixth real primary accepted at cap"), Source.ASC->TryActivateAbility(ActiveHandle))) return true;
				bSubCheckDone = true;
			}
			if (ReviewObservedHits < 6) return false;
			if (World->GetTimeSeconds() >= ReviewDuration - .1f && World->GetTimeSeconds() < ReviewDuration - .04f)
			{
				bReviewDeadlineSample = true;
				Test->TestTrue(TEXT("Refreshed full shred stack remains just before expiry"), FMath::IsNearlyEqual(Armor(Enemy), -10.f, .001f));
			}
			if (World->GetTimeSeconds() < ReviewDuration + .15f) return false;
			Test->TestTrue(TEXT("Observed shred pre-expiry boundary"), bReviewDeadlineSample);
			ReviewHandlesGone(ReviewEffectHandles);
			Test->TestTrue(TEXT("Whole shred stack expires to original armor"), FMath::IsNearlyZero(Armor(Enemy), .001f));
			Test->TestFalse(TEXT("Expired shred removes its tag"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_ArmorShred));
		}
		else
		{
			if (ReviewCase == 2 && PhaseElapsed >= 1.9f && PhaseElapsed < 1.96f)
				Test->TestTrue(TEXT("Burden survives just before two seconds"), FMath::IsNearlyEqual(MoveMult(Enemy), .425f, .001f));
			if (ReviewCase == 2 && PhaseElapsed >= 2.1f && !bSubCheckDone)
			{
				Test->TestTrue(TEXT("Burden expires while exact Circle handles remain"), FMath::IsNearlyEqual(MoveMult(Enemy), .85f, .001f) && ReviewEffectsFrom(ReviewActor.Get()) == ReviewEffectHandles);
				bSubCheckDone = true;
			}
			if (PhaseElapsed >= ReviewDuration - .1f && PhaseElapsed < ReviewDuration - .04f)
			{
				bReviewDeadlineSample = true;
				if (ReviewCase == 2) Test->TestTrue(TEXT("Circle remains just before five seconds"), ReviewActor.IsValid() && !ReviewActor->IsActorBeingDestroyed());
				else if (ReviewCase == 4) Test->TestTrue(TEXT("Silence remains just before five seconds"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced));
			}
			if (PhaseElapsed < ReviewDuration + .3f) return false;
			Test->TestTrue(TEXT("Observed duration boundary"), bReviewDeadlineSample);
			if (ReviewCase == 2)
			{
				Test->TestTrue(TEXT("Both original durations observed"), bSubCheckDone);
				ReviewHandlesGone(ReviewEffectHandles);
				Test->TestTrue(TEXT("Expiry restores independent slow and amplification baselines"), FMath::IsNearlyEqual(MoveMult(Enemy), 1.f, .001f) && FMath::IsNearlyEqual(DamageMult(Enemy), 1.f, .001f));
			}
			else if (ReviewCase == 4)
			{
				ReviewHandlesGone(ReviewEffectHandles);
				Test->TestFalse(TEXT("Silence expires without forced effect removal"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced));
			}
			else
			{
				Test->TestEqual(TEXT("Radiance applies exact configured heal count and no late tick"), ReviewHealApplications, ReviewExpectedHealCount);
				Test->TestTrue(TEXT("Radiance exact total and max-health clamp"), FMath::IsNearlyEqual(Health(Ally), FMath::Min(500.f, (ReviewCase == 5 ? 100.f : 490.f) + ReviewExpectedHealCount * ReviewExpectedHeal), .05f));
				Test->TestTrue(TEXT("Every scheduled heal has exact magnitude and source"), bReviewContextValid);
				const auto* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
				Test->TestFalse(TEXT("Radiance ends at original schedule"), Spec && Spec->IsActive());
			}
		}
		Test->AddInfo(FString::Printf(TEXT("StatusExpiry case=%d elapsed=%.3f scope=authority-production-effects"), ReviewCase, PhaseElapsed));
		return true;
	}

	bool UpdateReview()
	{
		if (FPlatformTime::Seconds() - Started > 30.)
		{
			Test->AddError(FString::Printf(TEXT("AllKit review timed out group=%d case=%d phase=%d"), ReviewGroup, ReviewCase, Phase));
			return true;
		}
		if (ReviewGroup == 1) return UpdateCircleLifecycle();
		if (ReviewGroup == 2) return UpdateProjectileLifecycle();
		if (ReviewGroup == 3) return UpdateTargeting();
		if (ReviewGroup == 4) return UpdateAsyncLifecycle();
		if (ReviewGroup == 5) return UpdateStatusOwnership();
		Test->AddError(TEXT("Unknown AllKit review group"));
		return true;
	}

	virtual bool Update() override
	{
		if (!World && !Setup()) return true;
		if (ReviewGroup != 0) return UpdateReview();
		if (bRestrictionsOnly) { CheckAuthoredRestrictions(); return true; }
		if (BlessingCase != INDEX_NONE) return UpdateBlessingLifecycle();
		if (FPlatformTime::Seconds() - Started > 30.0)
		{
			Test->AddError(FString::Printf(TEXT("AllKitCombatEffects timed out in phase %d"), Phase));
			return true;
		}

		constexpr float StepDelta = 0.025f;

		switch (Phase)
		{
		case 0: // Argus: PrimaryAttack + ForGlory (instant) + setup NoRetreat
		{
			if (!ConfigureHero(
				TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus"),
				TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_AbilitySet_Argus.DA_BW_AbilitySet_Argus")))
			{
				return true;
			}
			Test->TestEqual(TEXT("Argus AttackStrength"), Source.ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetAttackStrengthAttribute()), 50.f);
			Test->TestEqual(TEXT("Argus MaxHealth"), Source.ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()), 500.f);
			Test->TestEqual(TEXT("Argus Armor"), Armor(Source), 3.f);
			Test->TestEqual(TEXT("Argus WalkSpeed"), Source.Move->MaxWalkSpeed, 600.f);

			// 1. Argus Primary (10 + 0.4*50 = 30 raw; against 20 Armor -> 24 final damage, +2.4 leech)
			ResetArena(200.f, 500.f);
			Enemy.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 20.f);
			const FGameplayAbilitySpecHandle PrimaryH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_MeleePrimary"));
			Test->TestTrue(TEXT("Argus Primary activated"), Source.ASC->TryActivateAbility(PrimaryH));
			Test->TestTrue(TEXT("Argus Primary damage with 20 Armor"), FMath::IsNearlyEqual(Health(Enemy), 476.f, 0.05f));
			Test->TestTrue(TEXT("Argus Primary 10% life leech"), FMath::IsNearlyEqual(Health(Source), 202.4f, 0.05f));

			// Keep the positive hit above as the lane control. These are successful
			// immediate production swings, not activation-restriction tests.
			for (const bool bFriendlyInLane : {true, false})
			{
				ResetArena(200.f, 500.f, 200.f, FVector(-1500.f, 0.f, 200.f),
					bFriendlyInLane ? FVector(160.f, 0.f, 200.f) : FVector(-1500.f, 500.f, 200.f));
				Assister.Pawn->SetActorLocation(FVector(-1500.f, -500.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
				Test->TestEqual(TEXT("Friendly lane uses the source PlayerState team"), Ally.PS->GetTeamId(), Source.PS->GetTeamId());
				Test->TestTrue(TEXT("Enemy control uses a different PlayerState team"), Enemy.PS->GetTeamId() != Source.PS->GetTeamId());
				Test->TestTrue(TEXT("Production GameState agrees with friendly/enemy controls"), GS->AreOnSameTeam(Source.Pawn, Ally.Pawn) && !GS->AreOnSameTeam(Source.Pawn, Enemy.Pawn));
				if (bFriendlyInLane)
				{
					Test->TestEqual(TEXT("Friendly lane target uses the positive target's pawn class"), Ally.Pawn->GetClass(), Enemy.Pawn->GetClass());
					Test->TestTrue(TEXT("Friendly target is in the positive hit position"), Ally.Pawn->GetActorLocation().Equals(Source.Pawn->GetActorLocation() + FVector(160.f, 0.f, 0.f), .01f));
					Test->TestEqual(TEXT("Friendly target shares positive target Pawn collision response"), Ally.Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), Enemy.Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn));
				}
				const auto Before = PrimaryNegativeOutcomes();
				if (!Test->TestTrue(bFriendlyInLane ? TEXT("Argus friendly-lane swing activates") : TEXT("Argus empty-lane swing activates"), Source.ASC->TryActivateAbility(PrimaryH))) return true;
				// Assert before any reset/purge. Existing positive damage/credit totals
				// may be nonzero; neither negative case may add to or erase them.
				Test->TestTrue(bFriendlyInLane ? TEXT("Argus friendly-lane swing leaves combat outcomes unchanged") : TEXT("Argus empty-lane swing leaves combat outcomes unchanged"), Before == PrimaryNegativeOutcomes());
			}

			// 2. Argus ForGlory (2 + 0.4*50 = 22 raw; 0 Armor -> 22 damage, +2.2 non-primary leech, knockback)
			ResetArena(200.f, 500.f);
			const FGameplayAbilitySpecHandle ForGloryH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_ForGlory"));
			Test->TestTrue(TEXT("Argus ForGlory activated"), Source.ASC->TryActivateAbility(ForGloryH));
			Test->TestTrue(TEXT("Argus ForGlory damage (2 + 0.4*50 = 22)"), FMath::IsNearlyEqual(Health(Enemy), 478.f, 0.05f));
			Test->TestTrue(TEXT("Argus ForGlory non-primary life leech"), FMath::IsNearlyEqual(Health(Source), 202.2f, 0.05f));
			const FVector ForGloryVel = EffectiveVelocity(Enemy);
			Test->TestTrue(TEXT("Argus ForGlory knockback velocity"), ForGloryVel.X > 1000.f && ForGloryVel.Z > 200.f);

			// 3. Common Humanoid Slide (movement + State.Movement.Sliding)
			ResetArena(200.f, 500.f);
			Source.Move->Velocity = FVector(600.f, 0.f, 0.f);
			const FGameplayAbilitySpecHandle CommonSlideH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_Slide"));
			Test->TestTrue(TEXT("Common Slide activated"), Source.ASC->TryActivateAbility(CommonSlideH));
			Test->TestTrue(TEXT("Common Slide enters CMOVE_Slide"), Source.Move->IsSliding());
			Test->TestTrue(TEXT("Common Slide grants State.Movement.Sliding"), Source.ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sliding"))));
			Source.ASC->CancelAbilityHandle(CommonSlideH);
			Test->TestFalse(TEXT("Common Slide exited cleanly"), Source.Move->IsSliding());

			AdvancePhase();
			return false;
		}
		case 1: // Argus: ArgusSlide (dash + damage immunity)
		{
			if (!bPhaseInitialized)
			{
				ResetArena(200.f, 500.f);
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_ArgusSlide"));
				Test->TestTrue(TEXT("ArgusSlide activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				Test->TestTrue(TEXT("ArgusSlide grants DamageImmunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
				Test->TestTrue(TEXT("ArgusSlide launches forward"), EffectiveVelocity(Source).X > 1500.f);
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			const FGameplayAbilitySpec* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			if (Spec && Spec->IsActive() && PhaseElapsed < 1.0f) return false;
			Test->TestFalse(TEXT("ArgusSlide ended after dash duration"), Spec && Spec->IsActive());
			Test->TestFalse(TEXT("ArgusSlide cleared DamageImmunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
			AdvancePhase();
			return false;
		}
		case 2: // Argus: NoRetreat (charge trace 2 + 0.4*50 = 22 + knockback + leech)
		{
			if (!bPhaseInitialized)
			{
				ResetArena(200.f, 500.f);
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_NoRetreat"));
				Test->TestTrue(TEXT("Argus NoRetreat activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			const FGameplayAbilitySpec* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			if (Spec && Spec->IsActive() && PhaseElapsed < 1.0f) return false;
			Test->TestTrue(TEXT("Argus NoRetreat damage (2 + 0.4*50 = 22)"), FMath::IsNearlyEqual(Health(Enemy), 478.f, 0.05f));
			Test->TestTrue(TEXT("Argus NoRetreat non-primary life leech"), FMath::IsNearlyEqual(Health(Source), 202.2f, 0.05f));
			Test->TestTrue(TEXT("Argus NoRetreat knockback"), EffectiveVelocity(Enemy).X > 800.f);
			AdvancePhase();
			return false;
		}
		case 3: // Argus: Retribution (Uppercut 10+0.5*50=35 at 0.05s + DownSmash 20+0.5*50=45 at 0.35s = 80 total)
		{
			if (!bPhaseInitialized)
			{
				ResetArena(200.f, 500.f);
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_Retribution"));
				Test->TestTrue(TEXT("Argus Retribution activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				bSubCheckDone = false;
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			if (!bSubCheckDone && PhaseElapsed >= 0.10f)
			{
				bSubCheckDone = true;
				Test->TestTrue(TEXT("Argus Retribution uppercut damage (35)"), FMath::IsNearlyEqual(Health(Enemy), 465.f, 0.05f));
				Enemy.Pawn->SetActorLocation(FVector(160.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			const FGameplayAbilitySpec* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			if (Spec && Spec->IsActive() && PhaseElapsed < 1.0f) return false;
			Test->TestTrue(TEXT("Argus Retribution total damage (35 + 45 = 80)"), FMath::IsNearlyEqual(Health(Enemy), 420.f, 0.05f));
			Test->TestTrue(TEXT("Argus Retribution non-primary life leech (+8.0)"), FMath::IsNearlyEqual(Health(Source), 208.f, 0.05f));
			AdvancePhase();
			return false;
		}
		case 4: // Alona: Setup + SunBurst (30 + 0.4*40 = 46 + knockback + leech) + Primary Projectile (28 + 0.25*40 = 38)
		{
			if (!bPhaseInitialized)
			{
				if (!ConfigureHero(
					TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona"),
					TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_AbilitySet_Alona.DA_BW_AbilitySet_Alona")))
				{
					return true;
				}
				Test->TestEqual(TEXT("Alona AttackStrength"), Source.ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetAttackStrengthAttribute()), 40.f);
				Test->TestEqual(TEXT("Alona MaxHealth"), Source.ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()), 350.f);

				// Alona SunBurst (instant AOE: 30 + 0.4*40 = 46 damage + knockback + leech)
				ResetArena(200.f, 500.f);
				const FGameplayAbilitySpecHandle SunBurstH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_AlonaSunBurst"));
				Test->TestTrue(TEXT("Alona SunBurst activated"), Source.ASC->TryActivateAbility(SunBurstH));
				Test->TestTrue(TEXT("Alona SunBurst damage (30 + 0.4*40 = 46)"), FMath::IsNearlyEqual(Health(Enemy), 454.f, 0.05f));
				Test->TestTrue(TEXT("Alona SunBurst non-primary life leech (+4.6)"), FMath::IsNearlyEqual(Health(Source), 204.6f, 0.05f));
				Test->TestTrue(TEXT("Alona SunBurst knockback"), EffectiveVelocity(Enemy).X > 800.f);

				// Alona Primary Projectile (28 + 0.25*40 = 38)
				ResetArena(200.f, 500.f);
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_AlonaPrimary"));
				Test->TestTrue(TEXT("Alona Primary activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			if (Health(Enemy) >= 499.f && PhaseElapsed < 0.5f) return false;
			Test->TestTrue(TEXT("Alona Primary projectile damage (28 + 0.25*40 = 38)"), FMath::IsNearlyEqual(Health(Enemy), 462.f, 0.05f));
			Test->TestTrue(TEXT("Alona Primary life leech (+3.8)"), FMath::IsNearlyEqual(Health(Source), 203.8f, 0.05f));
			AdvancePhase();
			return false;
		}
		case 5: // Alona: SunsGrace (teleport + 0.25s DamageImmunity)
		{
			if (!bPhaseInitialized)
			{
				ResetArena(200.f, 500.f, 200.f, FVector(1500.f, 0.f, 200.f));
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_AlonaSunsGrace"));
				Test->TestTrue(TEXT("Alona SunsGrace activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				Test->TestTrue(TEXT("Alona SunsGrace teleports forward"), Source.Pawn->GetActorLocation().X > 400.f);
				Test->TestTrue(TEXT("Alona SunsGrace grants DamageImmunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			const FGameplayAbilitySpec* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			if (Spec && Spec->IsActive() && PhaseElapsed < 0.6f) return false;
			Test->TestFalse(TEXT("Alona SunsGrace cleared DamageImmunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
			AdvancePhase();
			return false;
		}
		case 6: // Alona: Radiance (single-target ally HoT scaling with UBwayHeroAttributeSet::AttackStrength)
		{
			if (!bPhaseInitialized)
			{
				ResetArena(200.f, 500.f, 100.f, FVector(0.f, -1500.f, 200.f), FVector(160.f, 0.f, 200.f));
				// Boost AttackStrength to 78 (78 / 52 = 1.5x multiplier -> 20.0 * 1.5 = 30.0 heal per 1.0s tick)
				Source.ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetAttackStrengthAttribute(), 78.f);
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_AlonaRadiance"));
				Test->TestTrue(TEXT("Alona Radiance activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				Test->TestTrue(TEXT("Alona Radiance initial tick heal (+30.0 HP)"), FMath::IsNearlyEqual(Health(Ally), 130.f, 0.1f));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			if (PhaseElapsed < 1.05f) return false;
			// After initial tick + 1st interval tick at 1.0s: 2 * 30.0 = 60.0 heal -> 160.0 HP
			Test->TestTrue(TEXT("Alona Radiance scaled ally heal (2 ticks = +60.0 HP)"), FMath::IsNearlyEqual(Health(Ally), 160.f, 0.1f));
			Source.ASC->CancelAbilityHandle(ActiveHandle);
			AdvancePhase();
			return false;
		}
		case 7: // Alona: Blessing of the Sun (ABwayAlonaBlessingZone: 70 initial burst + 35 HP/s HoT)
		{
			if (!bPhaseInitialized)
			{
				ResetArena(100.f, 500.f, 100.f, FVector(0.f, -1500.f, 200.f), FVector(160.f, 0.f, 200.f));
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_AlonaBlessing"));
				Test->TestTrue(TEXT("Alona Blessing activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				EnsureZonesAndProjectilesBegunPlay();
				Test->TestTrue(TEXT("Alona Blessing initial 70 heal on Ally"), FMath::IsNearlyEqual(Health(Ally), 170.f, 0.1f));
				Test->TestTrue(TEXT("Alona Blessing initial 70 heal on Alona"), FMath::IsNearlyEqual(Health(Source), 170.f, 0.1f));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			if (PhaseElapsed < 1.05f) return false;
			// After 1.0s (1 periodic tick of 35 HP/s): 170 + 35 = 205 HP
			Test->TestTrue(TEXT("Alona Blessing HoT 35 HP/s on Ally"), FMath::IsNearlyEqual(Health(Ally), 205.f, 0.1f));
			AdvancePhase();
			return false;
		}
		case 8: // Korryn: Setup + BurdenOfSin + CircleOfSpite + AuraOfSilence (with Relic fumble & silence block) + Primary Projectile
		{
			if (!bPhaseInitialized)
			{
				if (!ConfigureHero(
					TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver"),
					TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver.DA_BW_AbilitySet_Hexweaver")))
				{
					return true;
				}
				Test->TestEqual(TEXT("Korryn AttackStrength"), Source.ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetAttackStrengthAttribute()), 40.f);
				Test->TestEqual(TEXT("Korryn MaxHealth"), Source.ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()), 335.f);

				// 1. Korryn BurdenOfSin (33 + 0.3*40 = 45 damage + 50% slow State.Status.Slowed + leech)
				ResetArena(200.f, 500.f);
				const FGameplayAbilitySpecHandle BurdenH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynBurdenOfSin"));
				Test->TestTrue(TEXT("Korryn BurdenOfSin activated"), Source.ASC->TryActivateAbility(BurdenH));
				Test->TestTrue(TEXT("Korryn BurdenOfSin damage (33 + 0.3*40 = 45)"), FMath::IsNearlyEqual(Health(Enemy), 455.f, 0.05f));
				Test->TestTrue(TEXT("Korryn BurdenOfSin non-primary life leech (+4.5)"), FMath::IsNearlyEqual(Health(Source), 204.5f, 0.05f));
				Test->TestTrue(TEXT("Korryn BurdenOfSin applies State.Status.Slowed"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed));
				Test->TestTrue(TEXT("Korryn BurdenOfSin 50% MoveSpeedMultiplier"), FMath::IsNearlyEqual(MoveMult(Enemy), 0.5f, 0.01f));

				// 2. Korryn CircleOfSpite (ABwayKorrynSpiteZone: 15% slow + +35% IncomingDamageMultiplier = 1.35 via State.Status.DamageAmplified)
				ResetArena(200.f, 500.f);
				const FGameplayAbilitySpecHandle CircleH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynCircleOfSpite"));
				const float CircleBaselineMoveMultiplier = MoveMult(Enemy);
				Test->TestTrue(TEXT("Korryn CircleOfSpite activated"), Source.ASC->TryActivateAbility(CircleH));
				EnsureZonesAndProjectilesBegunPlay();
				Test->TestTrue(TEXT("Korryn CircleOfSpite applies State.Status.DamageAmplified"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_DamageAmplified));
				Test->TestTrue(TEXT("Korryn CircleOfSpite applies State.Status.Slowed"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed));
				Test->TestTrue(TEXT("Korryn CircleOfSpite applies the expected 15 percent slow"), FMath::IsNearlyEqual(MoveMult(Enemy), .85f, .01f));
				Test->TestTrue(TEXT("Korryn CircleOfSpite IncomingDamageMultiplier == 1.35"), FMath::IsNearlyEqual(DamageMult(Enemy), 1.35f, 0.01f));
				Test->TestTrue(TEXT("Korryn CircleOfSpite ResolveFinalDamage(100) == 135"), FMath::IsNearlyEqual(UBwayDamageLibrary::ResolveFinalDamage(100.f, Enemy.ASC), 135.f, 0.05f));
				Enemy.Pawn->SetActorLocation(FVector(-1200.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
				Test->TestFalse(TEXT("Leaving CircleOfSpite removes DamageAmplified"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_DamageAmplified));
				Test->TestTrue(TEXT("Leaving CircleOfSpite restores IncomingDamageMultiplier == 1.0"), FMath::IsNearlyEqual(DamageMult(Enemy), 1.0f, 0.01f));
				Test->TestFalse(TEXT("Leaving CircleOfSpite removes Slowed"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed));
				Test->TestTrue(TEXT("Leaving CircleOfSpite restores the prior movement multiplier"), FMath::IsNearlyEqual(MoveMult(Enemy), CircleBaselineMoveMultiplier, .01f));

				// 3. Korryn AuraOfSilence (13 + 0.5*40 = 33 damage + State.Status.Silenced blocking non-primary ability + Relic carrier fumble)
				ResetArena(200.f, 500.f);
				const ULyraAbilitySet* ArgusSet = LoadObject<ULyraAbilitySet>(nullptr,
					TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_AbilitySet_Argus.DA_BW_AbilitySet_Argus"));
				Enemy.ASC->ClearAllAbilities();
				GrantAbilitySet(Enemy.ASC, ArgusSet);
				const FGameplayAbilitySpecHandle EnemyNoRetreatH = FindAbilityHandle(Enemy.ASC, TEXT("BwayGameplayAbility_NoRetreat"));
				Test->TestTrue(TEXT("Enemy granted NoRetreat for silence test"), EnemyNoRetreatH.IsValid());
				Relic->OnPickedUp(Enemy.Pawn);
				Test->TestEqual(TEXT("Enemy carrying Relic before AuraOfSilence"), Relic->CurrentCarrier.Get(), Enemy.Pawn);
				const int32 FumblesBefore = Source.PS->GetForcedFumbles();
				const int32 VictimFumblesBefore = Enemy.PS->GetForcedFumbles();
				const FGameplayAbilitySpecHandle SilenceH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynAuraOfSilence"));
				Test->TestTrue(TEXT("Korryn AuraOfSilence activated"), Source.ASC->TryActivateAbility(SilenceH));
				Test->TestTrue(TEXT("Korryn AuraOfSilence damage (13 + 0.5*40 = 33)"), FMath::IsNearlyEqual(Health(Enemy), 467.f, 0.05f));
				Test->TestTrue(TEXT("Korryn AuraOfSilence non-primary life leech (+3.3)"), FMath::IsNearlyEqual(Health(Source), 203.3f, 0.05f));
				Test->TestTrue(TEXT("Korryn AuraOfSilence applies State.Status.Silenced"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced));
				Test->TestFalse(TEXT("State.Status.Silenced blocks non-primary ability activation"), Enemy.ASC->TryActivateAbility(EnemyNoRetreatH));
				Test->TestNull(TEXT("Korryn AuraOfSilence forces Relic carrier fumble"), Relic->CurrentCarrier.Get());
				Test->TestEqual(TEXT("Relic enters Dropped state on AuraOfSilence fumble"), Relic->GetCurrentState(), ERelicState::Dropped);
				Test->TestEqual(TEXT("AuraOfSilence credits the attacker"), Source.PS->GetForcedFumbles(), FumblesBefore + 1);
				Test->TestEqual(TEXT("AuraOfSilence does not credit the victim"), Enemy.PS->GetForcedFumbles(), VictimFumblesBefore);

				// 4. Korryn Primary Projectile (22 + 0.3*40 = 34 damage + -2 State.Status.ArmorShred)
				ResetArena(200.f, 500.f);
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynPrimary"));
				Test->TestTrue(TEXT("Korryn Primary activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			if (Health(Enemy) >= 499.f && PhaseElapsed < 0.5f) return false;
			Test->TestTrue(TEXT("Korryn Primary damage (22 + 0.3*40 = 34)"), FMath::IsNearlyEqual(Health(Enemy), 466.f, 0.05f));
			Test->TestTrue(TEXT("Korryn Primary life leech (+3.4)"), FMath::IsNearlyEqual(Health(Source), 203.4f, 0.05f));
			Test->TestTrue(TEXT("Korryn Primary applies State.Status.ArmorShred"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_ArmorShred));
			Test->TestTrue(TEXT("Korryn Primary shreds Armor by -2"), FMath::IsNearlyEqual(Armor(Enemy), -2.f, 0.01f));
			AdvancePhase();
			return false;
		}
		case 9: // Korryn: Flock (State.Status.Ethereal + untargetable flight + MaxFlySpeed restoration)
		{
			if (!bPhaseInitialized)
			{
				ResetArena(200.f, 500.f);
				Source.Move->MaxFlySpeed = 345.f;
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynFlock"));
				Test->TestTrue(TEXT("Korryn Flock activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				Test->TestTrue(TEXT("Korryn Flock grants State.Status.Ethereal"), Source.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal));
				Test->TestTrue(TEXT("Korryn Flock grants DamageImmunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
				Test->TestEqual(TEXT("Korryn Flock sets MOVE_Flying"), Source.Move->MovementMode.GetValue(), MOVE_Flying);
				Test->TestEqual(TEXT("Korryn Flock sets hover speed 600"), Source.Move->MaxFlySpeed, 600.f);
				Test->TestEqual(TEXT("Korryn Flock ignores pawn collision"), Source.Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			const FGameplayAbilitySpec* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			if (Spec && Spec->IsActive() && PhaseElapsed < 2.2f) return false;
			Test->TestFalse(TEXT("Korryn Flock ended naturally"), Spec && Spec->IsActive());
			Test->TestFalse(TEXT("Korryn Flock cleared Ethereal"), Source.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal));
			Test->TestEqual(TEXT("Korryn Flock restored MaxFlySpeed"), Source.Move->MaxFlySpeed, 345.f);
			Test->TestEqual(TEXT("Korryn Flock restored pawn collision"), Source.Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
			AdvancePhase();
			return false;
		}
		case 10: // Rawlins: Setup + PowerShot (30 + 0.65*60 = 69 + knockback + Relic carrier fumble) + Primary 2-shot burst (2 * 19 = 38)
		{
			if (!bPhaseInitialized)
			{
				if (!ConfigureHero(
					TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger"),
					TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_AbilitySet_Gunslinger.DA_BW_AbilitySet_Gunslinger")))
				{
					return true;
				}
				Test->TestEqual(TEXT("Rawlins AttackStrength"), Source.ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetAttackStrengthAttribute()), 60.f);
				Test->TestEqual(TEXT("Rawlins MaxHealth"), Source.ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()), 400.f);
				Test->TestEqual(TEXT("Rawlins WalkSpeed (10.5 -> 630)"), Source.Move->MaxWalkSpeed, 630.f);

				// 1. Rawlins PowerShot (30 + 0.65*60 = 69 damage + knockback + Relic carrier fumble + leech)
				ResetArena(200.f, 500.f);
				Relic->OnPickedUp(Enemy.Pawn);
				Test->TestEqual(TEXT("Enemy carrying Relic before PowerShot"), Relic->CurrentCarrier.Get(), Enemy.Pawn);
				const int32 FumblesBefore = Source.PS->GetForcedFumbles();
				const int32 VictimFumblesBefore = Enemy.PS->GetForcedFumbles();
				const FGameplayAbilitySpecHandle PowerShotH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_RawlinsPowerShot"));
				Test->TestTrue(TEXT("Rawlins PowerShot activated"), Source.ASC->TryActivateAbility(PowerShotH));
				Test->TestTrue(TEXT("Rawlins PowerShot damage (30 + 0.65*60 = 69)"), FMath::IsNearlyEqual(Health(Enemy), 431.f, 0.05f));
				Test->TestTrue(TEXT("Rawlins PowerShot non-primary life leech (+6.9)"), FMath::IsNearlyEqual(Health(Source), 206.9f, 0.05f));
				Test->TestTrue(TEXT("Rawlins PowerShot knockback"), EffectiveVelocity(Enemy).X > 1000.f);
				Test->TestNull(TEXT("Rawlins PowerShot forces Relic carrier fumble"), Relic->CurrentCarrier.Get());
				Test->TestEqual(TEXT("Relic enters Dropped state on PowerShot fumble"), Relic->GetCurrentState(), ERelicState::Dropped);
				Test->TestEqual(TEXT("PowerShot credits the attacker"), Source.PS->GetForcedFumbles(), FumblesBefore + 1);
				Test->TestEqual(TEXT("PowerShot does not credit the victim"), Enemy.PS->GetForcedFumbles(), VictimFumblesBefore);

				// 2. Rawlins Primary 2-shot burst (1 + 0.3*60 = 19 per shot, 38 total across 2 shots)
				ResetArena(200.f, 500.f);
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_RawlinsPrimary"));
				Test->TestTrue(TEXT("Rawlins Primary activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			if (PhaseElapsed < 0.30f) return false;
			Test->TestTrue(TEXT("Rawlins Primary 2-shot burst total damage (2 * 19 = 38)"), FMath::IsNearlyEqual(Health(Enemy), 462.f, 0.05f));
			Test->TestTrue(TEXT("Rawlins Primary 2-shot life leech (+3.8)"), FMath::IsNearlyEqual(Health(Source), 203.8f, 0.05f));
			AdvancePhase();
			return false;
		}
		case 11: // Rawlins: DoubleDown (roll + DamageImmunity)
		{
			if (!bPhaseInitialized)
			{
				ResetArena(200.f, 500.f, 200.f, FVector(1500.f, 0.f, 200.f));
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_RawlinsDoubleDown"));
				Test->TestTrue(TEXT("Rawlins DoubleDown activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				Test->TestTrue(TEXT("Rawlins DoubleDown grants DamageImmunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
				Test->TestTrue(TEXT("Rawlins DoubleDown roll velocity"), EffectiveVelocity(Source).X > 1500.f);
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			const FGameplayAbilitySpec* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			if (Spec && Spec->IsActive() && PhaseElapsed < 0.8f) return false;
			Test->TestFalse(TEXT("Rawlins DoubleDown ended"), Spec && Spec->IsActive());
			Test->TestFalse(TEXT("Rawlins DoubleDown cleared DamageImmunity"), Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity));
			AdvancePhase();
			return false;
		}
		case 12: // Rawlins: SlideShot (10 + 0.6*60 = 46 damage + upward launch + non-primary Kill/Assist Combat Credit)
		{
			if (!bPhaseInitialized)
			{
				ConfigureRoundActive();
				ResetArena(200.f, 60.f);
				// Assister deals 20 damage first -> Enemy at 40 HP
				Test->TestTrue(TEXT("Assister pre-damage applied"),
					UBwayDamageLibrary::ApplyDamageFromSource(Assister.ASC, Enemy.Pawn, 20.f, Assister.Pawn, Assister.Pawn));
				Test->TestTrue(TEXT("Enemy at 40 HP before lethal SlideShot"), FMath::IsNearlyEqual(Health(Enemy), 40.f, 0.05f));
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_RawlinsSlideShot"));
				Test->TestTrue(TEXT("Rawlins SlideShot activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			if (Health(Enemy) > 0.f && PhaseElapsed < 0.5f) return false;
			Test->TestEqual(TEXT("Rawlins SlideShot lethal damage (46 >= 40)"), Health(Enemy), 0.f);
			Test->TestTrue(TEXT("Rawlins SlideShot overkill-clamped life leech (+4.0)"), FMath::IsNearlyEqual(Health(Source), 204.f, 0.05f));
			Test->TestTrue(TEXT("Rawlins SlideShot launches enemy upward (650)"), EffectiveVelocity(Enemy).Z >= 500.f);
			ULyraHealthComponent* EnemyHealthComp = ULyraHealthComponent::FindHealthComponent(Enemy.Pawn);
			if (EnemyHealthComp && !EnemyHealthComp->IsDeadOrDying())
			{
				Enemy.PC->PlayerState = Enemy.PS;
				EnemyHealthComp->StartDeath();
				Enemy.PC->PlayerState = nullptr;
			}
			Test->TestEqual(TEXT("Non-primary lethal ability credits Killer"), Source.PS->GetKills(), 1);
			Test->TestEqual(TEXT("Non-primary lethal ability credits Assister"), Assister.PS->GetAssists(), 1);
			Test->TestEqual(TEXT("Non-primary lethal ability records Victim death"), Enemy.PS->GetDeaths(), 1);
			Test->TestEqual(TEXT("ConsumedAssisterCountForAudit == 1"), Enemy.Pawn->GetConsumedAssisterCountForAudit(), 1);
			Source.ASC->CancelAbilityHandle(ActiveHandle);
			AdvancePhase();
			return false;
		}
		case 13: // Rawlins: BlazingBarrage (4 + 0.3*60 = 22 per shot, airborne juggle 350, post-heal damage interruption)
		{
			if (!bPhaseInitialized)
			{
				if (!RespawnEnemyPawn())
				{
					Test->AddError(TEXT("Failed to respawn enemy pawn for BlazingBarrage phase"));
					return true;
				}
				ConfigureRoundActive();
				ResetArena(300.f, 500.f);
				// Put Enemy into MOVE_Falling so BlazingBarrage applies its 350 upward juggle impulse on hit
				Enemy.Move->SetMovementMode(MOVE_Falling);
				Enemy.Move->Velocity = FVector::ZeroVector;
				ActiveHandle = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_RawlinsBlazingBarrage"));
				Test->TestTrue(TEXT("Rawlins BlazingBarrage activated"), Source.ASC->TryActivateAbility(ActiveHandle));
				bPhaseInitialized = true;
				return false;
			}
			TickWorldStep(StepDelta);
			if (Health(Enemy) >= 499.f && PhaseElapsed < 0.3f) return false;
			const FGameplayAbilitySpec* Spec = Source.ASC->FindAbilitySpecFromHandle(ActiveHandle);
			Test->TestTrue(TEXT("Rawlins BlazingBarrage shot 1 damage (4 + 0.3*60 = 22)"), FMath::IsNearlyEqual(Health(Enemy), 478.f, 0.05f));
			Test->TestTrue(TEXT("Rawlins BlazingBarrage life leech heal (+2.2) does NOT interrupt channel"), FMath::IsNearlyEqual(Health(Source), 302.2f, 0.05f));
			Test->TestTrue(TEXT("Rawlins BlazingBarrage remains active after healing"), Spec && Spec->IsActive());
			Test->TestTrue(TEXT("Rawlins BlazingBarrage juggles airborne enemy upward (350)"), EffectiveVelocity(Enemy).Z >= 300.f);
			// Now apply post-heal enemy damage to Rawlins and verify immediate channel interruption
			Test->TestTrue(TEXT("Post-heal damage applied to Rawlins"),
				UBwayDamageLibrary::ApplyDamageFromSource(Enemy.ASC, Source.Pawn, 10.f, Enemy.Pawn, Enemy.Pawn));
			Test->TestFalse(TEXT("Rawlins BlazingBarrage immediately interrupted by post-heal damage"), Spec && Spec->IsActive());
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCombatEffects: PASS all 20 abilities (4 primaries + 16 kit abilities), life leech, combat credit, and relic fumbles verified."));
			return true;
		}
		default:
			return true;
		}
	}

private:
	FAutomationTestBase* Test = nullptr;
	TStrongObjectPtr<UGameInstance> Instance;
	UWorld* World = nullptr;
	ABwayGameState* GS = nullptr;
	ARelicActor* Relic = nullptr;
	const ULyraAbilitySet* HumanoidSet = nullptr;
	const UBwayHeroDataAsset* CurrentHeroData = nullptr;
	FParticipant Source, Ally, Enemy, Assister;
	FGameplayAbilitySpecHandle ActiveHandle;
	double Started = 0.0;
	float PhaseElapsed = 0.f;
	int32 Phase = 0;
	bool bPhaseInitialized = false;
	bool bSubCheckDone = false;
	bool bRestrictionsOnly = false;
	int32 BlessingCase = INDEX_NONE;
	TWeakObjectPtr<AActor> BlessingZone;
	FDelegateHandle BlessingHealObserver;
	int32 BlessingHealApplications = 0;
	float BlessingDuration = 0.f;
	double BlessingLastTick = 0.;
	bool bBlessingAttributionValid = true;
	bool bBlessingPostDeathChecked = false;
	ULyraPawnData* RestrictionPawnData = nullptr;
	int32 ReviewGroup = 0;
	int32 ReviewCase = 0;
	TWeakObjectPtr<AActor> ReviewActor;
	TArray<FActiveGameplayEffectHandle> ReviewEffectHandles;
	FDelegateHandle ReviewEffectObserver;
	float ReviewDuration = 0.f;
	float ReviewHitHealth = 0.f;
	int32 ReviewDamageEffects = 0;
	bool bReviewContextValid = true;
	bool bReviewDeadlineSample = false;
	FGameplayAbilitySpecHandle ReviewStrikeHandle;
	TArray<FString> ReviewOutcomes;
	float ReviewAllyHealth = 0.f;
	float ReviewSourceHealth = 0.f;
	int32 ReviewContributionCount = 0;
	int32 ReviewHealApplications = 0;
	int32 ReviewObservedHits = 0;
	float ReviewRefreshAt = 0.f;
	float ReviewOldStart = 0.f;
	float ReviewExpectedHeal = 0.f;
	int32 ReviewExpectedHealCount = 0;
};
} // namespace BwayAllKitCombatEffectsTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayAllKitCombatEffectsTest, "Breakaway.Abilities.AllKitCombatEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayAllKitCombatEffectsTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayAllKitActivationRestrictionsTest, "Breakaway.Abilities.AllKitActivationRestrictions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayAllKitActivationRestrictionsTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayBlessingLifecycleTest, "Breakaway.Abilities.Alona.BlessingLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayBlessingLifecycleTest::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 5; ++Case)
	{
		ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this, false, Case));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCircleLifecycleTest, "Breakaway.Abilities.Korryn.CircleLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayCircleLifecycleTest::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 6; ++Case) ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this, false, INDEX_NONE, 1, Case));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayProjectileLifecycleTest, "Breakaway.Abilities.ProjectileLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayProjectileLifecycleTest::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 21; ++Case) ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this, false, INDEX_NONE, 2, Case));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayAllKitTargetingTest, "Breakaway.Abilities.AllKitTargetingAndImmunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayAllKitTargetingTest::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 80; ++Case) ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this, false, INDEX_NONE, 3, Case));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayAllKitAsyncLifecycleTest, "Breakaway.Abilities.AllKitAsyncLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayAllKitAsyncLifecycleTest::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 30; ++Case) ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this, false, INDEX_NONE, 4, Case));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayStatusOwnershipExpiryTest, "Breakaway.Abilities.Korryn.StatusOwnershipAndExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayStatusOwnershipExpiryTest::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 7; ++Case) ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this, false, INDEX_NONE, 5, Case));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#if !UE_BUILD_SHIPPING

namespace BwayAllKitCombatRuntime
{
const TCHAR* HeroNames[] = { TEXT("Argus"), TEXT("Rawlins"), TEXT("Alona"), TEXT("Korryn") };
const TCHAR* HeroDataPaths[] = {
	TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus"),
	TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger"),
	TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona"),
	TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver")
};

struct FHeldActor
{
	TWeakObjectPtr<ACharacter> Pawn;
	FTransform Transform;
	FVector Velocity = FVector::ZeroVector;
	TWeakObjectPtr<AController> Controller;
	FRotator ControlRotation = FRotator::ZeroRotator;
	EMovementMode Mode = MOVE_Walking;
	uint8 CustomMode = 0;
	TWeakObjectPtr<UBrainComponent> Brain;
	bool bBrainRunning = false;
};

struct FPlayerRestore
{
	TWeakObjectPtr<ABwayPlayerState> PS;
	float Health = 0.f;
	float Armor = 0.f;
	float LifeLeech = 0.f;
};

struct FRuntimeProbe
{
	bool bClient = false;
	bool bCheckAllPieWorlds = false;
	bool bSelectedStaging = false;
	bool bLoggedClientConnected = false;
	int32 Step = 0;
	double Started = FPlatformTime::Seconds();
	double StepTime = 0.0;
	double StableSince = 0.0;
	FString LastSummary;
	TArray<FHeldActor> Held;
	TArray<FPlayerRestore> RestorePlayers;
	TMap<TWeakObjectPtr<ABwayBaseZone>, float> RestoreBases;
};

TSharedPtr<FRuntimeProbe> GActiveProbe;

UWorld* FindPrimaryWorld(bool bClient)
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* W = Context.World();
		if (!W || !W->IsGameWorld()) continue;
		if (bClient && W->GetNetMode() == NM_Client) return W;
		if (!bClient && W->GetNetMode() != NM_Client) return W;
	}
	return nullptr;
}

FGameplayAbilitySpecHandle FindHandle(ULyraAbilitySystemComponent* ASC, const TCHAR* ClassName)
{
	if (!ASC) return FGameplayAbilitySpecHandle();
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		for (const UClass* Cls = Spec.Ability ? Spec.Ability->GetClass() : nullptr; Cls; Cls = Cls->GetSuperClass())
		{
			if (Cls->GetName() == ClassName) return Spec.Handle;
		}
	}
	return FGameplayAbilitySpecHandle();
}

bool ActivateAuthoritative(ULyraAbilitySystemComponent* ASC, const TCHAR* ClassName)
{
	if (!ASC || !ASC->AbilityActorInfo.IsValid()) return false;
	const FGameplayAbilitySpecHandle Handle = FindHandle(ASC, ClassName);
	if (!Handle.IsValid()) return false;
	APawn* Avatar = Cast<APawn>(ASC->GetAvatarActor());
	TObjectPtr<AController> SavedPawnController = Avatar ? Avatar->Controller : nullptr;
	TWeakObjectPtr<APlayerController> SavedPC = ASC->AbilityActorInfo->PlayerController;
	if (Avatar) Avatar->Controller = nullptr;
	ASC->AbilityActorInfo->PlayerController = nullptr;
	const bool bActivated = ASC->TryActivateAbility(Handle);
	ASC->AbilityActorInfo->PlayerController = SavedPC;
	if (Avatar) Avatar->Controller = SavedPawnController;
	return bActivated;
}

void CleanupProbe(const TSharedPtr<FRuntimeProbe>& Probe)
{
	if (!Probe || Probe->bClient) return;
	for (const FHeldActor& H : Probe->Held)
	{
		if (ACharacter* P = H.Pawn.Get())
		{
			P->SetActorTransform(H.Transform, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* M = P->GetCharacterMovement())
			{
				M->SetMovementMode(H.Mode, H.CustomMode);
				M->Velocity = H.Velocity;
			}
			if (AController* Controller = H.Controller.Get()) Controller->SetControlRotation(H.ControlRotation);
		}
		if (H.bBrainRunning)
		{
			if (UBrainComponent* B = H.Brain.Get()) B->RestartLogic();
		}
	}
	for (const FPlayerRestore& R : Probe->RestorePlayers)
	{
		if (ABwayPlayerState* PS = R.PS.Get())
		{
			if (ULyraAbilitySystemComponent* ASC = PS->GetLyraAbilitySystemComponent())
			{
				ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), R.Health);
				ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), R.Armor);
				ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), R.LifeLeech);
			}
		}
	}
	for (const auto& Pair : Probe->RestoreBases)
	{
		if (ABwayBaseZone* Base = Pair.Key.Get()) Base->HealingPerSecond = Pair.Value;
	}
}

bool FinishProbe(const TSharedPtr<FRuntimeProbe>& Probe, const TCHAR* Error = nullptr)
{
	if (Error)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayAllKitCombatTest: FAIL role=%s step=%d reason=%s elapsed=%.2f summary=[%s]"),
			Probe->bClient ? TEXT("Client") : (Probe->bCheckAllPieWorlds ? TEXT("PIE") : TEXT("Host")),
			Probe->Step, Error, FPlatformTime::Seconds() - Probe->Started, *Probe->LastSummary);
	}
	CleanupProbe(Probe);
	GActiveProbe.Reset();
	if (!Error && Probe->bClient && FParse::Param(FCommandLine::Get(), TEXT("AllKitAutoExit")))
	{
		FPlatformMisc::RequestExit(false);
	}
	return false;
}

bool GatherSortedHumanPlayers(ABwayGameState* GS, TArray<ABwayPlayerState*>& OutPlayers)
{
	OutPlayers.Reset();
	if (!GS) return false;
	for (APlayerState* PS : GS->PlayerArray)
	{
		ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
		ULyraAbilitySystemComponent* ASC = BwayPS ? BwayPS->GetLyraAbilitySystemComponent() : nullptr;
		ABwayCharacterWithAbilities* Pawn = BwayPS ? Cast<ABwayCharacterWithAbilities>(BwayPS->GetPawn()) : nullptr;
		if (!BwayPS || BwayPS->IsABot() || !Pawn || !ASC || !ASC->GetSet<ULyraHealthSet>() || !ASC->GetSet<UBwayHeroAttributeSet>()
			|| GS->GetPlayerTeam(BwayPS) == INDEX_NONE)
		{
			continue;
		}
		OutPlayers.Add(BwayPS);
	}
	OutPlayers.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return OutPlayers.Num() == 4;
}

bool VerifyWorldConvergence(UWorld* World, FString& OutSummary)
{
	ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	TArray<ABwayPlayerState*> Players;
	if (!GatherSortedHumanPlayers(GS, Players)) return false;

	// Players: [0]=Argus (Team 0), [1]=Rawlins (Team 1), [2]=Alona (Team 0), [3]=Korryn (Team 1)
	ULyraAbilitySystemComponent* ArgusASC = Players[0]->GetLyraAbilitySystemComponent();
	ULyraAbilitySystemComponent* RawlinsASC = Players[1]->GetLyraAbilitySystemComponent();
	ULyraAbilitySystemComponent* AlonaASC = Players[2]->GetLyraAbilitySystemComponent();
	ULyraAbilitySystemComponent* KorrynASC = Players[3]->GetLyraAbilitySystemComponent();

	const float ArgusHP = ArgusASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
	const float ArgusArmor = ArgusASC->GetNumericAttribute(UBwayHeroAttributeSet::GetArmorAttribute());
	const float RawlinsHP = RawlinsASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
	const float AlonaHP = AlonaASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
	const float KorrynHP = KorrynASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());

	const bool bArgusShredded = ArgusASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_ArmorShred)
		&& ArgusArmor <= 1.1f; // Argus base 3 Armor - 2 Korryn shred per stack <= 1
	const bool bArgusSlowed = ArgusASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed);
	const bool bArgusAmped = ArgusASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_DamageAmplified);
	const bool bArgusSilenced = ArgusASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced);
	const bool bDamageConverged = ArgusHP < 450.f && RawlinsHP < 380.f && KorrynHP < 320.f && AlonaHP > 220.f;
	const int32 TotalFumbles = Players[0]->GetForcedFumbles() + Players[1]->GetForcedFumbles()
		+ Players[2]->GetForcedFumbles() + Players[3]->GetForcedFumbles();

	OutSummary = FString::Printf(TEXT("argusHP=%.1f argusArmor=%.1f rawlinsHP=%.1f alonaHP=%.1f korrynHP=%.1f shred=%d slow=%d amp=%d silence=%d fumbles=%d"),
		ArgusHP, ArgusArmor, RawlinsHP, AlonaHP, KorrynHP,
		bArgusShredded ? 1 : 0, bArgusSlowed ? 1 : 0, bArgusAmped ? 1 : 0, bArgusSilenced ? 1 : 0, TotalFumbles);

	return bArgusShredded && bArgusSlowed && bArgusAmped && bArgusSilenced && bDamageConverged && TotalFumbles >= 1;
}

bool TickRuntimeProbe(const TSharedPtr<FRuntimeProbe>& Probe)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - Probe->Started > 120.0) return FinishProbe(Probe, TEXT("timeout"));

	UWorld* World = FindPrimaryWorld(Probe->bClient);
	if (!World) return true;
	ABwayGameState* GS = World->GetGameState<ABwayGameState>();
	if (!GS) return true;

	if (Probe->bClient && !Probe->bLoggedClientConnected)
	{
		if (const APlayerController* LocalPC = World->GetFirstPlayerController())
		{
			if (LocalPC->PlayerState)
			{
				Probe->bLoggedClientConnected = true;
				UE_LOG(LogTemp, Display, TEXT("BwayAllKitCombatTest: ClientConnected player=%d"), LocalPC->PlayerState->GetPlayerId());
			}
		}
	}

	// Handle HeroSelect_Staging on Host
	if (!Probe->bClient && World->GetMapName().Contains(TEXT("HeroSelect_Staging")))
	{
		if (!Probe->bSelectedStaging && GS->PlayerArray.Num() == 4 && GS->HeroSelectionManager && GS->HeroSelectionManager->IsSelectionActive())
		{
			TArray<ABwayPlayerState*> Roster;
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS); BwayPS && !BwayPS->IsABot()) Roster.Add(BwayPS);
			}
			if (Roster.Num() == 4)
			{
				Roster.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
				Probe->bSelectedStaging = true;
				for (int32 I = 0; I < 4; ++I)
				{
					const FPrimaryAssetId HeroId = UBwayHeroRegistry::ResolveHeroIdByName(HeroNames[I]);
					if (!GS->HeroSelectionManager->AssignHeroToPlayer(Roster[I], HeroId, true))
					{
						return FinishProbe(Probe, TEXT("staging-hero-assign-failed"));
					}
					UE_LOG(LogTemp, Display, TEXT("BwayAllKitCombatTest: Locked hero=%s player=%d"), HeroNames[I], Roster[I]->GetPlayerId());
				}
			}
		}
		return true;
	}

	if (!World->GetMapName().Contains(TEXT("L_BW_Dorado"))) return true;
	UBwayRoundManagementComponent* Round = GS->GetRoundManagement();
	if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return true;
	}

	TArray<ABwayPlayerState*> Players;
	if (!GatherSortedHumanPlayers(GS, Players)) return true;

	if (Probe->bClient)
	{
		FString Summary;
		const bool bOk = VerifyWorldConvergence(World, Summary);
		Probe->LastSummary = Summary;
		if (!bOk)
		{
			Probe->StableSince = 0.0;
			return true;
		}
		if (Probe->StableSince == 0.0) Probe->StableSince = Now;
		if (Now - Probe->StableSince < 0.5) return true;
		const APlayerController* LocalPC = World->GetFirstPlayerController();
		const int32 ObserverId = (LocalPC && LocalPC->PlayerState) ? LocalPC->PlayerState->GetPlayerId() : INDEX_NONE;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitCombatTest: Client PASS observer=%d %s"), ObserverId, *Summary);
		return FinishProbe(Probe, nullptr);
	}

	auto Place = [](ABwayCharacterWithAbilities* P, const FVector& Loc, const FRotator& Rot)
	{
		if (!P) return;
		P->SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* C = P->GetController()) C->SetControlRotation(Rot);
		if (UCharacterMovementComponent* M = P->GetCharacterMovement())
		{
			M->SetMovementMode(MOVE_Flying);
			M->StopMovementImmediately();
		}
	};

	const FVector Origin(0.f, 0.f, 4000.f);
	ABwayCharacterWithAbilities* ArgusPawn = Cast<ABwayCharacterWithAbilities>(Players[0]->GetPawn());
	ABwayCharacterWithAbilities* RawlinsPawn = Cast<ABwayCharacterWithAbilities>(Players[1]->GetPawn());
	ABwayCharacterWithAbilities* AlonaPawn = Cast<ABwayCharacterWithAbilities>(Players[2]->GetPawn());
	ABwayCharacterWithAbilities* KorrynPawn = Cast<ABwayCharacterWithAbilities>(Players[3]->GetPawn());

	// Authority Steps
	if (Probe->Step == 0)
	{
		for (TActorIterator<ABwayBaseZone> It(World); It; ++It)
		{
			Probe->RestoreBases.Add(*It, It->HealingPerSecond);
			It->HealingPerSecond = 0.f;
		}
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* C = *It;
			if (!C) continue;
			FHeldActor H;
			H.Pawn = C;
			H.Transform = C->GetActorTransform();
			H.Controller = C->GetController();
			if (H.Controller.IsValid()) H.ControlRotation = H.Controller->GetControlRotation();
			if (UCharacterMovementComponent* M = C->GetCharacterMovement())
			{
				H.Mode = M->MovementMode;
				H.CustomMode = M->CustomMovementMode;
				H.Velocity = M->Velocity;
				M->StopMovementImmediately();
			}
			if (AAIController* AI = Cast<AAIController>(C->GetController()))
			{
				if (UBrainComponent* B = AI->GetBrainComponent())
				{
					H.Brain = B;
					H.bBrainRunning = B->IsRunning();
					B->StopLogic(TEXT("AllKitCombatTest"));
				}
			}
			Probe->Held.Add(H);
			// Isolate bots far away
			if (!Players.Contains(C->GetPlayerState<ABwayPlayerState>()))
			{
				C->SetActorLocation(FVector(0.f, -8000.f, 4000.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		// Ensure teams: Players[0] (Argus, Team 0), Players[1] (Rawlins, Team 1), Players[2] (Alona, Team 0), Players[3] (Korryn, Team 1)
		const int32 DesiredTeams[4] = { 0, 1, 0, 1 };
		for (int32 I = 0; I < 4; ++I)
		{
			if (GS->GetPlayerTeam(Players[I]) != DesiredTeams[I])
			{
				GS->RemovePlayerFromTeam(Players[I]);
				GS->AddPlayerToTeam(Players[I], DesiredTeams[I]);
				Players[I]->SetGenericTeamId(FGenericTeamId(DesiredTeams[I] + 1));
				if (IGenericTeamAgentInterface* TeamPawn = Cast<IGenericTeamAgentInterface>(Players[I]->GetPawn()))
				{
					TeamPawn->SetGenericTeamId(FGenericTeamId(DesiredTeams[I] + 1));
				}
			}
			const UBwayHeroDataAsset* HeroData = LoadObject<UBwayHeroDataAsset>(nullptr, HeroDataPaths[I]);
			ABwayCharacterWithAbilities* Pawn = Cast<ABwayCharacterWithAbilities>(Players[I]->GetPawn());
			if (HeroData && Pawn)
			{
				if (Pawn->HeroDataAsset != HeroData)
				{
					Pawn->InitializeHeroData(HeroData);
				}
				else
				{
					UBwayHeroStatsLibrary::ApplyHeroStatsFromDataAsset(Pawn, HeroData);
				}
			}
			ULyraAbilitySystemComponent* ASC = Players[I]->GetLyraAbilitySystemComponent();
			Probe->RestorePlayers.Add({ Players[I], ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()),
				ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetArmorAttribute()),
				ASC->GetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute()) });
			ASC->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), 0.10f);
		}

		Place(ArgusPawn, Origin, FRotator::ZeroRotator);
		Place(RawlinsPawn, Origin + FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
		Place(AlonaPawn, Origin + FVector(50.f, 0.f, 0.f), FRotator::ZeroRotator);
		Place(KorrynPawn, Origin + FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));

		// Set Alona to 200 HP so Blessing heals her above 220 HP at an isolated location
		ULyraAbilitySystemComponent* ArgusASC = Players[0]->GetLyraAbilitySystemComponent();
		ULyraAbilitySystemComponent* RawlinsASC = Players[1]->GetLyraAbilitySystemComponent();
		ULyraAbilitySystemComponent* AlonaASC = Players[2]->GetLyraAbilitySystemComponent();
		ULyraAbilitySystemComponent* KorrynASC = Players[3]->GetLyraAbilitySystemComponent();
		AlonaASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 200.f);

		// 1. Argus Primary + ForGlory on Rawlins & Korryn (before Argus picks up Relic)
		ActivateAuthoritative(ArgusASC, TEXT("BwayGameplayAbility_MeleePrimary"));
		ActivateAuthoritative(ArgusASC, TEXT("BwayGameplayAbility_ForGlory"));
		Place(RawlinsPawn, Origin + FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
		Place(KorrynPawn, Origin + FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));

		// 2. Alona SunBurst (at +50 X to damage Rawlins & Korryn) + Blessing (isolated at Y=-2000 so it never overlaps Argus)
		Place(AlonaPawn, Origin + FVector(50.f, 0.f, 0.f), FRotator::ZeroRotator);
		ActivateAuthoritative(AlonaASC, TEXT("BwayGameplayAbility_AlonaSunBurst"));
		Place(AlonaPawn, Origin + FVector(0.f, -2000.f, 0.f), FRotator::ZeroRotator);
		ActivateAuthoritative(AlonaASC, TEXT("BwayGameplayAbility_AlonaBlessing"));
		Place(AlonaPawn, Origin + FVector(400.f, -2000.f, 0.f), FRotator::ZeroRotator);
		Place(RawlinsPawn, Origin + FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
		Place(KorrynPawn, Origin + FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));

		// 3. Give Relic to Argus, then Rawlins PowerShot on Argus (forces Relic fumble + damages Argus)
		UBwayRelicManagerComponent* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>();
		ARelicActor* Relic = RelicMgr ? RelicMgr->GetRelicActor() : nullptr;
		if (!Relic)
		{
			for (TActorIterator<ARelicActor> It(World); It; ++It) { Relic = *It; break; }
			if (Relic && RelicMgr) RelicMgr->SetActiveRelic(Relic);
		}
		if (Relic)
		{
			if (Relic->CurrentCarrier) Relic->OnDropped();
			Relic->OnPickedUp(ArgusPawn);
		}
		ActivateAuthoritative(RawlinsASC, TEXT("BwayGameplayAbility_RawlinsPowerShot"));
		Place(ArgusPawn, Origin, FRotator::ZeroRotator);
		Place(RawlinsPawn, Origin + FVector(150.f, 300.f, 0.f), FRotator(0.f, 180.f, 0.f));

		// 4. Korryn CircleOfSpite (at +400 X facing -X -> zone spawns at Origin) + BurdenOfSin + AuraOfSilence + Primary
		Place(KorrynPawn, Origin + FVector(400.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
		ActivateAuthoritative(KorrynASC, TEXT("BwayGameplayAbility_KorrynCircleOfSpite"));
		Place(KorrynPawn, Origin + FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
		ActivateAuthoritative(KorrynASC, TEXT("BwayGameplayAbility_KorrynBurdenOfSin"));
		ActivateAuthoritative(KorrynASC, TEXT("BwayGameplayAbility_KorrynAuraOfSilence"));
		ActivateAuthoritative(KorrynASC, TEXT("BwayGameplayAbility_KorrynPrimary"));

		Probe->Step = 1;
		Probe->StepTime = Now;
		return true;
	}

	if (Probe->Step == 1)
	{
		// Keep all 4 heroes pinned at their post-combat locations so PendingLaunchVelocity cannot move them
		Place(ArgusPawn, Origin, FRotator::ZeroRotator);
		Place(RawlinsPawn, Origin + FVector(150.f, 300.f, 0.f), FRotator(0.f, 180.f, 0.f));
		Place(AlonaPawn, Origin + FVector(400.f, -2000.f, 0.f), FRotator::ZeroRotator);
		Place(KorrynPawn, Origin + FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));

		if (Now - Probe->StepTime >= 0.8)
		{
			Probe->StepTime = Now;
			ULyraAbilitySystemComponent* ArgusASC = Players[0]->GetLyraAbilitySystemComponent();
			ULyraAbilitySystemComponent* KorrynASC = Players[3]->GetLyraAbilitySystemComponent();
			if (ArgusASC && ArgusASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) < 260.f)
			{
				ArgusASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 340.f);
			}
			ActivateAuthoritative(KorrynASC, TEXT("BwayGameplayAbility_KorrynPrimary"));
		}

		FString HostSummary;
		bool bAllConverged = VerifyWorldConvergence(World, HostSummary);
		Probe->LastSummary = HostSummary;
		int32 PieWorldsVerified = 1;

		if (bAllConverged && Probe->bCheckAllPieWorlds && GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				UWorld* ClientWorld = Ctx.World();
				if (!ClientWorld || !ClientWorld->IsGameWorld() || ClientWorld->GetNetMode() != NM_Client) continue;
				FString ClientSummary;
				if (!VerifyWorldConvergence(ClientWorld, ClientSummary))
				{
					Probe->LastSummary = FString::Printf(TEXT("Host:[%s] Client:[%s]"), *HostSummary, *ClientSummary);
					bAllConverged = false;
					break;
				}
				++PieWorldsVerified;
			}
			if (PieWorldsVerified < 4) bAllConverged = false;
		}

		if (!bAllConverged)
		{
			Probe->StableSince = 0.0;
			return true;
		}
		if (Probe->StableSince == 0.0) Probe->StableSince = Now;
		// Keep state alive for 2.5s on packaged Host so 100ms-lag clients also observe settled state
		const double RequiredHold = Probe->bCheckAllPieWorlds ? 0.5 : 2.5;
		if (Now - Probe->StableSince < RequiredHold) return true;

		if (Probe->bCheckAllPieWorlds)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCombatTest: PIE PASS worlds=%d %s"), PieWorldsVerified, *HostSummary);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCombatTest: Host PASS %s"), *HostSummary);
		}
		return FinishProbe(Probe, nullptr);
	}

	return true;
}

void StartRuntimeProbe(bool bClient, bool bCheckAllPieWorlds)
{
	if (GActiveProbe) CleanupProbe(GActiveProbe);
	TSharedPtr<FRuntimeProbe> Probe = MakeShared<FRuntimeProbe>();
	Probe->bClient = bClient;
	Probe->bCheckAllPieWorlds = bCheckAllPieWorlds;
	GActiveProbe = Probe;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float)
	{
		if (GActiveProbe != Probe) return false;
		return TickRuntimeProbe(Probe);
	}), 0.05f);
}

FAutoConsoleCommand CmdAllKitPIE(
	TEXT("bway.Test.AllKitCombatPIE"),
	TEXT("Development 4-world PIE multi-kit combat, status tag, armor shred, leech, and fumble convergence check."),
	FConsoleCommandDelegate::CreateLambda([]() { StartRuntimeProbe(false, true); }));

FAutoConsoleCommand CmdAllKitHost(
	TEXT("bway.Test.AllKitCombatHost"),
	TEXT("Development 4-process packaged multi-kit combat authority coordinator."),
	FConsoleCommandDelegate::CreateLambda([]() { StartRuntimeProbe(false, false); }));

FAutoConsoleCommand CmdAllKitClient(
	TEXT("bway.Test.AllKitCombatClient"),
	TEXT("Development 4-process packaged multi-kit combat client observer."),
	FConsoleCommandDelegate::CreateLambda([]() { StartRuntimeProbe(true, false); }));
} // namespace BwayAllKitCombatRuntime

#endif // !UE_BUILD_SHIPPING
