// Copyright Epic Games, Inc. All Rights Reserved.

#include "BwayPlayerController.h"
#include "UI/BwayUpgradeShopWidget.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "UI/BwayResultsScreenWidget.h"
#include "UI/BwayPostRoundSummaryWidget.h"
#include "UI/BwayRespawnHUDWidget.h"
#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Development/BwayCheatManager.h"
#include "Development/BwayCombatFixtureAbilities.h"
#include "Combat/BwayCombatNumberPopComponent.h"
#include "Combat/BwayNameplatePolicyComponent.h"
#include "GameplayTagContainer.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "GameFramework/Pawn.h"
#include "BwayCharacterWithAbilities.h"
#include "Character/LyraHealthComponent.h"
#include "Abilities/BwayGameplayAbility_RelicPass.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "Relic/RelicActor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayPlayerController)

void ABwayPlayerController::Client_RunCreditDisconnectProbe_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController() && GetWorld() && GetWorld()->GetNetMode() == NM_Client && GetGameInstance())
	{
		UE_LOG(LogTemp, Display, TEXT("BwayCreditLifePeer: Disconnect requested=1 id=%d source=GameInstanceReturnToMainMenu"),
			PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE);
		GetGameInstance()->ReturnToMainMenu();
	}
#endif
}

void ABwayPlayerController::Client_RunRespawnUpgradePurchase_Implementation(int32 ExpectedRank, int32 AuditStage)
{
#if !UE_BUILD_SHIPPING
	ABwayPlayerState* OwnerState = GetPlayerState<ABwayPlayerState>();
	ABwayGameState* State = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	if (!IsLocalController() || !State || GetWorld()->GetNetMode() != NM_Client || !OwnerState
		|| !OwnerState->UpgradeComponent || ExpectedRank < 0 || ExpectedRank > 3 || AuditStage < 1 || AuditStage > 2) return;
	const auto Schedule = OwnerState->GetRespawnSchedule();
	const auto* Health = ULyraHealthComponent::FindHealthComponent(GetPawn());
	UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Request stage=%d expected=%d id=%d world=%s alive=%d deadline=%.6f serverTime=%.6f"),
		AuditStage, ExpectedRank, OwnerState->GetPlayerId(), *GetWorld()->GetPathName(),
		Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0, Schedule.ServerEndTime, State->GetServerWorldTimeSeconds());
	OwnerState->UpgradeComponent->ServerPurchaseUpgrade(TEXT("RespawnTimeReduction"), ExpectedRank);
#endif
}

#if !UE_BUILD_SHIPPING
namespace
{
void TryRemotePassActivation(TWeakObjectPtr<ABwayPlayerController> WeakController, int32 AttemptsRemaining)
{
	ABwayPlayerController* Controller = WeakController.Get();
	ABwayCharacterWithAbilities* LocalCarrier = Controller ? Cast<ABwayCharacterWithAbilities>(Controller->GetPawn()) : nullptr;
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!Controller || !LocalCarrier || !World || World->GetNetMode() != NM_Client)
	{
		return;
	}

	ABwayPlayerState* PlayerState = LocalCarrier->GetPlayerState<ABwayPlayerState>();
	ULyraAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetLyraAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: client pass ASC missing carrier=%s"), *GetNameSafe(LocalCarrier));
		return;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA(UBwayGameplayAbility_RelicPass::StaticClass()))
		{
			const bool bActivated = ASC->TryActivateAbility(Spec.Handle, true);
			UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: ClientPass carrier=%d ability=%s activated=%d attemptsRemaining=%d"),
				LocalCarrier->GetPlayerState() ? LocalCarrier->GetPlayerState()->GetPlayerId() : INDEX_NONE,
				*GetNameSafe(Spec.Ability), bActivated ? 1 : 0, AttemptsRemaining);
			if (bActivated || AttemptsRemaining <= 0)
			{
				return;
			}
			break;
		}
	}

	if (AttemptsRemaining > 0)
	{
		FTimerHandle RetryTimer;
		World->GetTimerManager().SetTimer(
			RetryTimer,
			FTimerDelegate::CreateLambda([WeakController, AttemptsRemaining]()
			{
				TryRemotePassActivation(WeakController, AttemptsRemaining - 1);
			}),
			0.50f,
			false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: client pass ability missing carrier=%s"), *GetNameSafe(LocalCarrier));
	}
}
}
#endif

