// Copyright Epic Games, Inc. All Rights Reserved.

#include "Matchmaking/LyraMatchmakingSubsystem.h"

#include "CommonSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/CustomGameConfig.h"
#include "LyraLogChannels.h"
#include "Player/LyraPlayerController.h"
#include "System/LyraGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraMatchmakingSubsystem)

namespace LyraMatchmakingDefaults
{
static TMap<FString, FString> GetDefaultQueueExtraArgs()
{
	TMap<FString, FString> ExtraArgs;
	ExtraArgs.Add(TEXT("SkipHeroSelection"), TEXT("1"));
	return ExtraArgs;
}
}

ULyraMatchmakingSubsystem::ULyraMatchmakingSubsystem()
{
	DefaultQueueExtraArgs = LyraMatchmakingDefaults::GetDefaultQueueExtraArgs();
}

ULyraMatchmakingSubsystem* ULyraMatchmakingSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	if (!World)
	{
		return nullptr;
	}

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		return GameInstance->GetSubsystem<ULyraMatchmakingSubsystem>();
	}

	return nullptr;
}

void ULyraMatchmakingSubsystem::StartMatchmakingQueue(const FLyraMatchmakingQueuePayload& Payload)
{
	if (!Payload.QueueTypeTag.IsValid())
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: StartMatchmakingQueue rejected — QueueTypeTag is unset."));
		return;
	}

	if (!Payload.MapID.IsValid())
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: StartMatchmakingQueue rejected — MapID is unset."));
		return;
	}

	if (Payload.AllowedExperiences.IsEmpty())
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: StartMatchmakingQueue rejected — AllowedExperiences is empty."));
		return;
	}

	CancelPendingMatchmaking();

	ActiveQueuePayload = Payload;
	const FString JsonPayload = BuildJsonPayload(Payload);
	SendMatchmakingPayloadToBackend(JsonPayload);
}

void ULyraMatchmakingSubsystem::CreateCustomLobby()
{
	ULyraGameInstance* LyraGameInstance = Cast<ULyraGameInstance>(GetGameInstance());
	if (!LyraGameInstance)
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: CreateCustomLobby requires ULyraGameInstance."));
		return;
	}

	if (UCustomGameConfig* CustomConfig = LyraGameInstance->GetCustomGameSettings())
	{
		CustomConfig->bIsCustomGameDefinition = true;
		UE_LOG(LogLyra, Log, TEXT("LyraMatchmaking: Custom lobby created (bIsCustomGameDefinition=true, CustomMaxPlayersPerTeam=%d)."),
			CustomConfig->CustomMaxPlayersPerTeam);
		ShowMatchmakingScreenMessage(TEXT("Custom lobby ready."), FColor::Cyan);
	}
}

void ULyraMatchmakingSubsystem::SendMatchmakingPayloadToBackend(const FString& JsonPayload)
{
	LastPayloadJson = JsonPayload;

	UE_LOG(LogLyra, Log, TEXT("LyraMatchmaking: SendMatchmakingPayloadToBackend\n%s"), *JsonPayload);
	ShowMatchmakingScreenMessage(FString::Printf(TEXT("Matchmaking payload sent:\n%s"), *JsonPayload), FColor::Yellow, 8.0f);

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: No world for mock timer — calling OnMockMatchFound immediately."));
		HandleMockMatchFound();
		return;
	}

	bMatchmakingPending = true;
	World->GetTimerManager().SetTimer(
		MockMatchFoundTimerHandle,
		this,
		&ThisClass::HandleMockMatchFound,
		MockMatchDelaySeconds,
		/*bLooping=*/false);
}

void ULyraMatchmakingSubsystem::CancelPendingMatchmaking()
{
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(MockMatchFoundTimerHandle);
	}

	bMatchmakingPending = false;
}

bool ULyraMatchmakingSubsystem::IsMatchmakingPending() const
{
	return bMatchmakingPending;
}

