// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayFrontendStateSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayFrontendStateSubsystem)

UBwayFrontendStateSubsystem::UBwayFrontendStateSubsystem()
{
	const FPrimaryAssetId DefaultMapId(FPrimaryAssetType(TEXT("Map")), FName(TEXT("L_BW_Dorado")));
	const FPrimaryAssetId DefaultExpId(FPrimaryAssetType(TEXT("LyraExperienceDefinition")), FName(TEXT("B_BW_Experience_CaptureTheRelic")));

	{
		FBwayMapOptionEntry ShrineOfAuros;
		ShrineOfAuros.MapAssetId = DefaultMapId;
		ShrineOfAuros.MapPackagePath = TEXT("/BreakawayCore/Maps/L_BW_Dorado");
		ShrineOfAuros.ExperienceAssetId = DefaultExpId;
		ShrineOfAuros.DisplayName = NSLOCTEXT("BreakawayLobby", "Map_ShrineOfAuros", "The Shrine of Auros");
		ShrineOfAuros.Description = NSLOCTEXT("BreakawayLobby", "Map_ShrineOfAuros_Desc", "Ancient sun-drenched temple sanctuary of Auros.");
		AvailableMaps.Add(ShrineOfAuros);
	}
	{
		FBwayMapOptionEntry StygianSea;
		StygianSea.MapAssetId = DefaultMapId;
		StygianSea.MapPackagePath = TEXT("/BreakawayCore/Maps/L_BW_Dorado");
		StygianSea.ExperienceAssetId = DefaultExpId;
		StygianSea.DisplayName = NSLOCTEXT("BreakawayLobby", "Map_StygianSea", "The Stygian Sea");
		StygianSea.Description = NSLOCTEXT("BreakawayLobby", "Map_StygianSea_Desc", "Storm-lashed fortress above the abyssal waters.");
		AvailableMaps.Add(StygianSea);
	}
	{
		FBwayMapOptionEntry ElDorado;
		ElDorado.MapAssetId = DefaultMapId;
		ElDorado.MapPackagePath = TEXT("/BreakawayCore/Maps/L_BW_Dorado");
		ElDorado.ExperienceAssetId = DefaultExpId;
		ElDorado.DisplayName = NSLOCTEXT("BreakawayLobby", "Map_ElDorado", "El Dorado");
		ElDorado.Description = NSLOCTEXT("BreakawayLobby", "Map_ElDorado_Desc", "City of gold hidden deep within the jungle canopy.");
		AvailableMaps.Add(ElDorado);
	}

	InitializeDefaultLobbyRoster();
}

FString UBwayFrontendStateSubsystem::GetResolvedLocalPlayerName() const
{
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (const APlayerController* PC = LP->GetPlayerController(LP->GetWorld()))
		{
			if (const APlayerState* PS = PC->PlayerState)
			{
				const FString PSName = PS->GetPlayerName();
				if (!PSName.IsEmpty())
				{
					return PSName;
				}
			}
		}
		const FString Nickname = LP->GetNickname();
		if (!Nickname.IsEmpty())
		{
			return Nickname;
		}
	}
	return TEXT("Player1");
}

void UBwayFrontendStateSubsystem::InitializeDefaultLobbyRoster()
{
	LobbySettings.BlueTeamSlots.Init(FString(), 4);
	LobbySettings.RedTeamSlots.Init(FString(), 4);
	LobbySettings.SpectatorSlots.Init(FString(), 4);

	LobbySettings.LocalHostTeam = EBwayLobbyTeam::BlueTeam;
	LobbySettings.LocalHostSlotIndex = 0;

	const FString HostLabel = FString::Printf(TEXT("(Host) %s"), *GetResolvedLocalPlayerName());
	LobbySettings.BlueTeamSlots[0] = HostLabel;
}

FBwayMapOptionEntry UBwayFrontendStateSubsystem::GetSelectedMapEntry() const
{
	if (AvailableMaps.IsValidIndex(LobbySettings.SelectedMapIndex))
	{
		return AvailableMaps[LobbySettings.SelectedMapIndex];
	}
	return AvailableMaps.Num() > 0 ? AvailableMaps[0] : FBwayMapOptionEntry();
}

void UBwayFrontendStateSubsystem::SetLobbySettings(const FBwayMatchLobbySettings& NewSettings)
{
	LobbySettings = NewSettings;
	OnLobbySettingsChanged.Broadcast();
}

void UBwayFrontendStateSubsystem::ApplyMatchSettingsModal(int32 InMapIndex, bool bInEnableBots, EBwayBotDifficulty InBotDifficulty)
{
	if (AvailableMaps.Num() > 0)
	{
		LobbySettings.SelectedMapIndex = (InMapIndex % AvailableMaps.Num() + AvailableMaps.Num()) % AvailableMaps.Num();
	}
	else
	{
		LobbySettings.SelectedMapIndex = 0;
	}
	LobbySettings.bBotsEnabled = bInEnableBots;
	LobbySettings.BotDifficulty = InBotDifficulty;
	OnLobbySettingsChanged.Broadcast();
}

void UBwayFrontendStateSubsystem::CycleInviteMode(int32 Direction)
{
	const int32 NumModes = 3;
	const int32 Current = static_cast<int32>(LobbySettings.InviteMode);
	const int32 Next = ((Current + Direction) % NumModes + NumModes) % NumModes;
	LobbySettings.InviteMode = static_cast<EBwayLobbyInviteMode>(Next);
	OnLobbySettingsChanged.Broadcast();
}

