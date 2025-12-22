// Copyright Epic Games, Inc. All Rights Reserved.

#include "Development/BwayHeroTestSpawner.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "UI/BwayHeroSpawnerWidget.h"
#include "BwayPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroTestSpawner)

ABwayHeroTestSpawner::ABwayHeroTestSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Create collision volume as root
	CollisionVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionVolume"));
	RootComponent = CollisionVolume;
	CollisionVolume->InitCapsuleSize(80.0f, 80.0f);
	CollisionVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionVolume->OnComponentBeginOverlap.AddDynamic(this, &ABwayHeroTestSpawner::OnOverlapBegin);

	// Create pad mesh
	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(RootComponent);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create widget component for floating display
	InfoWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InfoWidgetComponent"));
	InfoWidgetComponent->SetupAttachment(RootComponent);
	InfoWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	InfoWidgetComponent->SetDrawAtDesiredSize(true);
	InfoWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, WidgetHeightOffset));
}

void ABwayHeroTestSpawner::BeginPlay()
{
	Super::BeginPlay();

	UpdateDisplayWidget();
}

void ABwayHeroTestSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Update widget position based on configured offset
	if (InfoWidgetComponent)
	{
		InfoWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, WidgetHeightOffset));
	}

	UpdateDisplayWidget();
}

void ABwayHeroTestSpawner::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	// Only process on authority
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	if (OverlappingPawn && HeroDefinition)
	{
		TriggerHeroChange(OverlappingPawn);
	}
}

void ABwayHeroTestSpawner::TriggerHeroChange(APawn* Pawn)
{
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: TriggerHeroChange BEGIN"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	
	if (!Pawn || !HeroDefinition)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroTestSpawner: Invalid Pawn (%s) or HeroDefinition (%s)"),
			Pawn ? TEXT("Valid") : TEXT("NULL"),
			HeroDefinition ? TEXT("Valid") : TEXT("NULL"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroTestSpawner: Pawn = %s, HeroDefinition = %s"), 
		*Pawn->GetName(), *HeroDefinition->DisplayName.ToString());

	// Get the player controller
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: Overlapping pawn has no player controller"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroTestSpawner: PlayerController = %s"), *PC->GetName());

	// Get the Breakaway player state
	ABwayPlayerState* BwayPS = PC->GetPlayerState<ABwayPlayerState>();
	if (!BwayPS)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: Player state is not ABwayPlayerState"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroTestSpawner: BwayPlayerState = %s"), *BwayPS->GetName());

	// Get the hero's primary asset ID
	FPrimaryAssetId HeroId = HeroDefinition->GetPrimaryAssetId();
	if (!HeroId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: Hero definition has invalid asset ID"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroTestSpawner: Target HeroId = %s"), *HeroId.ToString());
	
	// Log current hero state
	FPrimaryAssetId CurrentHeroId = BwayPS->GetSelectedHeroId();
	UE_LOG(LogTemp, Log, TEXT("BwayHeroTestSpawner: Current PlayerState HeroId = %s"), 
		CurrentHeroId.IsValid() ? *CurrentHeroId.ToString() : TEXT("NONE"));

	// Check if already this hero to avoid unnecessary respawn
	if (BwayPS->GetSelectedHeroId() == HeroId)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: Player already has hero %s selected - SKIPPING"), 
			*HeroDefinition->DisplayName.ToString());
		return;
	}

	// Set the hero on the player state
	UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: CALLING ServerSetSelectedHeroId(%s)"), *HeroId.ToString());
	BwayPS->ServerSetSelectedHeroId(HeroId);

	// Verify the hero was set
	FPrimaryAssetId NewHeroId = BwayPS->GetSelectedHeroId();
	UE_LOG(LogTemp, Log, TEXT("BwayHeroTestSpawner: After SetHeroId, PlayerState HeroId = %s"), 
		NewHeroId.IsValid() ? *NewHeroId.ToString() : TEXT("NONE"));

	UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: Changing player to hero %s"), 
		*HeroDefinition->DisplayName.ToString());

	// Destroy current pawn and respawn
	UE_LOG(LogTemp, Log, TEXT("BwayHeroTestSpawner: DESTROYING current pawn: %s"), *Pawn->GetName());
	Pawn->Destroy();

	// Trigger respawn through game mode
	if (AGameModeBase* GameMode = UGameplayStatics::GetGameMode(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: CALLING GameMode->RestartPlayer(%s)"), *PC->GetName());
		GameMode->RestartPlayer(PC);
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: RestartPlayer returned"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroTestSpawner: NO GAME MODE FOUND - Cannot respawn player!"));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("BwayHeroTestSpawner: TriggerHeroChange END"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

void ABwayHeroTestSpawner::UpdateDisplayWidget()
{
	if (!InfoWidgetComponent)
	{
		return;
	}

	// Set the widget class if configured
	if (InfoWidgetClass && !InfoWidgetComponent->GetWidgetClass())
	{
		InfoWidgetComponent->SetWidgetClass(InfoWidgetClass);
	}

	// Get or create the widget instance
	UUserWidget* WidgetInstance = InfoWidgetComponent->GetWidget();
	InfoWidget = Cast<UBwayHeroSpawnerWidget>(WidgetInstance);

	if (InfoWidget && HeroDefinition)
	{
		InfoWidget->SetHeroInfo(HeroDefinition->DisplayName, HeroDefinition->Portrait);
	}
	else if (InfoWidget)
	{
		// Clear the widget if no hero is set
		InfoWidget->SetHeroInfo(FText::GetEmpty(), nullptr);
	}
}