ABwayPlayerController::ABwayPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if USING_CHEAT_MANAGER
	// Override the cheat manager with our Breakaway-specific one
	CheatClass = UBwayCheatManager::StaticClass();
#endif

	CombatNumberPopComponent = CreateDefaultSubobject<UBwayCombatNumberPopComponent>(TEXT("CombatNumberPopComponent"));
	NameplatePolicyComponent = CreateDefaultSubobject<UBwayNameplatePolicyComponent>(TEXT("NameplatePolicyComponent"));
	RespawnHUDWidgetClass = UBwayRespawnHUDWidget::StaticClass();
}

void ABwayPlayerController::ShowHeroSelect()
{
#if USING_CHEAT_MANAGER
	if (UBwayCheatManager* BwayCheatMgr = Cast<UBwayCheatManager>(CheatManager))
	{
		BwayCheatMgr->ShowHeroSelect();
	}
#endif
}

void ABwayPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* PreviousInput = ShopBoundInputComponent.Get())
	{
		PreviousInput->RemoveBindingByHandle(ShopInputBindingHandle);
	}
	ShopBoundInputComponent.Reset();
	UpgradeShopInputAction = LoadObject<UInputAction>(nullptr,
		TEXT("/BreakawayCore/Input/Actions/IA_BW_UpgradeShop.IA_BW_UpgradeShop"));
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent); EnhancedInput && UpgradeShopInputAction)
	{
		// Started fires once per press. The shared player UI feature context owns
		// the mappings and reinstalls them when Lyra rebuilds input on respawn.
		ShopInputBindingHandle = EnhancedInput->BindAction(UpgradeShopInputAction, ETriggerEvent::Started,
			this, &ThisClass::ToggleUpgradeShop).GetHandle();
		ShopBoundInputComponent = EnhancedInput;
	}
}

void ABwayPlayerController::ToggleUpgradeShop()
{
	if (!IsLocalController() || !GetLocalPlayer()) return;
	if (UpgradeShopWidget && UpgradeShopWidget->IsActivated())
	{
		UpgradeShopWidget->DeactivateWidget();
		return;
	}
	OpenUpgradeShop();
}

void ABwayPlayerController::OpenUpgradeShop()
{
	if (!IsLocalController() || !GetLocalPlayer()) return;
	if (UpgradeShopWidget && UpgradeShopWidget->IsActivated()) return;
	const ABwayGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	if (!GS || !GS->GetRoundManagement() || (HeroSelectionWidget && HeroSelectionWidget->IsActivated()) || ResultsWidget) return;
	const EBwayMatchPhase Phase = GS->GetRoundManagement()->GetCurrentMatchPhase();
	if (Phase == EBwayMatchPhase::None || Phase == EBwayMatchPhase::Prematch || Phase == EBwayMatchPhase::PostMatch) return;

	TSubclassOf<UCommonActivatableWidget> ShopClass = UBwayUpgradeShopWidget::StaticClass();
	const FSoftClassPath WMatchShopPath(TEXT("/BreakawayCore/UI/Match/W_MatchShop.W_MatchShop_C"));
	if (UClass* LoadedClass = WMatchShopPath.TryLoadClass<UCommonActivatableWidget>())
	{
		ShopClass = LoadedClass;
	}
	else
	{
		const FSoftClassPath WFrontEndMatchShopPath(TEXT("/BreakawayCore/UI/FrontEnd/W_MatchShop.W_MatchShop_C"));
		if (UClass* LoadedAltClass = WFrontEndMatchShopPath.TryLoadClass<UCommonActivatableWidget>())
		{
			ShopClass = LoadedAltClass;
		}
	}

	UpgradeShopWidget = Cast<UBwayUpgradeShopWidget>(UCommonUIExtensions::PushContentToLayer_ForPlayer(
		GetLocalPlayer(), FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), ShopClass));
}

void ABwayPlayerController::HideHeroSelect()
{
#if USING_CHEAT_MANAGER
	if (UBwayCheatManager* BwayCheatMgr = Cast<UBwayCheatManager>(CheatManager))
	{
		BwayCheatMgr->HideHeroSelect();
	}
#endif
}

