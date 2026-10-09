// Development-only observation of local UI across real world/controller replacement.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "UI/BwayRespawnHUDWidget.h"
#include "UObject/UObjectIterator.h"

namespace BwayUIScreenAudit
{
bool Running = false;

struct FObserver
{
	// 0 observes an existing flow; 1 selects normally; 2 holds a gameplay late-join screen.
	int32 Mode = 0;
	int32 Role = 0;
	double Started = FPlatformTime::Seconds();
	double LastSample = 0, StableSince = Started, SelectionSince = 0, LastSubmit = 0, RosterSince = 0, FinishAt = 0;
	uint32 LastWorld = 0, LastController = 0, LastPlayerState = 0;
	bool LateSelectionObserved = false, LateCompleted = false, RosterReported = false;
	TWeakObjectPtr<UGameInstance> Instance;

	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if (FinishAt && Now >= FinishAt)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayUIScreen: ClientComplete role=%d"), Role);
			Running = false;
			FPlatformMisc::RequestExitWithStatus(false, 0);
			return false;
		}
		if (Now - Started > 480.0)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayUIScreen: Stopped reason=declared-timeout mode=%d"), Mode);
			Running = false;
			return false;
		}
		UWorld* World = nullptr;
		if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Candidate && Candidate->WorldType == EWorldType::Game && Candidate->GetGameInstance()
				&& (!Instance.IsValid() || Candidate->GetGameInstance() == Instance.Get()))
			{
				World = Candidate;
				Instance = Candidate->GetGameInstance();
				break;
			}
		}
		APlayerController* PC = World ? World->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		if (!PC || !PC->GetLocalPlayer()) return true;
		auto* BPC = Cast<ABwayPlayerController>(PC);
		auto* PS = PC->GetPlayerState<ABwayPlayerState>();
		auto* GS = World->GetGameState<ABwayGameState>();
		auto* Round = GS ? GS->GetRoundManagement() : nullptr;
		const EBwayMatchPhase Phase = Round ? Round->GetCurrentMatchPhase() : EBwayMatchPhase::Prematch;
		const uint32 PlayerStateId = PS ? PS->GetUniqueID() : 0;
		if (LastWorld != World->GetUniqueID() || LastController != PC->GetUniqueID() || LastPlayerState != PlayerStateId)
		{
			LastWorld = World->GetUniqueID(); LastController = PC->GetUniqueID(); LastPlayerState = PlayerStateId;
			StableSince = Now; SelectionSince = 0; RosterSince = 0; RosterReported = false;
		}
		int32 HUDs = 0, StaleHUDs = 0, Shown = 0, Focusable = 0, Selection = 0;
		for (TObjectIterator<UBwayRespawnHUDWidget> It; It; ++It)
		{
			if (It->IsTemplate() || !It->IsInViewport() || It->GetOwningLocalPlayer() != PC->GetLocalPlayer()) continue;
			if (It->GetOwningPlayer() != PC || It->GetWorld() != World) { ++StaleHUDs; continue; }
			++HUDs; Shown += It->IsVisible(); Focusable += It->IsFocusable();
		}
		for (TObjectIterator<UBwayHeroSelectWidget> It; It; ++It)
			if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated() && It->IsVisible()) ++Selection;
		const bool Locked = PS && PS->IsHeroLocked();
		const bool Pawn = PC->GetPawn() != nullptr;
		// Baseline setup may begin directly in gameplay, where the host has no selection
		// screen. Only the late-join mode requires an observed, uninterrupted visible UI.
		if (Mode && BPC && PS && GS && !Locked && (Mode == 1 || Selection == 1))
		{
			if (!SelectionSince) SelectionSince = Now;
			const bool Late = Mode == 2 && Phase == EBwayMatchPhase::Playing;
			if (Late && Now - SelectionSince >= 3.0 && !LateSelectionObserved)
			{
				LateSelectionObserved = true;
				UE_LOG(LogTemp, Display, TEXT("BwayUIScreen: UnlockedLateSelection role=%d player=%d seconds=%.2f selection=%d hud=%d shown=%d"), Role, PS->GetPlayerId(), Now - SelectionSince, Selection, HUDs, Shown);
			}
			if ((Mode == 1 || LateSelectionObserved) && Now - SelectionSince >= 0.5 && Now - LastSubmit >= 2.0)
			{
				UBwayHeroRegistry* Registry = UBwayHeroRegistry::Get(World);
				if (Registry && GS->HeroSelectionManager && GS->GetPlayerTeam(PS) >= 0)
				{
					TArray<FPrimaryAssetId> Heroes;
					for (const auto& Soft : Registry->GetAllHeroSoftObjects())
						if (auto* Data = Registry->LoadHeroSync(Soft)) Heroes.AddUnique(Data->GetPrimaryAssetId());
					Heroes.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
					for (int32 Offset = 0; Offset < Heroes.Num(); ++Offset)
					{
						const FPrimaryAssetId Hero = Heroes[(Role + Offset) % Heroes.Num()];
						if (!GS->HeroSelectionManager->IsHeroAvailableForTeam(Hero, GS->GetPlayerTeam(PS))) continue;
						BPC->Server_SubmitPreSelectedHero(Hero, true);
						LastSubmit = Now;
						UE_LOG(LogTemp, Display, TEXT("BwayUIScreen: Submit role=%d player=%d hero=%s source=semantic-rpc"), Role, PS->GetPlayerId(), *Hero.ToString());
						break;
					}
				}
			}
		}
		else if (!Locked) SelectionSince = 0;
		if (Mode == 2 && LateSelectionObserved && Locked && Pawn && Selection == 0 && !LateCompleted)
		{
			LateCompleted = true;
			UE_LOG(LogTemp, Display, TEXT("BwayUIScreen: LateComplete role=%d player=%d hud=%d shown=%d stale=%d"), Role, PS->GetPlayerId(), HUDs, Shown, StaleHUDs);
		}
		int32 Humans = 0, HumanLocks = 0, HumanPawns = 0;
		if (GS) for (APlayerState* Peer : GS->PlayerArray)
			if (auto* Player = Cast<ABwayPlayerState>(Peer); Player && !Player->IsABot() && !Player->IsInactive())
			{ ++Humans; HumanLocks += Player->IsHeroLocked(); HumanPawns += Player->GetPawn() != nullptr; }
		if (Mode && Phase == EBwayMatchPhase::Playing && Humans == 4 && HumanLocks == 4 && HumanPawns == 4 && Selection == 0)
		{
			if (!RosterSince) RosterSince = Now;
			if (!RosterReported && Now - RosterSince >= 4.0)
			{
				RosterReported = true;
				UE_LOG(LogTemp, Display, TEXT("BwayUIScreen: RosterReady role=%d humans=4 locked=4 pawns=4 hud=%d shown=%d stale=%d"), Role, HUDs, Shown, StaleHUDs);
				// Keep the connection alive while the other peers collect settled evidence.
				if (World->GetNetMode() == NM_Client && !FinishAt) FinishAt = Now + 20.0;
			}
		}
		else RosterSince = 0;
		if (Now - LastSample >= 0.25)
		{
			LastSample = Now;
			const auto Schedule = PS ? PS->GetRespawnSchedule() : FBwayRespawnSchedule();
			UE_LOG(LogTemp, Display, TEXT("BwayUIScreen: Sample role=%d map=%s world=%u pc=%u ps=%u bway=%d stable=%.2f phase=%d round=%d locked=%d pawn=%d hud=%d stale=%d shown=%d focusable=%d selection=%d deadline=%.3f humans=%d humanLocks=%d humanPawns=%d"),
				Role, *World->GetMapName(), LastWorld, LastController, LastPlayerState, BPC != nullptr, Now - StableSince,
				int32(Phase), Round ? Round->GetCurrentRoundNumber() : 0, Locked, Pawn, HUDs, StaleHUDs, Shown, Focusable, Selection, Schedule.ServerEndTime, Humans, HumanLocks, HumanPawns);
		}
		return true;
	}
};

FAutoConsoleCommand Command(TEXT("bway.Test.UIScreenLifecycle"), TEXT("Development: packaged UI observer. Args: mode (0 passive, 1 select, 2 hold late selection) and role (0..3)."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Running) return;
		if (Args.Num() != 2 || !Args[0].IsNumeric() || !Args[1].IsNumeric()) return;
		const int32 Mode = FCString::Atoi(*Args[0]), Role = FCString::Atoi(*Args[1]);
		if (Mode < 0 || Mode > 2 || Role < 0 || Role > 3) return;
		Running = true;
		TSharedRef<FObserver> Observer = MakeShared<FObserver>();
		Observer->Mode = Mode; Observer->Role = Role;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Observer](float Delta) { return Observer->Tick(Delta); }), 0.1f);
	}));
}
#endif
