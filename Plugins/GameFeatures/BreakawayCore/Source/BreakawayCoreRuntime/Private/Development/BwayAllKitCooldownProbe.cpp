#include "Development/BwayAllKitCooldownProbe.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Economy/BwayBaseZone.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "Net/UnrealNetwork.h"
#include "Relic/RelicActor.h"
#include "UI/BwayAbilityBarHUDWidget.h"
#include "UI/BwayMatchAbilitySlotWidget.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAllKitCooldownProbe)

bool FBwayAllKitCooldownFrame::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	Ar << Serial << Case << Cycle << Stage << Source << Base << Bonus << Position << Rotation << Players << Health << Maximum;
	bOutSuccess = !Ar.IsError();
	return true;
}

namespace BwayAllKitCooldown
{
const TCHAR* Heroes[] = {TEXT("Argus"), TEXT("Rawlins"), TEXT("Alona"), TEXT("Korryn")};
struct FCase { const TCHAR* Family; const TCHAR* Field; };
const FCase Cases[] = {
	{TEXT("BwayGameplayAbility_ForGlory"), TEXT("ForGloryCooldown")},
	{TEXT("BwayGameplayAbility_NoRetreat"), TEXT("NoRetreatCooldown")},
	{TEXT("BwayGameplayAbility_Retribution"), TEXT("RetributionCooldown")},
	{TEXT("BwayGameplayAbility_ArgusSlide"), TEXT("SlideCooldown")},
	{TEXT("BwayGameplayAbility_RawlinsPowerShot"), TEXT("PowerShotCooldown")},
	{TEXT("BwayGameplayAbility_RawlinsDoubleDown"), TEXT("DoubleDownCooldown")},
	{TEXT("BwayGameplayAbility_RawlinsSlideShot"), TEXT("SlideShotCooldown")},
	{TEXT("BwayGameplayAbility_RawlinsBlazingBarrage"), TEXT("BarrageCooldown")},
	{TEXT("BwayGameplayAbility_AlonaRadiance"), TEXT("RadianceCooldown")},
	{TEXT("BwayGameplayAbility_AlonaBlessing"), TEXT("BlessingCooldown")},
	{TEXT("BwayGameplayAbility_AlonaSunBurst"), TEXT("SunBurstCooldown")},
	{TEXT("BwayGameplayAbility_AlonaSunsGrace"), TEXT("SunsGraceCooldown")},
	{TEXT("BwayGameplayAbility_KorrynBurdenOfSin"), TEXT("BurdenCooldown")},
	{TEXT("BwayGameplayAbility_KorrynCircleOfSpite"), TEXT("CircleCooldown")},
	{TEXT("BwayGameplayAbility_KorrynAuraOfSilence"), TEXT("AuraCooldown")},
	{TEXT("BwayGameplayAbility_KorrynFlock"), TEXT("FlockCooldown")}
};
ULyraAbilitySystemComponent* ASC(ABwayPlayerState* Player) { return Player ? Player->GetLyraAbilitySystemComponent() : nullptr; }
ABwayCharacterWithAbilities* Pawn(ABwayPlayerState* Player) { return Player ? Cast<ABwayCharacterWithAbilities>(Player->GetPawn()) : nullptr; }
float Health(ABwayPlayerState* Player) { return ASC(Player)->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()); }
float Maximum(ABwayPlayerState* Player) { return ASC(Player)->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()); }
bool Family(const UClass* Class, const TCHAR* Name)
{
	for (; Class; Class = Class->GetSuperClass()) if (Class->GetName() == Name) return true;
	return false;
}
FGameplayAbilitySpec* Ability(ABwayPlayerState* Player, int32 Index)
{
	if (!ASC(Player) || Index < 0 || Index >= UE_ARRAY_COUNT(Cases)) return nullptr;
	for (FGameplayAbilitySpec& Spec : ASC(Player)->GetActivatableAbilities())
		if (Spec.Ability && Family(Spec.Ability->GetClass(), Cases[Index].Family)) return &Spec;
	return nullptr;
}
float Duration(FGameplayAbilitySpec& Spec, int32 Index)
{
	UGameplayAbility* Instance = Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get();
	const FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Instance->GetClass(), TEXT("KitConfig"));
	UObject* Config = Property ? Property->GetPropertyValue_InContainer(Instance).LoadSynchronous() : nullptr;
	const FFloatProperty* Value = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), Cases[Index].Field) : nullptr;
	return Value ? Value->GetPropertyValue_InContainer(Config) : 0.f;
}
TArray<FActiveGameplayEffectHandle> Cooldowns(ULyraAbilitySystemComponent* Component, const FGameplayAbilitySpec& Spec)
{
	const FGameplayTagContainer* Tags = Spec.Ability->GetCooldownTags();
	return Tags ? Component->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*Tags)) : TArray<FActiveGameplayEffectHandle>();
}
TArray<ABwayPlayerState*> Players(UWorld* World)
{
	TArray<ABwayPlayerState*> Result;
	ABwayGameState* GS = World->GetGameState<ABwayGameState>();
	if (!GS || GS->PlayerArray.Num() != 4) return Result;
	Result.SetNumZeroed(4);
	for (APlayerState* State : GS->PlayerArray)
	{
		ABwayPlayerState* P = Cast<ABwayPlayerState>(State);
		if (!P || P->IsABot() || !Pawn(P) || !ASC(P) || ASC(P)->GetAvatarActor() != Pawn(P) || !P->UpgradeComponent
			|| !ASC(P)->GetSet<ULyraHealthSet>() || !ASC(P)->GetSet<UBwayHeroAttributeSet>() || GS->GetPlayerTeam(P) == INDEX_NONE) return {};
		for (int32 Index = 0; Index < 4; ++Index)
			if (P->GetSelectedHeroId() == UBwayHeroRegistry::ResolveHeroIdByName(Heroes[Index]))
			{
				if (Result[Index]) return {};
				Result[Index] = P;
			}
	}
	return Result.Contains(nullptr) ? TArray<ABwayPlayerState*>() : Result;
}
struct FMovement
{
	float Fly = 0, Walk = 0, Gravity = 0;
	bool Orient = false;
	ECollisionResponse Response = ECR_Block;
	void Save(ABwayCharacterWithAbilities* Character)
	{
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		Fly = Move->MaxFlySpeed; Walk = Move->MaxWalkSpeed; Gravity = Move->GravityScale; Orient = Move->bOrientRotationToMovement;
		Response = Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
	}
	bool Matches(ABwayCharacterWithAbilities* Character) const
	{
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		return FMath::IsNearlyEqual(Move->MaxFlySpeed, Fly) && FMath::IsNearlyEqual(Move->MaxWalkSpeed, Walk)
			&& FMath::IsNearlyEqual(Move->GravityScale, Gravity) && Move->bOrientRotationToMovement == Orient
			&& Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == Response && Move->MovementMode != MOVE_Flying;
	}
};
bool HudMatches(UBwayMatchAbilitySlotWidget* Slot, bool bCooling, float Remaining, float Total)
{
	if (!Slot || !Slot->GetCachedWidget().IsValid() || !Slot->IsVisible()) return false;
	const auto& VM = Slot->GetSlotViewModel();
	const UTextBlock* Timer = Cast<UTextBlock>(Slot->GetWidgetFromName(TEXT("Text_CooldownTimer")));
	const UProgressBar* Bar = Cast<UProgressBar>(Slot->GetWidgetFromName(TEXT("ProgressBar_Cooldown")));
	const UImage* Icon = Cast<UImage>(Slot->GetWidgetFromName(TEXT("Image_Icon")));
	const UImage* Overlay = Cast<UImage>(Slot->GetWidgetFromName(TEXT("Image_CooldownOverlay")));
	if (!Timer || !Bar || !Icon || !VM.bIsAvailable || VM.bIsOnCooldown != bCooling) return false;
	if (!Icon->GetColorAndOpacity().Equals(bCooling ? FLinearColor(0.22f, 0.22f, 0.22f, 1.f) : FLinearColor::White, 0.001f)) return false;
	if (!bCooling) return VM.CountdownTime == 0 && VM.CooldownPercent == 0 && Timer->GetText().IsEmpty()
		&& Timer->GetVisibility() == ESlateVisibility::Collapsed && Bar->GetVisibility() == ESlateVisibility::Collapsed
		&& Bar->GetPercent() == 0 && (!Overlay || Overlay->GetVisibility() == ESlateVisibility::Collapsed);
	return Total > 0 && FMath::Abs(VM.CountdownTime - Remaining) <= 0.35f
		&& FMath::Abs(VM.CooldownPercent - Remaining / Total) <= 0.03f
		&& FMath::IsNearlyEqual(Bar->GetPercent(), VM.CooldownPercent, 0.001f)
		&& Timer->GetText().ToString() == FText::AsNumber(FMath::Max(1, FMath::CeilToInt(VM.CountdownTime))).ToString()
		&& Timer->IsVisible() && Bar->IsVisible() && (!Overlay || Overlay->IsVisible());
}
bool LingeringActor(UWorld* World, APawn* Source)
{
	for (TActorIterator<AActor> It(World); It; ++It)
		if (It->GetOwner() == Source && (Family(It->GetClass(), TEXT("BwayAlonaBlessingZone"))
			|| Family(It->GetClass(), TEXT("BwayKorrynSpiteZone")) || Family(It->GetClass(), TEXT("BwayRawlinsBulletProjectile")))) return true;
	return false;
}
}
using namespace BwayAllKitCooldown;