void ABwayPlayerController::ToggleHeroSelect()
{
#if USING_CHEAT_MANAGER
	if (UBwayCheatManager* BwayCheatMgr = Cast<UBwayCheatManager>(CheatManager))
	{
		BwayCheatMgr->ToggleHeroSelect();
	}
#endif
}

void ABwayPlayerController::Client_RequestPreSelectedHero_Implementation()
{
	FPrimaryAssetId PreSelectedHeroId;

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UBwayFrontendStateSubsystem* FrontendState = LocalPlayer->GetSubsystem<UBwayFrontendStateSubsystem>())
		{
			PreSelectedHeroId = FrontendState->GetSelectedHeroId();
		}
	}

	if (PreSelectedHeroId.IsValid())
	{
		const bool bLockSelection = !UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this);
		Server_SubmitPreSelectedHero(PreSelectedHeroId, bLockSelection);
	}
}

void ABwayPlayerController::Server_SubmitPreSelectedHero_Implementation(FPrimaryAssetId PreSelectedHeroId, bool bLockSelection)
{
	if (UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidWorld(this))
	{
		return;
	}

	if (!PreSelectedHeroId.IsValid())
	{
		return;
	}

	if (ABwayPlayerState* BwayPS = GetPlayerState<ABwayPlayerState>())
	{
		if (!BwayPS->IsHeroLocked())
		{
			// A rejected request must not lock the player's previous selection.
			if (!BwayPS->TrySetSelectedHeroId(PreSelectedHeroId))
			{
				return;
			}

			if (bLockSelection)
			{
				BwayPS->ServerLockHeroSelection();
			}
			else if (ABwayGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
			{
				if (UBwayHeroSelectionManager* SelectionManager = GameState->FindComponentByClass<UBwayHeroSelectionManager>())
				{
					SelectionManager->SynchronizePlayerSelectionState(BwayPS);
				}
			}
		}

		if (bLockSelection && GetPawn())
		{
			if (ABreakawayGameMode* BreakawayGameMode = GetWorld() ? Cast<ABreakawayGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
			{
				BreakawayGameMode->ApplyHeroDataToNewPawn(this);
			}
		}
	}
}

bool ABwayPlayerController::CanRetryHeroSelectionTravel() const
{
	const auto* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	return GS && GS->HeroSelectionPhaseComponent && GS->HeroSelectionPhaseComponent->CanRetryPostSelectionTravel(this);
}

bool ABwayPlayerController::RetryHeroSelectionTravel()
{
	auto* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	return GS && GS->HeroSelectionPhaseComponent && GS->HeroSelectionPhaseComponent->RetryPostSelectionTravel(this);
}

void ABwayPlayerController::Client_ShowHeroSelection_Implementation(const TSoftClassPtr<UCommonActivatableWidget>& WidgetClass)
{
	if (WidgetClass.IsNull())
	{
		return;
	}

	TSubclassOf<UCommonActivatableWidget> LoadedClass = WidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerController: Failed to load hero selection widget"));
		return;
	}

	if (HeroSelectionWidget)
	{
		UCommonUIExtensions::PopContentFromLayer(HeroSelectionWidget);
		HeroSelectionWidget = nullptr;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerController: Cannot show hero selection without a local player"));
		return;
	}

	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	HeroSelectionWidget = UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, MenuLayer, LoadedClass);
	if (!HeroSelectionWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerController: Failed to push hero selection widget to the CommonUI layer"));
		return;
	}

	DismissPostRoundSummary();
}

void ABwayPlayerController::Client_HideHeroSelection_Implementation()
{
	if (HeroSelectionWidget)
	{
		UCommonUIExtensions::PopContentFromLayer(HeroSelectionWidget);
		HeroSelectionWidget = nullptr;
	}

	RestoreGameplayInputMode();
	if (const UBwayRoundManagementComponent* RoundMgmt = BoundRoundManagementComponent.Get())
	{
		if (RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound && RoundMgmt->HasActivePostRoundSummary())
		{
			ShowPostRoundSummary(RoundMgmt->GetActivePostRoundSummary());
		}
	}
}

void ABwayPlayerController::RestoreGameplayInputMode()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UCommonUIActionRouterBase* Router = GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>())
	{
		if (Router->GetLeafmostActivatableWidget())
		{
			Router->RefreshUIInputConfig();
			Router->RefreshActiveRootFocus();
			return;
		}
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);
}

