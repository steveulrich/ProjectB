// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayTeamPortraitsHUDWidget.h"
#include "UI/BwayHUDHelpers.h"
#include "Components/Image.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayTeamPortraitsHUDWidget)

UBwayTeamPortraitsHUDWidget::UBwayTeamPortraitsHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayTeamPortraitsHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UBwayTeamPortraitsHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (PortraitRefreshInterval > 0.0f)
	{
		TimeSinceLastPortraitRefresh += InDeltaTime;
		if (TimeSinceLastPortraitRefresh >= PortraitRefreshInterval)
		{
			TimeSinceLastPortraitRefresh = 0.0f;
			RefreshTeamPortraits();
		}
	}
}

void UBwayTeamPortraitsHUDWidget::NotifyRoundStateChanged(FName NewState)
{
	RefreshTeamPortraits();
}

void UBwayTeamPortraitsHUDWidget::RefreshTeamPortraits()
{
	const int32 LocalTeam = GetLocalPlayerTeamForWidget(this);

	if (const TArray<UImage*> Team1Images = GetTeamPortraitImages(0); Team1Images.Num() > 0)
	{
		UBwayHUDHelpers::UpdateTeamPortraitsForDisplaySlot(this, 0, LocalTeam, Team1Images, EmptyPortraitTexture);
	}

	if (const TArray<UImage*> Team2Images = GetTeamPortraitImages(1); Team2Images.Num() > 0)
	{
		UBwayHUDHelpers::UpdateTeamPortraitsForDisplaySlot(this, 1, LocalTeam, Team2Images, EmptyPortraitTexture);
	}
}

TArray<UImage*> UBwayTeamPortraitsHUDWidget::GetTeamPortraitImages(int32 DisplaySlotIndex) const
{
	TArray<UImage*> Images;
	if (DisplaySlotIndex == 0)
	{
		if (Team1_Portrait_1) { Images.Add(Team1_Portrait_1); }
		if (Team1_Portrait_2) { Images.Add(Team1_Portrait_2); }
		if (Team1_Portrait_3) { Images.Add(Team1_Portrait_3); }
		if (Team1_Portrait_4) { Images.Add(Team1_Portrait_4); }
	}
	else if (DisplaySlotIndex == 1)
	{
		if (Team2_Portrait_1) { Images.Add(Team2_Portrait_1); }
		if (Team2_Portrait_2) { Images.Add(Team2_Portrait_2); }
		if (Team2_Portrait_3) { Images.Add(Team2_Portrait_3); }
		if (Team2_Portrait_4) { Images.Add(Team2_Portrait_4); }
	}

	return Images;
}
