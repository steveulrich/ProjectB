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
#include "Character/LyraHealthComponent.h"
#include "BwayPlayerState.h"
#include "UI/BwayCombatFeedbackWidget.h"

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
	PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::OnPossessedPawnChanged);
	OnPossessedPawnChanged(nullptr, PC->GetPawn());
	EnsureFeedbackWidget();

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
	// Remote controllers never registered these local presentation listeners.
	IncomingDamageHandle.Unregister();
	OutgoingDamageHandle.Unregister();
	IncomingHealHandle.Unregister();
	if (APlayerController* PC = GetController<APlayerController>())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessedPawnChanged);
	}
	if (BoundHealthComponent.IsValid())
	{
		BoundHealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::OnOwnerDeathStarted);
	}
	if (FeedbackWidget)
	{
		FeedbackWidget->ClearFeedback();
		FeedbackWidget->RemoveFromParent();
		FeedbackWidget = nullptr;
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
	if (Payload.bHasWorldLocation && !Payload.WorldLocation.ContainsNaN()) return Payload.WorldLocation;
	const FVector Offset = CachedConfig ? CachedConfig->NumberWorldOffset : FVector(0.f, 0.f, 90.f);
	return UBwayCombatReadabilityLibrary::ResolveNumberPopWorldLocation(Payload.Target, Offset);
}

void UBwayCombatNumberPopComponent::EnsureFeedbackWidget()
{
	APlayerController* PC = GetController<APlayerController>();
	if (!FeedbackWidget && PC && PC->IsLocalPlayerController() && PC->GetLocalPlayer())
	{
		FeedbackWidget = CreateWidget<UBwayCombatFeedbackWidget>(PC, UBwayCombatFeedbackWidget::StaticClass());
		if (FeedbackWidget)
		{
			FeedbackWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			FeedbackWidget->AddToPlayerScreen(25);
		}
	}
}

void UBwayCombatNumberPopComponent::ClearLocalFeedback()
{
	if (FeedbackWidget) FeedbackWidget->ClearFeedback();
	for (FBwayLiveCombatNumberPop& Pop : LivePops) RecycleTextComponent(Pop.TextComponent);
	LivePops.Reset();
	SetComponentTickEnabled(false);
}

void UBwayCombatNumberPopComponent::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (BoundHealthComponent.IsValid())
	{
		BoundHealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::OnOwnerDeathStarted);
	}
	ClearLocalFeedback();
	BoundHealthComponent = ULyraHealthComponent::FindHealthComponent(NewPawn);
	if (BoundHealthComponent.IsValid())
	{
		BoundHealthComponent->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::OnOwnerDeathStarted);
	}
}

void UBwayCombatNumberPopComponent::OnOwnerDeathStarted(AActor* OwningActor)
{
	ClearLocalFeedback();
}

bool UBwayCombatNumberPopComponent::IsMessageForPlayer(FGameplayTag NumberTag, const FLyraVerbMessage& Payload, const APlayerState* Player)
{
	if (!Player) return false;
	if (NumberTag == BwayCombatFeedbackTags::Number_OutgoingDamage)
	{
		return UBwayCombatReadabilityLibrary::ResolveFeedbackInstigator(Payload.Instigator) == Player;
	}
	return (NumberTag == BwayCombatFeedbackTags::Number_IncomingDamage || NumberTag == BwayCombatFeedbackTags::Number_IncomingHeal)
		&& UBwayCombatReadabilityLibrary::ResolveFeedbackPlayerTarget(Payload.Target) == Player;
}

bool UBwayCombatNumberPopComponent::IsPawnPresentationActive(const APawn* Pawn)
{
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed()) return false;
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	return !Health || !Health->IsDeadOrDying();
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

	// Reapply on every acquisition so a reused component follows the current
	// config, including returning to the engine default when the override clears.
	EnsureConfig();
	Text->SetTextMaterial(CachedConfig && CachedConfig->NumberPopMaterial
		? CachedConfig->NumberPopMaterial.Get() : GetDefault<UTextRenderComponent>()->GetMaterial(0));
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

