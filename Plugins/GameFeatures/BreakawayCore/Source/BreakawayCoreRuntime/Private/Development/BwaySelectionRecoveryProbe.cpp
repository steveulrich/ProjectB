// Development-only packaged hero-selection travel recovery fixture.
// No production behavior or assets are changed.
//
// Four rendered packaged processes, -culture=en, -PktLag=200. Commands:
//   bway.Test.SelectionRecoveryHost <attempt-token>
//   bway.Test.SelectionRecoveryClient <1|2|3> <attempt-token>
// Every process receives -BwayRecoveryControlFile="<absolute>/<token>.retry".
// The runner must reject existing files/logs and write the control file atomically
// ONLY after four FailureHeld markers and three RemoteDenied markers. Payload:
// ProjectB.SelectionRecovery.v1
// attempt=<token>
// action=retry
// host=<host-player-id>
// roster=<ascending-id>=<hero-id>|<id>=<hero-id>|<id>=<hero-id>|<id>=<hero-id>
// After all four GameplayVerified markers and matching final rows, atomically
// replace action=retry with action=exit. No process exits before this barrier.
// Native code reads/consumes this instruction once; runner owns file cleanup.
// All gameplay/UI mutations are confined to this disposable development fixture.

#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if !UE_BUILD_SHIPPING && DO_ENABLE_NET_TEST
namespace BwaySelectionRecovery
{
constexpr double TimeoutSeconds = 240.0;
constexpr double FailureHoldSeconds = 65.0;
constexpr int32 ExpectedPlayers = 4;
bool bRunning = false;

struct FRun
{
	int32 Role = 0;
	int32 LocalId = INDEX_NONE;
	int32 ControlHostId = INDEX_NONE;
	uint32 StagingWorldId = 0;
	uint32 ArenaWorldId = 0;
	int32 ArenaArrivals = 0;
	int32 HoldSamples = 0;
	double Started = FPlatformTime::Seconds();
	double HoldStarted = 0.0;
	double StableSince = 0.0;
	FString Token;
	FString ControlFile;
	FString OriginalOptions;
	FString RejectedOptions;
	FString LastWait;
	TMap<int32, FPrimaryAssetId> Original;
	TWeakObjectPtr<AGameModeBase> OriginalMode; // Cleanup only; never a strong travel reference.
	bool bOptionsInjected = false;
	bool bOptionsRestored = false;
	bool bFailureHeld = false;
	bool bRemoteDenied = false;
	bool bRetryInvoked = false;
	bool bRetryAccepted = false;
	bool bRetryReleased = false;
	bool bExitReleased = false;
	bool bGameplayVerified = false;
};

bool RestoreOptions(FRun& Run)
{
	if (!Run.bOptionsInjected || Run.bOptionsRestored) return true;
	auto* Mode = Run.OriginalMode.Get();
	UWorld* World = Mode ? Mode->GetWorld() : nullptr;
	if (!Mode || !World || World->bIsTearingDown || World->GetUniqueID() != Run.StagingWorldId) return false;
	Mode->OptionsString = Run.OriginalOptions;
	Run.bOptionsRestored = Mode->OptionsString == Run.OriginalOptions;
	UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: OptionsRestored role=%d token=%s exact=%d"), Run.Role, *Run.Token, Run.bOptionsRestored);
	return Run.bOptionsRestored;
}

bool Stop(FRun& Run, const TCHAR* Reason)
{
	const bool bRestored = RestoreOptions(Run);
	bRunning = false;
	if (Reason || !bRestored)
	{
		UE_LOG(LogTemp, Error, TEXT("BwaySelectionRecovery: FAIL role=%d token=%s reason=%s optionsRestored=%d observerCleanup=1 elapsed=%.2f"),
			Run.Role, *Run.Token, Reason ? Reason : TEXT("options-restore-failed"), bRestored, FPlatformTime::Seconds() - Run.Started);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: PASS role=%d token=%s player=%d peers=4 arenaArrivals=%d failureHeld=1 remoteDenied=%d retryInvoked=%d retryAccepted=%d optionsRestored=%d exitReleased=%d observerCleanup=1 physicalInput=0"),
			Run.Role, *Run.Token, Run.LocalId, Run.ArenaArrivals, Run.bRemoteDenied, Run.bRetryInvoked, Run.bRetryAccepted, bRestored, Run.bExitReleased);
	}
	return false; // Sole FTSTicker callback releases the scalar/weak state.
}

