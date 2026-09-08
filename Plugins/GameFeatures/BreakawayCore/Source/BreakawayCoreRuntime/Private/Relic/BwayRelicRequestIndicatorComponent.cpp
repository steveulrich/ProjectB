#include "Relic/BwayRelicRequestIndicatorComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BwayGameState.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "LyraGameplayTags.h"
#include "Relic/RelicSettings.h"
#include "Teams/LyraTeamAgentInterface.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRelicRequestIndicatorComponent)

UBwayRelicRequestIndicatorComponent::UBwayRelicRequestIndicatorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	RequestWidget.ComponentProperty = TEXT("RelicRequestWidgetComponent");
}

FGameplayTag UBwayRelicRequestIndicatorComponent::ResolveRequestTag(const AActor* Actor)
{
	const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	const ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	const UBwayRelicManagerComponent* Manager = GameState ? GameState->RelicManagerComponent : nullptr;
	const URelicSettings* Settings = Manager ? Manager->GetRelicSettings() : nullptr;
	return Settings ? Settings->RequestingTag : FGameplayTag();
}

void UBwayRelicRequestIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();
	IndicatorWidget = Cast<UWidgetComponent>(RequestWidget.GetComponent(GetOwner()));
	if (IndicatorWidget.IsValid())
	{
		IndicatorWidget->SetVisibility(false);
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (ILyraTeamAgentInterface* TeamAgent = Cast<ILyraTeamAgentInterface>(GetOwner()))
	{
		TeamAgent->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::HandleTeamChanged);
	}
	if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		LocalPlayer = GameInstance->GetFirstGamePlayer();
		if (LocalPlayer.IsValid())
		{
			ControllerChangedHandle = LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &ThisClass::SetViewer);
			SetViewer(LocalPlayer->GetPlayerController(GetWorld()));
		}
	}

	if (ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		Extension->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
		Extension->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
	}
}

void UBwayRelicRequestIndicatorComponent::HandleAbilitySystemInitialized()
{
	if (bEndingPlay)
	{
		return;
	}
	HandleAbilitySystemUninitialized();
	if (ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		AbilitySystem = Extension->GetLyraAbilitySystemComponent();
	}
	RequestTag = ResolveRequestTag(GetOwner());
	if (AbilitySystem.IsValid() && RequestTag.IsValid())
	{
		RequestTagHandle = AbilitySystem->RegisterGameplayTagEvent(RequestTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleTagChanged);
		DeathTagHandle = AbilitySystem->RegisterGameplayTagEvent(LyraGameplayTags::Status_Death, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleTagChanged);
	}
	RefreshVisibility();
}

void UBwayRelicRequestIndicatorComponent::HandleAbilitySystemUninitialized()
{
	if (AbilitySystem.IsValid())
	{
		if (RequestTagHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(RequestTag, EGameplayTagEventType::NewOrRemoved).Remove(RequestTagHandle);
		}
		if (DeathTagHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(LyraGameplayTags::Status_Death, EGameplayTagEventType::NewOrRemoved).Remove(DeathTagHandle);
		}
	}
	RequestTagHandle.Reset();
	DeathTagHandle.Reset();
	AbilitySystem.Reset();
	RequestTag = FGameplayTag();
	RefreshVisibility();
}

void UBwayRelicRequestIndicatorComponent::SetViewer(APlayerController* Controller)
{
	if (ILyraTeamAgentInterface* OldAgent = Cast<ILyraTeamAgentInterface>(Viewer.Get()))
	{
		OldAgent->GetTeamChangedDelegateChecked().RemoveDynamic(this, &ThisClass::HandleTeamChanged);
	}
	Viewer = Controller && Controller->GetWorld() == GetWorld() && Controller->IsLocalController() ? Controller : nullptr;
	if (ILyraTeamAgentInterface* NewAgent = Cast<ILyraTeamAgentInterface>(Viewer.Get()))
	{
		NewAgent->GetTeamChangedDelegateChecked().AddUniqueDynamic(this, &ThisClass::HandleTeamChanged);
	}
	if (IndicatorWidget.IsValid())
	{
		IndicatorWidget->SetOwnerPlayer(Viewer.IsValid() ? Viewer->GetLocalPlayer() : nullptr);
	}
	RefreshVisibility();
}

void UBwayRelicRequestIndicatorComponent::HandleTagChanged(FGameplayTag /*Tag*/, int32 /*Count*/)
{
	RefreshVisibility();
}

void UBwayRelicRequestIndicatorComponent::HandleTeamChanged(UObject* /*TeamAgent*/, int32 /*OldTeam*/, int32 /*NewTeam*/)
{
	RefreshVisibility();
}

void UBwayRelicRequestIndicatorComponent::RefreshVisibility()
{
	if (!IndicatorWidget.IsValid())
	{
		return;
	}
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	const bool bRequestActive = AbilitySystem.IsValid() && AbilitySystem->GetAvatarActor() == GetOwner()
		&& RequestTag.IsValid() && AbilitySystem->HasMatchingGameplayTag(RequestTag)
		&& !AbilitySystem->HasMatchingGameplayTag(LyraGameplayTags::Status_Death);
	const bool bEligibleViewer = !bEndingPlay && Viewer.IsValid() && Teams
		&& Teams->FindPlayerStateFromActor(Viewer.Get()) != Teams->FindPlayerStateFromActor(GetOwner())
		&& Teams->CompareTeams(Viewer.Get(), GetOwner()) == ELyraTeamComparison::OnSameTeam;
	IndicatorWidget->SetVisibility(bRequestActive && bEligibleViewer);
}

void UBwayRelicRequestIndicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	HandleAbilitySystemUninitialized();
	SetViewer(nullptr);
	if (LocalPlayer.IsValid())
	{
		LocalPlayer->OnPlayerControllerChanged().Remove(ControllerChangedHandle);
	}
	if (ILyraTeamAgentInterface* TeamAgent = Cast<ILyraTeamAgentInterface>(GetOwner()))
	{
		TeamAgent->GetTeamChangedDelegateChecked().RemoveDynamic(this, &ThisClass::HandleTeamChanged);
	}
	Super::EndPlay(EndPlayReason);
}
