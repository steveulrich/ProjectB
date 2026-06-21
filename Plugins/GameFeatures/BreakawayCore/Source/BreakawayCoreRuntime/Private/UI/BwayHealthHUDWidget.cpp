// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayHealthHUDWidget.h"
#include "Character/LyraHealthComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHealthHUDWidget)

UBwayHealthHUDWidget::UBwayHealthHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayHealthHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindToHealthComponent();
	RefreshHealthDisplay();
}

void UBwayHealthHUDWidget::NativeDestruct()
{
	UnbindFromHealthComponent();
	Super::NativeDestruct();
}

void UBwayHealthHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBoundToHealth || !CachedHealthComponent.IsValid())
	{
		BindToHealthComponent();
	}
}

void UBwayHealthHUDWidget::RefreshHealthDisplay()
{
	if (ULyraHealthComponent* HealthComp = GetLocalPlayerHealthComponent())
	{
		UpdateBoundHealthTexts(HealthComp->GetHealth(), HealthComp->GetMaxHealth(), HealthComp->GetHealthNormalized());
		OnHealthChanged(HealthComp->GetHealth(), HealthComp->GetMaxHealth(), HealthComp->GetHealthNormalized());
	}
}

void UBwayHealthHUDWidget::BindToHealthComponent()
{
	if (bBoundToHealth && CachedHealthComponent.IsValid())
	{
		return;
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (ULyraHealthComponent* HealthComp = ULyraHealthComponent::FindHealthComponent(Pawn))
			{
				if (CachedHealthComponent.IsValid() && bBoundToHealth)
				{
					CachedHealthComponent->OnHealthChanged.RemoveDynamic(this, &UBwayHealthHUDWidget::HandleHealthChanged);
				}

				CachedHealthComponent = HealthComp;
				HealthComp->OnHealthChanged.AddDynamic(this, &UBwayHealthHUDWidget::HandleHealthChanged);
				bBoundToHealth = true;
				RefreshHealthDisplay();
			}
		}
	}
}

void UBwayHealthHUDWidget::UnbindFromHealthComponent()
{
	if (CachedHealthComponent.IsValid() && bBoundToHealth)
	{
		CachedHealthComponent->OnHealthChanged.RemoveDynamic(this, &UBwayHealthHUDWidget::HandleHealthChanged);
	}

	CachedHealthComponent.Reset();
	bBoundToHealth = false;
}

void UBwayHealthHUDWidget::HandleHealthChanged(
	ULyraHealthComponent* HealthComp,
	float OldValue,
	float NewValue,
	AActor* Instigator)
{
	if (HealthComp)
	{
		UpdateBoundHealthTexts(NewValue, HealthComp->GetMaxHealth(), HealthComp->GetHealthNormalized());
		OnHealthChanged(NewValue, HealthComp->GetMaxHealth(), HealthComp->GetHealthNormalized());
	}
}

void UBwayHealthHUDWidget::UpdateBoundHealthTexts(float NewHealth, float MaxHealth, float HealthPercent)
{
	if (Progress_Health)
	{
		Progress_Health->SetPercent(HealthPercent);
	}

	if (Text_HealthValues)
	{
		Text_HealthValues->SetText(FText::Format(
			NSLOCTEXT("BwayHealthHUD", "HealthValues", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(NewHealth)),
			FText::AsNumber(FMath::RoundToInt(MaxHealth))));
	}
}

ULyraHealthComponent* UBwayHealthHUDWidget::GetLocalPlayerHealthComponent() const
{
	if (CachedHealthComponent.IsValid())
	{
		return CachedHealthComponent.Get();
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			return ULyraHealthComponent::FindHealthComponent(Pawn);
		}
	}

	return nullptr;
}