bool Wait(FRun& Run, const TCHAR* Reason)
{
	Run.StableSince = 0.0;
	if (Run.LastWait != Reason)
	{
		Run.LastWait = Reason;
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: Wait role=%d token=%s reason=%s elapsed=%.2f"),
			Run.Role, *Run.Token, Reason, FPlatformTime::Seconds() - Run.Started);
	}
	return true;
}

TArray<ABwayPlayerState*> Humans(const ABwayGameState* GS)
{
	TArray<ABwayPlayerState*> Out;
	for (APlayerState* Player : GS->PlayerArray)
	{
		auto* PS = Cast<ABwayPlayerState>(Player);
		if (IsValid(PS) && !PS->IsABot() && !PS->IsInactive() && !PS->IsOnlyASpectator()) Out.Add(PS);
	}
	return Out;
}

bool ValidLocked(const ABwayPlayerState* PS)
{
	return PS && PS->IsHeroLocked() && UBwayHeroRegistry::GetHeroDataById(PS->GetSelectedHeroId());
}

FString RosterSignature(const FRun& Run)
{
	TArray<int32> Ids;
	Run.Original.GetKeys(Ids);
	Ids.Sort();
	TArray<FString> Rows;
	for (const int32 Id : Ids) Rows.Add(FString::Printf(TEXT("%d=%s"), Id, *Run.Original.FindChecked(Id).ToString()));
	return FString::Join(Rows, TEXT("|"));
}

enum class EControl { Absent, Retry, Exit, Invalid };
EControl ReadControl(FRun& Run)
{
	if (!IFileManager::Get().FileExists(*Run.ControlFile)) return EControl::Absent;
	const int64 Size = IFileManager::Get().FileSize(*Run.ControlFile);
	FString Text;
	if (Size <= 0 || Size > 4096 || !FFileHelper::LoadFileToString(Text, *Run.ControlFile)) return EControl::Invalid;
	Text.ReplaceInline(TEXT("\r"), TEXT(""));
	Text.TrimStartAndEndInline();
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	if (Lines.Num() != 5 || !Lines[3].StartsWith(TEXT("host="))) return EControl::Invalid;
	const FString HostText = Lines[3].RightChop(5);
	if (HostText.IsEmpty() || HostText.Len() > 10) return EControl::Invalid;
	for (const TCHAR Ch : HostText) if (Ch < '0' || Ch > '9') return EControl::Invalid;
	const int64 HostWide = FCString::Atoi64(*HostText);
	if (HostWide > MAX_int32) return EControl::Invalid;
	const int32 Host = static_cast<int32>(HostWide);
	if (!Run.Original.Contains(Host) || (Run.Role == 0 ? Host != Run.LocalId : Host == Run.LocalId)
		|| (Run.ControlHostId != INDEX_NONE && Host != Run.ControlHostId)) return EControl::Invalid;
	const FString Prefix = FString::Printf(TEXT("ProjectB.SelectionRecovery.v1\nattempt=%s\naction="), *Run.Token);
	const FString Suffix = FString::Printf(TEXT("\nhost=%d\nroster=%s"), Host, *RosterSignature(Run));
	const EControl Kind = Text == Prefix + TEXT("retry") + Suffix ? EControl::Retry
		: Text == Prefix + TEXT("exit") + Suffix ? EControl::Exit : EControl::Invalid;
	if (Kind == EControl::Retry || Kind == EControl::Exit) Run.ControlHostId = Host;
	return Kind;
}

