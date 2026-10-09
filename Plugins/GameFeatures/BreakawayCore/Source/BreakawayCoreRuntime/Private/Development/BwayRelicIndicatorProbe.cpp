#include "Development/BwayRelicIndicatorProbe.h"
#include "Net/UnrealNetwork.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/BwayGameplayAbility_RelicRequest.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/LyraHealthComponent.h"
#include "Components/Image.h"
#include "Components/WidgetComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "LyraGameplayTags.h"
#include "Relic/BwayRelicRequestIndicatorComponent.h"
#include "Relic/RelicSettings.h"

namespace BwayRelicIndicatorTest
{
bool Running = false;
UWorld* World()
{
	if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (UWorld* W = Context.World(); W && W->WorldType == EWorldType::Game && W->GetGameInstance()) return W;
	return nullptr;
}
TArray<ABwayPlayerState*> Players(ABwayGameState* GS)
{
	TArray<ABwayPlayerState*> Result;
	if (GS) for (APlayerState* PS : GS->PlayerArray)
		if (auto* P = Cast<ABwayPlayerState>(PS); P && !P->IsABot() && !P->IsInactive()) Result.Add(P);
	Result.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return Result;
}
ULyraAbilitySystemComponent* ASC(ABwayPlayerState* PS) { return PS ? PS->GetLyraAbilitySystemComponent() : nullptr; }
UWidgetComponent* Widget(APawn* Pawn)
{
	FComponentReference Reference;
	Reference.ComponentProperty = TEXT("RelicRequestWidgetComponent");
	return Pawn ? Cast<UWidgetComponent>(Reference.GetComponent(Pawn)) : nullptr;
}
bool Request(ABwayPlayerState* PS)
{
	if (auto* Component = ASC(PS)) for (FGameplayAbilitySpec& Spec : Component->GetActivatableAbilities())
		if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_RelicRequest>()) return Component->TryActivateAbility(Spec.Handle);
	return false;
}
bool Active(ABwayPlayerState* PS)
{
	const FGameplayTag Tag = UBwayRelicRequestIndicatorComponent::ResolveRequestTag(PS);
	return ASC(PS) && Tag.IsValid() && ASC(PS)->HasMatchingGameplayTag(Tag);
}
FString TextureResource(UUserWidget* UI)
{
	FString Result;
	if (UI && UI->WidgetTree) UI->WidgetTree->ForEachWidget([&Result](UWidget* W)
	{
		if (auto* Image = Cast<UImage>(W))
			if (auto* Texture = Cast<UTexture>(Image->GetBrush().GetResourceObject())) Result = Texture->GetPathName();
	});
	return Result;
}
}
#endif

ABwayRelicIndicatorProbe::ABwayRelicIndicatorProbe()
{
#if !UE_BUILD_SHIPPING
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
#endif
}
void ABwayRelicIndicatorProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayRelicIndicatorProbe, Frame);
}
void ABwayRelicIndicatorProbe::SetFrame(const FBwayRelicIndicatorProbeFrame& Value)
{
	Frame = Value;
	ReportedStage = -1;
	ForceNetUpdate();
}
void ABwayRelicIndicatorProbe::ServerReport_Implementation(int32 Stage, bool Passed)
{
	if (Stage != Frame.Stage) return;
	ReportedStage = Stage;
	ReportFailed |= !Passed;
}
void ABwayRelicIndicatorProbe::ClientFinish_Implementation()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: PASS observer=%d mode=%d cleanup=1"),
		GetOwner<APlayerController>()->PlayerState->GetPlayerId(), int32(GetNetMode()));
	SetActorTickEnabled(false);
	if (GetNetMode() == NM_Client) FPlatformMisc::RequestExit(false);
