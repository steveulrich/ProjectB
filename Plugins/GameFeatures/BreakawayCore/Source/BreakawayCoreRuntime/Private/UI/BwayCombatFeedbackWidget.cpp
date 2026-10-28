// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCombatFeedbackWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "TimerManager.h"

namespace BwayCombatFeedback
{
	constexpr double HitDuration = 0.18;
	constexpr double DamageDuration = 0.8;
	constexpr int32 MaxDamagePulses = 4;
}

void UBwayCombatFeedbackWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(false);
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FeedbackRoot"));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBwayCombatFeedbackWidget::NativeDestruct()
{
	ClearFeedback();
	Super::NativeDestruct();
}

double UBwayCombatFeedbackWidget::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

bool UBwayCombatFeedbackWidget::HasLiveLocalPawn() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !PC->IsLocalController() || !Pawn)
	{
		return false;
	}
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
	return !Health || !Health->IsDeadOrDying();
}

void UBwayCombatFeedbackWidget::ShowHitConfirm()
{
	if (HasLiveLocalPawn())
	{
		HitConfirmStart = Now();
		StartFeedbackTimer();
	}
}

void UBwayCombatFeedbackWidget::ShowIncomingDamage(FVector SourceLocation, bool bHasSourceLocation)
{
	if (!HasLiveLocalPawn())
	{
		return;
	}
	if (DamagePulses.Num() == BwayCombatFeedback::MaxDamagePulses)
	{
		DamagePulses.RemoveAt(0);
	}
	FDamagePulse& Pulse = DamagePulses.AddDefaulted_GetRef();
	Pulse.Source = SourceLocation;
	Pulse.bHasSource = bHasSourceLocation && !SourceLocation.ContainsNaN();
	Pulse.StartTime = Now();
	StartFeedbackTimer();
}

void UBwayCombatFeedbackWidget::StartFeedbackTimer()
{
	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(FeedbackTimer))
		{
			World->GetTimerManager().SetTimer(FeedbackTimer, this,
				&ThisClass::RefreshFeedback, 1.f / 60.f, true);
		}
	}
	InvalidateLayoutAndVolatility();
}

void UBwayCombatFeedbackWidget::RefreshFeedback()
{
	if (!HasLiveLocalPawn())
	{
		ClearFeedback();
		return;
	}
	const double Time = Now();
	DamagePulses.RemoveAll([Time](const FDamagePulse& Pulse)
	{
		return Time - Pulse.StartTime >= BwayCombatFeedback::DamageDuration;
	});
	if (Time - HitConfirmStart >= BwayCombatFeedback::HitDuration)
	{
		HitConfirmStart = -1.0;
	}
	if (DamagePulses.IsEmpty() && HitConfirmStart < 0.0)
	{
		ClearFeedback();
	}
	else
	{
		InvalidateLayoutAndVolatility();
	}
}

void UBwayCombatFeedbackWidget::ClearFeedback()
{
	HitConfirmStart = -1.0;
	DamagePulses.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FeedbackTimer);
	}
	InvalidateLayoutAndVolatility();
}

FVector2D UBwayCombatFeedbackWidget::ResolveDamageDirection(const FVector& Source, const FVector& ViewLocation, float ViewYaw)
{
	const FVector Direction = (Source - ViewLocation).GetSafeNormal2D();
	const FRotationMatrix ViewRotation(FRotator(0.f, ViewYaw, 0.f));
	return FVector2D(FVector::DotProduct(Direction, ViewRotation.GetUnitAxis(EAxis::Y)),
		-FVector::DotProduct(Direction, ViewRotation.GetUnitAxis(EAxis::X))).GetSafeNormal();
}

int32 UBwayCombatFeedbackWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 TopLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!HasLiveLocalPawn() || (HitConfirmStart < 0.0 && DamagePulses.IsEmpty()))
	{
		return TopLayer;
	}
	const UBwayCombatReadabilityConfig* Config = UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(this);
	const float Scale = Config ? FMath::Clamp(Config->CombatFeedbackScaleMultiplier, 0.25f, 2.f) : 1.f;
	const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5;
	const double Time = Now();
	const auto DrawLine = [&](const TArray<FVector2D>& Points, FLinearColor Color, float Width)
	{
		Color *= InWidgetStyle.GetColorAndOpacityTint();
		FSlateDrawElement::MakeLines(OutDrawElements, ++TopLayer, AllottedGeometry.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, Color, true, Width * Scale);
	};
	if (HitConfirmStart >= 0.0)
	{
		FLinearColor Color = Config ? Config->ResolveOutgoingDamageColor() : FLinearColor::White;
		Color.A *= FMath::Clamp(1.0 - (Time - HitConfirmStart) / BwayCombatFeedback::HitDuration, 0.0, 1.0);
		for (const FVector2D Sign : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1)})
		{
			DrawLine({Center + Sign * 7.f * Scale, Center + Sign * 13.f * Scale}, Color, 2.f);
		}
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	GetOwningPlayer()->GetPlayerViewPoint(ViewLocation, ViewRotation);
	for (const FDamagePulse& Pulse : DamagePulses)
	{
		FLinearColor Color = Config ? Config->ResolveIncomingDamageColor() : FLinearColor::Red;
		Color.A *= FMath::Clamp(1.0 - (Time - Pulse.StartTime) / BwayCombatFeedback::DamageDuration, 0.0, 1.0);
		const FVector2D Direction = Pulse.bHasSource ? ResolveDamageDirection(Pulse.Source, ViewLocation, ViewRotation.Yaw) : FVector2D::ZeroVector;
		if (Direction.IsNearlyZero())
		{
			// Unknown/environmental sources have no invented direction.
			TArray<FVector2D> Ring;
			for (int32 Index = 0; Index <= 24; ++Index)
			{
				const double Angle = Index * UE_TWO_PI / 24.0;
				Ring.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 68.f * Scale);
			}
			DrawLine(Ring, Color, 2.f);
		}
		else
		{
			const FVector2D Side(-Direction.Y, Direction.X);
			const FVector2D Tip = Center + Direction * 72.f * Scale;
			DrawLine({Tip - Direction * 9.f * Scale + Side * 12.f * Scale, Tip,
				Tip - Direction * 9.f * Scale - Side * 12.f * Scale}, Color, 3.f);
		}
	}
	return TopLayer;
}
