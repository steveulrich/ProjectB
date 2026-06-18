// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/BwayMatchFlowConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchFlowConfig)

namespace BwayMatchFlowConfigDefaults
{
	const FPrimaryAssetType AssetType(TEXT("BwayMatchFlowConfig"));

	const TCHAR* PhasePrematchPath = TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Prematch.BW_Phase_Prematch_C");
	const TCHAR* PhaseWarmupPath = TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Warmup.BW_Phase_Warmup_C");
	const TCHAR* PhasePlayingPath = TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Playing.BW_Phase_Playing_C");
	const TCHAR* PhasePostRoundPath = TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_PostRound.BW_Phase_PostRound_C");
	const TCHAR* PhasePostMatchPath = TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_PostMatch.BW_Phase_PostMatch_C");

	TSubclassOf<ULyraGamePhaseAbility> LoadPhaseClass(const TCHAR* Path)
	{
		return TSoftClassPtr<ULyraGamePhaseAbility>(FSoftObjectPath(Path)).LoadSynchronous();
	}
}

UBwayMatchFlowConfig::UBwayMatchFlowConfig()
{
	PrematchPhaseAbility = BwayMatchFlowConfigDefaults::LoadPhaseClass(BwayMatchFlowConfigDefaults::PhasePrematchPath);
	WarmupPhaseAbility = BwayMatchFlowConfigDefaults::LoadPhaseClass(BwayMatchFlowConfigDefaults::PhaseWarmupPath);
	PlayingPhaseAbility = BwayMatchFlowConfigDefaults::LoadPhaseClass(BwayMatchFlowConfigDefaults::PhasePlayingPath);
	PostRoundPhaseAbility = BwayMatchFlowConfigDefaults::LoadPhaseClass(BwayMatchFlowConfigDefaults::PhasePostRoundPath);
	PostMatchPhaseAbility = BwayMatchFlowConfigDefaults::LoadPhaseClass(BwayMatchFlowConfigDefaults::PhasePostMatchPath);
}

FPrimaryAssetId UBwayMatchFlowConfig::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(BwayMatchFlowConfigDefaults::AssetType, GetFName());
}