struct FBwayAllKitCooldownState
{
	struct FSaved
	{
		TWeakObjectPtr<ABwayPlayerState> Player;
		FTransform Transform;
		FRotator Rotation;
		float Health = 0, Maximum = 0, Gold = 0;
	};
	TArray<FSaved> Saved;
	TMap<TWeakObjectPtr<ABwayBaseZone>, float> Bases;
	double Started = FPlatformTime::Seconds(), Changed = 0, StableAt = 0;
	int32 SetupStep = 0, LastSerial = 0, OwnerStep = 0, OwnerAck = -1, Completed = 0;
	float ServerStart = 0, OwnerStart = 0, SampleTime = 0, SampleRemaining = 0;
	float LastAuthorityTime = 0, LastOwnerTime = 0, MaxAuthorityGap = 0, MaxOwnerGap = 0;
	bool bServerStarted = false, bServerExpired = false, bCleaned = false, bPassed = false, bFailed = false, bPeersObserved = false;
	bool bRemovalSeen = false, bNaturalRemoval = false;
	FActiveGameplayEffectHandle ServerEffect;
	TWeakObjectPtr<ULyraAbilitySystemComponent> RemovalASC;
	FDelegateHandle RemovalObserver;
	FMovement AuthorityMovement, OwnerMovement;
	TWeakObjectPtr<UBwayAbilityBarHUDWidget> Bar;
	TWeakObjectPtr<UBwayMatchAbilitySlotWidget> Slot;
};

