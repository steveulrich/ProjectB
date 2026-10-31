// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayHealthHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHealthHUDWidget)

UBwayHealthHUDWidget::UBwayHealthHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayHealthHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureVisualPolishWidgets();
	BindToHealthComponent();
	BindToArmorAttribute();
	RefreshHealthDisplay();
}

void UBwayHealthHUDWidget::NativeDestruct()
{
	UnbindFromHealthComponent();
	UnbindFromArmorAttribute();
	Super::NativeDestruct();
}

void UBwayHealthHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Detect replacement pawns immediately on respawn or round reset.
	ULyraHealthComponent* LiveHealthComp = ResolveCurrentPawnHealthComponent();
	if (LiveHealthComp != CachedHealthComponent.Get() || !bBoundToHealth)
	{
		BindToHealthComponent();
	}

	UAbilitySystemComponent* LiveASC = ResolveLocalAbilitySystemComponent();
	if (LiveASC != CachedArmorASC.Get())
	{
		BindToArmorAttribute();
	}

	if (DamageTrailPercent > CurrentHealthPercent + KINDA_SMALL_NUMBER)
	{
		if (DamageTrailHoldRemaining > 0.0f)
		{
			DamageTrailHoldRemaining = FMath::Max(0.0f, DamageTrailHoldRemaining - InDeltaTime);
		}
		else
		{
			DamageTrailPercent = FMath::Max(
				CurrentHealthPercent,
				DamageTrailPercent - (DamageTrailDecayRate * InDeltaTime));
			if (Progress_DamageTrail)
			{
				Progress_DamageTrail->SetPercent(DamageTrailPercent);
			}
			InvalidateLayoutAndVolatility();
		}
	}
	else if (DamageTrailPercent < CurrentHealthPercent)
	{
		DamageTrailPercent = CurrentHealthPercent;
		if (Progress_DamageTrail)
		{
			Progress_DamageTrail->SetPercent(DamageTrailPercent);
		}
		InvalidateLayoutAndVolatility();
	}
}