void ABwayPlayerController::ResetIgnoreInputFlags()
{
	Super::ResetIgnoreInputFlags();

	// ClientRestart clears the controller's counted locks, even when a menu
	// opened before possession completed. CommonUI only updates those counts
	// when its desired config changes; a forced refresh alone cannot repair it.
	// Reapply the active screen's contribution once after the reset so closing
	// that screen still balances the counts and restores gameplay.
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UCommonUIActionRouterBase* Router = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>())
		{
			if (const UCommonActivatableWidget* Leaf = Router->GetLeafmostActivatableWidget())
			{
				if (const TOptional<FUIInputConfig> Config = Leaf->GetDesiredInputConfig(); Config.IsSet())
				{
					if (Config->bIgnoreMoveInput) SetIgnoreMoveInput(true);
					if (Config->bIgnoreLookInput) SetIgnoreLookInput(true);
					if (Config->bIgnoreMoveInput || Config->bIgnoreLookInput)
					{
						UE_LOG(LogTemp, Display, TEXT("BwayInput: Restored active UI locks after pawn input reset screen=%s move=%d look=%d"),
							*GetNameSafe(Leaf), IsMoveInputIgnored(), IsLookInputIgnored());
					}
				}
			}
		}
	}
}

void ABwayPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		BindPostRoundSummaryListeners();
		RefreshRespawnHUD();
	}
}

void ABwayPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshRespawnHUD();
}

void ABwayPlayerController::RefreshRespawnHUD()
{
	if (!HasActorBegunPlay() || !IsLocalPlayerController() || !GetLocalPlayer()) return;
	if (!RespawnHUDWidget && RespawnHUDWidgetClass)
	{
		RespawnHUDWidget = CreateWidget<UBwayRespawnHUDWidget>(this, RespawnHUDWidgetClass);
		if (RespawnHUDWidget) RespawnHUDWidget->AddToPlayerScreen(20);
	}
	if (RespawnHUDWidget) RespawnHUDWidget->RefreshPlayerState();
}

void ABwayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopArenaIntroFlyThrough();
	if (RespawnHUDWidget) RespawnHUDWidget->RemoveFromParent();
	RespawnHUDWidget = nullptr;
	if (UpgradeShopWidget && UpgradeShopWidget->IsActivated()) UpgradeShopWidget->DeactivateWidget();
	if (UEnhancedInputComponent* BoundInput = ShopBoundInputComponent.Get()) BoundInput->RemoveBindingByHandle(ShopInputBindingHandle);
	ShopBoundInputComponent.Reset();
	if (IsLocalPlayerController())
	{
		UnbindPostRoundSummaryListeners();
		DismissPostRoundSummary();
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayPlayerController::BindPostRoundSummaryListeners()
{
	UnbindPostRoundSummaryListeners();

	if (UWorld* World = GetWorld())
	{
		if (ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			if (UBwayRoundManagementComponent* RoundMgmt = GameState->FindComponentByClass<UBwayRoundManagementComponent>())
			{
				BoundRoundManagementComponent = RoundMgmt;
				RoundMgmt->OnPostRoundSummaryStarted.AddDynamic(this, &ABwayPlayerController::HandlePostRoundSummaryStarted);
				RoundMgmt->OnMatchPhaseChanged.AddDynamic(this, &ABwayPlayerController::HandleMatchPhaseChanged);

				if (RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound && RoundMgmt->HasActivePostRoundSummary())
				{
					ShowPostRoundSummary(RoundMgmt->GetActivePostRoundSummary());
				}
			}
		}
	}
}

void ABwayPlayerController::UnbindPostRoundSummaryListeners()
{
	if (UBwayRoundManagementComponent* RoundMgmt = BoundRoundManagementComponent.Get())
	{
		RoundMgmt->OnPostRoundSummaryStarted.RemoveDynamic(this, &ABwayPlayerController::HandlePostRoundSummaryStarted);
		RoundMgmt->OnMatchPhaseChanged.RemoveDynamic(this, &ABwayPlayerController::HandleMatchPhaseChanged);
	}

	BoundRoundManagementComponent.Reset();
}

void ABwayPlayerController::HandlePostRoundSummaryStarted(FBwayPostRoundSummaryData SummaryData)
{
	if (UBwayRoundManagementComponent* RoundMgmt = BoundRoundManagementComponent.Get())
	{
		if (RoundMgmt->GetCurrentMatchPhase() != EBwayMatchPhase::PostRound)
		{
			return;
		}
	}

	ShowPostRoundSummary(SummaryData);
}

void ABwayPlayerController::HandleMatchPhaseChanged(EBwayMatchPhase NewPhase)
{
	StopArenaIntroFlyThrough();

	if (NewPhase == EBwayMatchPhase::Warmup)
	{
		const UBwayRoundManagementComponent* RoundMgmt = BoundRoundManagementComponent.Get();
		const bool bIsArenaIntroMatch = (RoundMgmt && RoundMgmt->HasArenaIntroPlayed())
			|| UBwayGameplayUrlLibrary::HasGameplayUrlOption(this, TEXT("ArenaIntro"));
		if (bIsArenaIntroMatch && IsLocalController())
		{
			OpenUpgradeShop();
		}
	}
	else if (UpgradeShopWidget && UpgradeShopWidget->IsActivated())
	{
		UpgradeShopWidget->DeactivateWidget();
	}

	if (NewPhase != EBwayMatchPhase::PostRound)
	{
		DismissPostRoundSummary();
	}
}

void ABwayPlayerController::Client_StartArenaIntroFlyThrough_Implementation(float DurationSeconds)
{
	if (!IsLocalController())
	{
		return;
	}

	Client_HideHeroSelection_Implementation();
	StopArenaIntroFlyThrough();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ArenaIntroTotalDuration = FMath::Max(0.5f, DurationSeconds);
	ArenaIntroStartTime = FPlatformTime::Seconds();

	// Resolve Relic center location for the fly-through focal point
	ArenaIntroCenterLocation = FVector(0.f, 0.f, 200.f);
	if (const ABwayGameState* GS = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRelicManagerComponent* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>())
		{
			if (const ARelicActor* Relic = RelicMgr->GetRelicActor())
			{
				ArenaIntroCenterLocation = Relic->GetActorLocation();
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ArenaIntroCameraActor = World->SpawnActor<ACameraActor>(
		ArenaIntroCenterLocation + FVector(-1400.f, -900.f, 650.f),
		FRotator(-22.f, 32.f, 0.f),
		SpawnParams);

	if (ArenaIntroCameraActor && ArenaIntroCameraActor->GetCameraComponent())
	{
		ArenaIntroCameraActor->GetCameraComponent()->bConstrainAspectRatio = false;
		SetViewTargetWithBlend(ArenaIntroCameraActor, 0.35f);
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.85f, FLinearColor::Black, false, false);
	}

	UE_LOG(LogTemp, Log, TEXT("BwayArenaIntro: Announcer hook triggered — Welcome to The Shrine of Auros (%.1fs fly-through)"), ArenaIntroTotalDuration);

	TickArenaIntroFlyThrough();
	World->GetTimerManager().SetTimer(
		ArenaIntroTickTimerHandle,
		this,
		&ABwayPlayerController::TickArenaIntroFlyThrough,
		0.016f,
		true);
}

void ABwayPlayerController::TickArenaIntroFlyThrough()
{
	if (!ArenaIntroCameraActor)
	{
		StopArenaIntroFlyThrough();
		return;
	}

	const double Elapsed = FPlatformTime::Seconds() - ArenaIntroStartTime;
	const float Alpha = FMath::Clamp(static_cast<float>(Elapsed) / FMath::Max(0.1f, ArenaIntroTotalDuration), 0.0f, 1.0f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 1.6f);

	// Sweeping orbital pan across the Shrine of Auros / center Relic pedestal
	const float AngleDeg = FMath::Lerp(-55.0f, 55.0f, SmoothAlpha);
	const float AngleRad = FMath::DegreesToRadians(AngleDeg);
	const float Radius = FMath::Lerp(1650.0f, 1150.0f, SmoothAlpha);
	const float Height = FMath::Lerp(680.0f, 420.0f, SmoothAlpha);

	const FVector CamPos = ArenaIntroCenterLocation + FVector(
		-FMath::Cos(AngleRad) * Radius,
		FMath::Sin(AngleRad) * Radius,
		Height);
	const FVector LookTarget = ArenaIntroCenterLocation + FVector(0.f, 0.f, 90.f);
	const FRotator CamRot = (LookTarget - CamPos).Rotation();

	ArenaIntroCameraActor->SetActorLocationAndRotation(CamPos, CamRot);
}

void ABwayPlayerController::StopArenaIntroFlyThrough()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArenaIntroTickTimerHandle);
	}

	if (ArenaIntroCameraActor)
	{
		if (GetViewTarget() == ArenaIntroCameraActor && GetPawn())
		{
			SetViewTargetWithBlend(GetPawn(), 0.25f);
		}
		ArenaIntroCameraActor->Destroy();
		ArenaIntroCameraActor = nullptr;
	}
}

