#pragma once

#include "BreakawayGameMode.h"
#include "BwayPlayerController.h"
#include "BwayLateArrivalSelectionTestTypes.generated.h"

// Automation boundary observers: production selection/roster policy is unchanged.
// These transient types replace viewport presentation and actual map loading only.
UCLASS(Transient, NotBlueprintable)
class ABwayLateArrivalTestController : public ABwayPlayerController
{
	GENERATED_BODY()
public:
	int32 SelectionShows = 0;
	int32 SelectionHides = 0;
	int32 ResultsShows = 0;
	virtual void Client_RequestPreSelectedHero_Implementation() override {}
	virtual void Client_ShowHeroSelection_Implementation(const TSoftClassPtr<UCommonActivatableWidget>& WidgetClass) override { ++SelectionShows; }
	virtual void Client_HideHeroSelection_Implementation() override { ++SelectionHides; }
	virtual void Client_ShowResults_Implementation(const FBwayPostMatchSummaryData& Summary, const TSoftClassPtr<UUserWidget>& WidgetClass) override { ++ResultsShows; }
};

UCLASS(Transient, NotBlueprintable)
class ABwayLateArrivalTestGameMode : public ABreakawayGameMode
{
	GENERATED_BODY()
public:
	int32 TravelRequests = 0;
	int32 TravelDispatches = 0;
	bool bRejectTravel = false;
	FString DispatchedURL;
	virtual bool CanServerTravel(const FString& URL, bool bAbsolute) override
	{
		++TravelRequests;
		return Super::CanServerTravel(URL, bAbsolute) && !bRejectTravel;
	}
	virtual void ProcessServerTravel(const FString& URL, bool bAbsolute) override
	{
		++TravelDispatches;
		DispatchedURL = URL;
	}
	// The disposable authority fixture supplies PlayerState/ownership explicitly.
	// HUD, voice, streaming and local viewport setup are outside this regression.
	virtual void GenericPlayerInitialization(AController* Player) override {}
};