void UBwayHealthHUDWidget::EnsureVisualPolishWidgets()
{
	if (bVisualPolishInitialized || !WidgetTree)
	{
		return;
	}
	bVisualPolishInitialized = true;

	// Strip any legacy authored border backgrounds or constraining SizeBoxes from W_BW_HealthWidget
	WidgetTree->ForEachWidget([](UWidget* Candidate)
	{
		if (UBorder* Border = Cast<UBorder>(Candidate))
		{
			FSlateBrush NoBrush;
			NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			Border->SetBrush(NoBrush);
			Border->SetBrushColor(FLinearColor::Transparent);
			Border->SetPadding(FMargin(0.0f));
		}
		else if (USizeBox* SizeBox = Cast<USizeBox>(Candidate))
		{
			SizeBox->ClearWidthOverride();
			SizeBox->ClearHeightOverride();
		}
	});

	if (!WidgetTree->RootWidget && !Progress_Health)
	{
		UHorizontalBox* RootRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_130"));
		Progress_DamageTrail = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress_DamageTrail"));
		Progress_Health = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress_Health"));
		Text_HealthValues = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_HealthValues"));
		WidgetTree->RootWidget = RootRow;
	}

	UHorizontalBox* HealthRow = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("HorizontalBox_130")));
	if (!HealthRow)
	{
		WidgetTree->ForEachWidget([&HealthRow](UWidget* Candidate)
		{
			if (!HealthRow)
			{
				HealthRow = Cast<UHorizontalBox>(Candidate);
			}
		});
	}

	if (HealthRow)
	{
		if (!Progress_DamageTrail)
		{
			Progress_DamageTrail = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress_DamageTrail"));
		}
		if (!Progress_Health)
		{
			Progress_Health = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress_Health"));
		}
		if (!Text_HealthValues)
		{
			Text_HealthValues = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_HealthValues"));
		}

		Progress_DamageTrail->RemoveFromParent();
		Progress_Health->RemoveFromParent();
		Text_HealthValues->RemoveFromParent();
		HealthRow->ClearChildren();

		// Left column: bold condensed bright-cyan integer health value ("500")
		USizeBox* ValueSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthValueSizeBox"));
		ValueSizeBox->SetWidthOverride(68.0f);
		ValueSizeBox->SetHeightOverride(36.0f);
		Text_HealthValues->SetFont(GetBroadcastCondensedFont(30, 4, 2));
		Text_HealthValues->SetColorAndOpacity(FSlateColor(FLinearColor(0.10f, 0.92f, 0.96f, 1.0f)));
		Text_HealthValues->SetShadowOffset(FVector2D(1.25f, 1.25f));
		Text_HealthValues->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f));
		Text_HealthValues->SetJustification(ETextJustify::Right);
		ValueSizeBox->SetContent(Text_HealthValues);
		if (USizeBoxSlot* ValSlot = Cast<USizeBoxSlot>(Text_HealthValues->Slot))
		{
			ValSlot->SetHorizontalAlignment(HAlign_Right);
			ValSlot->SetVerticalAlignment(VAlign_Center);
			ValSlot->SetPadding(FMargin(0.0f, -3.0f, 0.0f, 0.0f));
		}
		if (UHorizontalBoxSlot* ValueHSlot = HealthRow->AddChildToHorizontalBox(ValueSizeBox))
		{
			ValueHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ValueHSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Right column: 344 x 26 horizontal Health Bar (custom-painted glowing cyan->aqua gradient + delayed damage trail)
		UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HealthBarOverlay"));
		if (UOverlaySlot* TrailOverlaySlot = BarOverlay->AddChildToOverlay(Progress_DamageTrail))
		{
			TrailOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			TrailOverlaySlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* ForeOverlaySlot = BarOverlay->AddChildToOverlay(Progress_Health))
		{
			ForeOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			ForeOverlaySlot->SetVerticalAlignment(VAlign_Fill);
		}
		// Keep UProgressBar Percent values live for state queries while NativePaint renders the crisp broadcast bar
		BarOverlay->SetRenderOpacity(0.0f);

		USizeBox* BarSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthBarSizeBox"));
		BarSizeBox->SetWidthOverride(344.0f);
		BarSizeBox->SetHeightOverride(26.0f);
		BarSizeBox->SetContent(BarOverlay);
		if (UHorizontalBoxSlot* BarHSlot = HealthRow->AddChildToHorizontalBox(BarSizeBox))
		{
			BarHSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
			BarHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BarHSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (!Text_ArmorValues)
		{
			ArmorBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ArmorBadgeBorder"));
			Text_ArmorValues = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ArmorValues"));
			ArmorBadgeBorder->SetContent(Text_ArmorValues);
			HealthRow->AddChildToHorizontalBox(ArmorBadgeBorder);
		}
		if (ArmorBadgeBorder)
		{
			// Broadcast HUD omits an inline armor pill next to the health bar; keep Text_ArmorValues updated for queries/tests.
			ArmorBadgeBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UBwayHealthHUDWidget::RefreshHealthDisplay()
{
	EnsureVisualPolishWidgets();
	BindToArmorAttribute();

	ULyraHealthComponent* LiveHealthComp = ResolveCurrentPawnHealthComponent();
	const bool bPawnChanged = (LiveHealthComp != CachedHealthComponent.Get());
	if (!bBoundToHealth || bPawnChanged)
	{
		BindToHealthComponent();
	}
	RefreshArmorValue();

	if (LiveHealthComp)
	{
		const bool bSnapTrail = !bHasInitialHealthSample || bPawnChanged;
		ApplyHealthState(LiveHealthComp->GetHealth(), LiveHealthComp->GetMaxHealth(), LiveHealthComp->GetHealthNormalized(), bSnapTrail);
		OnHealthChanged(LiveHealthComp->GetHealth(), LiveHealthComp->GetMaxHealth(), LiveHealthComp->GetHealthNormalized());
	}
}

void UBwayHealthHUDWidget::BindToHealthComponent()
{
	ULyraHealthComponent* LiveHealthComp = ResolveCurrentPawnHealthComponent();
	if (bBoundToHealth && CachedHealthComponent.Get() == LiveHealthComp && LiveHealthComp != nullptr)
	{
		return;
	}

	UnbindFromHealthComponent();

	if (LiveHealthComp)
	{
		CachedHealthComponent = LiveHealthComp;
		LiveHealthComp->OnHealthChanged.AddUniqueDynamic(this, &UBwayHealthHUDWidget::HandleHealthChanged);
		LiveHealthComp->OnMaxHealthChanged.AddUniqueDynamic(this, &UBwayHealthHUDWidget::HandleMaxHealthChanged);
		bBoundToHealth = true;

		// Snap damage trail cleanly onto the newly possessed replacement pawn's health.
		ApplyHealthState(
			LiveHealthComp->GetHealth(),
			LiveHealthComp->GetMaxHealth(),
			LiveHealthComp->GetHealthNormalized(),
			/*bSnapTrail*/ true);
		OnHealthChanged(LiveHealthComp->GetHealth(), LiveHealthComp->GetMaxHealth(), LiveHealthComp->GetHealthNormalized());
	}
}

void UBwayHealthHUDWidget::UnbindFromHealthComponent()
{
	if (CachedHealthComponent.IsValid() && bBoundToHealth)
	{
		CachedHealthComponent->OnHealthChanged.RemoveDynamic(this, &UBwayHealthHUDWidget::HandleHealthChanged);
		CachedHealthComponent->OnMaxHealthChanged.RemoveDynamic(this, &UBwayHealthHUDWidget::HandleMaxHealthChanged);
	}

	CachedHealthComponent.Reset();
	bBoundToHealth = false;
}

void UBwayHealthHUDWidget::BindToArmorAttribute()
{
	UAbilitySystemComponent* ASC = ResolveLocalAbilitySystemComponent();
	if (CachedArmorASC.Get() == ASC && ArmorChangedHandle.IsValid())
	{
		return;
	}

	UnbindFromArmorAttribute();

	if (ASC)
	{
		CachedArmorASC = ASC;
		ArmorChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBwayHeroAttributeSet::GetArmorAttribute())
			.AddUObject(this, &UBwayHealthHUDWidget::HandleArmorAttributeChanged);
	}

	RefreshArmorValue();
}

void UBwayHealthHUDWidget::UnbindFromArmorAttribute()
{
	if (CachedArmorASC.IsValid() && ArmorChangedHandle.IsValid())
	{
		CachedArmorASC->GetGameplayAttributeValueChangeDelegate(UBwayHeroAttributeSet::GetArmorAttribute())
			.Remove(ArmorChangedHandle);
	}

	ArmorChangedHandle.Reset();
	CachedArmorASC.Reset();
}

void UBwayHealthHUDWidget::HandleArmorAttributeChanged(const FOnAttributeChangeData& /*ChangeData*/)
{
	RefreshArmorValue();
}

void UBwayHealthHUDWidget::RefreshArmorValue()
{
	float ResolvedArmor = 0.0f;
	bool bFoundOnASC = false;

	if (UAbilitySystemComponent* ASC = ResolveLocalAbilitySystemComponent())
	{
		if (ASC->HasAttributeSetForAttribute(UBwayHeroAttributeSet::GetArmorAttribute()))
		{
			ResolvedArmor = ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetArmorAttribute());
			bFoundOnASC = true;
		}
	}

	if (!bFoundOnASC)
	{
		if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(GetOwningPlayerState()))
		{
			const FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
			if (HeroId.IsValid())
			{
				if (const UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
				{
					ResolvedArmor = HeroData->HeroStats.Armor;
				}
			}
		}
	}

	CurrentArmorValue = FMath::Max(0, FMath::RoundToInt(ResolvedArmor));
	if (Text_ArmorValues)
	{
		Text_ArmorValues->SetText(FText::FromString(FString::Printf(TEXT("ARMOR %d"), CurrentArmorValue)));
	}
}

void UBwayHealthHUDWidget::HandleHealthChanged(
	ULyraHealthComponent* HealthComp,
	float /*OldValue*/,
	const float NewValue,
	AActor* /*Instigator*/)
{
	if (HealthComp)
	{
		const float MaxHP = HealthComp->GetMaxHealth();
		const float Normalized = (MaxHP > KINDA_SMALL_NUMBER) ? FMath::Clamp(NewValue / MaxHP, 0.0f, 1.0f) : 0.0f;
		ApplyHealthState(NewValue, MaxHP, Normalized, /*bSnapTrail*/ false);
		OnHealthChanged(NewValue, MaxHP, Normalized);
	}
}

void UBwayHealthHUDWidget::HandleMaxHealthChanged(
	ULyraHealthComponent* HealthComp,
	float /*OldValue*/,
	const float NewValue,
	AActor* /*Instigator*/)
{
	if (HealthComp)
	{
		const float CurHP = HealthComp->GetHealth();
		const float Normalized = (NewValue > KINDA_SMALL_NUMBER) ? FMath::Clamp(CurHP / NewValue, 0.0f, 1.0f) : 0.0f;
		ApplyHealthState(CurHP, NewValue, Normalized, /*bSnapTrail*/ false);
		OnHealthChanged(CurHP, NewValue, Normalized);
	}
}

void UBwayHealthHUDWidget::ApplyHealthState(
	const float NewHealth,
	const float MaxHealth,
	const float HealthPercent,
	const bool bSnapTrail)
{
	const float ClampedPercent = FMath::Clamp(HealthPercent, 0.0f, 1.0f);
	const float PreviousPercent = CurrentHealthPercent;

	CurrentHealthValue = FMath::Max(0.0f, NewHealth);
	CurrentMaxHealthValue = FMath::Max(0.0f, MaxHealth);
	CurrentHealthPercent = ClampedPercent;

	if (bSnapTrail || !bHasInitialHealthSample || ClampedPercent >= DamageTrailPercent)
	{
		DamageTrailPercent = ClampedPercent;
		DamageTrailHoldRemaining = 0.0f;
	}
	else if (ClampedPercent < PreviousPercent - KINDA_SMALL_NUMBER)
	{
		DamageTrailHoldRemaining = DamageTrailHoldSeconds;
	}

	bHasInitialHealthSample = true;

	if (Progress_Health)
	{
		Progress_Health->SetPercent(CurrentHealthPercent);
	}
	if (Progress_DamageTrail)
	{
		Progress_DamageTrail->SetPercent(DamageTrailPercent);
	}

	if (Text_HealthValues)
	{
		const int32 RoundedCur = FMath::Max(0, FMath::RoundToInt(CurrentHealthValue));
		Text_HealthValues->SetText(FText::FromString(FString::Printf(TEXT("%d"), RoundedCur)));
	}

	InvalidateLayoutAndVolatility();
}

int32 UBwayHealthHUDWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	int32 CurrentLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 80.0f || Size.Y <= 8.0f)
	{
		return CurrentLayer;
	}

	double BarW = FMath::Clamp(Size.X - 78.0, 120.0, 344.0);
	double BarH = FMath::Min(26.0, Size.Y);
	double BarX = FMath::Max(0.0, Size.X - BarW);
	double BarY = (Size.Y - BarH) * 0.5;
	if (Progress_Health)
	{
		const FGeometry& BarGeo = Progress_Health->GetCachedGeometry();
		if (BarGeo.GetLocalSize().X > 16.0 && BarGeo.GetLocalSize().Y > 6.0)
		{
			const FVector2D LocalTopLeft = AllottedGeometry.AbsoluteToLocal(BarGeo.GetAbsolutePosition());
			const FVector2D LocalBottomRight = AllottedGeometry.AbsoluteToLocal(
				BarGeo.GetAbsolutePosition() + BarGeo.GetLocalSize() * BarGeo.Scale);
			BarX = LocalTopLeft.X;
			BarY = LocalTopLeft.Y;
			BarW = FMath::Max(16.0, LocalBottomRight.X - LocalTopLeft.X);
			BarH = FMath::Max(6.0, LocalBottomRight.Y - LocalTopLeft.Y);
		}
	}

	// Dark recessed bar track
	DrawSolidBox(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		FVector2D(BarX, BarY),
		FVector2D(BarW, BarH),
		FLinearColor(0.02f, 0.05f, 0.09f, 0.88f));

	const double InnerW = FMath::Max(0.0, BarW - 2.0);
	const double InnerH = FMath::Max(0.0, BarH - 2.0);
	const double InnerX = BarX + 1.0;
	const double InnerY = BarY + 1.0;

	// Delayed coral-orange damage trail
	const double ClampedTrail = FMath::Clamp(static_cast<double>(DamageTrailPercent), 0.0, 1.0);
	const double ClampedHealth = FMath::Clamp(static_cast<double>(CurrentHealthPercent), 0.0, 1.0);
	if (ClampedTrail > ClampedHealth + KINDA_SMALL_NUMBER && InnerW > 1.0)
	{
		DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			FVector2D(InnerX, InnerY),
			FVector2D(InnerW * ClampedTrail, InnerH),
			FLinearColor(0.96f, 0.36f, 0.18f, 0.92f));
	}

	// Smooth horizontal cyan-to-bright-aqua glowing health fill
	if (ClampedHealth > KINDA_SMALL_NUMBER && InnerW > 1.0)
	{
		const double FillW = InnerW * ClampedHealth;
		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
		if (WhiteBrush && FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			const FSlateResourceHandle ResourceHandle = WhiteBrush->GetRenderingResource();
			const FSlateRenderTransform& AccumRenderTransform = AllottedGeometry.GetAccumulatedRenderTransform();
			const FColor LeftCyan = FLinearColor(0.04f, 0.84f, 0.96f, 1.0f).ToFColor(true);
			const FColor RightAqua = FLinearColor(0.38f, 1.0f, 0.98f, 1.0f).ToFColor(true);

			TArray<FSlateVertex> Verts;
			Verts.Reserve(4);
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				AccumRenderTransform, FVector2f(static_cast<float>(InnerX), static_cast<float>(InnerY)), FVector2f(0.5f, 0.5f), LeftCyan));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				AccumRenderTransform, FVector2f(static_cast<float>(InnerX + FillW), static_cast<float>(InnerY)), FVector2f(0.5f, 0.5f), RightAqua));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				AccumRenderTransform, FVector2f(static_cast<float>(InnerX + FillW), static_cast<float>(InnerY + InnerH)), FVector2f(0.5f, 0.5f), RightAqua));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				AccumRenderTransform, FVector2f(static_cast<float>(InnerX), static_cast<float>(InnerY + InnerH)), FVector2f(0.5f, 0.5f), LeftCyan));

			const TArray<SlateIndex> Indices = { 0, 1, 2, 0, 2, 3 };
			FSlateDrawElement::MakeCustomVerts(
				OutDrawElements,
				static_cast<uint32>(++CurrentLayer),
				ResourceHandle,
				Verts,
				Indices,
				nullptr,
				0,
				0);
		}
		else
		{
			DrawSolidBox(
				AllottedGeometry,
				OutDrawElements,
				++CurrentLayer,
				FVector2D(InnerX, InnerY),
				FVector2D(FillW, InnerH),
				FLinearColor(0.10f, 0.92f, 0.96f, 1.0f));
		}

		// Crisp upper specular highlight along the top edge of the active health bar
		DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			FVector2D(InnerX, InnerY),
			FVector2D(FillW, FMath::Min(3.0, InnerH)),
			FLinearColor(0.76f, 1.0f, 1.0f, 0.28f));
	}

	// Crisp dark-cyan bevel frame
	const FVector2D BarOutline[4] = {
		FVector2D(BarX, BarY),
		FVector2D(BarX + BarW, BarY),
		FVector2D(BarX + BarW, BarY + BarH),
		FVector2D(BarX, BarY + BarH),
	};
	DrawPolygonOutline(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		BarOutline,
		FLinearColor(0.06f, 0.44f, 0.54f, 0.72f),
		1.2f,
		true);

	return CurrentLayer;
}

ULyraHealthComponent* UBwayHealthHUDWidget::ResolveCurrentPawnHealthComponent() const
{
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			return ULyraHealthComponent::FindHealthComponent(Pawn);
		}
	}

	return nullptr;
}

UAbilitySystemComponent* UBwayHealthHUDWidget::ResolveLocalAbilitySystemComponent() const
{
	if (APlayerState* PS = GetOwningPlayerState())
	{
		if (UAbilitySystemComponent* PSASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
		{
			return PSASC;
		}
	}

	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		}
	}

	return nullptr;
}

FText UBwayHealthHUDWidget::GetDisplayedHealthText() const
{
	return Text_HealthValues ? Text_HealthValues->GetText() : FText::GetEmpty();
}

FText UBwayHealthHUDWidget::GetDisplayedArmorText() const
{
	return Text_ArmorValues ? Text_ArmorValues->GetText() : FText::GetEmpty();
}
