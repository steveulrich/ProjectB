// Development-only full-flow observations; physical input and visual layout require separate acceptance.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/BwayGameplayAbility_PlaceBuildable.h"
#include "Abilities/BwayGameplayAbility_RelicPass.h"
#include "Abilities/BwayGameplayAbility_RelicRequest.h"
#include "BwayGameplayAbility_Slide.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "ActorPlacementTargeting/BwayTargetActor_ActorPlacementFace.h"
#include "ActorPlacementTargeting/BwayWorldReticle_ActorVisualization.h"
#include "AIController.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "BrainComponent.h"
#include "Buildable/BuildableBase.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayGoalVolume.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "CommonActivatableWidget.h"
#include "CommonSessionSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/ComboBoxString.h"
#include "Components/WidgetComponent.h"
#include "Containers/Ticker.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameModes/BwayUserFacingExperienceDefinition.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"
#include "Relic/BwayRelicRequestIndicatorComponent.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "UI/BwayMatchAbilitySlotWidget.h"
#include "UI/BwayMatchBreakdownWidget.h"
#include "UI/BwayPostMatchInterstitialWidget.h"
#include "UI/BwayRespawnHUDWidget.h"
#include "UI/BwayResultsScreenWidget.h"
#include "UI/BwayUpgradeShopWidget.h"
#include "UObject/UObjectIterator.h"

namespace BwayGoal4PolishAudit
{
bool bRunning = false;

const TCHAR* HeroNames[] = {TEXT("Argus"), TEXT("Alona"), TEXT("Korryn"), TEXT("Rawlins")};

struct FKitAbilityFamily
{
	const TCHAR* Hero;
	const TCHAR* AbilityClass;
};

const FKitAbilityFamily KitFamilies[] = {
	{TEXT("Argus"), TEXT("BwayGameplayAbility_ForGlory")},
	{TEXT("Argus"), TEXT("BwayGameplayAbility_NoRetreat")},
	{TEXT("Argus"), TEXT("BwayGameplayAbility_Retribution")},
	{TEXT("Argus"), TEXT("BwayGameplayAbility_ArgusSlide")},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaRadiance")},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaBlessing")},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaSunBurst")},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaSunsGrace")},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynBurdenOfSin")},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynCircleOfSpite")},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynAuraOfSilence")},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynFlock")},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsPowerShot")},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsDoubleDown")},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsSlideShot")},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsBlazingBarrage")},
};

bool MatchesClassFamily(const UClass* Class, const TCHAR* FamilyName)
{
	for (; Class; Class = Class->GetSuperClass())
	{
		if (Class->GetName().Contains(FamilyName))
		{
			return true;
		}
	}
	return false;
}

UWorld* FindActiveGameWorld()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && World->GetGameInstance() && !World->bIsTearingDown)
		{
			return World;
		}
	}
	return nullptr;
}

FIntPoint GetViewportSize(UWorld* World)
{
	if (World && World->GetGameViewport() && World->GetGameViewport()->Viewport)
	{
		return World->GetGameViewport()->Viewport->GetSizeXY();
	}
	return FIntPoint::ZeroValue;
}

bool CheckWidgetBounds(UUserWidget* Widget, const FIntPoint& ViewportSize, int32& OutClippingCount)
{
	if (!Widget || !Widget->IsVisible()) return false;
	const FGeometry& Geom = Widget->GetCachedGeometry();
	const FVector2D LocalSize = Geom.GetLocalSize();
	if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f)
	{
		++OutClippingCount;
		return false;
	}
	if (!FMath::IsFinite(LocalSize.X) || !FMath::IsFinite(LocalSize.Y) || LocalSize.X > float(ViewportSize.X) * 2.0f || LocalSize.Y > float(ViewportSize.Y) * 2.0f)
	{
		++OutClippingCount;
		return false;
	}
	return true;
}

UUserWidget* FindMatchHUDLayout(APlayerController* PC)
{
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		if (It->IsTemplate() || It->GetOwningPlayer() != PC || !It->IsVisible()) continue;
		const FString ClassName = It->GetClass()->GetName();
		if (ClassName.Contains(TEXT("WBP_BW_MatchHUDLayout")) || ClassName.Contains(TEXT("W_BW_HUDLayout")))
		{
			return *It;
		}
	}
	return nullptr;
}

UBwayUpgradeShopWidget* FindActiveShopWidget(APlayerController* PC)
{
	for (TObjectIterator<UBwayUpgradeShopWidget> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated() && It->IsVisible())
		{
			return *It;
		}
	}
	return nullptr;
}

UBwayRespawnHUDWidget* FindRespawnHUDWidget(APlayerController* PC)
{
	for (TObjectIterator<UBwayRespawnHUDWidget> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsInViewport())
		{
			return *It;
		}
	}
	return nullptr;
}

UBwayResultsScreenWidget* FindResultsScreenWidget(APlayerController* PC)
{
	for (TObjectIterator<UBwayResultsScreenWidget> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated())
		{
			return *It;
		}
	}
	return nullptr;
}

UBwayPostMatchInterstitialWidget* FindInterstitialWidget(APlayerController* PC)
{
	for (TObjectIterator<UBwayPostMatchInterstitialWidget> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated())
		{
			return *It;
		}
	}
	return nullptr;
}