bool SameLocks(const FRun& Run, const TArray<ABwayPlayerState*>& Roster, bool bPawnless)
{
	if (Run.Original.Num() != ExpectedPlayers || Roster.Num() != ExpectedPlayers) return false;
	TSet<int32> Seen;
	for (const auto* PS : Roster)
	{
		const auto* Hero = Run.Original.Find(PS->GetPlayerId());
		if (!Hero || Seen.Contains(PS->GetPlayerId()) || !ValidLocked(PS) || PS->GetSelectedHeroId() != *Hero || (bPawnless && PS->GetPawn())) return false;
		Seen.Add(PS->GetPlayerId());
	}
	return Seen.Num() == ExpectedPlayers;
}

// Includes both panel ancestry and foreign UserWidget roots (button label trees).
bool EffectivelyVisible(const UWidget* Widget)
{
	TSet<const UWidget*> Seen;
	for (const UWidget* Current = Widget; Current;)
	{
		if (Seen.Num() >= 64 || Seen.Contains(Current) || !Current->IsRendered()) return false;
		Seen.Add(Current);
		Current = Current->GetParent() ? static_cast<const UWidget*>(Current->GetParent()) : Current->GetTypedOuter<UUserWidget>();
	}
	return !Seen.IsEmpty();
}

bool EffectivelyEnabled(const UWidget* Widget)
{
	TSet<const UWidget*> Seen;
	for (const UWidget* Current = Widget; Current;)
	{
		if (Seen.Num() >= 64 || Seen.Contains(Current) || !Current->GetIsEnabled()) return false;
		Seen.Add(Current);
		Current = Current->GetParent() ? static_cast<const UWidget*>(Current->GetParent()) : Current->GetTypedOuter<UUserWidget>();
	}
	return !Seen.IsEmpty();
}

UBwayHeroSelectWidget* ActiveSelection(UWorld* World, ABwayPlayerController* PC, int32& Count)
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UBwayHeroSelectWidget::StaticClass(), false);
	UBwayHeroSelectWidget* Result = nullptr;
	Count = 0;
	for (auto* Candidate : Widgets)
	{
		auto* Widget = Cast<UBwayHeroSelectWidget>(Candidate);
		if (Widget && Widget->GetWorld() == World && Widget->GetOwningPlayer() == PC && Widget->IsActivated())
		{
			++Count;
			Result = Widget;
		}
	}
	return Result;
}

bool FailureUI(UWorld* World, ABwayPlayerController* PC, bool bHost, FString& Label, FString& Details)
{
	int32 Count = 0;
	auto* Widget = ActiveSelection(World, PC, Count);
	if (Count != 1 || !Widget || !EffectivelyVisible(Widget)) return false;
	auto* Button = Cast<UCommonButtonBase>(Widget->GetWidgetFromName(TEXT("Button_Lock")));
	auto* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_HeroDetails")));
	if (!Button || !Text || !Button->WidgetTree || !EffectivelyVisible(Button) || !EffectivelyVisible(Text)
		|| Button->GetIsEnabled() != bHost || EffectivelyEnabled(Button) != bHost || (bHost && !Button->IsInteractionEnabled())) return false;
	TArray<FString> Labels;
	Button->WidgetTree->ForEachWidgetAndDescendants([&Labels](UWidget* Child)
	{
		if (const auto* Block = Cast<UTextBlock>(Child); Block && EffectivelyVisible(Block))
		{
			const FString Value = Block->GetText().ToString();
			if (!Value.TrimStartAndEnd().IsEmpty()) Labels.Add(Value);
		}
	});
	if (Labels.Num() != 1) return false;
	Label = Labels[0];
	Details = Text->GetText().ToString();
	return Label == (bHost ? TEXT("Retry travel") : TEXT("Waiting for host")) && Details.Contains(TEXT("Match travel failed."));
}