TSubclassOf<UUserWidget> ABwayPlayerController::ResolvePostRoundSummaryWidgetClass() const
{
	if (const ABwayGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
	{
		if (!GameState->PostRoundSummaryWidgetClass.IsNull())
		{
			if (UClass* LoadedClass = GameState->PostRoundSummaryWidgetClass.LoadSynchronous())
			{
				return LoadedClass;
			}
		}
	}

	const TSoftClassPtr<UBwayPostRoundSummaryWidget> DefaultWidgetClass(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Match/WBP_BW_PostRoundSummary.WBP_BW_PostRoundSummary_C")));

	return DefaultWidgetClass.LoadSynchronous();
}

void ABwayPlayerController::ShowPostRoundSummary(const FBwayPostRoundSummaryData& SummaryData)
{
	// A late arrival must be able to finish choosing a hero while rounds continue.
	if (HeroSelectionWidget && HeroSelectionWidget->IsActivated())
	{
		return;
	}

	const TSubclassOf<UUserWidget> WidgetClass = ResolvePostRoundSummaryWidgetClass();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerController: PostRound summary widget class not configured"));
		return;
	}

	if (!PostRoundSummaryWidget)
	{
		PostRoundSummaryWidget = CreateWidget<UBwayPostRoundSummaryWidget>(this, WidgetClass);
		if (!PostRoundSummaryWidget)
		{
			return;
		}

		PostRoundSummaryWidget->AddToViewport(150);
	}

	FBwayPostRoundSummaryData ResolvedSummary = SummaryData;
	if (UBwayRoundManagementComponent* RoundMgmt = BoundRoundManagementComponent.Get())
	{
		if (RoundMgmt->HasActivePostRoundSummary())
		{
			ResolvedSummary = RoundMgmt->GetActivePostRoundSummary();
		}
	}

	ResolvedSummary = UBwayMatchStatsLibrary::BuildPostRoundSummaryData(
		this,
		ResolvedSummary.CompletedRoundNumber,
		ResolvedSummary.RoundWinningTeam,
		ResolvedSummary.DisplayDurationSeconds);

	PostRoundSummaryWidget->ApplySummaryData(ResolvedSummary);

	UE_LOG(LogTemp, Log, TEXT("BwayPlayerController: PostRound summary shown (round %d, winner team %d)"),
		ResolvedSummary.CompletedRoundNumber, ResolvedSummary.RoundWinningTeam + 1);
}

void ABwayPlayerController::DismissPostRoundSummary()
{
	if (PostRoundSummaryWidget)
	{
		PostRoundSummaryWidget->RemoveFromParent();
		PostRoundSummaryWidget = nullptr;
	}
}

void ABwayPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RefreshRespawnHUD();

	if (!IsLocalPlayerController() || !InPawn)
	{
		return;
	}

	if (ResultsWidget && ResultsWidget->IsInViewport())
	{
		return;
	}

	RestoreGameplayInputMode();
}

void ABwayPlayerController::Client_ShowResults_Implementation(
	const FBwayPostMatchSummaryData& Summary,
	const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	Client_HideHeroSelection_Implementation();
	DismissPostRoundSummary();

	if (WidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerController: Client_ShowResults called with null widget class"));
		return;
	}

	UClass* LoadedClass = WidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerController: Client_ShowResults failed to load widget class"));
		return;
	}

	// Clear any previous results widget before creating a new one.
	if (ResultsWidget)
	{
		ResultsWidget->RemoveFromParent();
		ResultsWidget = nullptr;
	}

	ResultsWidget = CreateWidget<UUserWidget>(this, LoadedClass);
	if (!ResultsWidget)
	{
		return;
	}

	ResultsWidget->AddToViewport(200);

	SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);

	// The collapsed orchestration widget is not an input target. Its visible
	// interstitial and breakdown take focus after their data and controls exist.
	if (UBwayResultsScreenWidget* ResultsScreen = Cast<UBwayResultsScreenWidget>(ResultsWidget))
	{
		ResultsScreen->ApplyAuthoritativeResults(Summary);
	}
	else if (ResultsWidget->IsFocusable())
	{
		ResultsWidget->SetUserFocus(this);
	}

	UE_LOG(LogTemp, Log, TEXT("BwayPlayerController: Results screen shown — Team %d wins (%d-%d, %d rounds)"),
		Summary.WinningTeam + 1, Summary.Team0Score, Summary.Team1Score, Summary.TotalRounds);
}

