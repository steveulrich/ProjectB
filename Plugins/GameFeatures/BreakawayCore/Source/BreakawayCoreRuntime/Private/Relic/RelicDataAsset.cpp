// RelicDataAsset.cpp
#include "Relic/RelicDataAsset.h"
#include "Relic/RelicActor.h"

URelicDataAsset::URelicDataAsset()
{
	// Default to base relic actor class
	RelicActorClass = ARelicActor::StaticClass();
}

FPrimaryAssetId URelicDataAsset::GetPrimaryAssetId() const
{
	// This allows the asset to be referenced by type and name
	return FPrimaryAssetId(TEXT("RelicData"), GetFName());
}