bool Arm(FRun& Run, UWorld* World, ABwayGameState* GS, const TArray<ABwayPlayerState*>& Roster)
{
	auto* Manager = GS->HeroSelectionManager.Get();
	auto* Phase = GS->HeroSelectionPhaseComponent.Get();
	auto* Mode = World->GetAuthGameMode();
	if (!Mode || !Manager || !Phase || !Phase->IsHeroSelectionActive() || !Manager->IsSelectionActive() || Manager->GetTotalPlayers() != ExpectedPlayers || Roster.Num() != ExpectedPlayers || GS->PlayerArray.Num() != ExpectedPlayers)
		return Wait(Run, TEXT("four-registered-staging-humans"));
	if (Manager->GetSelectionTimeRemaining() < 20.f) return Stop(Run, TEXT("initial-selection-window-too-short"));
	for (auto* PS : Roster)
	{
		const auto* PC = PS->GetPlayerController();
		if (!PC || (!PC->IsLocalController() && !PC->GetNetConnection())) return Wait(Run, TEXT("four-owner-connections"));
		if (PS->IsHeroLocked() || PS->GetPawn()) return Stop(Run, TEXT("initial-roster-already-locked-or-spawned"));
	}
	Run.OriginalMode = Mode;
	Run.OriginalOptions = Mode->OptionsString;
	TArray<FString> Fields;
	Run.OriginalOptions.ParseIntoArray(Fields, TEXT("?"), false);
	int32 Targets = 0;
	for (FString& Field : Fields)
	{
		if (Field.StartsWith(TEXT("HeroSelectTargetMap="), ESearchCase::IgnoreCase))
		{
			++Targets;
			Field = TEXT("HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado%RecoveryReject");
		}
	}
	if (Targets != 1) return Stop(Run, TEXT("requires-one-explicit-target-map"));
	Run.RejectedOptions = FString::Join(Fields, TEXT("?"));
	UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: OptionsBackup role=0 token=%s original=\"%s\" rejected=\"%s\""), *Run.Token, *Run.OriginalOptions, *Run.RejectedOptions);
	Run.bOptionsInjected = true; // Cleanup owns restoration before the mutation.
	Mode->OptionsString = Run.RejectedOptions;
	if (Mode->OptionsString != Run.RejectedOptions) return Stop(Run, TEXT("rejected-options-readback-mismatch"));
	Manager->ResolveMissingSelectionsAndLockAll(Phase->ResolveFallbackHeroId());
	for (auto* PS : Roster)
	{
		const auto Selection = Manager->GetPlayerSelection(PS);
		if (!ValidLocked(PS) || !Selection.bIsLocked || Selection.SelectedHeroId != PS->GetSelectedHeroId()) return Stop(Run, TEXT("production-lock-failed"));
		Run.Original.Add(PS->GetPlayerId(), PS->GetSelectedHeroId());
	}
	if (Run.Original.Num() != ExpectedPlayers) return Stop(Run, TEXT("duplicate-player-id"));
	UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: Armed role=0 token=%s host=%d roster=%s selectionRemaining=%.2f syntheticSetup=1"),
		*Run.Token, Run.LocalId, *RosterSignature(Run), Manager->GetSelectionTimeRemaining());
	return true;
}