void ABwayPlayerController::Server_RequestReturnToFrontEnd_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (ABwayGameState* GS = World->GetGameState<ABwayGameState>())
		{
			const UBwayRoundManagementComponent* RoundMgmt = GS->GetRoundManagement();
			if (!RoundMgmt || RoundMgmt->GetCurrentMatchPhase() != EBwayMatchPhase::PostMatch)
			{
				UE_LOG(LogTemp, Warning, TEXT("BwayPlayerController: Ignoring results return request outside PostMatch"));
				return;
			}
			GS->ReturnToFrontEnd();
		}
	}
}

void ABwayPlayerController::Server_RequestPlayAgain_Implementation()
{
	if (ABwayGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
	{
		GS->RestartMatchFromResults();
	}
}

void ABwayPlayerController::Client_DismissResultsScreen_Implementation()
{
	if (ResultsWidget)
	{
		ResultsWidget->RemoveFromParent();
		ResultsWidget = nullptr;
	}

	RestoreGameplayInputMode();
}

void ABwayPlayerController::Client_RunTeamPeerConvergence_Implementation(int32 EnemyPlayerId)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	ABwayCharacterWithAbilities* EnemyCharacter = nullptr;
	for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
	{
		ABwayCharacterWithAbilities* Candidate = *It;
		if (Candidate && Candidate->GetPlayerState() && Candidate->GetPlayerState()->GetPlayerId() == EnemyPlayerId)
		{
			EnemyCharacter = Candidate;
			break;
		}
	}

	const ULyraHealthComponent* Health = EnemyCharacter ? ULyraHealthComponent::FindHealthComponent(EnemyCharacter) : nullptr;
	const float EnemyHealth = Health ? Health->GetHealth() : -1.0f;
	const int32 EnemyTeam = (GameState && EnemyCharacter && EnemyCharacter->GetPlayerState())
		? GameState->GetPlayerTeam(EnemyCharacter->GetPlayerState()) : INDEX_NONE;
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: PeerConvergenceClient world=%s netMode=%d local=%d enemy=%d found=%d health=%.1f team=%d"),
		*GetPathNameSafe(World), static_cast<int32>(World->GetNetMode()), IsLocalController() ? 1 : 0,
		EnemyPlayerId, EnemyCharacter ? 1 : 0, EnemyHealth, EnemyTeam);
#else
	UE_UNUSED(EnemyPlayerId);
#endif
}

