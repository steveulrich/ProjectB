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
    ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(OtherActor);
    if (!Character)
    {
        return;
    }
    
    // Check if character is from opposing team
    if (!IsOpposingTeam(Character))
    {
        return;
    }
    
    // Check if character is carrying the Relic
    ARelicActor* Relic = nullptr;
    // Process scoring attempt if character has the Relic
    if (Relic && Relic->IsCarried() && Relic->GetCarrier() == Character)
    {
        ProcessRelicScoringAttempt(Character, Relic);
    }
}

bool AGoalZone::IsOpposingTeam(ABwayCharacterWithAbilities* Character) const
{
    if (!Character)
    {
        return false;
    }
    
    // Check if character is on a different team
    int32 CharacterTeamId = Character->GetTeamId();
    return CharacterTeamId != TeamId && CharacterTeamId != -1 && TeamId != -1;
}

void AGoalZone::ProcessRelicScoringAttempt(ABwayCharacterWithAbilities* Character, ARelicActor* Relic)
{
    if (!Character || !Relic)
    {
        return;
    }
    
    // Attempt to score
    Relic->TryScore(Character, TeamId);
}