bool ObserveFailure(FRun& Run, UWorld* World, ABwayGameState* GS, ABwayPlayerController* PC)
{
	// The runner releases only after every peer held the unchanged failure. A
	// client's old world can empty during accepted travel; do not judge teardown
	// as a changed pre-retry roster. Arena verification still requires full proof.
	if (Run.bFailureHeld && ((Run.Role == 0 && Run.bRetryAccepted)
		|| (Run.Role != 0 && Run.bRetryReleased))) return Wait(Run, TEXT("released-travel-in-flight"));
	const auto Roster = Humans(GS);
	auto* Manager = GS->HeroSelectionManager.Get();
	auto* Phase = GS->HeroSelectionPhaseComponent.Get();
	if (!Manager || !Phase) return Wait(Run, TEXT("selection-components"));
	if (Run.Role == 0 && !Run.bOptionsInjected) return Arm(Run, World, GS, Roster);
	if (Run.Original.IsEmpty())
	{
		if (Roster.Num() != ExpectedPlayers) return Wait(Run, TEXT("four-replicated-players"));
		for (const auto* PS : Roster) if (!ValidLocked(PS) || PS->GetPawn()) return Wait(Run, TEXT("four-coherent-replicated-locks"));
		for (const auto* PS : Roster) Run.Original.Add(PS->GetPlayerId(), PS->GetSelectedHeroId());
		if (Run.Original.Num() != ExpectedPlayers) return Stop(Run, TEXT("duplicate-replicated-player-id"));
	}
	if (!SameLocks(Run, Roster, true)) return Stop(Run, TEXT("failure-lock-or-pawnless-state-changed"));
	if (!Phase->HasPostSelectionTravelFailed())
	{
		if (Run.HoldStarted > 0.0) return Stop(Run, TEXT("failure-latch-cleared-before-release"));
		return Wait(Run, TEXT("replicated-travel-rejection"));
	}
	if (Run.Role == 0 && (!Phase->IsHeroSelectionActive() || !Manager->IsSelectionActive() || !World->NextURL.IsEmpty())) return Stop(Run, TEXT("authority-selection-closed-or-travel-queued"));
	const UNetDriver* Driver = World->GetNetDriver();
	if (!Driver || Driver->PacketSimulationSettings.PktLag != 200) return Stop(Run, TEXT("failure-packet-lag-not-200"));
	const int32 PacketLag = Driver->PacketSimulationSettings.PktLag;
	for (auto* PS : Roster)
	{
		const auto Selection = Manager->GetPlayerSelection(PS);
		if (!Selection.bIsLocked || Selection.SelectedHeroId != PS->GetSelectedHeroId())
		{
			if (Run.HoldStarted > 0.0) return Stop(Run, TEXT("manager-projection-changed-during-hold"));
			return Wait(Run, TEXT("manager-projection-settle"));
		}
	}
	FString Label, Details;
	if (!FailureUI(World, PC, Run.Role == 0, Label, Details))
	{
		if (Run.HoldStarted > 0.0) return Stop(Run, TEXT("visible-recovery-control-regressed"));
		return Wait(Run, TEXT("visible-owned-recovery-controls"));
	}
	if (PC->CanRetryHeroSelectionTravel() != (Run.Role == 0)) return Stop(Run, TEXT("retry-authority-mismatch"));
	if (Run.Role != 0 && !Run.bRemoteDenied)
	{
		const bool bCanRetry = PC->CanRetryHeroSelectionTravel();
		const bool bAccepted = PC->RetryHeroSelectionTravel();
		if (bCanRetry || bAccepted) return Stop(Run, TEXT("remote-retry-authorized"));
		Run.bRemoteDenied = true;
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: RemoteDenied role=%d token=%s player=%d canRetry=0 accepted=0"), Run.Role, *Run.Token, Run.LocalId);
	}
	const double Now = FPlatformTime::Seconds();
	if (Run.HoldStarted == 0.0)
	{
		Run.HoldStarted = Now;
		for (const auto* PS : Roster)
		{
			UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: FailurePeer role=%d token=%s player=%d hero=%s locked=1 pawn=0"),
				Run.Role, *Run.Token, PS->GetPlayerId(), *PS->GetSelectedHeroId().ToString());
		}
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: FailureObserved role=%d token=%s player=%d roster=%s packetLag=%d failed=1 pawnless=1 activated=1 localOwner=1 widgetVisible=1 buttonVisible=1 detailsVisible=1 buttonEnabled=%d label=\"%s\" details=\"%s\""),
			Run.Role, *Run.Token, Run.LocalId, *RosterSignature(Run), PacketLag, Run.Role == 0, *Label, *Details);
	}
	++Run.HoldSamples;
	if (!Run.bFailureHeld && Now - Run.HoldStarted >= FailureHoldSeconds)
	{
		if (Run.Role == 0 && Manager->GetSelectionTimeRemaining() > 0.f) return Stop(Run, TEXT("global-selection-timeout-not-reached"));
		Run.bFailureHeld = true;
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: FailureHeld role=%d token=%s player=%d seconds=%.2f samples=%d roster=%s packetLag=%d failed=1 pawnless=1 visibleControls=1 selectionRemaining=%.2f"),
			Run.Role, *Run.Token, Run.LocalId, Now - Run.HoldStarted, Run.HoldSamples, *RosterSignature(Run), PacketLag, Manager->GetSelectionTimeRemaining());
	}
	if (Run.Role != 0 || !Run.bFailureHeld || Run.bRetryInvoked || !Run.bRetryReleased) return true;
	if (!RestoreOptions(Run)) return Stop(Run, TEXT("options-restore-before-retry-failed"));
	bool bScriptGuard = false;