void ABwayPlayerController::Client_RunTeamRemoteAction_Implementation(int32 TeammatePlayerId, int32 EnemyPlayerId, bool bPassPhase)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	ABwayCharacterWithAbilities* Carrier = Cast<ABwayCharacterWithAbilities>(GetPawn());
	if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Client || !Carrier)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: client action unavailable world=%s local=%d carrier=%s"),
			*GetPathNameSafe(World), IsLocalController() ? 1 : 0, *GetNameSafe(Carrier));
		return;
	}

	auto FindCharacterByPlayerId = [World](int32 PlayerId) -> ABwayCharacterWithAbilities*
	{
		for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
		{
			ABwayCharacterWithAbilities* Candidate = *It;
			if (Candidate && Candidate->GetPlayerState() && Candidate->GetPlayerState()->GetPlayerId() == PlayerId)
			{
				return Candidate;
			}
		}
		return nullptr;
	};

	if (!bPassPhase)
	{
		ABwayCharacterWithAbilities* Enemy = FindCharacterByPlayerId(EnemyPlayerId);
		if (!Enemy)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: client damage target missing enemy=%d"), EnemyPlayerId);
			return;
		}

		const FRotator AimRotation = (Enemy->GetActorLocation() - Carrier->GetActorLocation()).Rotation();
		Carrier->SetActorRotation(AimRotation);
		SetControlRotation(AimRotation);
		TWeakObjectPtr<ABwayPlayerController> WeakController(this);
		FTimerHandle DamageTimer;
		World->GetTimerManager().SetTimer(
			DamageTimer,
			FTimerDelegate::CreateLambda([WeakController]()
			{
				ABwayPlayerController* Controller = WeakController.Get();
				ABwayCharacterWithAbilities* LocalCarrier = Controller ? Cast<ABwayCharacterWithAbilities>(Controller->GetPawn()) : nullptr;
				if (!Controller || !LocalCarrier || !Controller->GetWorld() || Controller->GetWorld()->GetNetMode() != NM_Client)
				{
					return;
				}

				ULyraAbilitySystemComponent* ASC = LocalCarrier->GetLyraAbilitySystemComponent();
				if (!ASC)
				{
					UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: client damage ASC missing carrier=%s"), *GetNameSafe(LocalCarrier));
					return;
				}

				for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
				{
					const FString AbilityName = GetNameSafe(Spec.Ability);
					if (BwayCombatFixture::IsPrimaryAbility(Spec.Ability))
					{
						const bool bActivated = ASC->TryActivateAbility(Spec.Handle, false);
						UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: ClientDamage carrier=%d ability=%s activated=%d"),
							LocalCarrier->GetPlayerState() ? LocalCarrier->GetPlayerState()->GetPlayerId() : INDEX_NONE,
							*AbilityName, bActivated ? 1 : 0);
						return;
					}
				}

				UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: client damage ability missing carrier=%s"), *GetNameSafe(LocalCarrier));
			}),
			0.50f,
			false);
		return;
	}

	ABwayCharacterWithAbilities* Teammate = FindCharacterByPlayerId(TeammatePlayerId);
	if (!Teammate)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: client pass target missing teammate=%d"), TeammatePlayerId);
		return;
	}

	const FRotator AimRotation = (Teammate->GetActorLocation() - Carrier->GetActorLocation()).Rotation();
	Carrier->SetActorRotation(AimRotation);
	SetControlRotation(AimRotation);
	Server_SetTeamRemoteAim(AimRotation, TeammatePlayerId);
	TWeakObjectPtr<ABwayPlayerController> WeakController(this);
	FTimerHandle PassTimer;
	World->GetTimerManager().SetTimer(
		PassTimer,
		FTimerDelegate::CreateLambda([WeakController]()
		{
			TryRemotePassActivation(WeakController, 8);
		}),
			1.00f,
		false);
#else
	UE_UNUSED(TeammatePlayerId);
	UE_UNUSED(EnemyPlayerId);
		UE_UNUSED(bPassPhase);
#endif
}

void ABwayPlayerController::Server_SetTeamRemoteAim_Implementation(FRotator AimRotation, int32 TeammatePlayerId)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || AimRotation.ContainsNaN())
	{
		return;
	}

	AimRotation.Normalize();
	if (APawn* ControlledPawn = GetPawn())
	{
		for (TActorIterator<ABwayCharacterWithAbilities> It(GetWorld()); It; ++It)
		{
			ABwayCharacterWithAbilities* Candidate = *It;
			if (Candidate && Candidate->GetPlayerState() && Candidate->GetPlayerState()->GetPlayerId() == TeammatePlayerId)
			{
				// The fixture positions the intended recipient on the client aim ray
				// immediately before server-only activation. Reapply that authoritative
				// placement here so adverse-latency movement correction cannot move the
				// recipient out of the production pass cone.
				const FVector AimDirection = AimRotation.Vector().GetSafeNormal2D();
				if (!AimDirection.IsNearlyZero())
				{
					Candidate->SetActorLocation(ControlledPawn->GetActorLocation() + AimDirection * 400.0f, false, nullptr, ETeleportType::TeleportPhysics);
					Candidate->SetActorRotation(AimRotation);
				}
				break;
			}
		}
	}
	SetControlRotation(AimRotation);
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->SetActorRotation(AimRotation);
	}
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: ServerAim player=%d pitch=%.1f yaw=%.1f"),
		GetPlayerState<APlayerState>() ? GetPlayerState<APlayerState>()->GetPlayerId() : INDEX_NONE,
		AimRotation.Pitch, AimRotation.Yaw);
#else
	UE_UNUSED(AimRotation);
	UE_UNUSED(TeammatePlayerId);
#endif
}