#endif
}
void ABwayRelicIndicatorProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	using namespace BwayRelicIndicatorTest;
	APlayerController* PC = GetOwner<APlayerController>();
	ABwayPlayerState* Viewer = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>();
	if (!PC || !PC->IsLocalController() || !Viewer || !GS || Frame.Stage < 0 || SubmittedStage == Frame.Stage) return;
	const double Now = FPlatformTime::Seconds();
	if (ObservedStage != Frame.Stage) { ObservedStage = Frame.Stage; StageStarted = Now; StableStarted = 0; }
	if (Frame.Stage == 1 || Frame.Stage == 10)
	{
		if (Viewer->GetPlayerId() != Frame.Requester) return;
		const bool Activated = Request(Viewer);
		UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: Activate stage=%d observer=%d mode=%d accepted=%d"), Frame.Stage, Viewer->GetPlayerId(), int32(GetNetMode()), Activated);
		SubmittedStage = Frame.Stage;
		ServerReport(Frame.Stage, Activated);
		return;
	}
	// The initial hold frame intentionally precedes client3's launch. Begin
	// the bounded observation only when authority publishes the complete roster.
	if (Frame.Stage == 4 && Frame.Players.Num() != Frame.ExpectedPlayers)
	{
		StageStarted = Now;
		return;
	}
	const TArray<ABwayPlayerState*> Peers = Players(GS);
	bool Passed = Peers.Num() == Frame.ExpectedPlayers && Frame.Players.Num() == Peers.Num();
	TArray<FString> Samples;
	for (ABwayPlayerState* Peer : Peers)
	{
		const int32 Index = Frame.Players.IndexOfByKey(Peer->GetPlayerId());
		const int32 Team = GS->GetPlayerTeam(Peer);
		Passed &= Frame.Teams.IsValidIndex(Index) && Frame.Teams[Index] == Team;
		APawn* Pawn = Peer->GetPawn();
		UWidgetComponent* WC = Widget(Pawn);
		UUserWidget* UI = WC ? WC->GetUserWidgetObject() : nullptr;
		const bool IsRequester = Peer->GetPlayerId() == Frame.Requester;
		const bool TagActive = Active(Peer);
		const bool Dead = ASC(Peer) && ASC(Peer)->HasMatchingGameplayTag(LyraGameplayTags::Status_Death);
		const bool Expected = IsRequester && Frame.Active && !Dead && Peer != Viewer && Team >= 0 && Team == GS->GetPlayerTeam(Viewer);
		const bool Visible = WC && WC->IsVisible();
		const bool Screen = WC && WC->GetWidgetSpace() == EWidgetSpace::Screen;
		const bool OwnerMatches = WC && WC->GetOwnerPlayer() == PC->GetLocalPlayer() && UI && UI->GetOwningPlayer() == PC;
		const bool Draw = WC && WC->GetDrawSize() == FVector2D(72, 72);
		const bool Render = WC && (WC->GetRenderTarget() || WC->GetMaterialInstance());
		const FString Texture = TextureResource(UI);
		const bool HasIndicator = Pawn && Pawn->FindComponentByClass<UBwayRelicRequestIndicatorComponent>();
		const bool AbsentDyingPawn = Frame.Stage == 8 && IsRequester && !Pawn;
		Passed &= TagActive == (IsRequester && Frame.Active) && Visible == Expected;
		Passed &= AbsentDyingPawn || (Screen && OwnerMatches && Draw && !Render && HasIndicator && !Texture.IsEmpty());
		Samples.Add(FString::Printf(TEXT("stage=%d observer=%d player=%d source=%d team=%d viewerTeam=%d active=%d dead=%d visible=%d expected=%d screen=%d owner=%d draw=%d render=%d indicator=%d pawn=%s texture=%s"),
			Frame.Stage, Viewer->GetPlayerId(), Peer->GetPlayerId(), IsRequester, Team, GS->GetPlayerTeam(Viewer), TagActive, Dead, Visible, Expected, Screen, OwnerMatches, Draw, Render, HasIndicator, *GetNameSafe(Pawn), Texture.IsEmpty() ? TEXT("None") : *Texture));
	}
	if (Passed) { if (StableStarted == 0) StableStarted = Now; } else StableStarted = 0;
	if ((!Passed || Now - StableStarted < 0.25) && Now - StageStarted < 8.0) return;
	for (const FString& Sample : Samples) UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: Peer %s"), *Sample);
	UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: Observed stage=%d observer=%d mode=%d peers=%d passed=%d"), Frame.Stage, Viewer->GetPlayerId(), int32(GetNetMode()), Peers.Num(), Passed);
	SubmittedStage = Frame.Stage;
	ServerReport(Frame.Stage, Passed);