ABwayAllKitCooldownProbe::ABwayAllKitCooldownProbe()
{
	bReplicates = true; bAlwaysRelevant = true; bNetLoadOnClient = false;
	SetReplicateMovement(false); SetNetUpdateFrequency(10.f);
	PrimaryActorTick.bCanEverTick = !UE_BUILD_SHIPPING;
	PrimaryActorTick.TickInterval = 0.05f;
}
void ABwayAllKitCooldownProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayAllKitCooldownProbe, Frame);
}
void ABwayAllKitCooldownProbe::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	if (UGameViewportClient* VP = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		VP->EngineShowFlags.SetLighting(false);
		VP->EngineShowFlags.SetPostProcessing(false);
		VP->EngineShowFlags.SetDynamicShadows(false);
		VP->EngineShowFlags.SetGlobalIllumination(false);
		VP->EngineShowFlags.SetLumenGlobalIllumination(false);
		VP->EngineShowFlags.SetLumenReflections(false);
		VP->EngineShowFlags.SetStaticMeshes(false);
		VP->EngineShowFlags.SetNaniteMeshes(false);
		VP->EngineShowFlags.SetInstancedStaticMeshes(false);
		VP->EngineShowFlags.SetLandscape(false);
		VP->EngineShowFlags.SetParticles(false);
		VP->EngineShowFlags.SetTranslucency(false);
		VP->EngineShowFlags.SetVolumetricFog(false);
		VP->EngineShowFlags.SetAtmosphere(false);
		VP->EngineShowFlags.SetFog(false);
	}
	State = MakeShared<FBwayAllKitCooldownState>();
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Connected role=%s"), HasAuthority() ? TEXT("host") : TEXT("client"));
#endif
}
void ABwayAllKitCooldownProbe::Fail(const TCHAR* Reason)
{
	if (!State || State->bFailed) return;
	State->bFailed = true;
	UE_LOG(LogTemp, Error, TEXT("BwayAllKitCooldown: FAIL role=%s case=%d cycle=%d serial=%d ownerStep=%d ack=%d reason=%s"), HasAuthority() ? TEXT("host") : TEXT("client"), Frame.Case, Frame.Cycle, Frame.Serial, State->OwnerStep, State->OwnerAck, Reason);
	if (HasAuthority()) { Cleanup(); Frame.Stage = 3; ForceNetUpdate(); }
}
void ABwayAllKitCooldownProbe::ServerOwnerStage_Implementation(int32 Serial, int32 Stage)
{
	if (!State || State->bFailed || Frame.Stage != 1 || Serial != Frame.Serial || Stage < 0 || Stage > 2) return;
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->PlayerState || PC->PlayerState->GetPlayerId() != Frame.Source) return;
	if (Stage != State->OwnerAck + 1) { Fail(TEXT("owner-stage-order")); return; }
	State->OwnerAck = Stage;
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Ack case=%d cycle=%d player=%d stage=%d"), Frame.Case, Frame.Cycle, Frame.Source, Stage);
}

bool ABwayAllKitCooldownProbe::Setup()
{
	const auto All = Players(GetWorld());
	ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>();
	UBwayRoundManagementComponent* Rounds = GS ? GS->GetRoundManagement() : nullptr;
	if (All.Num() != 4 || !Rounds) return false;
	if (State->SetupStep == 0)
	{
		if (Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return false;
		for (ABwayPlayerState* P : All) if (ASC(P)->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity)) return false;
		for (ABwayPlayerState* P : All)
		{
			const UBwayUpgradeCatalog* Catalog = P->UpgradeComponent->GetCatalog();
			const FBwayUpgradeDefinition* Upgrade = Catalog ? Catalog->FindUpgrade(TEXT("CooldownReduction")) : nullptr;
			if (!Upgrade || Upgrade->Ranks.IsEmpty() || !P->UpgradeComponent->GetOwnedUpgrades().IsEmpty()) { Fail(TEXT("invalid-catalog-or-baseline")); return false; }
			const float Bonus = Upgrade->Ranks[0].Magnitude;
			if (!FMath::IsFinite(Bonus) || Bonus <= 0 || Bonus >= 0.5f || (Frame.Bonus != 0 && Frame.Bonus != Bonus)) { Fail(TEXT("invalid-reduction")); return false; }
			Frame.Bonus = Bonus;
			auto& Saved = State->Saved.AddDefaulted_GetRef();
			Saved.Player = P; Saved.Gold = ASC(P)->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute());
			ASC(P)->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), Upgrade->Ranks[0].Cost + 100.f);
		}
		for (TActorIterator<ABwayBaseZone> It(GetWorld()); It; ++It) { State->Bases.Add(*It, It->HealingPerSecond); It->HealingPerSecond = 0; }
		State->SetupStep = 1;
		Rounds->EndRound(0, EBwayWinCondition::GoalScored);
		return false;
	}
	if (State->SetupStep == 1)
	{
		for (ABwayPlayerState* P : All) if (!P->UpgradeComponent->IsPurchaseWindowOpen()) return false;
		for (ABwayPlayerState* P : All)
		{
			P->UpgradeComponent->ServerPurchaseUpgrade(TEXT("CooldownReduction"), 0);
			if (P->UpgradeComponent->GetOwnedRank(TEXT("CooldownReduction")) != 1) { Fail(TEXT("purchase-rejected")); return false; }
		}
		State->SetupStep = 2;
		return false;
	}
	if (State->SetupStep == 2)
	{
		if (Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return false;
		for (ABwayPlayerState* P : All) if (ASC(P)->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity)) return false;
		// Round reset replaces avatars: capture movement and health only now.
		for (int32 Index = 0; Index < 4; ++Index)
		{
			ABwayPlayerState* P = All[Index];
			auto& Saved = State->Saved[Index];
			if (Saved.Player != P) { Fail(TEXT("roster-changed")); return false; }
			Saved.Transform = Pawn(P)->GetActorTransform(); Saved.Rotation = P->GetPlayerController()->GetControlRotation();
			Saved.Health = Health(P); Saved.Maximum = Maximum(P);
			Frame.Players.Add(P->GetPlayerId()); Frame.Health.Add(Saved.Health); Frame.Maximum.Add(Saved.Maximum);
		}
		State->SetupStep = 3;
		Frame.Case = 0; Frame.Cycle = 1;
		return Prepare();
	}
	return true;
}