UBwayMatchBreakdownWidget* FindBreakdownWidget(APlayerController* PC)
{
	for (TObjectIterator<UBwayMatchBreakdownWidget> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated())
		{
			return *It;
		}
	}
	return nullptr;
}

struct FAuditRunner
{
	bool bExitOnComplete = true;
	int32 Step = 0;
	int32 SubStep = 0;
	int32 HeroIndex = 0;
	double StartedAt = FPlatformTime::Seconds();
	double StepChangedAt = StartedAt;
	double PlayingStartedAt = 0.0;
	int32 PerfFramesCaptured = 0;
	double PerfTotalFrameMs = 0.0;
	double PerfMaxFrameMs = 0.0;
	TArray<double> PerfFrameSamplesMs;
	bool bCsvActive = false;
	bool bPrematchAudited = false;
	bool bPlayingHudAudited = false;
	bool bShopAudited = false;
	bool bRelicMarkerAudited = false;
	bool bDeadRespawnAudited = false;
	bool bPostRoundAudited = false;
	bool bInterstitialAudited = false;
	bool bPostMatchAudited = false;
	int32 HeroesSampled = 0;
	int32 TotalClippingCount = 0;
	int32 TotalFocusLossCount = 0;
	uint32 PreDeathPawnId = 0;
	int32 PreDeathCount = 0;

	void Advance(int32 NextStep, int32 NextSubStep = 0)
	{
		Step = NextStep;
		SubStep = NextSubStep;
		StepChangedAt = FPlatformTime::Seconds();
	}

	bool Fail(const TCHAR* Reason)
	{
		if (bCsvActive)
		{
			if (UWorld* W = FindActiveGameWorld()) GEngine->Exec(W, TEXT("CsvProfile Stop"));
			bCsvActive = false;
		}
		UE_LOG(LogTemp, Error, TEXT("BwayGoal4Audit: FAIL step=%d subStep=%d heroIndex=%d reason=%s"), Step, SubStep, HeroIndex, Reason);
		bRunning = false;
		return false;
	}

	void AuditStateLayout(UWorld* World, ABwayPlayerController* PC, const TCHAR* StateLabel, UUserWidget* PrimaryWidget, bool bExpectMenuFocus)
	{
		const FIntPoint Res = GetViewportSize(World);
		int32 Clipping = 0;
		const bool bBoundsObserved = CheckWidgetBounds(PrimaryWidget, Res, Clipping);
		UCommonUIActionRouterBase* Router = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
		const ECommonInputMode InputMode = Router ? Router->GetActiveInputMode() : ECommonInputMode::Game;
		const bool bHasMenuOrWidgetFocus = (InputMode != ECommonInputMode::Game)
			&& PrimaryWidget && PC && (PrimaryWidget->HasUserFocus(PC) || PrimaryWidget->HasUserFocusedDescendants(PC));
		const bool bFocusLoss = bExpectMenuFocus ? !bHasMenuOrWidgetFocus : false;
		if (bFocusLoss) ++TotalFocusLossCount;
		TotalClippingCount += Clipping;
		UE_LOG(LogTemp, Display, TEXT("BwayGoal4HUD: State=%s res=%dx%d widget=%s visible=%d inputMode=%d boundsObserved=%d boundsFailures=%d menuFocusMissing=%d visualLayoutVerified=0"),
			StateLabel, Res.X, Res.Y, *GetNameSafe(PrimaryWidget), PrimaryWidget && PrimaryWidget->IsVisible(),
			int32(InputMode), bBoundsObserved, Clipping, bFocusLoss ? 1 : 0);
	}

	void IsolateBots(UWorld* World, APawn* LocalPawn)
	{
		for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
		{
			ABwayCharacterWithAbilities* Pawn = *It;
			if (!Pawn || Pawn == LocalPawn) continue;
			if (auto* AI = Cast<AAIController>(Pawn->GetController()))
			{
				if (auto* Brain = AI->GetBrainComponent()) Brain->StopLogic(TEXT("Goal4AuditIsolation"));
			}
			if (auto* Move = Pawn->GetBwayCharacterMovement())
			{
				Move->StopMovementImmediately();
				Move->DisableMovement();
			}
		}
	}

