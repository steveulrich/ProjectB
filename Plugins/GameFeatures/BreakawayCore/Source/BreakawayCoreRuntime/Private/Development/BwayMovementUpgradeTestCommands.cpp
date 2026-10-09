#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Containers/Ticker.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputKeyEventArgs.h"

namespace BwayMovementUpgradeTest
{
const FName UpgradeId(TEXT("MoveSpeed"));
struct FProbe
{
	TWeakObjectPtr<ABwayPlayerController> Controller;
	double Started = FPlatformTime::Seconds();
	double Changed = 0;
	double StableSince = 0;
	int32 Step = 0;
	bool bFunded = false;
	bool bInputCaptured = false;
	ECommonInputType OriginalInput = ECommonInputType::MouseAndKeyboard;
	float BeforeGold = 0;
	float Income = 0;
	float Bonus = 0;
	int32 Cost = 0;
	FVector StartLocation = FVector::ZeroVector;
};
TSharedPtr<FProbe> Active;

void MovementKey(ABwayPlayerController* PC, EInputEvent Event)
{
	if (!PC || !PC->GetLocalPlayer()) return;
	if (Event == IE_Pressed)
	{
		if (UCommonInputSubsystem* Common = PC->GetLocalPlayer()->GetSubsystem<UCommonInputSubsystem>()) Common->SetCurrentInputType(ECommonInputType::MouseAndKeyboard);
	}
	const FInputDeviceId Device = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetLocalPlayer()->GetPlatformUserId());
	PC->InputKey(FInputKeyEventArgs(nullptr, Device, EKeys::W, Event, FPlatformTime::Cycles64()));
}

bool Finish(const TCHAR* Error)
{
	if (Active && Active->Controller.IsValid())
	{
		ABwayPlayerController* PC = Active->Controller.Get();
		MovementKey(PC, IE_Released);
		if (Active->bInputCaptured && PC->GetLocalPlayer())
		{
			if (UCommonInputSubsystem* Common = PC->GetLocalPlayer()->GetSubsystem<UCommonInputSubsystem>()) Common->SetCurrentInputType(Active->OriginalInput);
		}
	}
	if (Error)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayMovementUpgradeTest: FAIL step=%d reason=%s"), Active ? Active->Step : -1, Error);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BwayMovementUpgradeTest: PASS peers=4 scope=replicated-speed-limit-native-movement-input"));
	}
	Active.Reset();
	return false;
}