void ULyraMatchmakingSubsystem::HandleMockMatchFound()
{
	bMatchmakingPending = false;
	MockMatchFoundTimerHandle.Invalidate();

	const FString Message = TEXT("Match Found! Simulating Server Handoff.");
	UE_LOG(LogLyra, Log, TEXT("LyraMatchmaking: %s"), *Message);
	ShowMatchmakingScreenMessage(Message, FColor::Green, 8.0f);

	OnMockMatchFound.Broadcast();
	ExecuteQueueSessionCreation();
}

void ULyraMatchmakingSubsystem::ExecuteQueueSessionCreation()
{
	if (ActiveQueuePayload.AllowedExperiences.IsEmpty())
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: ExecuteQueueSessionCreation — no AllowedExperiences on active payload."));
		return;
	}

	const FPrimaryAssetId ExperienceID = ActiveQueuePayload.AllowedExperiences[0];
	if (!ExperienceID.IsValid())
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: ExecuteQueueSessionCreation — first AllowedExperience is invalid."));
		return;
	}

	TMap<FString, FString> ExtraArgs = DefaultQueueExtraArgs;
	const int32 MaxPlayerCount = FMath::Max(ActiveQueuePayload.PlayersPerTeam * 2, 2);
	const FString ModeName = ActiveQueuePayload.QueueTypeTag.IsValid()
		? ActiveQueuePayload.QueueTypeTag.ToString()
		: ActiveQueuePayload.GoalAssetId.PrimaryAssetName.ToString();

	UCommonSession_HostSessionRequest* Request = BuildHostSessionRequest(
		ActiveQueuePayload.MapID,
		ExperienceID,
		MaxPlayerCount,
		ModeName,
		ExtraArgs);

	if (HostSessionFromRequest(Request))
	{
		ShowMatchmakingScreenMessage(TEXT("Queue match hosting — traveling to map."), FColor::Green, 5.0f);
	}
}

void ULyraMatchmakingSubsystem::ExecuteCustomSessionCreation()
{
	ULyraGameInstance* LyraGameInstance = Cast<ULyraGameInstance>(GetGameInstance());
	if (!LyraGameInstance)
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: ExecuteCustomSessionCreation requires ULyraGameInstance."));
		return;
	}

	UCustomGameConfig* CustomConfig = LyraGameInstance->GetCustomGameSettings();
	if (!CustomConfig || !CustomConfig->bIsCustomGameDefinition)
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: ExecuteCustomSessionCreation — call CreateCustomLobby() first (bIsCustomGameDefinition is false)."));
		return;
	}

	if (!CustomConfig->SelectedMapOverride.IsValid())
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: ExecuteCustomSessionCreation — SelectedMapOverride is unset."));
		return;
	}

	if (!CustomConfig->SelectedExperienceOverride.IsValid())
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: ExecuteCustomSessionCreation — SelectedExperienceOverride is unset."));
		return;
	}

	TMap<FString, FString> ExtraArgs = CustomConfig->ExtraArgs;
	if (!ExtraArgs.Contains(TEXT("SkipHeroSelection")))
	{
		ExtraArgs.Add(TEXT("SkipHeroSelection"), TEXT("1"));
	}

	const int32 MaxPlayerCount = FMath::Max(CustomConfig->CustomMaxPlayersPerTeam * 2, 2);
	UCommonSession_HostSessionRequest* Request = BuildHostSessionRequest(
		CustomConfig->SelectedMapOverride,
		CustomConfig->SelectedExperienceOverride,
		MaxPlayerCount,
		TEXT("CustomGame"),
		ExtraArgs);

	if (HostSessionFromRequest(Request))
	{
		ShowMatchmakingScreenMessage(TEXT("Custom game hosting — traveling to override map."), FColor::Green, 5.0f);
	}
}

