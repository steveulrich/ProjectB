#include "GoalZone.h"
#include "Components/BoxComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Relic/RelicActor.h"
#include "Kismet/GameplayStatics.h"

AGoalZone::AGoalZone()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Create trigger volume
    TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
    SetRootComponent(TriggerVolume);
    TriggerVolume->SetCollisionProfileName(TEXT("Trigger"));
    TriggerVolume->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
    TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AGoalZone::OnTriggerBeginOverlap);
    
    // Initialize properties
    TeamId = -1;
}

void AGoalZone::BeginPlay()
{
    Super::BeginPlay();
}

void AGoalZone::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    // Check if the overlapping actor is a character
    ARelicActor* Relic = Cast<ARelicActor>(OtherActor);
    if (!Relic)
    {
        return;
    }
    
    // Check if character is from opposing team
    if (Relic->GetLastPossessingTeam() == TeamId)
    {
        return;
    }

    ProcessRelicScoringAttempt(Relic);
}

bool AGoalZone::IsOpposingTeam(ARelicActor* Relic) const
{
    if (!Relic)
    {
        return false;
    }
    
    // Check if character is on a different team
    int32 RelicCarrierTeamId = Relic->GetLastPossessingTeam();
    return RelicCarrierTeamId != TeamId && RelicCarrierTeamId != -1 && TeamId != -1;
}

void AGoalZone::ProcessRelicScoringAttempt(ARelicActor* Relic)
{
    // Attempt to score
    Relic->TryScore(TeamId);
}