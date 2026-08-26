// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayCombatNumberPopComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Combat/BwayCombatFeedbackTags.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCombatNumberPopComponent)

DEFINE_LOG_CATEGORY_STATIC(LogBwayCombatNumbers, Log, All);

static TAutoConsoleVariable<int32> CVarBwayDebugCombatNumbers(
	TEXT("bway.Combat.DebugNumbers"),
	0,
	TEXT("Log combat number-pop present/routing (0=off, 1=on)."),
	ECVF_Default);

UBwayCombatNumberPopComponent::UBwayCombatNumberPopComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UBwayCombatNumberPopComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}

	EnsureConfig();

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	IncomingDamageHandle = MessageSubsystem.RegisterListener(
		BwayCombatFeedbackTags::Number_IncomingDamage, this, &ThisClass::OnCombatNumberMessage);
	OutgoingDamageHandle = MessageSubsystem.RegisterListener(
		BwayCombatFeedbackTags::Number_OutgoingDamage, this, &ThisClass::OnCombatNumberMessage);
	IncomingHealHandle = MessageSubsystem.RegisterListener(
		BwayCombatFeedbackTags::Number_IncomingHeal, this, &ThisClass::OnCombatNumberMessage);
}

void UBwayCombatNumberPopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.UnregisterListener(IncomingDamageHandle);
		MessageSubsystem.UnregisterListener(OutgoingDamageHandle);
		MessageSubsystem.UnregisterListener(IncomingHealHandle);
	}

	for (FBwayLiveCombatNumberPop& LivePop : LivePops)
	{
		if (LivePop.TextComponent)
		{
			LivePop.TextComponent->DestroyComponent();
		}
	}
	LivePops.Reset();

	for (UTextRenderComponent* Pooled : PooledTextComponents)
	{
		if (Pooled)
		{
			Pooled->DestroyComponent();
		}
	}
	PooledTextComponents.Reset();

	Super::EndPlay(EndPlayReason);
}

void UBwayCombatNumberPopComponent::EnsureConfig() const
{
	if (!CachedConfig)
	{
		CachedConfig = UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(this);
	}
}

FVector UBwayCombatNumberPopComponent::ResolvePopLocation(const FLyraVerbMessage& Payload) const
{
	const FVector Offset = CachedConfig ? CachedConfig->NumberWorldOffset : FVector(0.f, 0.f, 90.f);
	return UBwayCombatReadabilityLibrary::ResolveNumberPopWorldLocation(Payload.Target, Offset);
}

FLinearColor UBwayCombatNumberPopComponent::ResolveColor(const FGameplayTagContainer& TargetTags) const
{
	EnsureConfig();
	if (!CachedConfig)
	{
		return FLinearColor::White;
	}

	if (TargetTags.HasTag(BwayCombatFeedbackTags::Number_IncomingDamage))
	{
		return CachedConfig->ResolveIncomingDamageColor();
	}
	if (TargetTags.HasTag(BwayCombatFeedbackTags::Number_IncomingHeal))
	{
		return CachedConfig->ResolveIncomingHealColor();
	}
	if (TargetTags.HasTag(BwayCombatFeedbackTags::Number_OutgoingDamage))
	{
		return CachedConfig->ResolveOutgoingDamageColor();
	}

	return CachedConfig->ResolveOutgoingDamageColor();
}

UTextRenderComponent* UBwayCombatNumberPopComponent::AcquireTextComponent()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	UTextRenderComponent* Text = nullptr;
	while (PooledTextComponents.Num() > 0 && !Text)
	{
		Text = PooledTextComponents.Pop(EAllowShrinking::No);
		if (!Text)
		{
			continue;
		}
	}

	if (!Text)
	{
		Text = NewObject<UTextRenderComponent>(OwnerActor);
		Text->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Text->SetCastShadow(false);
		Text->SetMobility(EComponentMobility::Movable);
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetHiddenInGame(false);
		Text->SetCanEverAffectNavigation(false);
		Text->SetRenderCustomDepth(true);
		Text->SetCustomDepthStencilValue(123);
		Text->SetupAttachment(nullptr);
		Text->RegisterComponent();
	}
	else if (!Text->IsRegistered())
	{
		Text->RegisterComponent();
	}

	Text->SetHiddenInGame(false);
	Text->SetVisibility(true, true);
	return Text;
}

void UBwayCombatNumberPopComponent::RecycleTextComponent(UTextRenderComponent* TextComponent)
{
	if (!TextComponent)
	{
		return;
	}

	TextComponent->SetVisibility(false, true);
	TextComponent->SetHiddenInGame(true);

	if (PooledTextComponents.Num() < MaxPooledNumbers)
	{
		PooledTextComponents.Add(TextComponent);
	}
	else
	{
		TextComponent->DestroyComponent();
	}
}

