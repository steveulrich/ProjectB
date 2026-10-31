// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "BwayHealthHUDWidget.generated.h"

class UAbilitySystemComponent;
class UBorder;
class ULyraHealthComponent;
class UProgressBar;
class UTextBlock;

/**
 * Lyra HUD slot widget for local player health, delayed damage trail, and armor.
 * Injected via UGameFeatureAction_AddWidgets into HUD.Slot.Health.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Health HUD Widget"))
class BREAKAWAYCORERUNTIME_API UBwayHealthHUDWidget : public UBwayMatchHUDWidgetBase
{
	GENERATED_BODY()

public:
	UBwayHealthHUDWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "HUD|Health")
	void RefreshHealthDisplay();

	UFUNCTION(BlueprintPure, Category = "HUD|Health")
	float GetDisplayedHealth() const { return CurrentHealthValue; }

	UFUNCTION(BlueprintPure, Category = "HUD|Health")
	float GetDisplayedMaxHealth() const { return CurrentMaxHealthValue; }

	UFUNCTION(BlueprintPure, Category = "HUD|Health")
	int32 GetDisplayedArmor() const { return CurrentArmorValue; }

	UFUNCTION(BlueprintPure, Category = "HUD|Health")
	float GetHealthPercent() const { return CurrentHealthPercent; }

	UFUNCTION(BlueprintPure, Category = "HUD|Health")
	float GetDamageTrailPercent() const { return DamageTrailPercent; }

	UFUNCTION(BlueprintPure, Category = "HUD|Health")
	FText GetDisplayedHealthText() const;

	UFUNCTION(BlueprintPure, Category = "HUD|Health")
	FText GetDisplayedArmorText() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnHealthChanged(float NewHealth, float MaxHealth, float HealthPercent);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UProgressBar> Progress_Health;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UProgressBar> Progress_DamageTrail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UTextBlock> Text_HealthValues;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UTextBlock> Text_ArmorValues;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Health", meta = (ClampMin = "0.0"))
	float DamageTrailHoldSeconds = 0.28f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Health", meta = (ClampMin = "0.1"))
	float DamageTrailDecayRate = 1.25f;

private:
	void EnsureVisualPolishWidgets();
	void BindToHealthComponent();
	void UnbindFromHealthComponent();
	void BindToArmorAttribute();
	void UnbindFromArmorAttribute();
	void RefreshArmorValue();

	UFUNCTION()
	void HandleHealthChanged(ULyraHealthComponent* HealthComp, float OldValue, float NewValue, AActor* Instigator);

	UFUNCTION()
	void HandleMaxHealthChanged(ULyraHealthComponent* HealthComp, float OldValue, float NewValue, AActor* Instigator);

	void HandleArmorAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void ApplyHealthState(float NewHealth, float MaxHealth, float HealthPercent, bool bSnapTrail);

	ULyraHealthComponent* ResolveCurrentPawnHealthComponent() const;
	UAbilitySystemComponent* ResolveLocalAbilitySystemComponent() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULyraHealthComponent> CachedHealthComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> CachedArmorASC;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ArmorBadgeBorder;

	FDelegateHandle ArmorChangedHandle;

	float CurrentHealthValue = 0.0f;
	float CurrentMaxHealthValue = 0.0f;
	float CurrentHealthPercent = 1.0f;
	float DamageTrailPercent = 1.0f;
	float DamageTrailHoldRemaining = 0.0f;
	int32 CurrentArmorValue = 0;
	bool bBoundToHealth = false;
	bool bHasInitialHealthSample = false;
	bool bVisualPolishInitialized = false;
};