void UBwayCombatNumberPopComponent::EvaluateNumberPopAnimation(
	float Age,
	float Lifespan,
	const FVector& BaseLocation,
	const FVector& OutwardDirection,
	float LateralOffset,
	float OutwardDistance,
	float RiseDistance,
	float EndScale,
	FVector& OutLocation,
	float& OutScaleMultiplier)
{
	const float Alpha = FMath::Clamp(Age / FMath::Max(Lifespan, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float MotionAlpha = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);
	const float Lateral = LateralOffset + (OutwardDistance * MotionAlpha);

	OutLocation = BaseLocation
		+ (OutwardDirection * Lateral)
		+ FVector(0.f, 0.f, RiseDistance * MotionAlpha);
	OutScaleMultiplier = FMath::Lerp(1.f, EndScale, Alpha);
}

void UBwayCombatNumberPopComponent::ArrangeNumberPop(FBwayLiveCombatNumberPop& Pop,
	TConstArrayView<FBwayLiveCombatNumberPop> Existing, const FVector& CameraRight, float WorldSize,
	float TextWidth, float LateralOffset, float OutwardDistance, float RiseDistance, float Lifespan)
{
	Pop.LayoutHalfWidth = FMath::Max(WorldSize * 0.5f, TextWidth * 0.5f);
	Pop.LayoutHeight = WorldSize;
	const float Margin = FMath::Max(8.f, WorldSize * 0.15f);
	float ColumnSpacing = 2.f * Pop.LayoutHalfWidth + OutwardDistance + Margin;
	float RowSpacing = WorldSize + RiseDistance + Margin;
	for (const FBwayLiveCombatNumberPop& Other : Existing)
	{
		if (Pop.SpawnWorldTime - Other.SpawnWorldTime >= Lifespan) continue;
		ColumnSpacing = FMath::Max(ColumnSpacing, 2.f * Other.LayoutHalfWidth + OutwardDistance + Margin);
		RowSpacing = FMath::Max(RowSpacing, Other.LayoutHeight + RiseDistance + Margin);
	}

	// Four columns keep a 12-shot burst compact. Reserve the complete motion envelope so
	// an older rising number cannot cross a newly spawned number halfway through its life.
	for (int32 Slot = 0; ; ++Slot)
	{
		const float Sign = (Slot % 2 == 0) ? 1.f : -1.f;
		const float InnerOffset = FMath::Max(LateralOffset, ColumnSpacing * 0.5f);
		const float X = Sign * (InnerOffset + ((Slot % 4) / 2) * ColumnSpacing);
		const float Z = (Slot / 4) * RowSpacing;
		const float MinX = X + FMath::Min(0.f, Sign * OutwardDistance) - Pop.LayoutHalfWidth;
		const float MaxX = X + FMath::Max(0.f, Sign * OutwardDistance) + Pop.LayoutHalfWidth;
		const float MinZ = Z - WorldSize * 0.5f;
		const float MaxZ = Z + RiseDistance + WorldSize * 0.5f;
		bool bOccupied = false;
		for (const FBwayLiveCombatNumberPop& Other : Existing)
		{
			if (Pop.SpawnWorldTime - Other.SpawnWorldTime >= Lifespan) continue;
			const FVector RelativeBase = Other.BaseLocation + Other.LayoutOffset - Pop.BaseLocation;
			// Numbers on substantially different depth planes need not reserve each other's lanes.
			if (FMath::Abs(FVector::DotProduct(RelativeBase, FVector::CrossProduct(CameraRight, FVector::UpVector))) > ColumnSpacing * 2.f) continue;
			const float OtherStartX = FVector::DotProduct(RelativeBase + Other.OutwardDirection * LateralOffset, CameraRight);
			const float OtherEndX = OtherStartX + FVector::DotProduct(Other.OutwardDirection * OutwardDistance, CameraRight);
			const float OtherMinX = FMath::Min(OtherStartX, OtherEndX) - Other.LayoutHalfWidth;
			const float OtherMaxX = FMath::Max(OtherStartX, OtherEndX) + Other.LayoutHalfWidth;
			const float OtherMinZ = RelativeBase.Z - Other.LayoutHeight * 0.5f;
			const float OtherMaxZ = RelativeBase.Z + RiseDistance + Other.LayoutHeight * 0.5f;
			if (MinX < OtherMaxX + Margin && MaxX > OtherMinX - Margin
				&& MinZ < OtherMaxZ + Margin && MaxZ > OtherMinZ - Margin)
			{
				bOccupied = true;
				break;
			}
		}
		if (!bOccupied)
		{
			Pop.OutwardDirection = CameraRight * Sign;
			Pop.LayoutOffset = CameraRight * (X - Sign * LateralOffset) + FVector(0.f, 0.f, Z);
			return;
		}
	}
}

float UBwayCombatNumberPopComponent::EvaluateNumberPopDepthScale(
	const FVector& WorldLocation, const FVector& CameraLocation, const FVector& CameraForward, float NearClipPlane)
{
	if (WorldLocation.ContainsNaN() || CameraLocation.ContainsNaN() || CameraForward.ContainsNaN()
		|| !FMath::IsFinite(NearClipPlane)) return 0.f;
	const FVector Forward = CameraForward.GetSafeNormal();
	if (Forward.IsNearlyZero()) return 0.f;
	const double Depth = FVector::DotProduct(WorldLocation - CameraLocation, Forward);
	if (!FMath::IsFinite(Depth) || Depth <= FMath::Max(0.f, NearClipPlane)) return 0.f;
	// Perspective size follows forward depth, not radial distance. The former
	// 1x minimum made nearby owner heals occupy most of the view at steep pitch.
	// Centered distant text retains its existing scale and 8x cap.
	return static_cast<float>(FMath::Clamp(Depth / 800.0, 0.0, 8.0));
}

FVector UBwayCombatNumberPopComponent::ResolveOutwardDirection(const APlayerController* PC, int32 SideSign) const
{
	const float Sign = (SideSign >= 0) ? 1.f : -1.f;
	FVector CameraRight = FVector::RightVector;

	if (PC && PC->PlayerCameraManager)
	{
		const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
		CameraRight = CameraRotation.Quaternion().GetRightVector();
		CameraRight.Z = 0.f;
		if (!CameraRight.Normalize())
		{
			CameraRight = FVector::RightVector;
		}
	}

	return CameraRight * Sign;
}

void UBwayCombatNumberPopComponent::UpdateLiveNumber(
	FBwayLiveCombatNumberPop& LivePop,
	float Now,
	const FVector& CameraLocation,
	const FRotator& BillboardRotation,
	const FVector& CameraForward, float NearClipPlane) const
{
	if (!LivePop.TextComponent)
	{
		return;
	}

	EnsureConfig();

	const float Lifespan = CachedConfig ? CachedConfig->NumberPopLifespan : 1.f;
	const float RiseDistance = CachedConfig ? CachedConfig->NumberPopRiseDistance : 80.f;
	const float LateralOffset = CachedConfig ? CachedConfig->NumberPopLateralOffset : 35.f;
	const float OutwardDistance = CachedConfig ? CachedConfig->NumberPopOutwardDistance : 45.f;
	const float EndScale = CachedConfig ? CachedConfig->NumberPopEndScale : 0.f;

	FVector WorldLocation = LivePop.BaseLocation;
	float ScaleMultiplier = 1.f;
	EvaluateNumberPopAnimation(
		Now - LivePop.SpawnWorldTime,
		Lifespan,
		LivePop.BaseLocation + LivePop.LayoutOffset,
		LivePop.OutwardDirection,
		LateralOffset,
		OutwardDistance,
		RiseDistance,
		EndScale,
		WorldLocation,
		ScaleMultiplier);

	const float Alpha = FMath::Clamp((Now - LivePop.SpawnWorldTime) / FMath::Max(Lifespan, KINDA_SMALL_NUMBER), 0.f, 1.f);
	FLinearColor DrawColor = LivePop.BaseColor;
	DrawColor.A = 1.f - Alpha;
	FColor SRGB = DrawColor.ToFColor(true);
	SRGB.A = static_cast<uint8>(FMath::Clamp(DrawColor.A * 255.f, 0.f, 255.f));

	const float DistanceScale = EvaluateNumberPopDepthScale(WorldLocation, CameraLocation, CameraForward, NearClipPlane);
	LivePop.TextComponent->SetVisibility(DistanceScale > 0.f);
	if (DistanceScale <= 0.f) return;
	const float WorldSize = FMath::Max(0.01f,
		(CachedConfig ? CachedConfig->NumberPopWorldSize : 72.f)
		* DistanceScale
		* (CachedConfig ? CachedConfig->CombatFeedbackScaleMultiplier : 1.f)
		* ScaleMultiplier);

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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float Lifespan = CachedConfig ? CachedConfig->NumberPopLifespan : 1.f;
	FVector CameraLocation = FVector::ZeroVector;
	FRotator BillboardRotation = FRotator::ZeroRotator;
	FVector CameraForward = FVector::ForwardVector;
	float NearClipPlane = 0.f;
	if (const APlayerController* PC = GetController<APlayerController>())
	{
		if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			CameraLocation = CameraManager->GetCameraLocation();
			BillboardRotation = CameraManager->GetCameraRotation();
			CameraForward = BillboardRotation.Vector();
			NearClipPlane = CameraManager->GetCameraCacheView().GetFinalPerspectiveNearClipPlane();
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

		UpdateLiveNumber(LivePop, Now, CameraLocation, BillboardRotation, CameraForward, NearClipPlane);
	}

	if (LivePops.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}

void UBwayCombatNumberPopComponent::AddNumberPop(const FLyraNumberPopRequest& NewRequest)
{
	if (NewRequest.NumberToDisplay <= 0) return;
	AddFormattedNumberPop(NewRequest, FText::AsNumber(NewRequest.NumberToDisplay));
}

FText UBwayCombatNumberPopComponent::FormatCombatNumber(double Magnitude)
{
	if (!FMath::IsFinite(Magnitude) || Magnitude <= 0.0) return FText::GetEmpty();
	FNumberFormattingOptions Options;
	Options.SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(1).SetRoundingMode(ERoundingMode::HalfFromZero);
	if (Magnitude < 0.1)
	{
		return FText::Format(NSLOCTEXT("BwayCombatNumbers", "BelowDisplayPrecision", "<{0}"), FText::AsNumber(0.1, &Options));
	}
	return FText::AsNumber(Magnitude, &Options);
}

void UBwayCombatNumberPopComponent::AddFormattedNumberPop(const FLyraNumberPopRequest& NewRequest, const FText& DisplayText)
{
	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalPlayerController() || !IsPawnPresentationActive(PC->GetPawn()))
	{
		return;
	}

	if (DisplayText.IsEmpty())
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
	Text->SetText(DisplayText);
	Text->SetTextRenderColor(Color.ToFColor(true));

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	FBwayLiveCombatNumberPop LivePop;
	LivePop.TextComponent = Text;
	LivePop.BaseLocation = NewRequest.WorldLocation;
	LivePop.BaseColor = Color;
	LivePop.SpawnWorldTime = Now;

	FVector CameraLocation = NewRequest.WorldLocation;
	FRotator BillboardRotation = FRotator::ZeroRotator;
	FVector CameraForward = FVector::ForwardVector;
	float NearClipPlane = 0.f;
	if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
	{
		CameraLocation = CameraManager->GetCameraLocation();
		BillboardRotation = CameraManager->GetCameraRotation();
		CameraForward = BillboardRotation.Vector();
		NearClipPlane = CameraManager->GetCameraCacheView().GetFinalPerspectiveNearClipPlane();
		BillboardRotation.Yaw += 180.f;
		BillboardRotation.Pitch = -BillboardRotation.Pitch;
	}
	const float DistanceScale = EvaluateNumberPopDepthScale(NewRequest.WorldLocation, CameraLocation, CameraForward, NearClipPlane);
	const float WorldSize = FMath::Max(0.01f, (CachedConfig ? CachedConfig->NumberPopWorldSize : 72.f) * DistanceScale
		* (CachedConfig ? CachedConfig->CombatFeedbackScaleMultiplier : 1.f));
	Text->SetWorldSize(WorldSize);
	const FVector TextSize = Text->GetTextLocalSize();
	ArrangeNumberPop(LivePop, LivePops, ResolveOutwardDirection(PC, 1), WorldSize, FMath::Max(TextSize.X, TextSize.Y),
		CachedConfig ? CachedConfig->NumberPopLateralOffset : 35.f,
		CachedConfig ? CachedConfig->NumberPopOutwardDistance : 45.f,
		CachedConfig ? CachedConfig->NumberPopRiseDistance : 80.f,
		CachedConfig ? CachedConfig->NumberPopLifespan : 1.f);
	LivePops.Add(LivePop);

	UpdateLiveNumber(LivePops.Last(), Now, CameraLocation, BillboardRotation, CameraForward, NearClipPlane);

	if (CVarBwayDebugCombatNumbers.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogBwayCombatNumbers, Log, TEXT("Number pop %s at %s tags=%s"),
			*DisplayText.ToString(),
			*NewRequest.WorldLocation.ToCompactString(),
			*NewRequest.TargetTags.ToStringSimple());
	}

	SetComponentTickEnabled(true);
}