bool UBwayFrontendStateSubsystem::MoveLocalHostToSlot(EBwayLobbyTeam TargetTeam, int32 TargetSlotIndex)
{
	if (TargetSlotIndex < 0 || TargetSlotIndex >= 4)
	{
		return false;
	}

	if (LobbySettings.BlueTeamSlots.Num() < 4) LobbySettings.BlueTeamSlots.SetNum(4);
	if (LobbySettings.RedTeamSlots.Num() < 4) LobbySettings.RedTeamSlots.SetNum(4);
	if (LobbySettings.SpectatorSlots.Num() < 4) LobbySettings.SpectatorSlots.SetNum(4);

	auto ClearHostSlot = [&](EBwayLobbyTeam Team, int32 SlotIdx)
	{
		if (SlotIdx < 0 || SlotIdx >= 4) return;
		switch (Team)
		{
		case EBwayLobbyTeam::BlueTeam:   LobbySettings.BlueTeamSlots[SlotIdx].Empty(); break;
		case EBwayLobbyTeam::RedTeam:    LobbySettings.RedTeamSlots[SlotIdx].Empty(); break;
		case EBwayLobbyTeam::Spectators: LobbySettings.SpectatorSlots[SlotIdx].Empty(); break;
		}
	};

	ClearHostSlot(LobbySettings.LocalHostTeam, LobbySettings.LocalHostSlotIndex);

	LobbySettings.LocalHostTeam = TargetTeam;
	LobbySettings.LocalHostSlotIndex = TargetSlotIndex;

	const FString HostLabel = FString::Printf(TEXT("(Host) %s"), *GetResolvedLocalPlayerName());
	switch (TargetTeam)
	{
	case EBwayLobbyTeam::BlueTeam:   LobbySettings.BlueTeamSlots[TargetSlotIndex] = HostLabel; break;
	case EBwayLobbyTeam::RedTeam:    LobbySettings.RedTeamSlots[TargetSlotIndex] = HostLabel; break;
	case EBwayLobbyTeam::Spectators: LobbySettings.SpectatorSlots[TargetSlotIndex] = HostLabel; break;
	}

	OnLobbySettingsChanged.Broadcast();
	return true;
}

void UBwayFrontendStateSubsystem::BuildMatchExtraArgs(TMap<FString, FString>& OutExtraArgs) const
{
	OutExtraArgs.Add(TEXT("NumBots"), LobbySettings.bBotsEnabled ? TEXT("7") : TEXT("0"));
	OutExtraArgs.Add(TEXT("BotDifficulty"), LobbySettings.BotDifficulty == EBwayBotDifficulty::Elite ? TEXT("Elite") : TEXT("Normal"));
	OutExtraArgs.Add(TEXT("HostTeam"), FString::FromInt(static_cast<int32>(LobbySettings.LocalHostTeam)));
	OutExtraArgs.Add(TEXT("ArenaIntro"), TEXT("1"));
	OutExtraArgs.Add(TEXT("WarmupDuration"), TEXT("15.0"));
}

FString UBwayFrontendStateSubsystem::BuildMatchTravelUrl(bool bListenServer) const
{
	const FBwayMapOptionEntry MapEntry = GetSelectedMapEntry();
	const FString MapPath = MapEntry.MapPackagePath.IsEmpty()
		? TEXT("/BreakawayCore/Maps/L_BW_Dorado")
		: MapEntry.MapPackagePath;
	const FString ExpName = MapEntry.ExperienceAssetId.IsValid()
		? MapEntry.ExperienceAssetId.PrimaryAssetName.ToString()
		: TEXT("B_BW_Experience_CaptureTheRelic");

	FString Url = FString::Printf(TEXT("%s?Experience=%s"), *MapPath, *ExpName);
	if (bListenServer)
	{
		Url += TEXT("?listen");
	}
	TMap<FString, FString> ExtraArgs;
	BuildMatchExtraArgs(ExtraArgs);
	for (const TPair<FString, FString>& Pair : ExtraArgs)
	{
		Url += FString::Printf(TEXT("?%s=%s"), *Pair.Key, *Pair.Value);
	}
	return Url;
}

FText UBwayFrontendStateSubsystem::GetBotDifficultyDisplayText(EBwayBotDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EBwayBotDifficulty::Elite:
		return NSLOCTEXT("BreakawayLobby", "BotDiff_Elite", "ELITE");
	case EBwayBotDifficulty::Normal:
	default:
		return NSLOCTEXT("BreakawayLobby", "BotDiff_Normal", "NORMAL");
	}
}

FText UBwayFrontendStateSubsystem::GetInviteModeDisplayText(EBwayLobbyInviteMode Mode)
{
	switch (Mode)
	{
	case EBwayLobbyInviteMode::FriendsAndInvited:
		return NSLOCTEXT("BreakawayLobby", "Invite_Friends", "FRIENDS & INVITED");
	case EBwayLobbyInviteMode::OpenToAll:
		return NSLOCTEXT("BreakawayLobby", "Invite_Open", "OPEN TO ALL");
	case EBwayLobbyInviteMode::HostOnly:
	default:
		return NSLOCTEXT("BreakawayLobby", "Invite_HostOnly", "HOST ONLY");
	}
}
