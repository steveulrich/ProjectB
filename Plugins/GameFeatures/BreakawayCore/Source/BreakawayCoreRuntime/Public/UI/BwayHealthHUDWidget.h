// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "BwayHealthHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class ULyraHealthComponent;

/**
 * Lyra HUD slot widget for local player health.
 * Injected via UGameFeatureAction_AddWidgets into HUD.Slot.Health.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Health HUD Widget"))
class BREAKAWAYCORERUNTIME_API UBwayHealthHUDWidget : public UBwayMatchHUDWidgetBase
{
	GENERATED_BODY()

public:
	UBwayHealthHUDWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "HUD|Health")
	void RefreshHealthDisplay();

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnHealthChanged(float NewHealth, float MaxHealth, float HealthPercent);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UProgressBar> Progress_Health;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UTextBlock> Text_HealthValues;

private:
	void BindToHealthComponent();
	void UnbindFromHealthComponent();

	UFUNCTION()
	void HandleHealthChanged(ULyraHealthComponent* HealthComp, float OldValue, float NewValue, AActor* Instigator);

	void UpdateBoundHealthTexts(float NewHealth, float MaxHealth, float HealthPercent);

	ULyraHealthComponent* GetLocalPlayerHealthComponent() const;

	UPROPERTY()
	TWeakObjectPtr<ULyraHealthComponent> CachedHealthComponent;

	bool bBoundToHealth = false;
};
