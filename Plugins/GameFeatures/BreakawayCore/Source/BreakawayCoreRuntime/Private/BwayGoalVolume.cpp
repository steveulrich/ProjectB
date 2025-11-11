// Copyright Epic Games, Inc. All Rights Reserved.

#include "BwayGoalVolume.h"
#include "Relic/RelicActor.h"
#include "BreakawayGameMode.h"
#include "BwayCharacterWithAbilities.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

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

	// Color the goal based on team for visual clarity in editor/debug
	if (GoalMesh && GoalMesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* DynMat = GoalMesh->CreateDynamicMaterialInstance(0);
		if (DynMat)
		{
			FLinearColor TeamColor = (OwningTeam == 0) ? FLinearColor::Blue : FLinearColor::Red;
			DynMat->SetVectorParameterValue(FName("TeamColor"), TeamColor);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Goal Volume initialized for Team %d at location %s"), 
		OwningTeam + 1, *GetActorLocation().ToString());
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

	// Check if the overlapping actor is the relic
	ARelicActor* Relic = Cast<ARelicActor>(OtherActor);
	if (!Relic)
	{
		return;
	}

	// Get the game mode
	ABreakawayGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakawayGameMode>();
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("GoalVolume: Cannot score - GameMode is null"));
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

	// Play effects
	PlayScoringEffects();

	// Notify game mode
	GameMode->OnRelicScored(ScoringTeam);

	// Reset processing flag after a delay
	FTimerHandle UnusedHandle;
	GetWorldTimerManager().SetTimer(UnusedHandle, this, &ABwayGoalVolume::ResetScoreProcessing, 2.0f, false);
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