bool ABwayAllKitCooldownProbe::Prepare()
{
	const auto All = Players(GetWorld());
	if (All.Num() != 4 || Frame.Case < 0 || Frame.Case >= UE_ARRAY_COUNT(Cases)) { Fail(TEXT("prepare-roster")); return false; }
	ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>();
	const int32 Hero = Frame.Case / 4;
	ABwayPlayerState* Source = All[Hero];
	ABwayPlayerState* Enemy = nullptr;
	ABwayPlayerState* Ally = nullptr;
	for (ABwayPlayerState* P : All) if (P != Source)
	{
		if (GS->GetPlayerTeam(P) == GS->GetPlayerTeam(Source)) Ally = P;
		else if (!Enemy) Enemy = P;
	}
	FGameplayAbilitySpec* Spec = Ability(Source, Frame.Case);
	if (!Spec || !Enemy || !Ally || !Cooldowns(ASC(Source), *Spec).IsEmpty() || Spec->IsActive()) { Fail(TEXT("prepare-spec-target-or-cooldown")); return false; }
	Frame.Base = Duration(*Spec, Frame.Case);
	if (!FMath::IsFinite(Frame.Base) || Frame.Base < 6 || Frame.Base > 60) { Fail(TEXT("unsupported-kit-duration")); return false; }
	Frame.Source = Source->GetPlayerId();
	const FVector Origin = State->Saved[Hero].Transform.GetLocation();
	const FVector EnemyOrigin = State->Saved[All.IndexOfByKey(Enemy)].Transform.GetLocation();
	const FVector ArenaDirection = (EnemyOrigin - Origin).GetSafeNormal2D();
	if (ArenaDirection.IsNearlyZero()) { Fail(TEXT("no-arena-direction")); return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(AllKitCooldownGround), false);
	for (ABwayPlayerState* P : All) Query.AddIgnoredActor(Pawn(P));
	auto Ground = [this, &Query](FVector Point, float HalfHeight, FVector& Out)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0, 0, 200), Point - FVector(0, 0, 1000), ECC_WorldStatic, Query)
			|| Hit.ImpactNormal.Z < 0.71f) return false;
		Out = Hit.ImpactPoint + FVector(0, 0, HalfHeight + 3.f);
		return true;
	};
	FVector Start, Forward, Right;
	const float HalfHeight = Pawn(Source)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	bool bFound = false;
	int32 Attempts = 0;
	int32 MissingFloor = 0, UnevenRoute = 0, RelicBlocked = 0, TargetFloor = 0, Obstructed = 0;
	TArray<FVector> Relics;
	for (TActorIterator<ARelicActor> It(GetWorld()); It; ++It) Relics.Add(It->GetActorLocation());
	// Spawn platforms are not necessarily long, flat combat lanes. Search a
	// bounded set of arena points and headings, retaining every geometry guard.
	for (float Fraction : {0.35f, 0.5f, 0.65f, 0.2f, 0.8f})
	{
		for (float Side : {-900.f, 900.f, -500.f, 500.f, 0.f})
		{
			for (float Yaw : {0.f, 90.f, -90.f, 180.f})
			{
				++Attempts;
				const FVector Direction = ArenaDirection.RotateAngleAxis(Yaw, FVector::UpVector);
				const FVector Sideways = FVector::CrossProduct(FVector::UpVector, Direction);
				const FVector Candidate = FMath::Lerp(Origin, EnemyOrigin, Fraction) + FVector::CrossProduct(FVector::UpVector, ArenaDirection) * Side;
				FVector Location;
				if (!Ground(Candidate, HalfHeight, Location)) { ++MissingFloor; continue; }
				bool bClear = true;
				for (float Distance : {0.f, 250.f, 500.f, 750.f, 1000.f, 1300.f})
				{
					FVector Floor;
					const FVector Point = Location + Direction * Distance;
					if (!Ground(Point, HalfHeight, Floor) || FMath::Abs(Floor.Z - Location.Z) > 80.f) { ++UnevenRoute; bClear = false; break; }
					for (const FVector& Relic : Relics) if (FVector::Dist2D(Point, Relic) < 650.f) { ++RelicBlocked; bClear = false; }
				}
				if (!bClear) continue;
				for (const FVector& Offset : {Direction * 400.f + Sideways * 120.f, Direction * 180.f + Sideways * ((Frame.Case == 8 || Frame.Case == 9) ? 650.f : 0.f)})
				{
					FVector Floor;
					if (!Ground(Location + Offset, HalfHeight, Floor)) { ++TargetFloor; bClear = false; }
					for (const FVector& Relic : Relics) if (FVector::Dist2D(Location + Offset, Relic) < 650.f) bClear = false;
				}
				FHitResult Obstacle;
				if (bClear && !GetWorld()->SweepSingleByChannel(Obstacle, Location, Location + Direction * 1300.f, FQuat::Identity, ECC_WorldStatic,
					FCollisionShape::MakeCapsule(Pawn(Source)->GetCapsuleComponent()->GetScaledCapsuleRadius(), HalfHeight), Query))
				{ Start = Location; Forward = Direction; Right = Sideways; bFound = true; break; }
				if (bClear) ++Obstructed;
			}
			if (bFound) break;
		}
		if (bFound) break;
	}
	if (!bFound)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: LaneRejected attempts=%d missingFloor=%d uneven=%d relic=%d targetFloor=%d obstructed=%d origin=%s enemy=%s"), Attempts, MissingFloor, UnevenRoute, RelicBlocked, TargetFloor, Obstructed, *Origin.ToCompactString(), *EnemyOrigin.ToCompactString());
		Fail(TEXT("no-clear-arena-lane")); return false;
	}
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Lane case=%d cycle=%d attempts=%d x=%.2f y=%.2f z=%.2f yaw=%.2f relics=%d"), Frame.Case, Frame.Cycle, Attempts, Start.X, Start.Y, Start.Z, Forward.Rotation().Yaw, Relics.Num());
	const FRotator Rotation = Forward.Rotation();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ABwayPlayerState* P = All[Index];
		// Extra health prevents a cooldown observation from turning into a death test.
		ASC(P)->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 5000.f);
		ASC(P)->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 4000.f);
		FVector Point = State->Saved[Index].Transform.GetLocation();
		if (P == Source) Point = Start;
		else if (P == Enemy) Point = Start + Forward * 180.f + Right * ((Frame.Case == 8 || Frame.Case == 9) ? 650.f : 0.f);
		else if (P == Ally) Point = Start + Forward * 400.f + Right * 120.f;
		FVector Location;
		if (!Ground(Point, Pawn(P)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Location)) { Fail(TEXT("target-floor-missing")); return false; }
		Pawn(P)->GetCharacterMovement()->StopMovementImmediately();
		Pawn(P)->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
		Pawn(P)->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		if (APlayerController* PC = P->GetPlayerController()) { PC->SetControlRotation(Rotation); PC->ClientSetRotation(Rotation, true); }
		P->ForceNetUpdate(); Pawn(P)->ForceNetUpdate();
	}
	Frame.Position = Pawn(Source)->GetActorLocation(); Frame.Rotation = Rotation;
	Frame.Stage = 1; ++Frame.Serial;
	State->Changed = FPlatformTime::Seconds(); State->OwnerAck = -1;
	State->bServerStarted = false; State->bServerExpired = false;
	State->bRemovalSeen = false; State->bNaturalRemoval = false;
	State->LastAuthorityTime = GetWorld()->GetTimeSeconds(); State->MaxAuthorityGap = 0;
	State->AuthorityMovement.Save(Pawn(Source));
	SetOwner(Source->GetPlayerController());
	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Plan case=%d cycle=%d serial=%d hero=%s source=%d enemy=%d ally=%d ability=%s family=%s field=%s base=%.4f bonus=%.4f expected=%.4f clearRoute=1300 controlledHealth=4000"),
		Frame.Case, Frame.Cycle, Frame.Serial, Heroes[Hero], Frame.Source, Enemy->GetPlayerId(), Ally->GetPlayerId(), *Spec->Ability->GetClass()->GetPathName(), Cases[Frame.Case].Family, Cases[Frame.Case].Field, Frame.Base, Frame.Bonus, Frame.Base * (1.f - Frame.Bonus));
	return true;
}

