// Fill out your copyright notice in the Description page of Project Settings.


#include "HeroSystems/BwayHeroDataAsset.h"

TArray<UBwayBuildableDataAsset*> UBwayHeroDataAsset::GetAllBuildableDataAssets() const
{
	TArray<UBwayBuildableDataAsset*> Result;

	if (BuildableDataAsset)
	{
		Result.Add(BuildableDataAsset);
	}

	for (const TObjectPtr<UBwayBuildableDataAsset>& Entry : BuildableDataAssets)
	{
		if (Entry && !Result.Contains(Entry.Get()))
		{
			Result.Add(Entry.Get());
		}
	}

	return Result;
}