#if WITH_EDITOR
	bScriptGuard = GAllowActorScriptExecutionInEditor;
#endif
	if (bScriptGuard) return Stop(Run, TEXT("editor-script-guard-active"));
	if (!PC->CanRetryHeroSelectionTravel()) return Stop(Run, TEXT("host-no-longer-eligible"));
	Run.bRetryInvoked = true;
	UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: RetryIntent role=0 token=%s player=%d controlConsumed=1 optionsRestored=1 scriptGuard=0"), *Run.Token, Run.LocalId);
	// Do not dereference the prior World/PC/GS after crossing the travel boundary.
	Run.bRetryAccepted = PC->RetryHeroSelectionTravel();
	UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: RetryResult role=0 token=%s accepted=%d"), *Run.Token, Run.bRetryAccepted);
	return Run.bRetryAccepted ? true : Stop(Run, TEXT("explicit-host-retry-rejected"));
}

bool ObserveArena(FRun& Run, UWorld* World, ABwayGameState* GS, ABwayPlayerController* PC)
{
	if (!Run.bFailureHeld || (Run.Role == 0 ? !Run.bRetryAccepted || !Run.bOptionsRestored : !Run.bRemoteDenied)) return Stop(Run, TEXT("arena-before-required-recovery-evidence"));
	if (Run.ArenaWorldId != World->GetUniqueID())
	{
		Run.ArenaWorldId = World->GetUniqueID();
		++Run.ArenaArrivals;
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: ArenaArrival role=%d token=%s count=%d"), Run.Role, *Run.Token, Run.ArenaArrivals);
	}
	if (Run.ArenaArrivals != 1) return Stop(Run, TEXT("duplicate-arena-arrival"));
	if (!GS->GetRoundManagement() || GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return Wait(Run, TEXT("playing"));
	const auto Roster = Humans(GS);
	if (!SameLocks(Run, Roster, false)) return Wait(Run, TEXT("four-gameplay-hero-locks"));
	for (const auto* PS : Roster)
	{
		const auto* Pawn = Cast<ABwayCharacterWithAbilities>(PS->GetPawn());
		if (!Pawn || Pawn->GetPlayerState() != PS || !Pawn->HeroDataAsset || Pawn->HeroDataAsset->GetPrimaryAssetId() != PS->GetSelectedHeroId()) return Wait(Run, TEXT("four-correct-hero-pawns"));
		if (Run.Role == 0 && (!PS->GetPlayerController() || PS->GetPlayerController()->GetPawn() != Pawn)) return Wait(Run, TEXT("four-authority-possessions"));
	}
	if (!PC->GetPawn() || PC->GetPawn()->GetPlayerState() != PC->PlayerState || PC->PlayerState->GetPlayerId() != Run.LocalId) return Wait(Run, TEXT("local-possession"));
	if (!GS->HeroSelectionPhaseComponent || GS->HeroSelectionPhaseComponent->HasPostSelectionTravelFailed()) return Wait(Run, TEXT("cleared-gameplay-failure-state"));
	int32 WidgetCount = 0;
	ActiveSelection(World, PC, WidgetCount);
	if (WidgetCount != 0) return Wait(Run, TEXT("recovery-widget-dismissed"));
	const double Now = FPlatformTime::Seconds();
	if (Run.StableSince == 0.0) Run.StableSince = Now;
	if (Now - Run.StableSince < 1.0) return true;
	for (const auto* PS : Roster)
	{
		const auto* Pawn = CastChecked<ABwayCharacterWithAbilities>(PS->GetPawn());
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: PeerFinal role=%d token=%s player=%d hero=%s pawnHero=%s locked=1 pawn=1 playing=1"),
			Run.Role, *Run.Token, PS->GetPlayerId(), *PS->GetSelectedHeroId().ToString(), *Pawn->HeroDataAsset->GetPrimaryAssetId().ToString());
	}
	Run.bGameplayVerified = true;
	UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: GameplayVerified role=%d token=%s local=%d peers=4 failureCleared=1 widgetDismissed=1"), Run.Role, *Run.Token, Run.LocalId);
	return true;
}