void ABwayAllKitCooldownProbe::AuthorityTick()
{
	const auto All = Players(GetWorld());
	if (All.Num() != 4) { Fail(TEXT("authority-roster-lost")); return; }
	ABwayPlayerState* Source = All[Frame.Case / 4];
	ULyraAbilitySystemComponent* Component = ASC(Source);
	FGameplayAbilitySpec* Spec = Ability(Source, Frame.Case);
	if (!Spec || Health(Source) <= 0) { Fail(TEXT("authority-spec-or-source-lost")); return; }
	const float Time = GetWorld()->GetTimeSeconds();
	State->MaxAuthorityGap = FMath::Max(State->MaxAuthorityGap, Time - State->LastAuthorityTime);
	State->LastAuthorityTime = Time;
	const float Expected = Frame.Base * (1.f - Frame.Bonus);
	const auto Handles = Cooldowns(Component, *Spec);
	if (Handles.Num() > 1) { Fail(TEXT("duplicate-authority-cooldown")); return; }
	if (Handles.Num() == 1)
	{
		const FActiveGameplayEffect* Effect = Component->GetActiveGameplayEffect(Handles[0]);
		if (!Effect || !FMath::IsNearlyEqual(Effect->GetDuration(), Expected, 0.01f)
			|| !FMath::IsNearlyEqual(Component->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()), Frame.Bonus, 0.001f)
			|| Source->UpgradeComponent->GetOwnedRank(TEXT("CooldownReduction")) != 1) { Fail(TEXT("authority-duration-or-rank")); return; }
		if (!State->bServerStarted)
		{
			State->bServerStarted = true; State->ServerStart = Time - (Effect->GetDuration() - Effect->GetTimeRemaining(Time)); State->ServerEffect = Handles[0];
			FOnActiveGameplayEffectRemoved_Info* Removed = Component->OnGameplayEffectRemoved_InfoDelegate(Handles[0]);
			if (!Removed) { Fail(TEXT("removal-observer-unavailable")); return; }
			State->RemovalASC = Component;
			const TWeakObjectPtr<ABwayAllKitCooldownProbe> Weak(this);
			const int32 Serial = Frame.Serial;
			State->RemovalObserver = Removed->AddLambda([Weak, Serial](const FGameplayEffectRemovalInfo& Info)
			{
				ABwayAllKitCooldownProbe* Probe = Weak.Get();
				if (!Probe || !Probe->State || Probe->State->bFailed || Probe->Frame.Serial != Serial) return;
				Probe->State->bRemovalSeen = true;
				Probe->State->bNaturalRemoval = !Info.bPrematureRemoval && !Info.bPredictionRejected;
				UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Authority REMOVE case=%d cycle=%d player=%d elapsed=%.4f expected=%.4f natural=%d predictionRejected=%d worldDelta=%.4f"),
					Probe->Frame.Case, Probe->Frame.Cycle, Probe->Frame.Source, Probe->GetWorld()->GetTimeSeconds() - Probe->State->ServerStart,
					Probe->Frame.Base * (1.f - Probe->Frame.Bonus), Probe->State->bNaturalRemoval, Info.bPredictionRejected, Probe->GetWorld()->GetDeltaSeconds());
			});
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Authority BEGIN case=%d cycle=%d player=%d base=%.4f bonus=%.4f duration=%.4f"), Frame.Case, Frame.Cycle, Frame.Source, Frame.Base, Frame.Bonus, Effect->GetDuration());
		}
		else if (State->ServerEffect != Handles[0] || State->bServerExpired) { Fail(TEXT("unexpected-authority-reactivation")); return; }
	}
	if (State->bServerStarted && Handles.IsEmpty() && !State->bServerExpired)
	{
		const float Elapsed = Time - State->ServerStart;
		const bool bLingering = LingeringActor(GetWorld(), Pawn(Source));
		if (FMath::Abs(Elapsed - Expected) > 0.75f || Spec->IsActive() || bLingering || !State->bRemovalSeen || !State->bNaturalRemoval)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: ExpiryDiagnostic role=host case=%d cycle=%d elapsed=%.4f expected=%.4f active=%d lingering=%d removalSeen=%d natural=%d maxTickGap=%.4f worldDelta=%.4f"),
				Frame.Case, Frame.Cycle, Elapsed, Expected, Spec->IsActive(), bLingering, State->bRemovalSeen, State->bNaturalRemoval, State->MaxAuthorityGap, GetWorld()->GetDeltaSeconds());
			Fail(TEXT("authority-expiry-or-cleanup")); return;
		}
		if (Frame.Case == 15 && (!State->AuthorityMovement.Matches(Pawn(Source)) || Component->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal)
			|| Component->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity))) { Fail(TEXT("authority-flock-restoration")); return; }
		State->bServerExpired = true;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Authority EXPIRE case=%d cycle=%d player=%d elapsed=%.4f expected=%.4f inactive=1 lingering=0 movementRestored=1 maxTickGap=%.4f"), Frame.Case, Frame.Cycle, Frame.Source, Elapsed, Expected, State->MaxAuthorityGap);
	}
	if (State->bServerExpired && State->OwnerAck == 2)
	{
		++State->Completed;
		if (++Frame.Cycle > 2) { Frame.Cycle = 1; ++Frame.Case; }
		if (Frame.Case == UE_ARRAY_COUNT(Cases))
		{
			Cleanup(); Frame.Stage = 2; ++Frame.Serial; State->Changed = FPlatformTime::Seconds(); ForceNetUpdate();
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Authority PASS cases=16 cycles=%d"), State->Completed);
			SetLifeSpan(12.f);
		}
		else Prepare();
	}
	else if (FPlatformTime::Seconds() - State->Changed > Expected + 20.f) Fail(TEXT("cycle-timeout"));
}

