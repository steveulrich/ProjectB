#include "Misc/AutomationTest.h"

#include "Abilities/GameplayAbility.h"
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
#include "Containers/Ticker.h"
#include "Economy/BwayBaseZone.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "LyraGameplayTags.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
	explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual ~FRun() override
	{
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
		Ext->SetPawnData(NewObject<ULyraPawnData>(World));
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
		HumanoidSet = LoadObject<ULyraAbilitySet>(nullptr, TEXT("/BreakawayCore/Characters/DA_BW_AbilitySet_Humanoid.DA_BW_AbilitySet_Humanoid"));
		if (!Test->TestNotNull(TEXT("Humanoid ability set"), HumanoidSet)) return false;
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

	virtual bool Update() override
	{
		if (!World && !Setup()) return true;
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
				Test->TestTrue(TEXT("Korryn CircleOfSpite activated"), Source.ASC->TryActivateAbility(CircleH));
				EnsureZonesAndProjectilesBegunPlay();
				Test->TestTrue(TEXT("Korryn CircleOfSpite applies State.Status.DamageAmplified"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_DamageAmplified));
				Test->TestTrue(TEXT("Korryn CircleOfSpite applies State.Status.Slowed"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed));
				Test->TestTrue(TEXT("Korryn CircleOfSpite IncomingDamageMultiplier == 1.35"), FMath::IsNearlyEqual(DamageMult(Enemy), 1.35f, 0.01f));
				Test->TestTrue(TEXT("Korryn CircleOfSpite ResolveFinalDamage(100) == 135"), FMath::IsNearlyEqual(UBwayDamageLibrary::ResolveFinalDamage(100.f, Enemy.ASC), 135.f, 0.05f));
				Enemy.Pawn->SetActorLocation(FVector(-1200.f, 0.f, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
				Test->TestFalse(TEXT("Leaving CircleOfSpite removes DamageAmplified"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_DamageAmplified));
				Test->TestTrue(TEXT("Leaving CircleOfSpite restores IncomingDamageMultiplier == 1.0"), FMath::IsNearlyEqual(DamageMult(Enemy), 1.0f, 0.01f));

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
				const int32 FumblesBefore = Enemy.PS->GetForcedFumbles();
				const FGameplayAbilitySpecHandle SilenceH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_KorrynAuraOfSilence"));
				Test->TestTrue(TEXT("Korryn AuraOfSilence activated"), Source.ASC->TryActivateAbility(SilenceH));
				Test->TestTrue(TEXT("Korryn AuraOfSilence damage (13 + 0.5*40 = 33)"), FMath::IsNearlyEqual(Health(Enemy), 467.f, 0.05f));
				Test->TestTrue(TEXT("Korryn AuraOfSilence non-primary life leech (+3.3)"), FMath::IsNearlyEqual(Health(Source), 203.3f, 0.05f));
				Test->TestTrue(TEXT("Korryn AuraOfSilence applies State.Status.Silenced"), Enemy.ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced));
				Test->TestFalse(TEXT("State.Status.Silenced blocks non-primary ability activation"), Enemy.ASC->TryActivateAbility(EnemyNoRetreatH));
				Test->TestNull(TEXT("Korryn AuraOfSilence forces Relic carrier fumble"), Relic->CurrentCarrier.Get());
				Test->TestEqual(TEXT("Relic enters Dropped state on AuraOfSilence fumble"), Relic->GetCurrentState(), ERelicState::Dropped);
				Test->TestEqual(TEXT("ForcedFumbles incremented on AuraOfSilence fumble"), Enemy.PS->GetForcedFumbles(), FumblesBefore + 1);

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
				const int32 FumblesBefore = Enemy.PS->GetForcedFumbles();
				const FGameplayAbilitySpecHandle PowerShotH = FindAbilityHandle(Source.ASC, TEXT("BwayGameplayAbility_RawlinsPowerShot"));
				Test->TestTrue(TEXT("Rawlins PowerShot activated"), Source.ASC->TryActivateAbility(PowerShotH));
				Test->TestTrue(TEXT("Rawlins PowerShot damage (30 + 0.65*60 = 69)"), FMath::IsNearlyEqual(Health(Enemy), 431.f, 0.05f));
				Test->TestTrue(TEXT("Rawlins PowerShot non-primary life leech (+6.9)"), FMath::IsNearlyEqual(Health(Source), 206.9f, 0.05f));
				Test->TestTrue(TEXT("Rawlins PowerShot knockback"), EffectiveVelocity(Enemy).X > 1000.f);
				Test->TestNull(TEXT("Rawlins PowerShot forces Relic carrier fumble"), Relic->CurrentCarrier.Get());
				Test->TestEqual(TEXT("Relic enters Dropped state on PowerShot fumble"), Relic->GetCurrentState(), ERelicState::Dropped);
				Test->TestEqual(TEXT("ForcedFumbles incremented on PowerShot fumble"), Enemy.PS->GetForcedFumbles(), FumblesBefore + 1);

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
};
} // namespace BwayAllKitCombatEffectsTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayAllKitCombatEffectsTest, "Breakaway.Abilities.AllKitCombatEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayAllKitCombatEffectsTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(BwayAllKitCombatEffectsTest::FRun(this));
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
			if (UCharacterMovementComponent* M = P->GetCharacterMovement()) M->SetMovementMode(H.Mode, H.CustomMode);
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
			if (UCharacterMovementComponent* M = C->GetCharacterMovement())
			{
				H.Mode = M->MovementMode;
				H.CustomMode = M->CustomMovementMode;
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
				ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetArmorAttribute()) });
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