bool Tick(float)
{
	if (!Active) return false;
	FProbe& P = *Active;
	const double Now = FPlatformTime::Seconds();
	if (Now - P.Started > 60.0) return Finish(TEXT("timeout"));
	UWorld* World = P.Controller.IsValid() ? P.Controller->GetWorld() : nullptr;
	if (!World)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (!Candidate || !Candidate->IsGameWorld()) continue;
			if (Candidate->WorldType == EWorldType::PIE) return Finish(TEXT("requires four separate packaged processes"));
			const ABwayGameState* GS = Candidate->GetGameState<ABwayGameState>();
			if (!GS || !GS->GetRoundManagement() || GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) continue;
			for (TActorIterator<ABwayPlayerController> It(Candidate); It; ++It)
			{
				if (It->IsLocalController() && It->GetLocalPlayer() && It->GetPawn()) { P.Controller = *It; World = Candidate; break; }
			}
		}
		if (!World) return true;
	}
	ABwayPlayerController* PC = P.Controller.Get();
	ABwayPlayerState* Owner = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	UBwayUpgradeComponent* Shop = Owner ? Owner->UpgradeComponent : nullptr;
	const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
	const FBwayUpgradeDefinition* Definition = Catalog ? Catalog->FindUpgrade(UpgradeId) : nullptr;
	ULyraAbilitySystemComponent* OwnerASC = Owner ? Owner->GetLyraAbilitySystemComponent() : nullptr;
	const UBwayGoldAttributeSet* Gold = OwnerASC ? OwnerASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	if (!Definition || Definition->Ranks.IsEmpty() || !Gold) return true;
	TArray<ABwayPlayerState*> Players;
	TSet<int32> PlayerIds;
	for (TActorIterator<ABwayPlayerState> It(World); It; ++It)
	{
		ABwayCharacterWithAbilities* Pawn = Cast<ABwayCharacterWithAbilities>(It->GetPawn());
		const ULyraAbilitySystemComponent* ASC = It->GetLyraAbilitySystemComponent();
		const UBwayCharacterMovementComponent* Movement = Pawn ? Pawn->GetBwayCharacterMovement() : nullptr;
		if (!Movement || Movement->MovementMode != MOVE_Walking || !ASC || !ASC->GetSet<UBwayHeroAttributeSet>()) return true;
		Players.Add(*It);
		PlayerIds.Add(It->GetPlayerId());
	}
	if (Players.Num() != 4 || PlayerIds.Num() != 4) return true;
	if (P.Step == 0)
	{
		P.Cost = Definition->Ranks[0].Cost;
		P.Bonus = Definition->Ranks[0].Magnitude;
		if (P.Cost <= 0 || P.Bonus <= 0 || !FMath::IsFinite(P.Bonus)) return Finish(TEXT("invalid movement rank data"));
		if (World->GetNetMode() != NM_Client && !P.bFunded)
		{
			for (ABwayPlayerState* Player : Players)
			{
				if (!Player->UpgradeComponent || !Player->UpgradeComponent->IsPurchaseWindowOpen() || Player->UpgradeComponent->GetOwnedRank(UpgradeId) != 0) return true;
				ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
				if (!ASC->GetSet<UBwayGoldAttributeSet>() || !FMath::IsNearlyEqual(ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()), 1.0f)) return true;
			}
			for (ABwayPlayerState* Player : Players)
			{
				// Development-only funding in an owned disposable match. Purchases
				// still use the production owner RPC and reversible gameplay effect.
				Player->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), float(P.Cost));
				Player->ForceNetUpdate();
				UE_LOG(LogTemp, Display, TEXT("BwayMovementUpgradeTest: Seed player=%d cost=%d"), Player->GetPlayerId(), P.Cost);
			}
			P.bFunded = true;
		}
		if (Gold->GetCurrentGold() < P.Cost || !Shop->IsPurchaseWindowOpen()) return true;
		if (Shop->GetOwnedRank(UpgradeId) != 0 || !FMath::IsNearlyEqual(OwnerASC->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()), 1.0f)) return Finish(TEXT("owner baseline already upgraded"));
		if (P.Changed == 0) { P.Changed = Now; return true; }
		if (Now - P.Changed < 2.0) return true;
		P.BeforeGold = Gold->GetCurrentGold();
		P.Income = Gold->GetGoldPerSecond();
		Shop->ServerPurchaseUpgrade(UpgradeId, 0);
		UE_LOG(LogTemp, Display, TEXT("BwayMovementUpgradeTest: Submitted player=%d netmode=%d id=MoveSpeed before=%.2f cost=%d bonus=%.4f"), Owner->GetPlayerId(), int32(World->GetNetMode()), P.BeforeGold, P.Cost, P.Bonus);
		P.Step = 1;
		P.Changed = Now;
		return true;
	}
	if (P.Step == 1)
	{
		const float Tolerance = FMath::Max(0.0f, P.Income) * (FMath::CeilToInt(Now - P.Changed) + 2) + 0.01f;
		const float Balance = Gold->GetCurrentGold();
		const bool bBalance = Balance >= P.BeforeGold - P.Cost - 0.01f && Balance <= P.BeforeGold - P.Cost + Tolerance && Balance < P.BeforeGold;
		if (Shop->GetOwnedRank(UpgradeId) != 1 || !bBalance) return true;
		UE_LOG(LogTemp, Display, TEXT("BwayMovementUpgradeTest: Purchase PASS player=%d netmode=%d rank=1 before=%.2f after=%.2f cost=%d tolerance=%.2f"), Owner->GetPlayerId(), int32(World->GetNetMode()), P.BeforeGold, Balance, P.Cost, Tolerance);
		P.Step = 2;
		P.Changed = Now;
	}
	if (P.Step == 2)
	{
		for (ABwayPlayerState* Player : Players)
		{
			const UBwayCharacterMovementComponent* Movement = CastChecked<ABwayCharacterWithAbilities>(Player->GetPawn())->GetBwayCharacterMovement();
			const float Multiplier = Player->GetLyraAbilitySystemComponent()->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute());
			const bool bRank = Player == Owner || World->GetNetMode() != NM_Client ? Player->UpgradeComponent->GetOwnedRank(UpgradeId) == 1 : Player->UpgradeComponent->GetOwnedUpgrades().IsEmpty();
			if (!bRank || !FMath::IsNearlyEqual(Multiplier, 1.0f + P.Bonus, 0.001f)
				|| !FMath::IsNearlyEqual(Movement->GetMaxSpeed(), Movement->MaxWalkSpeed * (1.0f + P.Bonus), 0.01f)) { P.StableSince = 0; return true; }
		}
		if (P.StableSince == 0) P.StableSince = Now;
		if (Now - P.StableSince < 1.0) return true;
		for (ABwayPlayerState* Player : Players)
		{
			const UBwayCharacterMovementComponent* Movement = CastChecked<ABwayCharacterWithAbilities>(Player->GetPawn())->GetBwayCharacterMovement();
			UE_LOG(LogTemp, Display, TEXT("BwayMovementUpgradeTest: Peer PASS player=%d netmode=%d base=%.2f bonus=%.4f multiplier=%.4f cap=%.2f rankVisibility=1"),
				Player->GetPlayerId(), int32(World->GetNetMode()), Movement->MaxWalkSpeed, P.Bonus,
				Player->GetLyraAbilitySystemComponent()->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()), Movement->GetMaxSpeed());
		}
		if (PC->IsMoveInputIgnored() || !PC->GetPawn()) return Finish(TEXT("gameplay movement input is blocked"));
		if (UCommonInputSubsystem* Common = PC->GetLocalPlayer()->GetSubsystem<UCommonInputSubsystem>()) { P.OriginalInput = Common->GetCurrentInputType(); P.bInputCaptured = true; }
		P.StartLocation = PC->GetPawn()->GetActorLocation();
		MovementKey(PC, IE_Pressed);
		P.Step = 3;
		P.Changed = Now;
		return true;
	}
	if (P.Step == 3 && Now - P.Changed >= 0.75)
	{
		MovementKey(PC, IE_Released);
		const float Distance = FVector::Dist2D(P.StartLocation, PC->GetPawn()->GetActorLocation());
		UE_LOG(LogTemp, Display, TEXT("BwayMovementUpgradeTest: Movement player=%d netmode=%d distance=%.2f seconds=%.3f"), Owner->GetPlayerId(), int32(World->GetNetMode()), Distance, Now - P.Changed);
		if (Distance <= 10.0f) return Finish(TEXT("injected movement did not move the upgraded pawn"));
		return Finish(nullptr);
	}
	return true;
}

void Start()
{
	if (Active) return;
	Active = MakeShared<FProbe>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.1f);
}
FAutoConsoleCommand Command(TEXT("bway.Test.MovementUpgrade"), TEXT("Development-only four-process movement upgrade RPC, all-peer speed/owner privacy and injected walking check; use an owned disposable match."), FConsoleCommandDelegate::CreateStatic(&Start));
}
#endif