void ABwayAllKitCooldownProbe::Observe()
{
	if (Frame.Stage == 0 || Frame.Serial == 0 || State->bPassed) return;
	if (Frame.Stage == 3) { Fail(TEXT("authority-failed")); return; }
	const auto All = Players(GetWorld());
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ABwayPlayerState* Local = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (All.Num() != 4 || !Local || !PC->IsLocalController()) return;
	const float Time = GetWorld()->GetTimeSeconds();
	if (Frame.Stage == 2)
	{
		if (Frame.Players.Num() != 4 || Frame.Health.Num() != 4 || Frame.Maximum.Num() != 4) { Fail(TEXT("invalid-cleanup-frame")); return; }
		for (int32 Index = 0; Index < 4; ++Index)
		{
			ABwayPlayerState* P = All[Index];
			if (Frame.Players[Index] != P->GetPlayerId() || !FMath::IsNearlyEqual(Health(P), Frame.Health[Index], 0.1f)
				|| !FMath::IsNearlyEqual(Maximum(P), Frame.Maximum[Index], 0.1f)
				|| ASC(P)->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()) != 0.f) return;
		}
		if (!Local->UpgradeComponent->GetOwnedUpgrades().IsEmpty()) return;
		State->bPassed = true;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: %s PASS observer=%d cases=16 cycles=32 restoredPlayers=4"), HasAuthority() ? TEXT("Host") : TEXT("Client"), Local->GetPlayerId());
		return;
	}
	if (Frame.Case < 0 || Frame.Case >= UE_ARRAY_COUNT(Cases)) { Fail(TEXT("invalid-case-frame")); return; }
	ABwayPlayerState* Source = All[Frame.Case / 4];
	if (Source->GetPlayerId() != Frame.Source) { Fail(TEXT("source-roster-mismatch")); return; }
	if (State->LastSerial != Frame.Serial)
	{
		State->LastSerial = Frame.Serial; State->OwnerStep = 0; State->Bar.Reset(); State->Slot.Reset(); State->StableAt = 0; State->bPeersObserved = false;
		State->LastOwnerTime = Time; State->MaxOwnerGap = 0;
	}
	if (!State->bPeersObserved)
	{
		for (ABwayPlayerState* P : All)
			if (!FMath::IsNearlyEqual(ASC(P)->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()), Frame.Bonus, 0.001f)
				|| Maximum(P) != 5000.f || Health(P) <= 0) return;
		State->bPeersObserved = true;
		for (ABwayPlayerState* P : All)
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Peer case=%d cycle=%d observer=%d player=%d source=%d team=%d reduction=%.4f maxHealth=%.4f health=%.4f"),
				Frame.Case, Frame.Cycle, Local->GetPlayerId(), P->GetPlayerId(), Frame.Source, GetWorld()->GetGameState<ABwayGameState>()->GetPlayerTeam(P),
				ASC(P)->GetNumericAttribute(UBwayHeroAttributeSet::GetCooldownReductionAttribute()), Maximum(P), Health(P));
	}
	if (Source != Local || GetOwner() != PC) return;
	State->MaxOwnerGap = FMath::Max(State->MaxOwnerGap, Time - State->LastOwnerTime); State->LastOwnerTime = Time;
	ULyraAbilitySystemComponent* Component = ASC(Source);
	FGameplayAbilitySpec* Spec = Ability(Source, Frame.Case);
	if (!Spec) return;
	const float Expected = Frame.Base * (1.f - Frame.Bonus);
	if (!State->Bar.IsValid())
	{
		for (TObjectIterator<UBwayAbilityBarHUDWidget> It; It; ++It)
			if (It->GetWorld() == GetWorld() && It->GetOwningPlayer() == PC && It->GetCachedWidget().IsValid()) { State->Bar = *It; break; }
	}
	if (State->Bar.IsValid()) State->Bar->RefreshAbilityBar();
	if (!State->Slot.IsValid())
	{
		for (TObjectIterator<UBwayMatchAbilitySlotWidget> It; It; ++It)
			if (It->GetWorld() == GetWorld() && It->GetOwningPlayer() == PC && It->GetCachedWidget().IsValid()
				&& Spec->GetDynamicSpecSourceTags().HasTagExact(It->GetSlotViewModel().DisplayInfo.InputActionTag)) { State->Slot = *It; break; }
	}
	if (State->OwnerStep == 0)
	{
		if (!HudMatches(State->Slot.Get(), false, 0, Expected) || Spec->IsActive() || !Cooldowns(Component, *Spec).IsEmpty()
			|| Source->UpgradeComponent->GetOwnedRank(TEXT("CooldownReduction")) != 1 || Component->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity)
			|| FVector::Dist(Pawn(Source)->GetActorLocation(), Frame.Position) > 30.f) { State->StableAt = 0; return; }
		if (State->StableAt == 0) State->StableAt = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - State->StableAt < 0.75) return;
		// Check the loaded kit once at the activation boundary, not on every HUD
		// sample; a soft config must not be repeatedly synchronously reloaded by observation.
		if (!FMath::IsNearlyEqual(Duration(*Spec, Frame.Case), Frame.Base, 0.001f)) { Fail(TEXT("owner-kit-mismatch")); return; }
		PC->SetControlRotation(Frame.Rotation);
		State->OwnerMovement.Save(Pawn(Source));
		if (!Component->TryActivateAbility(Spec->Handle)) { Fail(TEXT("owner-activation-rejected")); return; }
		State->OwnerStart = Time; State->OwnerStep = 1;
		ServerOwnerStage(Frame.Serial, 0);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Activate case=%d cycle=%d player=%d mode=%d base=%.4f bonus=%.4f expected=%.4f"), Frame.Case, Frame.Cycle, Frame.Source, int32(GetNetMode()), Frame.Base, Frame.Bonus, Expected);
		return;
	}
	const auto Handles = Cooldowns(Component, *Spec);
	if (Handles.Num() > 1 && Time - State->OwnerStart > 1.5f) { Fail(TEXT("duplicate-settled-owner-cooldown")); return; }
	if ((State->OwnerStep == 1 || State->OwnerStep == 2) && Handles.Num() == 1 && Time - State->OwnerStart >= 1.5f)
	{
		const FActiveGameplayEffect* Effect = Component->GetActiveGameplayEffect(Handles[0]);
		if (!Effect || !FMath::IsNearlyEqual(Effect->GetDuration(), Expected, 0.01f)) { Fail(TEXT("owner-duration")); return; }
		const float Remaining = Effect->GetTimeRemaining(Time);
		if (!HudMatches(State->Slot.Get(), true, Remaining, Expected) || Spec->IsActive()) return;
		if (Frame.Case == 15 && (!State->OwnerMovement.Matches(Pawn(Source)) || Component->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal)
			|| Component->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity))) { Fail(TEXT("owner-flock-restoration")); return; }
		if (State->OwnerStep == 1)
		{
			if (Spec->Ability->CheckCooldown(Spec->Handle, Component->AbilityActorInfo.Get()) || Component->TryActivateAbility(Spec->Handle)) { Fail(TEXT("reuse-during-cooldown")); return; }
			State->SampleTime = Time; State->SampleRemaining = Remaining; State->OwnerStep = 2;
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Rejected case=%d cycle=%d player=%d inactive=1"), Frame.Case, Frame.Cycle, Frame.Source);
		}
		else
		{
			if (Time - State->SampleTime < 1.f) return;
			if (Remaining >= State->SampleRemaining || FMath::Abs((State->SampleRemaining - Remaining) - (Time - State->SampleTime)) > 0.35f) { Fail(TEXT("countdown-did-not-advance")); return; }
			State->OwnerStep = 3; ServerOwnerStage(Frame.Serial, 1);
		}
		const auto& VM = State->Slot->GetSlotViewModel();
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: HUD case=%d cycle=%d player=%d sample=%d duration=%.4f remaining=%.4f countdown=%.4f percent=%.4f inactive=1 movementRestored=1"),
			Frame.Case, Frame.Cycle, Frame.Source, State->OwnerStep - 1, Expected, Remaining, VM.CountdownTime, VM.CooldownPercent);
	}
	if (State->OwnerStep == 3 && Handles.IsEmpty() && !Spec->IsActive() && Spec->Ability->CheckCooldown(Spec->Handle, Component->AbilityActorInfo.Get())
		&& HudMatches(State->Slot.Get(), false, 0, Expected))
	{
		const float Elapsed = Time - State->OwnerStart;
		if (FMath::Abs(Elapsed - Expected) > 1.5f)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: ExpiryDiagnostic role=owner case=%d cycle=%d elapsed=%.4f expected=%.4f maxTickGap=%.4f worldDelta=%.4f"),
				Frame.Case, Frame.Cycle, Elapsed, Expected, State->MaxOwnerGap, GetWorld()->GetDeltaSeconds());
			Fail(TEXT("owner-expiry-timing")); return;
		}
		State->OwnerStep = 4; ServerOwnerStage(Frame.Serial, 2);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Owner EXPIRE case=%d cycle=%d player=%d elapsed=%.4f expected=%.4f controls=1 inactive=1"), Frame.Case, Frame.Cycle, Frame.Source, Elapsed, Expected);
	}
}

