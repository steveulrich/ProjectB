// Copyright Epic Games, Inc. All Rights Reserved.

#include "Movement/BwayMovementFeelConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMovementFeelConfig)

namespace BwayMovementFeelConfigDefaults
{
	const FPrimaryAssetType AssetType(TEXT("BwayMovementFeelConfig"));
}

FPrimaryAssetId UBwayMovementFeelConfig::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(BwayMovementFeelConfigDefaults::AssetType, GetFName());
}
