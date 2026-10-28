// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Feedback/NumberPops/LyraNumberPopComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messages/LyraVerbMessage.h"
#include "BwayCombatNumberPopComponent.generated.h"

class APlayerController;
class UBwayCombatReadabilityConfig;
class UTextRenderComponent;
class UBwayCombatFeedbackWidget;
class ULyraHealthComponent;
class APawn;
class APlayerState;

/** One live world-space combat number. */
USTRUCT()
struct FBwayLiveCombatNumberPop
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> TextComponent;

	FVector BaseLocation = FVector::ZeroVector;
	/** Flattened camera-right at spawn, already signed for left (-1) or right (+1). */
	FVector OutwardDirection = FVector::RightVector;
	FLinearColor BaseColor = FLinearColor::White;
	float SpawnWorldTime = 0.f;
	FVector LayoutOffset = FVector::ZeroVector;
	float LayoutHalfWidth = 0.f;
	float LayoutHeight = 0.f;
};

/**
 * Local-controller combat number display.
 * Listens for Breakaway number-pop verb messages (ClientBroadcastMessage on remote clients).
 * Listen-server / standalone hosts are presented directly by UBwayCombatFeedbackRouterComponent.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayCombatNumberPopComponent : public ULyraNumberPopComponent
{
	GENERATED_BODY()

public:
	UBwayCombatNumberPopComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void AddNumberPop(const FLyraNumberPopRequest& NewRequest) override;

	/** Present a number from a routed verb message (local host or ClientBroadcastMessage). */
	void PresentCombatNumber(FGameplayTag NumberTag, const FLyraVerbMessage& Payload);

	/** Location and scale (1→EndScale) for a live pop at Age seconds. */
	static void EvaluateNumberPopAnimation(
		float Age,
		float Lifespan,
		const FVector& BaseLocation,
		const FVector& OutwardDirection,
		float LateralOffset,
		float OutwardDistance,
		float RiseDistance,
		float EndScale,
		FVector& OutLocation,
		float& OutScaleMultiplier);

	/** Reserve a non-overlapping motion envelope against still-visible numbers in this target cluster. */
	static void ArrangeNumberPop(FBwayLiveCombatNumberPop& Pop, TConstArrayView<FBwayLiveCombatNumberPop> Existing,
		const FVector& CameraRight, float WorldSize, float TextWidth, float LateralOffset,
		float OutwardDistance, float RiseDistance, float Lifespan);

	/** Perspective depth scale; zero for invalid/behind-near-plane positions. */
	static float EvaluateNumberPopDepthScale(const FVector& WorldLocation, const FVector& CameraLocation,
		const FVector& CameraForward, float NearClipPlane);

	/** Shared recipient filter for listen-host broadcasts and remote owner delivery. */
	static bool IsMessageForPlayer(FGameplayTag NumberTag, const FLyraVerbMessage& Payload, const APlayerState* Player);
	static bool IsPawnPresentationActive(const APawn* Pawn);

protected:
	friend class FBwayCombatNumberEventPositionTest;
	friend class FBwayCombatFractionalHealRoutingTest;
	friend class FBwayCombatFractionalNumberFormatTest;
	friend class FBwayCombatNumberMaterialPoolingTest;
	friend class FBwayCombatNumberCameraDepthTest;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void OnCombatNumberMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);
	void EnsureConfig() const;
	FVector ResolvePopLocation(const FLyraVerbMessage& Payload) const;
	FLinearColor ResolveColor(const FGameplayTagContainer& TargetTags) const;
	UTextRenderComponent* AcquireTextComponent();
	void RecycleTextComponent(UTextRenderComponent* TextComponent);
	void UpdateLiveNumber(FBwayLiveCombatNumberPop& LivePop, float Now, const FVector& CameraLocation, const FRotator& BillboardRotation,
		const FVector& CameraForward, float NearClipPlane) const;
	FVector ResolveOutwardDirection(const APlayerController* PC, int32 SideSign) const;
	void EnsureFeedbackWidget();
	void ClearLocalFeedback();
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION()
	void OnOwnerDeathStarted(AActor* OwningActor);

	FGameplayMessageListenerHandle IncomingDamageHandle;
	FGameplayMessageListenerHandle OutgoingDamageHandle;
	FGameplayMessageListenerHandle IncomingHealHandle;

	UPROPERTY(Transient)
	mutable TObjectPtr<const UBwayCombatReadabilityConfig> CachedConfig;

	UPROPERTY(Transient)
	TArray<FBwayLiveCombatNumberPop> LivePops;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> PooledTextComponents;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCombatFeedbackWidget> FeedbackWidget;
	UPROPERTY(Transient)
	TWeakObjectPtr<ULyraHealthComponent> BoundHealthComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop", meta = (ClampMin = "4"))
	int32 MaxPooledNumbers = 24;

private:
	/** Empty for invalid amounts; never invents a minimum whole-number heal. */
	static FText FormatCombatNumber(double Magnitude);
	void AddFormattedNumberPop(const FLyraNumberPopRequest& NewRequest, const FText& DisplayText);
};
