// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayMatchBreakdownWidget.h"
#include "CommonButtonBase.h"

#include "Stats/BwayMatchStatsLibrary.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchBreakdownWidget)

UBwayMatchBreakdownWidget::UBwayMatchBreakdownWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

UWidget* UBwayMatchBreakdownWidget::NativeGetDesiredFocusTarget() const
{
	if (Btn_ReturnToLobby && Btn_ReturnToLobby->GetIsEnabled() && Btn_ReturnToLobby->IsVisible())
	{
		return Btn_ReturnToLobby;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UBwayMatchBreakdownWidget::ApplyBreakdownData(FBwayPostMatchSummaryData SummaryData)
{
	const int32 LocalTeam = SummaryData.LocalPlayerTeamIndex;
	CachedSummary = UBwayMatchStatsLibrary::RemapPostMatchSummaryForDisplay(SummaryData, LocalTeam);
	OnBreakdownReady(CachedSummary);
}

void UBwayMatchBreakdownWidget::RequestReturnToLobby()
{
	OnReturnToLobbyRequested.Broadcast();
}

void UBwayMatchBreakdownWidget::RequestPlayAgain()
{
	OnPlayAgainRequested.Broadcast();
}
