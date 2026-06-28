// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayMatchBreakdownWidget.h"

#include "Stats/BwayMatchStatsLibrary.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchBreakdownWidget)

UBwayMatchBreakdownWidget::UBwayMatchBreakdownWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayMatchBreakdownWidget::ApplyBreakdownData(FBwayPostMatchSummaryData SummaryData)
{
	const int32 LocalTeam = UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(this);
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
