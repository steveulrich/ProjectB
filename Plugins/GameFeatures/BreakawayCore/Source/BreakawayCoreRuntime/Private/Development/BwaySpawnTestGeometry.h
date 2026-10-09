#pragma once

#include "Components/BoxComponent.h"
#include "Engine/World.h"

namespace BwaySpawnTestGeometry
{
// The central box blocks the requested location; its neighbors block the engine's
// one-step penetration adjustments. Removing this single actor clears the start.
inline AActor* SpawnObstruction(UWorld* World, const FVector& Location)
{
    AActor* Actor = World->SpawnActor<AActor>();
    if (!Actor) return nullptr;
    auto* Root = NewObject<USceneComponent>(Actor);
    Actor->SetRootComponent(Root); Actor->AddInstanceComponent(Root); Root->RegisterComponent();
    Actor->SetActorLocation(Location);
    for (int32 X = -1; X <= 1; ++X) for (int32 Y = -1; Y <= 1; ++Y) for (int32 Z = -1; Z <= 1; ++Z)
    {
        auto* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Box->SetupAttachment(Root);
        Box->SetRelativeLocation(FVector(X, Y, Z) * 2000); Box->SetBoxExtent(FVector(1000));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
    }
    return Actor;
}
}