bool Tick(FRun& Run)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - Run.Started > TimeoutSeconds) return Stop(Run, TEXT("observer-timeout"));
	const EControl Control = ReadControl(Run);
	if (Control == EControl::Invalid) return Stop(Run, TEXT("control-token-or-roster-mismatch"));
	if (Control != EControl::Absent && !Run.bFailureHeld) return Stop(Run, TEXT("control-before-local-failure-hold"));
	if (Control == EControl::Retry) Run.bRetryReleased = true;
	if (Control == EControl::Exit && (!Run.bGameplayVerified || !Run.bRetryReleased)) return Stop(Run, TEXT("exit-before-local-gameplay-proof"));
	if (Run.bGameplayVerified)
	{
		if (Control != EControl::Exit) return Wait(Run, TEXT("all-peer-exit-release"));
		Run.bExitReleased = true;
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: ExitRelease role=%d token=%s host=%d roster=%s allPeerBarrier=1"), Run.Role, *Run.Token, Run.ControlHostId, *RosterSignature(Run));
		const bool bContinue = Stop(Run, nullptr);
		if (Run.Role != 0) FPlatformMisc::RequestExit(false);
		return bContinue;
	}
	UWorld* SelectedWorld = nullptr;
	int32 Count = 0;
	if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType != EWorldType::Game || !World || World->bIsTearingDown || World->GetNetMode() != (Run.Role == 0 ? NM_ListenServer : NM_Client)) continue;
		SelectedWorld = World;
		++Count;
	}
	if (Count > 1) return Stop(Run, TEXT("multiple-matching-game-worlds"));
	if (!SelectedWorld) return Wait(Run, TEXT("owned-game-world"));
	auto* GS = SelectedWorld->GetGameState<ABwayGameState>();
	auto* PC = Cast<ABwayPlayerController>(SelectedWorld->GetFirstPlayerController());
	if (!GS || !PC || !PC->IsLocalController() || !PC->GetLocalPlayer() || !PC->GetPlayerState<ABwayPlayerState>()) return Wait(Run, TEXT("local-owner-state"));
	if (Run.LocalId == INDEX_NONE)
	{
		const auto* LocalPS = PC->GetPlayerState<ABwayPlayerState>();
		const int32 CandidateId = LocalPS->GetPlayerId();
		// PlayerState can resolve before its PlayerId arrives. This project uses
		// GameSession's positive IDs (starting at256); zero is the CDO default.
		if (CandidateId <= 0) return Wait(Run, TEXT("replicated-local-player-id"));
		int32 IdMatches = 0;
		int32 PointerMatches = 0;
		for (const auto* PS : Humans(GS))
		{
			IdMatches += PS->GetPlayerId() == CandidateId ? 1 : 0;
			PointerMatches += PS == LocalPS ? 1 : 0;
		}
		if (IdMatches != 1 || PointerMatches != 1) return Wait(Run, TEXT("unique-local-roster-identity"));
		Run.LocalId = CandidateId;
		UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: StartupConnected role=%d token=%s player=%d netMode=%d"), Run.Role, *Run.Token, Run.LocalId, int32(SelectedWorld->GetNetMode()));
	}
	if (SelectedWorld->GetMapName().Contains(TEXT("HeroSelect_Staging")))
	{
		if (Run.StagingWorldId == 0) Run.StagingWorldId = SelectedWorld->GetUniqueID();
		if (Run.StagingWorldId != SelectedWorld->GetUniqueID()) return Stop(Run, TEXT("staging-world-replaced-during-hold"));
		return ObserveFailure(Run, SelectedWorld, GS, PC);
	}
	if (SelectedWorld->GetMapName().Contains(TEXT("L_BW_Dorado"))) return ObserveArena(Run, SelectedWorld, GS, PC);
	return Wait(Run, TEXT("expected-map"));
}