void UBwayCombatNumberPopComponent::PresentCombatNumber(FGameplayTag NumberTag, const FLyraVerbMessage& Payload)
{
	const APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalPlayerController() || !IsPawnPresentationActive(PC->GetPawn())) return;
	const FText DisplayText = FormatCombatNumber(Payload.Magnitude);
	if (DisplayText.IsEmpty() || !NumberTag.IsValid())
	{
		return;
	}

	EnsureConfig();
	EnsureFeedbackWidget();
	if (NumberTag == BwayCombatFeedbackTags::Number_OutgoingDamage)
	{
		if (const UWorld* World = GetWorld())
		{
			LastOutgoingHitWorldTime = World->GetTimeSeconds();
		}
	}
	if (FeedbackWidget && (!BoundHealthComponent.IsValid() || !BoundHealthComponent->IsDeadOrDying()))
	{
		if (NumberTag == BwayCombatFeedbackTags::Number_OutgoingDamage) FeedbackWidget->ShowHitConfirm();
		else if (NumberTag == BwayCombatFeedbackTags::Number_IncomingDamage)
		{
			FeedbackWidget->ShowIncomingDamage(Payload.InstigatorWorldLocation,
				Payload.bHasInstigatorWorldLocation && !Payload.InstigatorWorldLocation.ContainsNaN());
		}
	}

	FLyraNumberPopRequest Request;
	Request.WorldLocation = ResolvePopLocation(Payload);
	Request.bIsCriticalDamage = false;
	Request.TargetTags.AddTag(NumberTag);
	Request.SourceTags = Payload.InstigatorTags;
	AddFormattedNumberPop(Request, DisplayText);
}

void UBwayCombatNumberPopComponent::OnCombatNumberMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	const APlayerController* PC = GetController<APlayerController>();
	if (PC && IsMessageForPlayer(Channel, Payload, PC->PlayerState)) PresentCombatNumber(Channel, Payload);
}
