// Copyright Epic Games, Inc. All Rights Reserved.

#include "BwayGoalVolume.h"
#include "Relic/RelicActor.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
/** Carried relics have no collision (attached to mesh), so only the carrier pawn overlaps the goal. */
ARelicActor* ResolveRelicForGoalOverlap(AActor* OtherActor, UWorld* World)
{
	if (!OtherActor || !World)
	{
		return nullptr;
	}

	if (ARelicActor* Relic = Cast<ARelicActor>(OtherActor))
	{
		return Relic;
	}

	const ABwayCharacterWithAbilities* Carrier = Cast<ABwayCharacterWithAbilities>(OtherActor);
	if (!Carrier)
	{
		return nullptr;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayRelicManagerComponent* RelicMgr = GameState->RelicManagerComponent)
		{
			if (ARelicActor* Relic = RelicMgr->GetRelicActor())
			{
				if (Relic->CurrentCarrier == Carrier && Relic->GetCurrentState() == ERelicState::Carried)
				{
					return Relic;
				}
			}
		}
	}

	return nullptr;
}
}

ABwayGoalVolume::ABwayGoalVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Create trigger volume
	GoalTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("GoalTrigger"));
	GoalTrigger->SetupAttachment(RootComponent);
	GoalTrigger->SetBoxExtent(FVector(100.0f, 200.0f, 200.0f));
	GoalTrigger->SetCollisionProfileName(TEXT("OverlapAll"));
	GoalTrigger->SetGenerateOverlapEvents(true);

	// Create optional visual mesh
	GoalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalMesh"));
	GoalMesh->SetupAttachment(RootComponent);
	GoalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Set replication
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ABwayGoalVolume::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap event
	if (GoalTrigger)
	{
		GoalTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABwayGoalVolume::OnGoalOverlapBegin);
	}

	RefreshTeamColor();

	UE_LOG(LogTemp, Log, TEXT("Goal Volume initialized for Team %d at location %s"),
		OwningTeam + 1, *GetActorLocation().ToString());
}

void ABwayGoalVolume::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayGoalVolume, OwningTeam);
}

void ABwayGoalVolume::SetOwningTeam(int32 NewTeam)
{
	if (!HasAuthority())
	{
		return;
	}
	OwningTeam = NewTeam;
	RefreshTeamColor();
	ForceNetUpdate();
}

void ABwayGoalVolume::OnRep_OwningTeam()
{
	RefreshTeamColor();
}

void ABwayGoalVolume::RefreshTeamColor()
{
	if (GoalMesh && GoalMesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(GoalMesh->GetMaterial(0));
		if (!DynMat)
		{
			DynMat = GoalMesh->CreateDynamicMaterialInstance(0);
		}
		if (DynMat)
		{
			FLinearColor TeamColor = (OwningTeam == 0) ? FLinearColor::Blue : FLinearColor::Red;
			DynMat->SetVectorParameterValue(FName("TeamColor"), TeamColor);
		}
	}
}