	bool Tick(float DeltaSeconds)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - StartedAt > 300.0)
		{
			return Fail(TEXT("overall-timeout"));
		}

		UWorld* World = FindActiveGameWorld();
		if (!World) return true;
		APlayerController* RawPC = World->GetGameInstance()->GetFirstLocalPlayerController();
		ABwayPlayerController* PC = Cast<ABwayPlayerController>(RawPC);
		const FString MapName = World->GetMapName();

		// Step 0: Frontend (L_LyraFrontEnd) -> Host LAN Session (or skip if already in HeroSelect/Dorado)
		if (Step == 0)
		{
			if (MapName.Contains(TEXT("HeroSelect_Staging")))
			{
				Advance(1);
				return true;
			}
			if (MapName.Contains(TEXT("L_BW_Dorado")))
			{
				Advance(2);
				return true;
			}
			if (!RawPC || !RawPC->GetLocalPlayer()) return true;
			UCommonActivatableWidget* FrontEndWidget = nullptr;
			for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
			{
				if (!It->IsTemplate() && It->GetOwningPlayer() == RawPC && It->IsActivated() && It->GetClass()->GetName().Contains(TEXT("FrontEnd")))
				{
					FrontEndWidget = *It;
					break;
				}
			}
			if (!FrontEndWidget || Now - StepChangedAt < 1.0) return true;
			const FIntPoint Res = GetViewportSize(World);
			UE_LOG(LogTemp, Display, TEXT("BwayGoal4Audit: FrontendReady map=%s res=%dx%d widget=%s"), *MapName, Res.X, Res.Y, *GetNameSafe(FrontEndWidget));
			UCommonSessionSubsystem* Sessions = World->GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>();
			const auto* Playlist = LoadObject<UBwayUserFacingExperienceDefinition>(nullptr, TEXT("/Game/System/DA_Playlist_CTR_LAN.DA_Playlist_CTR_LAN"));
			UCommonSession_HostSessionRequest* Request = Playlist ? Playlist->CreateHostingRequest(World) : nullptr;
			if (!Sessions || !Request) return Fail(TEXT("frontend-lan-host-request-failed"));
			Request->ExtraArgs.Add(TEXT("NumBots"), TEXT("7"));
			Request->ExtraArgs.Add(TEXT("PointsToWin"), TEXT("2"));
			Request->ExtraArgs.Add(TEXT("RoundDuration"), TEXT("300"));
			Request->ExtraArgs.Add(TEXT("WarmupDuration"), TEXT("4"));
			Request->ExtraArgs.Add(TEXT("PostRoundDuration"), TEXT("3"));
			Request->ExtraArgs.Add(TEXT("DisableRelicBotAI"), TEXT("1"));
			Sessions->HostSession(RawPC, Request);
			UE_LOG(LogTemp, Display, TEXT("BwayGoal4Audit: HostedLAN playlist=DA_Playlist_CTR_LAN numBots=7 pointsToWin=2"));
			Advance(1);
			return true;
		}

		// Step 1: Hero Selection (L_BW_HeroSelect_Staging)
		if (Step == 1)
		{
			if (MapName.Contains(TEXT("L_BW_Dorado")))
			{
				Advance(2);
				return true;
			}
			if (!MapName.Contains(TEXT("HeroSelect_Staging")) || !PC) return true;
			auto* GS = World->GetGameState<ABwayGameState>();
			auto* PS = PC->GetPlayerState<ABwayPlayerState>();
			if (!GS || !PS || !GS->HeroSelectionManager || !GS->HeroSelectionPhaseComponent || !GS->HeroSelectionManager->IsSelectionActive()) return true;
			UBwayHeroSelectWidget* SelectWidget = nullptr;
			for (TObjectIterator<UBwayHeroSelectWidget> It; It; ++It)
			{
				if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated() && It->IsVisible())
				{
					SelectWidget = *It;
					break;
				}
			}
			if (!SelectWidget || Now - StepChangedAt < 1.0) return true;
			AuditStateLayout(World, PC, TEXT("HeroSelection"), SelectWidget, true);
			const FPrimaryAssetId ArgusId = UBwayHeroRegistry::ResolveHeroIdByName(TEXT("Argus"));
			PC->Server_SubmitPreSelectedHero(ArgusId, true);
			GS->HeroSelectionManager->ResolveMissingSelectionsAndLockAll(GS->HeroSelectionPhaseComponent->ResolveFallbackHeroId());
			UE_LOG(LogTemp, Display, TEXT("BwayGoal4Audit: HeroSelectionLocked hero=%s totalPlayers=%d"), *ArgusId.ToString(), GS->PlayerArray.Num());
			Advance(2);
			return true;
		}

		// Step 2: Dorado Prematch HUD Audit & Wait for Playing
		if (Step == 2)
		{
			if (!MapName.Contains(TEXT("L_BW_Dorado")) || !PC) return true;
			auto* GS = World->GetGameState<ABwayGameState>();
			auto* Round = GS ? GS->GetRoundManagement() : nullptr;
			if (!GS || !Round) return true;
			GEngine->SetMaxFPS(0.0f);
			if (auto* CVarMaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) CVarMaxFPS->Set(0.0f);
			if (auto* CVarVSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))) CVarVSync->Set(0);
			UUserWidget* MatchHUD = FindMatchHUDLayout(PC);
			UBwayRespawnHUDWidget* RespawnHUD = FindRespawnHUDWidget(PC);
			if (!bPrematchAudited && MatchHUD && RespawnHUD && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Prematch)
			{
				AuditStateLayout(World, PC, TEXT("Prematch"), MatchHUD, false);
				bPrematchAudited = true;
			}
			if (Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && PC->GetPawn() && MatchHUD && RespawnHUD)
			{
				if (!bPrematchAudited)
				{
					AuditStateLayout(World, PC, TEXT("Prematch"), MatchHUD, false);
					bPrematchAudited = true;
				}
				if (PlayingStartedAt == 0.0)
				{
					PlayingStartedAt = Now;
					IsolateBots(World, PC->GetPawn());
					return true;
				}
				if (Now - PlayingStartedAt < 1.5) return true;
				IsolateBots(World, PC->GetPawn());
				PerfFramesCaptured = 0;
				PerfTotalFrameMs = 0.0;
				PerfMaxFrameMs = 0.0;
				PerfFrameSamplesMs.Reset();
				PerfFrameSamplesMs.Reserve(400);
				GEngine->Exec(World, TEXT("CsvProfile Start"));
				bCsvActive = true;
				const FIntPoint Res = GetViewportSize(World);
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4Perf: StartedCsvCapture res=%dx%d targetFrames=360"), Res.X, Res.Y);
				Advance(3);
			}
			return true;
		}

		// Step 3: Unperturbed 1080p Playing Render Performance Capture (360 frames)
		if (Step == 3)
		{
			if (!PC || !PC->GetPawn()) return true;
			GEngine->SetMaxFPS(0.0f);
			if (auto* CVarMaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) CVarMaxFPS->Set(0.0f);
			if (auto* CVarVSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))) CVarVSync->Set(0);
			const double FrameMs = FApp::GetDeltaTime() * 1000.0;
			// Skip the first 10 frames after CsvProfile Start to avoid command-start overhead
			++PerfFramesCaptured;
			if (PerfFramesCaptured > 10)
			{
				PerfTotalFrameMs += FrameMs;
				PerfMaxFrameMs = FMath::Max(PerfMaxFrameMs, FrameMs);
				PerfFrameSamplesMs.Add(FrameMs);
			}
			// Smooth forward/strafe movement on Dorado flat lane during capture
			const float DirSign = ((PerfFramesCaptured / 90) % 2 == 0) ? 1.f : -1.f;
			PC->GetPawn()->AddMovementInput(FVector(DirSign, 0.f, 0.f), 0.5f);
			if (PerfFrameSamplesMs.Num() >= 360)
			{
				GEngine->Exec(World, TEXT("CsvProfile Stop"));
				bCsvActive = false;
				PerfFrameSamplesMs.Sort();
				const double MeanMs = PerfTotalFrameMs / double(PerfFrameSamplesMs.Num());
				const double Fps = 1000.0 / MeanMs;
				const double P95Ms = PerfFrameSamplesMs[FMath::Clamp(int32(PerfFrameSamplesMs.Num() * 0.95), 0, PerfFrameSamplesMs.Num() - 1)];
				const FIntPoint Res = GetViewportSize(World);
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4Perf: SAMPLE resolution=%dx%d frames=%d meanFrameMs=%.4f fps=%.2f p95FrameMs=%.4f maxFrameMs=%.4f vsmOverflows=unmeasured budgetMs=8.33 withinBudget=%d"),
					Res.X, Res.Y, PerfFrameSamplesMs.Num(), MeanMs, Fps, P95Ms, PerfMaxFrameMs, MeanMs < 8.33 && P95Ms < 11.0 && Res == FIntPoint(1920, 1080));
				HeroIndex = 0;
				Advance(4, 0);
			}
			return true;
		}

		// Step 4: Playing HUD + Upgrade Shop + Call-for-Relic + All 4 Heroes Input/Kit Verification
		if (Step == 4)
		{
			if (!PC || !PC->GetPawn()) return true;
			auto* GS = World->GetGameState<ABwayGameState>();
			auto* PS = PC->GetPlayerState<ABwayPlayerState>();
			auto* Pawn = Cast<ABwayCharacterWithAbilities>(PC->GetPawn());
			auto* ASC = PS ? PS->GetLyraAbilitySystemComponent() : nullptr;
			auto* Move = Pawn ? Pawn->GetBwayCharacterMovement() : nullptr;
			if (!GS || !PS || !Pawn || !ASC || !Move) return true;

			if (!bPlayingHudAudited)
			{
				UUserWidget* MatchHUD = FindMatchHUDLayout(PC);
				int32 AbilitySlots = 0;
				for (TObjectIterator<UBwayMatchAbilitySlotWidget> It; It; ++It)
				{
					if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsVisible()) ++AbilitySlots;
				}
				AuditStateLayout(World, PC, TEXT("Playing"), MatchHUD, false);
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4HUD: PlayingDetails matchHud=%s abilitySlots=%d respawnHud=%d"),
					*GetNameSafe(MatchHUD), AbilitySlots, FindRespawnHUDWidget(PC) != nullptr);
				bPlayingHudAudited = true;
			}

			if (HeroIndex >= 4)
			{
				Advance(5, 0);
				return true;
			}

			const TCHAR* HeroName = HeroNames[HeroIndex];
			// SubStep 0: Initialize hero on local pawn and place on clear flat lane
			if (SubStep == 0)
			{
				IsolateBots(World, Pawn);
				UBwayHeroRegistry* Registry = UBwayHeroRegistry::Get(World);
				const FPrimaryAssetId HeroId = UBwayHeroRegistry::ResolveHeroIdByName(HeroName);
				const UBwayHeroDataAsset* HeroData = Registry ? Registry->GetHeroDataById(HeroId) : nullptr;
				if (!HeroData && Registry)
				{
					for (const auto& Soft : Registry->GetAllHeroSoftObjects())
					{
						if (const UBwayHeroDataAsset* Loaded = Registry->LoadHeroSync(Soft))
						{
							if (Loaded->GetPrimaryAssetId() == HeroId || Loaded->GetName().Contains(HeroName))
							{
								HeroData = Loaded;
								break;
							}
						}
					}
				}
				if (!HeroData) return Fail(TEXT("hero-data-not-found"));
				PS->UnlockHeroSelection();
				PS->TrySetSelectedHeroId(HeroData->GetPrimaryAssetId());
				PS->ServerLockHeroSelection();
				Pawn->InitializeHeroData(HeroData);
				Move->StopMovementImmediately();
				Move->SetMovementMode(MOVE_Walking);
				Pawn->SetActorLocation(FVector(-400.f, 2000.f, 102.f), false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->SetActorRotation(FRotator::ZeroRotator);
				PC->SetControlRotation(FRotator::ZeroRotator);
				Advance(4, 1);
				return true;
			}

			// SubStep 1: LMB Primary + RMB Call-for-Relic + Q/E/F Combat Abilities verification
			if (SubStep == 1)
			{
				if (Now - StepChangedAt < 0.25) return true;
				// 1. LMB Primary
				const FGameplayTag PrimaryTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Primary"), false);
				const FGameplayTag WeaponFireTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Fire"), false);
				int32 PrimaryCount = 0;
				for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
				{
					if (Spec.Ability && (Spec.GetDynamicSpecSourceTags().HasTagExact(PrimaryTag)
						|| Spec.GetDynamicSpecSourceTags().HasTagExact(WeaponFireTag)
						|| Spec.Ability->GetClass()->GetName().Contains(TEXT("Primary"))
						|| Spec.Ability->GetClass()->GetName().Contains(TEXT("Melee"))
						|| Spec.Ability->GetClass()->GetName().Contains(TEXT("Ranged"))))
					{
						++PrimaryCount;
						ASC->TryActivateAbility(Spec.Handle);
						ASC->CancelAbilityHandle(Spec.Handle);
					}
				}

				// 2. RMB Secondary / Prepared Catch / Call-for-Relic + W_BW_CaptureTheRelic_RelicRequestMarker
				bool bRelicRequestOk = false;
				for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
				{
					if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_RelicRequest>())
					{
						bRelicRequestOk = ASC->TryActivateAbility(Spec.Handle);
						break;
					}
				}
				FComponentReference MarkerRef;
				MarkerRef.ComponentProperty = TEXT("RelicRequestWidgetComponent");
				UWidgetComponent* MarkerComp = Cast<UWidgetComponent>(MarkerRef.GetComponent(Pawn));
				UUserWidget* MarkerWidget = MarkerComp ? MarkerComp->GetUserWidgetObject() : nullptr;
				const FGameplayTag ReqTag = UBwayRelicRequestIndicatorComponent::ResolveRequestTag(PS);
				const bool bReqTagActive = ReqTag.IsValid() && ASC->HasMatchingGameplayTag(ReqTag);
				if (!bRelicMarkerAudited && MarkerWidget)
				{
					AuditStateLayout(World, PC, TEXT("Playing_CallForRelicMarker"), MarkerWidget, false);
					bRelicMarkerAudited = true;
				}
				if (ReqTag.IsValid())
				{
					ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ReqTag));
				}

				// 3. Q/E/F Combat Abilities (4 kit abilities per hero)
				int32 KitAbilitiesVerified = 0;
				for (const FKitAbilityFamily& Entry : KitFamilies)
				{
					if (FCString::Strcmp(Entry.Hero, HeroName) != 0) continue;
					for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
					{
						if (Spec.Ability && MatchesClassFamily(Spec.Ability->GetClass(), Entry.AbilityClass))
						{
							++KitAbilitiesVerified;
							ASC->TryActivateAbility(Spec.Handle);
							ASC->CancelAbilityHandle(Spec.Handle);
							if (const FGameplayTagContainer* CdTags = Spec.Ability->GetCooldownTags())
							{
								ASC->RemoveActiveEffectsWithGrantedTags(*CdTags);
							}
							break;
						}
					}
				}

				UE_LOG(LogTemp, Display, TEXT("BwayGoal4HeroKit: CombatAndRelicRequest hero=%s primarySpecs=%d relicRequestActivated=%d relicRequestTag=%d markerComp=%d kitAbilities=%d/4"),
					HeroName, PrimaryCount, bRelicRequestOk, bReqTagActive, MarkerComp != nullptr, KitAbilitiesVerified);
				Advance(4, 2);
				return true;
			}

			// SubStep 2: Shift Slide activation & release verification
			if (SubStep == 2)
			{
				Pawn->AddMovementInput(FVector(1.f, 0.f, 0.f), 1.f);
				if (Move->Velocity.Size2D() < 320.f && Now - StepChangedAt < 1.5) return true;
				const FGameplayTag SlideInputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Slide"));
				ASC->AbilityInputTagPressed(SlideInputTag);
				const bool bSlideActivated = Move->IsSliding() && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sliding")));
				ASC->AbilityInputTagReleased(SlideInputTag);
				Move->StopMovementImmediately();
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4HeroKit: ShiftSlide hero=%s slideActivated=%d cleanRelease=%d"),
					HeroName, bSlideActivated, !Move->IsSliding());
				Advance(4, 3);
				return true;
			}

			// SubStep 3: G Buildable Preview & Placement verification
			if (SubStep == 3)
			{
				PS->BeginRoundStatTracking();
				Pawn->SetActorLocation(FVector(-200.f + HeroIndex * 180.f, 2000.f, 102.f), false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->SetActorRotation(FRotator::ZeroRotator);
				PC->SetControlRotation(FRotator::ZeroRotator);
				FGameplayAbilitySpec* BuildSpec = nullptr;
				for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
				{
					if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_PlaceBuildable>())
					{
						BuildSpec = &Spec;
						break;
					}
				}
				const bool bPreviewStarted = BuildSpec && ASC->TryActivateAbility(BuildSpec->Handle);
				Advance(4, 4);
				(void)bPreviewStarted;
				return true;
			}

			// SubStep 4: Confirm G Buildable Placement after preview target actor spawns
			if (SubStep == 4)
			{
				if (Now - StepChangedAt < 0.25) return true;
				int32 PreviewTargets = 0, PreviewReticles = 0;
				for (TActorIterator<ABwayTargetActor_ActorPlacementFace> It(World); It; ++It) if (!It->IsActorBeingDestroyed()) ++PreviewTargets;
				for (TActorIterator<ABwayWorldReticle_ActorVisualization> It(World); It; ++It) if (!It->IsActorBeingDestroyed()) ++PreviewReticles;
				const int32 BeforeCount = GS->BuildableRegistryComponent ? GS->BuildableRegistryComponent->GetBuildableCountForPlayer(PS) : 0;
				ASC->LocalInputConfirm();
				const int32 AfterCount = GS->BuildableRegistryComponent ? GS->BuildableRegistryComponent->GetBuildableCountForPlayer(PS) : 0;
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4HeroKit: BuildablePlace hero=%s previewTargets=%d previewReticles=%d beforeCount=%d afterCount=%d"),
					HeroName, PreviewTargets, PreviewReticles, BeforeCount, AfterCount);
				Advance(4, 5);
				return true;
			}

			// SubStep 5: B / D-Pad Up Upgrade Shop toggle, layout audit, and purchase
			if (SubStep == 5)
			{
				const UEnhancedInputLocalPlayerSubsystem* InputSub = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
				const TArray<FKey> MappedKeys = (InputSub && PC->GetUpgradeShopInputAction())
					? InputSub->QueryKeysMappedToAction(PC->GetUpgradeShopInputAction())
					: TArray<FKey>();
				const bool bHasBAndDPadUp = MappedKeys.Contains(EKeys::B) && MappedKeys.Contains(EKeys::Gamepad_DPad_Up);
				PC->ToggleUpgradeShop();
				UBwayUpgradeShopWidget* ShopWidget = FindActiveShopWidget(PC);
				bool bPurchased = false;
				if (ShopWidget)
				{
					if (!bShopAudited)
					{
						AuditStateLayout(World, PC, TEXT("Playing_UpgradeShop"), ShopWidget, true);
						bShopAudited = true;
					}
					if (HeroIndex == 0 && PS->UpgradeComponent && PS->UpgradeComponent->GetCatalog() && !PS->UpgradeComponent->GetCatalog()->Upgrades.IsEmpty())
					{
						const FBwayUpgradeDefinition& Def = PS->UpgradeComponent->GetCatalog()->Upgrades[0];
						if (!Def.Ranks.IsEmpty())
						{
							PS->UpgradeComponent->SetInHealingZone(true, GS->GetPlayerTeam(PS));
							ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), float(Def.Ranks[0].Cost + 500));
							PS->UpgradeComponent->TryPurchaseUpgrade(Def.Id, 0);
							bPurchased = PS->UpgradeComponent->GetOwnedRank(Def.Id) >= 1;
							PS->UpgradeComponent->SetInHealingZone(false, -1);
						}
					}
					PC->ToggleUpgradeShop();
				}
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4HeroKit: UpgradeShop hero=%s mappedBAndDPadUp=%d shopOpened=%d purchased=%d shopClosedClean=%d"),
					HeroName, bHasBAndDPadUp, ShopWidget != nullptr, bPurchased, FindActiveShopWidget(PC) == nullptr);
				Advance(4, 6);
				return true;
			}

			// SubStep 6: Relic Pickup, Pass (RMB), and Throw (LMB) verification
			if (SubStep == 6)
			{
				auto* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>();
				ARelicActor* Relic = RelicMgr ? RelicMgr->GetRelicActor() : nullptr;
				bool bPickedUpPass = false, bPassed = false, bPickedUpThrow = false, bThrown = false;
				if (RelicMgr && Relic)
				{
					// Pickup 1 -> Pass (RMB)
					RelicMgr->ResetRelic();
					Pawn->SetActorLocation(FVector(0.f, 2000.f, 102.f), false, nullptr, ETeleportType::TeleportPhysics);
					Relic->SetActorLocation(Pawn->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
					Relic->OnInteractionSphereOverlap(nullptr, Pawn, nullptr, 0, false, FHitResult());
					bPickedUpPass = (Relic->CurrentCarrier == Pawn);
					for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
					{
						if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_RelicPass>())
						{
							ASC->TryActivateAbility(Spec.Handle);
							break;
						}
					}
					bPassed = (Relic->GetCurrentState() == ERelicState::BeingPassed && Relic->CurrentCarrier == nullptr);

					// Pickup 2 -> Throw (LMB)
					RelicMgr->ResetRelic();
					Relic->SetActorLocation(Pawn->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
					Relic->OnInteractionSphereOverlap(nullptr, Pawn, nullptr, 0, false, FHitResult());
					bPickedUpThrow = (Relic->CurrentCarrier == Pawn);
					for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
					{
						if (Spec.Ability && Spec.Ability->GetClass()->GetName().Contains(TEXT("Relic_Throw")))
						{
							ASC->TryActivateAbility(Spec.Handle);
							ASC->AbilityInputTagReleased(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Primary"), false));
							ASC->CancelAbilityHandle(Spec.Handle);
							break;
						}
					}
					bThrown = (Relic->GetCurrentState() == ERelicState::Thrown && Relic->CurrentCarrier == nullptr);
					RelicMgr->ResetRelic();
				}
				++HeroesSampled;
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4HeroKit: OBSERVED hero=%s index=%d physicalInputVerified=0 relicPass=%d relicThrow=%d"),
					HeroName, HeroIndex, bPickedUpPass && bPassed, bPickedUpThrow && bThrown);
				++HeroIndex;
				Advance(4, 0);
				return true;
			}
		}

		// Step 5: Dead/Respawning State & UBwayRespawnHUDWidget Countdown Verification
		if (Step == 5)
		{
			if (!PC) return true;
			auto* PS = PC->GetPlayerState<ABwayPlayerState>();
			auto* ASC = PS ? PS->GetLyraAbilitySystemComponent() : nullptr;
			if (!PS || !ASC) return true;

			if (SubStep == 0)
			{
				APawn* Pawn = PC->GetPawn();
				if (!Pawn) return true;
				PreDeathPawnId = Pawn->GetUniqueID();
				PreDeathCount = PS->GetDeaths();
				UBwayDamageLibrary::ApplyDamageFromSource(ASC, Pawn, 100000.0f, Pawn, Pawn);
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4Audit: AppliedLethalDamage oldPawn=%s deathsBefore=%d"), *Pawn->GetName(), PreDeathCount);
				Advance(5, 1);
				return true;
			}

			if (SubStep == 1)
			{
				UBwayRespawnHUDWidget* RespawnHUD = FindRespawnHUDWidget(PC);
				if (!bDeadRespawnAudited && RespawnHUD && RespawnHUD->GetDisplayedSeconds() >= 0)
				{
					AuditStateLayout(World, PC, TEXT("Dead_Respawning"), RespawnHUD, false);
					UE_LOG(LogTemp, Display, TEXT("BwayGoal4HUD: RespawnCountdownDisplayed seconds=%d status=%s"),
						RespawnHUD->GetDisplayedSeconds(), *RespawnHUD->GetDisplayedStatus().ToString());
					bDeadRespawnAudited = true;
				}
				APawn* NewPawn = PC->GetPawn();
				const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(NewPawn);
				if (NewPawn && NewPawn->GetUniqueID() != PreDeathPawnId && Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.f)
				{
					if (!bDeadRespawnAudited && RespawnHUD)
					{
						AuditStateLayout(World, PC, TEXT("Dead_Respawning"), RespawnHUD, false);
						bDeadRespawnAudited = true;
					}
					UE_LOG(LogTemp, Display, TEXT("BwayGoal4Audit: RespawnComplete newPawn=%s deathsAfter=%d"), *NewPawn->GetName(), PS->GetDeaths());
					Advance(6, 0);
				}
				else if (Now - StepChangedAt > 25.0)
				{
					return Fail(TEXT("respawn-timeout"));
				}
				return true;
			}
		}

		// Step 6: Score Goal 1 -> Verify PostRound HUD -> Score Goal 2 -> Verify PostMatch & EndOfMatchSummary
		if (Step == 6)
		{
			if (!PC) return true;
			auto* GS = World->GetGameState<ABwayGameState>();
			auto* Round = GS ? GS->GetRoundManagement() : nullptr;
			auto* PS = PC->GetPlayerState<ABwayPlayerState>();
			if (!GS || !Round || !PS) return true;

			const EBwayMatchPhase Phase = Round->GetCurrentMatchPhase();
			if (Phase == EBwayMatchPhase::PostRound && !bPostRoundAudited)
			{
				UUserWidget* MatchHUD = FindMatchHUDLayout(PC);
				AuditStateLayout(World, PC, TEXT("PostRound"), MatchHUD, false);
				bPostRoundAudited = true;
				UE_LOG(LogTemp, Display, TEXT("BwayGoal4Audit: PostRoundObserved round=%d"), Round->GetCurrentRoundNumber());
				return true;
			}

			if (Phase == EBwayMatchPhase::PostMatch)
			{
				UBwayResultsScreenWidget* ResultsScreen = FindResultsScreenWidget(PC);
				UBwayPostMatchInterstitialWidget* Interstitial = FindInterstitialWidget(PC);
				UBwayMatchBreakdownWidget* Breakdown = FindBreakdownWidget(PC);
				if (Interstitial && !bInterstitialAudited)
				{
					AuditStateLayout(World, PC, TEXT("PostMatch_Interstitial"), Interstitial, true);
					bInterstitialAudited = true;
				}
				if (Breakdown && Breakdown->GetPlayerColumns().Num() > 0)
				{
					AuditStateLayout(World, PC, TEXT("PostMatch_EndOfMatchSummary"), Breakdown, true);
					bPostMatchAudited = true;
					UE_LOG(LogTemp, Display, TEXT("BwayGoal4HUD: EndOfMatchSummaryPopulated resultsScreen=%s breakdown=%s columns=%d mvp=%s mvpHero=%s"),
						*GetNameSafe(ResultsScreen), *GetNameSafe(Breakdown), Breakdown->GetPlayerColumns().Num(),
						*Breakdown->GetMVPPlayerName().ToString(), *Breakdown->GetMVPHeroName().ToString());
					Advance(7, 0);
				}
				else if (Now - StepChangedAt > 20.0)
				{
					return Fail(TEXT("postmatch-breakdown-timeout"));
				}
				return true;
			}

			// In Playing: score a goal for the local player's team once every 2.0s if still in Playing
			if (Phase == EBwayMatchPhase::Playing && PC->GetPawn() && Now - StepChangedAt >= 1.5)
			{
				auto* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>();
				ARelicActor* Relic = RelicMgr ? RelicMgr->GetRelicActor() : nullptr;
				auto* Pawn = Cast<ABwayCharacterWithAbilities>(PC->GetPawn());
				if (RelicMgr && Relic && Pawn)
				{
					const int32 MyTeam = GS->GetPlayerTeam(PS);
					ABwayGoalVolume* EnemyGoal = nullptr;
					for (TActorIterator<ABwayGoalVolume> It(World); It; ++It)
					{
						if (It->OwningTeam == 1 - MyTeam)
						{
							EnemyGoal = *It;
							break;
						}
					}
					if (EnemyGoal)
					{
						RelicMgr->ResetRelic();
						Pawn->SetActorLocation(FVector(0.f, 2000.f, 102.f), false, nullptr, ETeleportType::TeleportPhysics);
						Relic->SetActorLocation(Pawn->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
						Relic->OnInteractionSphereOverlap(nullptr, Pawn, nullptr, 0, false, FHitResult());
						Pawn->SetActorLocation(EnemyGoal->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
						StepChangedAt = Now;
						UE_LOG(LogTemp, Display, TEXT("BwayGoal4Audit: ScoredGoal round=%d team=%d"), Round->GetCurrentRoundNumber(), MyTeam);
					}
				}
			}
			return true;
		}

		// Step 7: Final Summary & Clean Exit
		if (Step == 7)
		{
			PerfFrameSamplesMs.Sort();
			const double MeanMs = PerfFrameSamplesMs.IsEmpty() ? 0.0 : PerfTotalFrameMs / double(PerfFrameSamplesMs.Num());
			const double Fps = MeanMs > 0.0 ? 1000.0 / MeanMs : 0.0;
			const FIntPoint Res = GetViewportSize(World);
			UE_LOG(LogTemp, Display, TEXT("BwayGoal4Audit: COMPLETE status=observed acceptancePassed=0 physicalInputVerified=0 visualLayoutVerified=0 resolution=%dx%d prematchAudited=%d playingAudited=%d shopAudited=%d relicMarkerAudited=%d deadRespawnAudited=%d postRoundAudited=%d postMatchAudited=%d heroesSampled=%d boundsFailures=%d menuFocusMissing=%d meanFrameMs=%.4f fps=%.2f vsmOverflows=unmeasured"),
				Res.X, Res.Y,
				bPrematchAudited, bPlayingHudAudited, bShopAudited, bRelicMarkerAudited, bDeadRespawnAudited, bPostRoundAudited, bPostMatchAudited,
				HeroesSampled, TotalClippingCount, TotalFocusLossCount, MeanMs, Fps);
			bRunning = false;
			if (bExitOnComplete && World->WorldType == EWorldType::Game)
			{
				FPlatformMisc::RequestExit(false);
			}
			return false;
		}

		return true;
	}
};

void StartGoal4Audit(bool bExitOnComplete)
{
	if (bRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayGoal4Audit: Already running"));
		return;
	}
	bRunning = true;
	TSharedRef<FAuditRunner> Runner = MakeShared<FAuditRunner>();
	Runner->bExitOnComplete = bExitOnComplete;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Runner](float DeltaSeconds)
	{
		return Runner->Tick(DeltaSeconds);
	}), 0.0f);
}

FAutoConsoleCommand Goal4FullFlowCommand(
	TEXT("bway.Test.Goal4FullFlow1080pAudit"),
	TEXT("Development full-flow HUD, timing, and hero observations; does not establish physical input or visual acceptance."),
	FConsoleCommandDelegate::CreateLambda([]() { StartGoal4Audit(true); }));

FAutoConsoleCommand Goal4InteractiveNoExitCommand(
	TEXT("bway.Test.Goal4InteractiveAudit"),
	TEXT("Development full-flow observations without auto-exit; does not establish physical input or visual acceptance."),
	FConsoleCommandDelegate::CreateLambda([]() { StartGoal4Audit(false); }));
} // namespace BwayGoal4PolishAudit
#endif