void ABwayAllKitCooldownProbe::Cleanup()
{
	if (!State || State->bCleaned || !HasAuthority()) return;
	State->bCleaned = true;
	if (State->RemovalASC.IsValid() && State->RemovalObserver.IsValid())
		if (FOnActiveGameplayEffectRemoved_Info* Removed = State->RemovalASC->OnGameplayEffectRemoved_InfoDelegate(State->ServerEffect)) Removed->Remove(State->RemovalObserver);
	State->RemovalObserver.Reset(); State->RemovalASC.Reset();
	int32 Restored = 0, Bases = 0;
	for (const auto& Saved : State->Saved)
	{
		ABwayPlayerState* P = Saved.Player.Get();
		if (!P || !ASC(P) || !P->UpgradeComponent) continue;
		if (State->bFailed) ASC(P)->CancelAllAbilities();
		P->UpgradeComponent->ResetUpgrades();
		ASC(P)->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), Saved.Gold);
		if (State->SetupStep >= 3 && Pawn(P))
		{
			ASC(P)->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), Saved.Maximum);
			ASC(P)->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Saved.Health);
			Pawn(P)->GetCharacterMovement()->StopMovementImmediately();
			Pawn(P)->SetActorTransform(Saved.Transform, false, nullptr, ETeleportType::TeleportPhysics);
			Pawn(P)->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			if (APlayerController* PC = P->GetPlayerController()) { PC->SetControlRotation(Saved.Rotation); PC->ClientSetRotation(Saved.Rotation, true); }
			Pawn(P)->ForceNetUpdate();
		}
		P->ForceNetUpdate(); ++Restored;
	}
	for (const auto& Pair : State->Bases) if (ABwayBaseZone* Base = Pair.Key.Get()) { Base->HealingPerSecond = Pair.Value; ++Bases; }
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Cleanup players=%d bases=%d failed=%d"), Restored, Bases, State->bFailed);
}

void ABwayAllKitCooldownProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!State || State->bFailed) return;
	if (FPlatformTime::Seconds() - State->Started > 1500.0) { Fail(TEXT("overall-timeout")); return; }
	if (HasAuthority())
	{
		if (State->SetupStep < 3) { Setup(); return; }
		if (Frame.Stage == 1) AuthorityTick();
	}
	if (!State->bFailed) Observe();
}
void ABwayAllKitCooldownProbe::EndPlay(const EEndPlayReason::Type Reason)
{
	Cleanup();
	if (State)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Teardown role=%s passed=%d failed=%d"), HasAuthority() ? TEXT("host") : TEXT("client"), State->bPassed, State->bFailed);
		// The listen host remains alive for the runner's four-live checkpoint.
		// Clients exit only after replicated cleanup and fixture destruction.
		if (!HasAuthority() && State->bPassed && !State->bFailed && Reason == EEndPlayReason::Destroyed) FPlatformMisc::RequestExit(false);
	}
	Super::EndPlay(Reason);
}

#if !UE_BUILD_SHIPPING
namespace BwayAllKitCooldown
{
void Start(bool bClient)
{
	for (const auto& Pair : TArray<TPair<const TCHAR*, int32>>{
		{TEXT("r.Lumen.HardwareRayTracing"), 0}, {TEXT("r.DynamicGlobalIlluminationMethod"), 0},
		{TEXT("r.ReflectionMethod"), 0}, {TEXT("r.Shadow.Virtual.Enable"), 0}, {TEXT("r.VSync"), 0}})
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Pair.Key)) CVar->Set(Pair.Value, ECVF_SetByCode);
	const double Started = FPlatformTime::Seconds();
	const TSharedRef<bool> Selected = MakeShared<bool>(false);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([bClient, Started, Selected](float)
	{
		if (FPlatformTime::Seconds() - Started > 100) { UE_LOG(LogTemp, Error, TEXT("BwayAllKitCooldown: FAIL startup-timeout")); return false; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld() || (World->GetNetMode() == NM_Client) != bClient) continue;
			if (UGameViewportClient* VP = World->GetGameViewport())
			{
				VP->EngineShowFlags.SetLighting(false);
				VP->EngineShowFlags.SetPostProcessing(false);
				VP->EngineShowFlags.SetDynamicShadows(false);
				VP->EngineShowFlags.SetGlobalIllumination(false);
				VP->EngineShowFlags.SetLumenGlobalIllumination(false);
				VP->EngineShowFlags.SetLumenReflections(false);
				VP->EngineShowFlags.SetStaticMeshes(false);
				VP->EngineShowFlags.SetNaniteMeshes(false);
				VP->EngineShowFlags.SetInstancedStaticMeshes(false);
				VP->EngineShowFlags.SetLandscape(false);
				VP->EngineShowFlags.SetParticles(false);
				VP->EngineShowFlags.SetTranslucency(false);
				VP->EngineShowFlags.SetVolumetricFog(false);
				VP->EngineShowFlags.SetAtmosphere(false);
				VP->EngineShowFlags.SetFog(false);
			}
			ABwayGameState* GS = World->GetGameState<ABwayGameState>();
			if (!GS) continue;
			if (bClient)
			{
				APlayerController* PC = World->GetFirstPlayerController();
				if (!PC || !PC->PlayerState) continue;
				UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: StartupConnected role=client"));
				return false;
			}
			if (World->GetMapName().Contains(TEXT("HeroSelect_Staging")))
			{
				if (*Selected || GS->PlayerArray.Num() != 4 || !GS->HeroSelectionManager || !GS->HeroSelectionManager->IsSelectionActive()) continue;
				TArray<ABwayPlayerState*> Roster;
				for (APlayerState* P : GS->PlayerArray) if (ABwayPlayerState* B = Cast<ABwayPlayerState>(P); B && !B->IsABot()) Roster.Add(B);
				if (Roster.Num() != 4) continue;
				Roster.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
				*Selected = true;
				for (int32 Index = 0; Index < 4; ++Index)
				{
					const auto Hero = UBwayHeroRegistry::ResolveHeroIdByName(Heroes[Index]);
					if (!GS->HeroSelectionManager->AssignHeroToPlayer(Roster[Index], Hero, true) || Roster[Index]->GetSelectedHeroId() != Hero)
					{ UE_LOG(LogTemp, Error, TEXT("BwayAllKitCooldown: FAIL hero-selection")); return false; }
					UE_LOG(LogTemp, Display, TEXT("BwayAllKitCooldown: Selected hero=%s player=%d"), Heroes[Index], Roster[Index]->GetPlayerId());
				}
			}
			else if (World->GetMapName().Contains(TEXT("L_BW_Dorado")) && Players(World).Num() == 4)
			{
				for (TActorIterator<ABwayAllKitCooldownProbe> It(World); It; ++It) return false;
				World->SpawnActor<ABwayAllKitCooldownProbe>();
				return false;
			}
		}
		return true;
	}), 0.1f);
}
FAutoConsoleCommand Host(TEXT("bway.Test.AllKitCooldownHost"), TEXT("Development owned four-process mixed-roster actual cooldown/HUD lifecycle fixture."), FConsoleCommandDelegate::CreateLambda([] { Start(false); }));
FAutoConsoleCommand Client(TEXT("bway.Test.AllKitCooldownClient"), TEXT("Development mixed-roster cooldown client readiness; replicated probe observes real local abilities."), FConsoleCommandDelegate::CreateLambda([] { Start(true); }));
}
#endif