#endif
}

#if !UE_BUILD_SHIPPING
namespace BwayRelicIndicatorTest
{
struct FRun
{
	int32 Role = 0;
	int32 Stage = -1;
	double Started = FPlatformTime::Seconds();
	double StageStarted = Started;
	bool SelectionSent = false;
	TWeakObjectPtr<ABwayPlayerState> Source;
	TWeakObjectPtr<ABwayPlayerState> ChangedViewer;
	TWeakObjectPtr<APawn> OldPawn;
	TWeakObjectPtr<ULyraAbilitySystemComponent> HeldASC;
	FActiveGameplayEffectHandle HeldEffect;
	TArray<TWeakObjectPtr<ABwayRelicIndicatorProbe>> Probes;
	TMap<TWeakObjectPtr<ABwayPlayerState>, int32> OriginalTeams;
	FBwayRelicIndicatorProbeFrame Current;

	void Cleanup(ABwayGameState* GS)
	{
		if (HeldASC.IsValid() && HeldEffect.IsValid()) HeldASC->RemoveActiveGameplayEffect(HeldEffect);
		HeldEffect.Invalidate();
		if (GS) for (const auto& Pair : OriginalTeams)
			if (Pair.Key.IsValid() && GS->GetPlayerTeam(Pair.Key.Get()) != Pair.Value) GS->AddPlayerToTeam(Pair.Key.Get(), Pair.Value);
	}
	bool Fail(ABwayGameState* GS, const TCHAR* Reason)
	{
		Cleanup(GS);
		for (const auto& Probe : Probes) if (Probe.IsValid()) Probe->Destroy();
		UE_LOG(LogTemp, Error, TEXT("BwayRelicIndicator: FAIL stage=%d role=%d reason=%s"), Stage, Role, Reason);
		Running = false;
		return false;
	}
	void AddProbes(UWorld* W, const TArray<ABwayPlayerState*>& Peers)
	{
		for (ABwayPlayerState* Peer : Peers)
		{
			APlayerController* PC = Cast<APlayerController>(Peer->GetOwner());
			if (!PC) continue;
			bool Exists = false;
			for (const auto& Probe : Probes) Exists |= Probe.IsValid() && Probe->GetOwner() == PC;
			if (Exists) continue;
			FActorSpawnParameters Params;
			Params.Owner = PC;
			auto* Probe = W->SpawnActor<ABwayRelicIndicatorProbe>(Params);
			Probe->SetFrame(Current);
			Probes.Add(Probe);
		}
	}
	void SetStage(ABwayGameState* GS, int32 Value, bool RequestActive, int32 Count)
	{
		Stage = Value;
		StageStarted = FPlatformTime::Seconds();
		Current.Stage = Stage; Current.Requester = Source.IsValid() ? Source->GetPlayerId() : -1;
		Current.Active = RequestActive; Current.ExpectedPlayers = Count;
		Current.Players.Reset(); Current.Teams.Reset();
		for (ABwayPlayerState* Peer : Players(GS))
		{
			Current.Players.Add(Peer->GetPlayerId()); Current.Teams.Add(GS->GetPlayerTeam(Peer));
		}
		for (const auto& Probe : Probes) if (Probe.IsValid()) Probe->SetFrame(Current);
		UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: Stage stage=%d source=%d active=%d players=%d"), Stage, Current.Requester, RequestActive, Count);
	}
	bool AllObserved(int32 Count) const
	{
		if (Probes.Num() != Count) return false;
		for (const auto& Probe : Probes) if (!Probe.IsValid() || Probe->ReportedStage != Stage || Probe->ReportFailed) return false;
		return true;
	}
	bool Tick(float)
	{
		UWorld* W = World();
		ABwayGameState* GS = W ? W->GetGameState<ABwayGameState>() : nullptr;
		auto* PC = W ? Cast<ABwayPlayerController>(W->GetGameInstance()->GetFirstLocalPlayerController()) : nullptr;
		auto* Local = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
		if (FPlatformTime::Seconds() - Started > 210) return Fail(GS, TEXT("overall-timeout"));
		if (!GS || !Local) return true;
		if (!Local->IsHeroLocked() && !SelectionSent)
		{
			UBwayHeroRegistry* Registry = UBwayHeroRegistry::Get(W);
			if (!Registry) return true;
			TArray<FPrimaryAssetId> Heroes;
			for (const auto& Soft : Registry->GetAllHeroSoftObjects()) if (auto* Data = Registry->LoadHeroSync(Soft)) Heroes.AddUnique(Data->GetPrimaryAssetId());
			Heroes.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
			if (Heroes.Num() < 4) return true;
			const int32 Team = GS->GetPlayerTeam(Local);
			if (Team < 0 || !GS->HeroSelectionManager) return true;
			for (int32 Offset = 0; Offset < Heroes.Num(); ++Offset)
			{
				const FPrimaryAssetId Hero = Heroes[(Role + Offset) % Heroes.Num()];
				if (GS->HeroSelectionManager->IsHeroAvailableForTeam(Hero, Team))
				{
					PC->Server_SubmitPreSelectedHero(Hero, true);
					SelectionSent = true;
					break;
				}
			}
		}
		if (Role != 0)
		{
			if (Local->IsHeroLocked() && Local->GetPawn())
			{
				UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: Ready role=%d player=%d team=%d"), Role, Local->GetPlayerId(), GS->GetPlayerTeam(Local));
				Running = false;
				return false;
			}
			return true;
		}
		const auto Peers = Players(GS);
		bool Ready = Peers.Num() >= 3;
		for (ABwayPlayerState* Peer : Peers) Ready &= Peer->GetPawn() && Peer->IsHeroLocked() && ASC(Peer) && ASC(Peer)->GetAvatarActor() == Peer->GetPawn();
		if (Stage < 0)
		{
			if (!Ready || !GS->GetRoundManagement() || GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return true;
			Source = Peers[2]; ChangedViewer = Peers[2];
			AddProbes(W, Peers);
			SetStage(GS, 0, false, 3);
		}
		for (const auto& Probe : Probes) if (!Probe.IsValid() || Probe->ReportFailed) return Fail(GS, TEXT("peer-observation-failed"));
		if (FPlatformTime::Seconds() - StageStarted > (Stage == 4 ? 80.0 : 20.0)) return Fail(GS, TEXT("stage-timeout"));
		if (Stage == 0 && AllObserved(3)) SetStage(GS, 1, false, 3);
		else if (Stage == 1 && Active(Source.Get())) SetStage(GS, 2, true, 3);
		else if (Stage == 2 && AllObserved(3)) SetStage(GS, 3, false, 3);
		else if (Stage == 3 && !Active(Source.Get()) && AllObserved(3))
		{
			Source = Peers[1];
			const URelicSettings* Settings = GS->RelicManagerComponent ? GS->RelicManagerComponent->GetRelicSettings() : nullptr;
			if (!Settings || !Settings->RequestingGameplayEffectClass) return Fail(GS, TEXT("missing-request-effect"));
			HeldASC = ASC(Source.Get());
			FGameplayEffectSpecHandle Spec = HeldASC->MakeOutgoingSpec(Settings->RequestingGameplayEffectClass, 1, HeldASC->MakeEffectContext());
			if (!Spec.IsValid()) return Fail(GS, TEXT("hold-spec-invalid"));
			Spec.Data->SetDuration(90.0f, true);
			HeldEffect = HeldASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if (!HeldEffect.IsValid() || !Active(Source.Get())) return Fail(GS, TEXT("hold-not-active"));
			SetStage(GS, 4, true, 4);
			UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: LateJoinWindow source=%d active=1 fixtureDuration=90"), Source->GetPlayerId());
		}
		else if (Stage == 4)
		{
			if (Peers.Num() == 4 && Ready && Current.Players.Num() != 4)
			{
				for (ABwayPlayerState* Peer : Peers) OriginalTeams.Add(Peer, GS->GetPlayerTeam(Peer));
				AddProbes(W, Peers);
				SetStage(GS, 4, true, 4);
			}
			if (AllObserved(4))
			{
				GS->AddPlayerToTeam(Source.Get(), 0);
				SetStage(GS, 5, true, 4);
			}
		}
		else if (Stage == 5 && AllObserved(4)) { GS->AddPlayerToTeam(ChangedViewer.Get(), 1); SetStage(GS, 6, true, 4); }
		else if (Stage == 6 && AllObserved(4))
		{
			for (const auto& Pair : OriginalTeams) if (Pair.Key.IsValid()) GS->AddPlayerToTeam(Pair.Key.Get(), Pair.Value);
			SetStage(GS, 7, true, 4);
		}
		else if (Stage == 7 && AllObserved(4))
		{
			OldPawn = Source->GetPawn();
			auto* Health = ULyraHealthComponent::FindHealthComponent(OldPawn.Get());
			if (!Health) return Fail(GS, TEXT("missing-health"));
			Health->DamageSelfDestruct();
			UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: Death source=%d pawn=%s health=%.1f active=%d"), Source->GetPlayerId(), *GetNameSafe(OldPawn.Get()), Health->GetHealth(), Active(Source.Get()));
			SetStage(GS, 8, false, 4);
		}
		else if (Stage == 8 && AllObserved(4) && Source->GetPawn() && Source->GetPawn() != OldPawn.Get() && ASC(Source.Get())->GetAvatarActor() == Source->GetPawn())
		{
			UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: Respawn source=%d pawn=%s active=%d"), Source->GetPlayerId(), *GetNameSafe(Source->GetPawn()), Active(Source.Get()));
			SetStage(GS, 9, false, 4);
		}
		else if (Stage == 9 && AllObserved(4)) SetStage(GS, 10, false, 4);
		else if (Stage == 10 && Active(Source.Get())) SetStage(GS, 11, true, 4);
		else if (Stage == 11 && AllObserved(4)) SetStage(GS, 12, false, 4);
		else if (Stage == 12 && !Active(Source.Get()) && AllObserved(4)) { Cleanup(GS); SetStage(GS, 13, false, 4); }
		else if (Stage == 13 && AllObserved(4))
		{
			for (const auto& Pair : OriginalTeams)
				if (!Pair.Key.IsValid() || GS->GetPlayerTeam(Pair.Key.Get()) != Pair.Value) return Fail(GS, TEXT("team-restore-failed"));
			if (Active(Source.Get())) return Fail(GS, TEXT("request-effect-not-removed"));
			UE_LOG(LogTemp, Display, TEXT("BwayRelicIndicator: AuthorityPASS stages=14 players=4 teamsRestored=1 effectRemoved=1"));
			for (const auto& Probe : Probes) if (Probe.IsValid()) { Probe->ClientFinish(); Probe->SetLifeSpan(2.0f); }
			Running = false;
			return false;
		}
		return true;
	}
};
void Start(const TArray<FString>& Args)
{
	if (Running || Args.Num() != 1) return;
	const int32 Role = FCString::Atoi(*Args[0]);
	if (Role < 0 || Role > 3) return;
	Running = true;
	TSharedRef<FRun> Run = MakeShared<FRun>(); Run->Role = Role;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.1f);
}
FAutoConsoleCommand Command(TEXT("bway.Test.RelicIndicatorPeer"), TEXT("Development: packaged relic indicator fixture, role 0 host or 1/2/3 client."), FConsoleCommandWithArgsDelegate::CreateStatic(&Start));
}
#endif
