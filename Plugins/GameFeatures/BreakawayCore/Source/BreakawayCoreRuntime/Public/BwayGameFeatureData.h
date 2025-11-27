// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureData.h"
#include "HeroSystems/BwayHeroDataAsset.h" // Forward declare or include
#include "BwayGameFeatureData.generated.h"

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameFeatureData : public UGameFeatureData
{
	GENERATED_BODY()

public:
	// The Hero Data Asset associated with this Game Feature.
	// You could also make this an array: TArray<TSoftObjectPtr<UBwayHeroDataAsset>> HeroDataAssets;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hero")
	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> HeroDataAssets;
};