void UBwayCombatNumberPopComponent::UpdateLiveNumber(
	FBwayLiveCombatNumberPop& LivePop,
	float Now,
	const FVector& CameraLocation,
	const FRotator& BillboardRotation,
	float Lifespan,
	float RiseDistance) const
{
	if (!LivePop.TextComponent)
	{
		return;
	}

	const float Alpha = FMath::Clamp((Now - LivePop.SpawnWorldTime) / FMath::Max(Lifespan, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const FVector WorldLocation = LivePop.BaseLocation + FVector(0.f, 0.f, RiseDistance * Alpha);

	FLinearColor DrawColor = LivePop.BaseColor;
	DrawColor.A = 1.f - Alpha;
	FColor SRGB = DrawColor.ToFColor(true);
	SRGB.A = static_cast<uint8>(FMath::Clamp(DrawColor.A * 255.f, 0.f, 255.f));

	const float Distance = FVector::Distance(CameraLocation, WorldLocation);
	const float DistanceScale = FMath::Clamp(Distance / 800.f, 1.f, 8.f);
	const float WorldSize = (CachedConfig ? CachedConfig->NumberPopWorldSize : 72.f)
		* DistanceScale
		* (CachedConfig ? CachedConfig->CombatFeedbackScaleMultiplier : 1.f);

	LivePop.TextComponent->SetWorldLocationAndRotation(WorldLocation, BillboardRotation);
	LivePop.TextComponent->SetTextRenderColor(SRGB);
	LivePop.TextComponent->SetWorldSize(WorldSize);
}

void UBwayCombatNumberPopComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (LivePops.Num() == 0)
	{
		SetComponentTickEnabled(false);
		return;
	}

	EnsureConfig();
	const float Lifespan = CachedConfig ? CachedConfig->NumberPopLifespan : 1.f;
	const float RiseDistance = CachedConfig ? CachedConfig->NumberPopRiseDistance : 80.f;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	FVector CameraLocation = FVector::ZeroVector;
	FRotator BillboardRotation = FRotator::ZeroRotator;
	if (const APlayerController* PC = GetController<APlayerController>())
	{
		if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			CameraLocation = CameraManager->GetCameraLocation();
			BillboardRotation = CameraManager->GetCameraRotation();
			BillboardRotation.Yaw += 180.f;
			BillboardRotation.Pitch = -BillboardRotation.Pitch;
		}
	}

	for (int32 Index = LivePops.Num() - 1; Index >= 0; --Index)
	{
		FBwayLiveCombatNumberPop& LivePop = LivePops[Index];
		if ((Now - LivePop.SpawnWorldTime) >= Lifespan)
		{
			RecycleTextComponent(LivePop.TextComponent);
			LivePops.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		UpdateLiveNumber(LivePop, Now, CameraLocation, BillboardRotation, Lifespan, RiseDistance);
	}

	if (LivePops.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}

void UBwayCombatNumberPopComponent::AddNumberPop(const FLyraNumberPopRequest& NewRequest)
{
	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}

	if (NewRequest.NumberToDisplay <= 0)
	{
		return;
	}

	EnsureConfig();

	UTextRenderComponent* Text = AcquireTextComponent();
	if (!Text)
	{
		UE_LOG(LogBwayCombatNumbers, Warning, TEXT("UBwayCombatNumberPopComponent: failed to create text component"));
		return;
	}

	const FLinearColor Color = ResolveColor(NewRequest.TargetTags);
	Text->SetText(FText::AsNumber(NewRequest.NumberToDisplay));
	Text->SetTextRenderColor(Color.ToFColor(true));

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	FBwayLiveCombatNumberPop LivePop;
	LivePop.TextComponent = Text;
	LivePop.BaseLocation = NewRequest.WorldLocation;
	LivePop.BaseColor = Color;
	LivePop.SpawnWorldTime = Now;
	LivePops.Add(LivePop);

	FVector CameraLocation = NewRequest.WorldLocation;
	FRotator BillboardRotation = FRotator::ZeroRotator;
	if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
	{
		CameraLocation = CameraManager->GetCameraLocation();
		BillboardRotation = CameraManager->GetCameraRotation();
		BillboardRotation.Yaw += 180.f;
		BillboardRotation.Pitch = -BillboardRotation.Pitch;
	}

	const float Lifespan = CachedConfig ? CachedConfig->NumberPopLifespan : 1.f;
	const float RiseDistance = CachedConfig ? CachedConfig->NumberPopRiseDistance : 80.f;
	UpdateLiveNumber(LivePops.Last(), Now, CameraLocation, BillboardRotation, Lifespan, RiseDistance);

	if (CVarBwayDebugCombatNumbers.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogBwayCombatNumbers, Log, TEXT("Number pop %d at %s tags=%s"),
			NewRequest.NumberToDisplay,
			*NewRequest.WorldLocation.ToCompactString(),
			*NewRequest.TargetTags.ToStringSimple());
	}

	SetComponentTickEnabled(true);
}

void UBwayCombatNumberPopComponent::PresentCombatNumber(FGameplayTag NumberTag, const FLyraVerbMessage& Payload)
{
	const int32 Magnitude = FMath::RoundToInt(static_cast<float>(Payload.Magnitude));
	if (Magnitude <= 0 || !NumberTag.IsValid())
	{
		return;
	}

	EnsureConfig();

	FLyraNumberPopRequest Request;
	Request.WorldLocation = ResolvePopLocation(Payload);
	Request.NumberToDisplay = Magnitude;
	Request.bIsCriticalDamage = false;
	Request.TargetTags.AddTag(NumberTag);
	Request.SourceTags = Payload.InstigatorTags;
	AddNumberPop(Request);
}

void UBwayCombatNumberPopComponent::OnCombatNumberMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	PresentCombatNumber(Channel, Payload);
}
