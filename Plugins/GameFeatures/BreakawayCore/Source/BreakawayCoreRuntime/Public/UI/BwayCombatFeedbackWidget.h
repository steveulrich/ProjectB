// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayCombatFeedbackWidget.generated.h"

/** Local, non-interactive feedback from server-confirmed combat messages. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayCombatFeedbackWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void ShowHitConfirm();
	void ShowIncomingDamage(FVector SourceLocation, bool bHasSourceLocation);
	void ClearFeedback();

	/** Screen direction in camera-yaw space: right is +X, forward is -Y. */
	static FVector2D ResolveDamageDirection(const FVector& Source, const FVector& ViewLocation, float ViewYaw);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FDamagePulse
	{
		FVector Source = FVector::ZeroVector;
		double StartTime = 0.0;
		bool bHasSource = false;
	};

	void StartFeedbackTimer();
	void RefreshFeedback();
	double Now() const;
	bool HasLiveLocalPawn() const;
	TArray<FDamagePulse> DamagePulses;
	double HitConfirmStart = -1.0;
	FTimerHandle FeedbackTimer;
};
