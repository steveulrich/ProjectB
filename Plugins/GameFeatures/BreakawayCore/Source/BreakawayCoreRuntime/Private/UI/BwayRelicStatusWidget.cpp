// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayRelicStatusWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRelicStatusWidget)

UBwayRelicStatusWidget::UBwayRelicStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayRelicStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshRelicStatus();

	if (RelicPollInterval > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RelicPollHandle,
				this,
				&UBwayRelicStatusWidget::RefreshRelicStatus,
				RelicPollInterval,
				true);
		}
	}
}

void UBwayRelicStatusWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RelicPollHandle);
	}

	Super::NativeDestruct();
}

void UBwayRelicStatusWidget::RefreshRelicStatus()
{
	const int32 LocalTeam = GetLocalPlayerTeamForWidget(this);
	const int32 DisplayPossessingTeam = GetDisplayRelicPossessingTeam(GetWorld(), LocalTeam);
	const bool bIsCarried = IsRelicCarriedInWorld(GetWorld());
	const FText StatusText = BuildStatusText(DisplayPossessingTeam);
	const FText CarrierName = bIsCarried ? GetRelicCarrierNameFromWorld(GetWorld()) : FText::GetEmpty();

	const bool bStateChanged =
		DisplayPossessingTeam != LastPossessingTeamIndex
		|| bIsCarried != bLastIsCarried
		|| !CarrierName.EqualTo(LastCarrierName);

	if (!bStateChanged && LastPossessingTeamIndex != INDEX_NONE)
	{
		return;
	}

	LastPossessingTeamIndex = DisplayPossessingTeam;
	bLastIsCarried = bIsCarried;
	LastCarrierName = CarrierName;

	UpdateBoundStatusTexts(DisplayPossessingTeam, bIsCarried, StatusText, CarrierName);
	OnRelicStatusUpdated(DisplayPossessingTeam, bIsCarried, StatusText, CarrierName);
}

FText UBwayRelicStatusWidget::BuildStatusText(const int32 DisplayPossessingTeam) const
{
	switch (DisplayPossessingTeam)
	{
	case 0:
		return Team1StatusText;
	case 1:
		return Team2StatusText;
	default:
		return NeutralStatusText;
	}
}

void UBwayRelicStatusWidget::UpdateBoundStatusTexts(
	int32 PossessingTeamIndex,
	bool bIsCarried,
	const FText& StatusText,
	const FText& CarrierName)
{
	if (Text_Status)
	{
		Text_Status->SetText(StatusText);
	}

	if (Text_Carrier)
	{
		if (bIsCarried && !CarrierName.IsEmpty())
		{
			Text_Carrier->SetText(CarrierName);
			Text_Carrier->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_Carrier->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
