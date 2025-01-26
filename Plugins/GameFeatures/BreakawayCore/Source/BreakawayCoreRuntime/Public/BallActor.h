// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "BallActor.generated.h"

UCLASS()
class BREAKAWAYCORERUNTIME_API ABallActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ABallActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	USphereComponent* SphereComponent;
	
};
