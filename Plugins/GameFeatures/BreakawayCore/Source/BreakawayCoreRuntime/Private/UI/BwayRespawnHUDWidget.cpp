#include "UI/BwayRespawnHUDWidget.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "PrimaryGameLayout.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRespawnHUDWidget)

UBwayRespawnHUDWidget::UBwayRespawnHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UBwayRespawnHUDWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>();
		WidgetTree->RootWidget = SafeZone;
		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
		SafeZone->AddChild(Overlay);
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetPadding(FMargin(28.0f, 16.0f));
		Panel->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.045f, 0.92f));
		UOverlaySlot* Center = Overlay->AddChildToOverlay(Panel);
		Center->SetHorizontalAlignment(HAlign_Center);
		Center->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Panel->AddChild(Column);
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
		Title->SetText(NSLOCTEXT("BwayRespawn", "Defeated", "You were defeated"));
		Title->SetJustification(ETextJustify::Center);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.85f, 0.9f)));
		Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 8));
		Text_RespawnStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RespawnStatus"));
		Text_RespawnStatus->SetJustification(ETextJustify::Center);
		Text_RespawnStatus->SetMinDesiredWidth(260.0f);
		Text_RespawnStatus->SetAutoWrapText(true);
		FSlateFontInfo Font = Text_RespawnStatus->GetFont();
		Font.Size = 28;
		Text_RespawnStatus->SetFont(Font);
		Column->AddChildToVerticalBox(Text_RespawnStatus);
	}
	return Super::RebuildWidget();
}

void UBwayRespawnHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshPlayerState();
}

void UBwayRespawnHUDWidget::NativeDestruct()
{
	UnbindPlayerState();
	Super::NativeDestruct();
}

void UBwayRespawnHUDWidget::UnbindPlayerState()
{
	if (BoundPlayerState.IsValid())
		BoundPlayerState->OnRespawnScheduleChanged.RemoveDynamic(this, &ThisClass::HandleScheduleChanged);
	BoundPlayerState.Reset();
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(CountdownTimer);
}

void UBwayRespawnHUDWidget::RefreshPlayerState()
{
	ABwayPlayerState* Player = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (BoundPlayerState.Get() != Player)
	{
		UnbindPlayerState();
		BoundPlayerState = Player;
		if (Player) Player->OnRespawnScheduleChanged.AddUniqueDynamic(this, &ThisClass::HandleScheduleChanged);
	}
	HandleScheduleChanged();
}

void UBwayRespawnHUDWidget::HandleScheduleChanged()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
		if (BoundPlayerState.IsValid() && BoundPlayerState->GetRespawnSchedule().ServerEndTime > 0.0)
			World->GetTimerManager().SetTimer(CountdownTimer, this, &ThisClass::RefreshDisplay, 0.1f, true);
	}
	RefreshDisplay();
}

FText UBwayRespawnHUDWidget::GetDisplayedStatus() const
{
	return Text_RespawnStatus ? Text_RespawnStatus->GetText() : FText::GetEmpty();
}

bool UBwayRespawnHUDWidget::IsSuppressedByMenu() const
{
	if (UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningPlayer()))
	{
		for (const TCHAR* LayerName : {TEXT("UI.Layer.Menu"), TEXT("UI.Layer.Modal")})
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(LayerName), false);
			if (UCommonActivatableWidgetContainerBase* Layer = Tag.IsValid() ? Layout->GetLayerWidget(Tag) : nullptr)
			{
				if (const UCommonActivatableWidget* Active = Layer->GetActiveWidget(); Active && Active->IsActivated() && Active->IsVisible()) return true;
			}
		}
	}
	return false;
}

void UBwayRespawnHUDWidget::RefreshDisplay()
{
	const ABwayGameState* State = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	const UBwayRoundManagementComponent* Rounds = State ? State->GetRoundManagement() : nullptr;
	const FBwayRespawnSchedule Schedule = BoundPlayerState.IsValid() ? BoundPlayerState->GetRespawnSchedule() : FBwayRespawnSchedule();
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const bool bShown = LocalPlayer && !IsSuppressedByMenu()
		&& Rounds && Schedule.ServerEndTime > 0.0
		&& Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
		&& Schedule.RoundNumber == Rounds->GetCurrentRoundNumber();
	const int32 Seconds = bShown ? FMath::CeilToInt(FMath::Max(0.0, Schedule.ServerEndTime - State->GetServerWorldTimeSeconds())) : -1;
	SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Text_RespawnStatus)
	{
		Text_RespawnStatus->SetText(!bShown ? FText::GetEmpty() : Seconds > 0
			? FText::Format(NSLOCTEXT("BwayRespawn", "Countdown", "Respawning in {0}"), FText::AsNumber(Seconds))
			: NSLOCTEXT("BwayRespawn", "WaitingForServer", "Respawning..."));
	}
	if (DisplayedSeconds != Seconds)
	{
		DisplayedSeconds = Seconds;
		OnCountdownChanged(Seconds, bShown);
	}
}