UCommonSession_HostSessionRequest* ULyraMatchmakingSubsystem::BuildHostSessionRequest(
	FPrimaryAssetId MapID,
	FPrimaryAssetId ExperienceID,
	int32 MaxPlayerCount,
	const FString& ModeNameForAdvertisement,
	const TMap<FString, FString>& AdditionalExtraArgs) const
{
	if (!MapID.IsValid() || !ExperienceID.IsValid())
	{
		return nullptr;
	}

	UCommonSession_HostSessionRequest* Request = nullptr;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>())
		{
			Request = SessionSubsystem->CreateOnlineHostSessionRequest();
		}
	}

	if (!Request)
	{
		Request = NewObject<UCommonSession_HostSessionRequest>();
	}

	Request->OnlineMode = DevHostOnlineMode;
	Request->bUseLobbies = false;
	Request->bUsePresence = false;
	Request->MapID = MapID;
	Request->ModeNameForAdvertisement = ModeNameForAdvertisement;
	Request->MaxPlayerCount = FMath::Max(MaxPlayerCount, 2);
	Request->ExtraArgs = AdditionalExtraArgs;
	Request->ExtraArgs.Add(TEXT("Experience"), ExperienceID.PrimaryAssetName.ToString());

	return Request;
}

bool ULyraMatchmakingSubsystem::HostSessionFromRequest(UCommonSession_HostSessionRequest* Request)
{
	if (!Request)
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: HostSessionFromRequest — null request."));
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: HostSessionFromRequest — no game instance."));
		return false;
	}

	UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>();
	if (!SessionSubsystem)
	{
		UE_LOG(LogLyra, Warning, TEXT("LyraMatchmaking: HostSessionFromRequest — UCommonSessionSubsystem missing."));
		return false;
	}

	FText ValidationError;
	if (!Request->ValidateAndLogErrors(ValidationError))
	{
		UE_LOG(LogLyra, Error, TEXT("LyraMatchmaking: HostSession request invalid — %s"), *ValidationError.ToString());
		return false;
	}

	APlayerController* HostingPlayer = nullptr;
	if (ULyraGameInstance* LyraGameInstance = Cast<ULyraGameInstance>(GameInstance))
	{
		HostingPlayer = LyraGameInstance->GetPrimaryPlayerController();
	}

	const FString TravelURL = Request->ConstructTravelURL();
	UE_LOG(LogLyra, Log, TEXT("LyraMatchmaking: HostSession MapID=%s Experience=%s URL=%s"),
		*Request->MapID.ToString(),
		*Request->ExtraArgs.FindRef(TEXT("Experience")),
		*TravelURL);

	SessionSubsystem->HostSession(HostingPlayer, Request);
	return true;
}

FString ULyraMatchmakingSubsystem::BuildJsonPayload(const FLyraMatchmakingQueuePayload& Payload) const
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("goalAssetId"), Payload.GoalAssetId.IsValid() ? Payload.GoalAssetId.ToString() : TEXT(""));
	RootObject->SetStringField(TEXT("queueTypeTag"), Payload.QueueTypeTag.ToString());
	RootObject->SetNumberField(TEXT("maxPartySize"), Payload.MaxPartySize);
	RootObject->SetNumberField(TEXT("playersPerTeam"), Payload.PlayersPerTeam);
	RootObject->SetStringField(TEXT("mapId"), Payload.MapID.ToString());

	TArray<TSharedPtr<FJsonValue>> ExperienceValues;
	for (const FPrimaryAssetId& ExperienceId : Payload.AllowedExperiences)
	{
		ExperienceValues.Add(MakeShared<FJsonValueString>(ExperienceId.ToString()));
	}
	RootObject->SetArrayField(TEXT("allowedExperiences"), ExperienceValues);

	FString JsonOutput;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOutput);
	FJsonSerializer::Serialize(RootObject, Writer);
	return JsonOutput;
}

void ULyraMatchmakingSubsystem::ShowMatchmakingScreenMessage(const FString& Message, const FColor& Color, float DurationSeconds) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, DurationSeconds, Color, Message);
	}
}