void ABwayGoalVolume::OnGoalOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only process on server
	if (!HasAuthority())
	{
		return;
	}

	// Prevent double-scoring
	if (bIsProcessingScore)
	{
		return;
	}

	// Thrown/dropped relic overlaps directly; carried relic overlaps via the carrier pawn.
	ARelicActor* Relic = ResolveRelicForGoalOverlap(OtherActor, GetWorld());
	if (!Relic)
	{
		return;
	}

	// Validate relic state - cannot score if already scored, resetting, or scoring
	if (Relic->bHasScoredThisRound || Relic->GetCurrentState() == ERelicState::Resetting || Relic->GetCurrentState() == ERelicState::Scoring)
	{
		UE_LOG(LogTemp, Verbose, TEXT("GoalVolume: Relic in invalid state for scoring (State: %d, HasScored: %d)"), 
			(int32)Relic->GetCurrentState(), Relic->bHasScoredThisRound);
		return;
	}

	// Determine scoring team based on relic state
	int32 ScoringTeam = -1;

	// If relic is carried, use the carrier's team
	if (Relic->CurrentCarrier && Relic->CurrentCarrier->GetPlayerState())
	{
		ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>();
		if (GameState)
		{
			ScoringTeam = GameState->GetPlayerTeam(Relic->CurrentCarrier->GetPlayerState());
		}
	}
	// If relic is thrown/passed/dropped, use the last team that possessed it
	else if (Relic->LastPossessingTeam >= 0)
	{
		ScoringTeam = Relic->LastPossessingTeam;
	}

	// Validate that we have a valid scoring team
	if (ScoringTeam < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GoalVolume: Cannot determine scoring team - LastPossessingTeam is %d"), Relic->LastPossessingTeam);
		return;
	}

	// Verify scoring team is the attacking team (not scoring in own goal)
	// In Breakaway, teams score in the OPPONENT's goal
	// So if this is Team 0's goal, Team 1 should score
	const int32 AttackingTeam = (OwningTeam == 0) ? 1 : 0;

	if (ScoringTeam != AttackingTeam)
	{
		// Wrong team or neutral relic - no score
		UE_LOG(LogTemp, Warning, TEXT("GoalVolume: Relic entered Team %d goal but scoring team is %d (should be %d)"), 
			OwningTeam + 1, ScoringTeam + 1, AttackingTeam + 1);
		return;
	}

	// Valid score!
	bIsProcessingScore = true;

	UE_LOG(LogTemp, Log, TEXT("GOAL! Team %d scored in Team %d's goal"), 
		ScoringTeam + 1, OwningTeam + 1);

	// Clear any existing timers before setting new ones
	GetWorldTimerManager().ClearTimer(ScoreDelayTimerHandle);
	GetWorldTimerManager().ClearTimer(ScoreResetTimerHandle);

	// Add small delay to allow state to settle (handles edge case of simultaneous pickup)
	constexpr float ScoreDelayDuration = 0.1f;
	const int32 CapturedScoringTeam = ScoringTeam;
	const TWeakObjectPtr<ABwayPlayerState> Scorer = Relic->CurrentCarrier
		? Cast<ABwayPlayerState>(Relic->CurrentCarrier->GetPlayerState())
		: Relic->GetLastPossessingPlayerState();
	const TWeakObjectPtr<ARelicActor> WeakRelic = Relic;
	GetWorldTimerManager().SetTimer(ScoreDelayTimerHandle, [this, WeakRelic, CapturedScoringTeam, Scorer]()
	{
		ARelicActor* Relic = WeakRelic.Get();

		// Double-check state after delay
		if (Relic && !Relic->bHasScoredThisRound && Relic->GetCurrentState() != ERelicState::Resetting && Relic->GetCurrentState() != ERelicState::Scoring)
		{
			// Notify relic that it entered the goal (handles state and scoring flag)
			Relic->OnEnteredGoal(CapturedScoringTeam);

			// Play effects
			PlayScoringEffects();

			// Notify round management component
			if (ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>())
			{
				if (UBwayRoundManagementComponent* RoundMgmt = GS->FindComponentByClass<UBwayRoundManagementComponent>())
				{
					RoundMgmt->OnRelicScored(CapturedScoringTeam, Scorer.Get());
				}
			}
		}
	}, ScoreDelayDuration, false);

	// Get scoring cooldown from RelicSettings
	constexpr float DefaultScoringCooldown = 3.0f;
	float CooldownDuration = DefaultScoringCooldown;
	if (Relic->GetRelicSettings())
	{
		CooldownDuration = Relic->GetRelicSettings()->ScoringCooldown;
	}

	// Reset processing flag after cooldown - stored as member to allow cancellation
	GetWorldTimerManager().SetTimer(ScoreResetTimerHandle, this, &ABwayGoalVolume::ResetScoreProcessing, CooldownDuration, false);
}

void ABwayGoalVolume::PlayScoringEffects_Implementation()
{
	// Debug visualization
	if (bShowDebugSphere)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), 300.0f, 16, FColor::Green, false, 3.0f, 0, 10.0f);
	}

	// Blueprint can override this to add VFX, SFX, etc.
}

void ABwayGoalVolume::ResetScoreProcessing()
{
	bIsProcessingScore = false;
}