void Start(const TArray<FString>& Args, bool bHost)
{
	const int32 ExpectedArgs = bHost ? 1 : 2;
	const bool bValidRole = bHost || (Args.Num() == 2 && (Args[0] == TEXT("1") || Args[0] == TEXT("2") || Args[0] == TEXT("3")));
	const FString Token = Args.Num() == ExpectedArgs ? Args.Last() : FString();
	bool bValidToken = !Token.IsEmpty() && Token.Len() <= 48;
	for (const TCHAR Ch : Token) if (!((Ch >= 'a' && Ch <= 'z') || (Ch >= '0' && Ch <= '9') || Ch == '-')) bValidToken = false;
	FString ControlFile;
	const bool bHasControl = FParse::Value(FCommandLine::Get(), TEXT("BwayRecoveryControlFile="), ControlFile);
	if (bRunning || !bValidRole || !bValidToken || !bHasControl || FPaths::IsRelative(ControlFile)
		|| FPaths::GetCleanFilename(ControlFile) != Token + TEXT(".retry") || IFileManager::Get().FileExists(*ControlFile))
	{
		UE_LOG(LogTemp, Error, TEXT("BwaySelectionRecovery: FAIL category=setup reason=invalid-or-stale-start-contract observerCreated=0 existingObserver=%d"), bRunning);
		return;
	}
	const auto Run = MakeShared<FRun>();
	Run->Role = bHost ? 0 : FCString::Atoi(*Args[0]);
	Run->Token = Token;
	Run->ControlFile = ControlFile;
	bRunning = true;
	UE_LOG(LogTemp, Display, TEXT("BwaySelectionRecovery: Start role=%d token=%s timeout=240 holdSeconds=65 renderedRequired=1 expectedCulture=en physicalInput=0 control=\"%s\""), Run->Role, *Run->Token, *Run->ControlFile);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float) { return Tick(*Run); }), 0.1f);
}

FAutoConsoleCommand HostCommand(TEXT("bway.Test.SelectionRecoveryHost"), TEXT("Development packaged rejected-travel recovery; one attempt token and -BwayRecoveryControlFile are required."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { Start(Args, true); }));
FAutoConsoleCommand ClientCommand(TEXT("bway.Test.SelectionRecoveryClient"), TEXT("Development packaged recovery observer; role 1/2/3 and attempt token, plus -BwayRecoveryControlFile."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { Start(Args, false); }));
}
#endif
