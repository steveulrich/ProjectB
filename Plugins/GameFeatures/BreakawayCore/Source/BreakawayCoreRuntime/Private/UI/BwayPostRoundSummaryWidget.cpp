// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayPostRoundSummaryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BwayGameState.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayPostRoundSummaryWidget)

namespace BwayPostRoundStyle
{
	FORCEINLINE FLinearColor Srgb(const uint8 R, const uint8 G, const uint8 B, const uint8 A = 255)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
	}

	FORCEINLINE FColor LerpSrgb(const FColor& A, const FColor& B, const float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return FColor(
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A.R), static_cast<float>(B.R), T)), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A.G), static_cast<float>(B.G), T)), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A.B), static_cast<float>(B.B), T)), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A.A), static_cast<float>(B.A), T)), 0, 255)));
	}

	FSlateFontInfo BuildFont(const UFont* FontAsset, const int32 PointSize, const int32 LetterSpacing = 0)
	{
		FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(6, PointSize));
		if (IsValid(FontAsset))
		{
			FontInfo.FontObject = FontAsset;
			FontInfo.TypefaceFontName = NAME_None;
		}
		FontInfo.Size = FMath::Max(6, PointSize);
		FontInfo.LetterSpacing = LetterSpacing;
		return FontInfo;
	}

	struct FMeasuredTextMetrics
	{
		FVector2D Size = FVector2D::ZeroVector;
		float MaxHeight = 0.0f;
		float Descender = 0.0f;
		float Ascender = 0.0f;
	};

	FMeasuredTextMetrics MeasureTextMetrics(const FString& Text, const FSlateFontInfo& FontInfo)
	{
		FMeasuredTextMetrics Out;
		if (Text.IsEmpty())
		{
			return Out;
		}

		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			Out.Size = FVector2D(FontMeasure->Measure(Text, FontInfo));
			Out.MaxHeight = static_cast<float>(FontMeasure->GetMaxCharacterHeight(FontInfo));
			Out.Descender = FMath::Abs(static_cast<float>(FontMeasure->GetBaseline(FontInfo)));
			Out.Ascender = FMath::Max(1.0f, Out.MaxHeight - Out.Descender);
			return Out;
		}

		Out.Size = FVector2D(static_cast<double>(Text.Len()) * FontInfo.Size * 0.54, FontInfo.Size * 1.18);
		Out.MaxHeight = static_cast<float>(Out.Size.Y);
		Out.Descender = Out.MaxHeight * 0.18f;
		Out.Ascender = Out.MaxHeight - Out.Descender;
		return Out;
	}

	int32 CalibratePointSizeForTargetWidth(
		const UFont* FontAsset,
		const FString& ReferenceString,
		const float TargetWidth,
		const int32 BaseGuessPt,
		const int32 MinPt,
		const int32 MaxPt,
		const int32 LetterSpacing = 0)
	{
		const FSlateFontInfo ProbeFont = BuildFont(FontAsset, BaseGuessPt, LetterSpacing);
		const FMeasuredTextMetrics Probe = MeasureTextMetrics(ReferenceString, ProbeFont);
		if (Probe.Size.X <= 1.0)
		{
			return FMath::Clamp(BaseGuessPt, MinPt, MaxPt);
		}
		const float Ratio = TargetWidth / static_cast<float>(Probe.Size.X);
		return FMath::Clamp(FMath::RoundToInt(static_cast<float>(BaseGuessPt) * Ratio), MinPt, MaxPt);
	}
} // namespace BwayPostRoundStyle

class SBwayPostRoundSummaryCanvas final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBwayPostRoundSummaryCanvas) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		(void)InArgs;
		SetCanTick(false);
	}

	void SetStyleAssets(
		UFont* InDisplayFont,
		UFont* InLabelFont,
		UTexture2D* InIconScore,
		UTexture2D* InIconTeamWipe,
		UTexture2D* InIconTerritory)
	{
		DisplayFont = InDisplayFont;
		LabelFont = InLabelFont;
		ConfigureIconBrush(BrushIconScore, InIconScore);
		ConfigureIconBrush(BrushIconTeamWipe, InIconTeamWipe);
		ConfigureIconBrush(BrushIconTerritory, InIconTerritory);
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetSummaryData(
		const FBwayPostRoundSummaryData& InSummary,
		const FString& InHeaderText,
		const FString& InBlueKDA,
		const FString& InRedKDA)
	{
		Summary = InSummary;
		HeaderText = InHeaderText;
		BlueKDAText = InBlueKDA;
		RedKDAText = InRedKDA;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void ClearResourceBrushes()
	{
		DisplayFont.Reset();
		LabelFont.Reset();
		BrushIconScore = FSlateBrush();
		BrushIconTeamWipe = FSlateBrush();
		BrushIconTerritory = FSlateBrush();
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		(void)LayoutScaleMultiplier;
		return FVector2D(1920.0, 1080.0);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const bool bParentEnabled) const override
	{
		(void)Args;
		(void)MyCullingRect;
		(void)InWidgetStyle;
		(void)bParentEnabled;

		const FVector2D LocalSize2D = AllottedGeometry.GetLocalSize();
		const float W = static_cast<float>(LocalSize2D.X);
		const float H = static_cast<float>(LocalSize2D.Y);
		if (W < 64.0f || H < 64.0f)
		{
			return LayerId;
		}

		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
		if (!WhiteBrush || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			return LayerId;
		}

		const FSlateResourceHandle WhiteHandle = WhiteBrush->GetRenderingResource();
		const FSlateRenderTransform& RenderTransform = AllottedGeometry.GetAccumulatedRenderTransform();

		const float Scale = FMath::Min(W / 1920.0f, H / 1080.0f);
		const float CenterX = W * 0.5f;
		const float CenterY = H * 0.5f;

		int32 CurrentLayer = LayerId;

		// 1. Cinematic Top & Bottom Letterbox Black Bars (center 3D arena stays vivid and undimmed)
		const float TopBarH = H * 0.1255f;
		const float BotBarY = H * 0.8760f;
		const float BotBarH = FMath::Max(0.0f, H - BotBarY);
		PaintSolidBox(AllottedGeometry, OutDrawElements, CurrentLayer, WhiteBrush, 0.0f, 0.0f, W, TopBarH, FColor(0, 0, 0, 255));
		PaintSolidBox(AllottedGeometry, OutDrawElements, CurrentLayer, WhiteBrush, 0.0f, BotBarY, W, BotBarH, FColor(0, 0, 0, 255));
		++CurrentLayer;

		// Hexagon layout bounds in 1920x1080 design space
		const float HexTop = CenterY - 276.0f * Scale;
		const float HexBot = CenterY + 252.0f * Scale;
		const float HexMidY = (HexTop + HexBot) * 0.5f;
		const float FlatHalfW = 500.0f * Scale;
		const float TipHalfW = 602.0f * Scale;

		// 2. Top Frosted Header Banner ("BLUE ROUND" / "RED ROUND" / "ROUND DRAW")
		const float BannerW = 684.0f * Scale;
		const float BannerH = 88.0f * Scale;
		const float BannerX = CenterX - BannerW * 0.5f;
		const float BannerY = HexTop - 108.0f * Scale;
		const float BannerBorder = FMath::Max(1.0f, 1.25f * Scale);

		PaintSolidBox(AllottedGeometry, OutDrawElements, CurrentLayer, WhiteBrush, BannerX, BannerY, BannerW, BannerH, FColor(255, 255, 255, 44));
		PaintRectStroke(AllottedGeometry, OutDrawElements, CurrentLayer + 1, WhiteBrush, BannerX, BannerY, BannerW, BannerH, BannerBorder, FColor(255, 255, 255, 84));
		CurrentLayer += 2;

		const FString DisplayHeader = HeaderText.IsEmpty() ? TEXT("BLUE ROUND") : HeaderText;
		const int32 HeaderPt = BwayPostRoundStyle::CalibratePointSizeForTargetWidth(
			DisplayFont.Get(),
			TEXT("BLUE ROUND"),
			416.0f * Scale,
			FMath::Max(16, FMath::RoundToInt(60.0f * Scale)),
			FMath::Max(12, FMath::RoundToInt(28.0f * Scale)),
			FMath::Max(24, FMath::RoundToInt(96.0f * Scale)),
			14);
		const FSlateFontInfo HeaderFont = BwayPostRoundStyle::BuildFont(DisplayFont.Get(), HeaderPt, 14);
		CurrentLayer = PaintMetallicHeaderText(
			AllottedGeometry,
			OutDrawElements,
			CurrentLayer,
			DisplayHeader,
			HeaderFont,
			CenterX,
			BannerY + BannerH * 0.5f,
			Scale);

		// 3. Outer Misty Wing Aura (left blue mist outside '<', right red mist outside '>')
		PaintHexagonOuterAura(RenderTransform, OutDrawElements, CurrentLayer, WhiteHandle, CenterX, HexTop, HexBot, FlatHalfW, TipHalfW, Scale);
		++CurrentLayer;

		// 4. 6-Sided Hexagonal Main Panel Interior (Royal Blue -> Pitch Black -> Crimson Red)
		PaintHexagonInteriorMesh(RenderTransform, OutDrawElements, CurrentLayer, WhiteHandle, CenterX, HexTop, HexBot, FlatHalfW, TipHalfW);
		++CurrentLayer;

		// 5. Alternating Translucent Horizontal Stat Stripes (Rows 1 & 3: GOLD EARNED and HEALING DONE)
		const float StripeW = 1080.0f * Scale;
		const float StripeH = 64.0f * Scale;
		const float StripeX = CenterX - StripeW * 0.5f;
		for (const int32 StripeRowIdx : { 1, 3 })
		{
			const float RowCenterY = HexTop + (93.0f + static_cast<float>(StripeRowIdx) * 90.0f) * Scale;
			PaintSolidBox(
				AllottedGeometry,
				OutDrawElements,
				CurrentLayer,
				WhiteBrush,
				StripeX,
				RowCenterY - StripeH * 0.5f,
				StripeW,
				StripeH,
				FColor(255, 255, 255, 16));
		}
		++CurrentLayer;

		// 6. Hexagon Crisp Metallic Top/Bottom Edges & Glowing Blue/Red Chevron Borders
		PaintHexagonBorders(RenderTransform, OutDrawElements, CurrentLayer, WhiteHandle, CenterX, HexTop, HexMidY, HexBot, FlatHalfW, TipHalfW, Scale);
		CurrentLayer += 2;

		// 7. Symmetrical Slanted Round Pips (\ \ \   / / /) & Round Win-Type Icons
		CurrentLayer = PaintRoundPipsRow(AllottedGeometry, RenderTransform, OutDrawElements, CurrentLayer, WhiteHandle, WhiteBrush, CenterX, HexTop, Scale);

		// 8. Five 3-Column Team Stat Rows (Blue Value | Center Label | Red Value)
		const int32 ValuePt = FMath::Clamp(
			FMath::RoundToInt(static_cast<float>(HeaderPt) * 0.595f),
			FMath::Max(10, FMath::RoundToInt(18.0f * Scale)),
			FMath::Max(18, FMath::RoundToInt(64.0f * Scale)));
		const FSlateFontInfo ValueFont = BwayPostRoundStyle::BuildFont(DisplayFont.Get(), ValuePt, 8);

		const int32 LabelPt = BwayPostRoundStyle::CalibratePointSizeForTargetWidth(
			LabelFont.Get(),
			TEXT("BUILDABLES DESTROYED"),
			324.0f * Scale,
			FMath::Max(10, FMath::RoundToInt(17.0f * Scale)),
			FMath::Max(8, FMath::RoundToInt(11.0f * Scale)),
			FMath::Max(14, FMath::RoundToInt(30.0f * Scale)),
			20);
		const FSlateFontInfo StatLabelFont = BwayPostRoundStyle::BuildFont(LabelFont.Get(), LabelPt, 20);

		const FColor BlueTextColor(24, 120, 244, 255);
		const FColor RedTextColor(228, 48, 44, 255);
		const FColor LabelTextColor(246, 247, 250, 255);

		const float BlueColCenterX = CenterX - 371.0f * Scale;
		const float RedColCenterX = CenterX + 371.0f * Scale;

		struct FStatRowEntry
		{
			FString BlueValue;
			const TCHAR* Label;
			FString RedValue;
		};

		const FStatRowEntry Rows[5] = {
			{ BlueKDAText.IsEmpty() ? TEXT("0/0/0") : BlueKDAText, TEXT("K/D/A"), RedKDAText.IsEmpty() ? TEXT("0/0/0") : RedKDAText },
			{ FString::FromInt(Summary.Team0Stats.Stats.GoldEarned), TEXT("GOLD EARNED"), FString::FromInt(Summary.Team1Stats.Stats.GoldEarned) },
			{ FString::FromInt(Summary.Team0Stats.Stats.DamageDealt), TEXT("DAMAGE DEALT"), FString::FromInt(Summary.Team1Stats.Stats.DamageDealt) },
			{ FString::FromInt(Summary.Team0Stats.Stats.HealingDone), TEXT("HEALING DONE"), FString::FromInt(Summary.Team1Stats.Stats.HealingDone) },
			{ FString::FromInt(Summary.Team0Stats.Stats.BuildablesDestroyed), TEXT("BUILDABLES DESTROYED"), FString::FromInt(Summary.Team1Stats.Stats.BuildablesDestroyed) }
		};

		for (int32 RowIdx = 0; RowIdx < 5; ++RowIdx)
		{
			const float RowCenterY = HexTop + (93.0f + static_cast<float>(RowIdx) * 90.0f) * Scale;
			PaintCenteredText(AllottedGeometry, OutDrawElements, CurrentLayer, Rows[RowIdx].BlueValue, ValueFont, BlueColCenterX, RowCenterY, BlueTextColor);
			PaintCenteredText(AllottedGeometry, OutDrawElements, CurrentLayer, Rows[RowIdx].Label, StatLabelFont, CenterX, RowCenterY, LabelTextColor);
			PaintCenteredText(AllottedGeometry, OutDrawElements, CurrentLayer, Rows[RowIdx].RedValue, ValueFont, RedColCenterX, RowCenterY, RedTextColor);
		}
		++CurrentLayer;

		return CurrentLayer;
	}

private:
	static void ConfigureIconBrush(FSlateBrush& OutBrush, UTexture2D* Texture)
	{
		OutBrush = FSlateBrush();
		if (IsValid(Texture))
		{
			OutBrush.SetResourceObject(Texture);
			OutBrush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			OutBrush.DrawAs = ESlateBrushDrawType::Image;
		}
		else
		{
			OutBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		}
	}

	static void PaintSolidBox(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FSlateBrush* WhiteBrush,
		const float X,
		const float Y,
		const float W,
		const float H,
		const FColor& Color)
	{
		if (W <= 0.0f || H <= 0.0f || Color.A == 0)
		{
			return;
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			static_cast<uint32>(FMath::Max(0, LayerId)),
			AllottedGeometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))),
			WhiteBrush,
			ESlateDrawEffect::None,
			BwayPostRoundStyle::Srgb(Color.R, Color.G, Color.B, Color.A));
	}

	static void PaintRectStroke(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FSlateBrush* WhiteBrush,
		const float X,
		const float Y,
		const float W,
		const float H,
		const float Thickness,
		const FColor& Color)
	{
		PaintSolidBox(AllottedGeometry, OutDrawElements, LayerId, WhiteBrush, X, Y, W, Thickness, Color);
		PaintSolidBox(AllottedGeometry, OutDrawElements, LayerId, WhiteBrush, X, Y + H - Thickness, W, Thickness, Color);
		PaintSolidBox(AllottedGeometry, OutDrawElements, LayerId, WhiteBrush, X, Y + Thickness, Thickness, FMath::Max(0.0f, H - 2.0f * Thickness), Color);
		PaintSolidBox(AllottedGeometry, OutDrawElements, LayerId, WhiteBrush, X + W - Thickness, Y + Thickness, Thickness, FMath::Max(0.0f, H - 2.0f * Thickness), Color);
	}

	static void AddQuad(
		TArray<FSlateVertex>& Verts,
		TArray<SlateIndex>& Indices,
		const FSlateRenderTransform& Transform,
		const FVector2f& P0,
		const FColor& C0,
		const FVector2f& P1,
		const FColor& C1,
		const FVector2f& P2,
		const FColor& C2,
		const FVector2f& P3,
		const FColor& C3)
	{
		const SlateIndex Base = static_cast<SlateIndex>(Verts.Num());
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, P0, FVector2f(0.5f, 0.5f), C0));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, P1, FVector2f(0.5f, 0.5f), C1));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, P2, FVector2f(0.5f, 0.5f), C2));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, P3, FVector2f(0.5f, 0.5f), C3));

		Indices.Add(Base + 0);
		Indices.Add(Base + 1);
		Indices.Add(Base + 2);
		Indices.Add(Base + 0);
		Indices.Add(Base + 2);
		Indices.Add(Base + 3);
	}

	static void PaintCenteredText(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FString& Text,
		const FSlateFontInfo& FontInfo,
		const float CenterX,
		const float CenterY,
		const FColor& Color)
	{
		if (Text.IsEmpty() || Color.A == 0)
		{
			return;
		}

		const BwayPostRoundStyle::FMeasuredTextMetrics Metrics = BwayPostRoundStyle::MeasureTextMetrics(Text, FontInfo);
		const float PosX = CenterX - static_cast<float>(Metrics.Size.X) * 0.5f;
		// Optical vertical center for ALL-CAPS / numeral glyphs sitting on the baseline (PosY + Ascender)
		const float PosY = CenterY - Metrics.Ascender * 0.56f;

		FSlateDrawElement::MakeText(
			OutDrawElements,
			static_cast<uint32>(FMath::Max(0, LayerId)),
			AllottedGeometry.ToPaintGeometry(
				FVector2f(static_cast<float>(Metrics.Size.X), static_cast<float>(Metrics.Size.Y)),
				FSlateLayoutTransform(FVector2f(PosX, PosY))),
			Text,
			FontInfo,
			ESlateDrawEffect::None,
			BwayPostRoundStyle::Srgb(Color.R, Color.G, Color.B, Color.A));
	}

	static int32 PaintMetallicHeaderText(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FString& Text,
		const FSlateFontInfo& FontInfo,
		const float CenterX,
		const float CenterY,
		const float Scale)
	{
		const BwayPostRoundStyle::FMeasuredTextMetrics Metrics = BwayPostRoundStyle::MeasureTextMetrics(Text, FontInfo);
		const float TextW = static_cast<float>(Metrics.Size.X);
		const float TextH = FMath::Max(static_cast<float>(Metrics.Size.Y), Metrics.MaxHeight);
		const float PosX = CenterX - TextW * 0.5f;
		const float PosY = CenterY - Metrics.Ascender * 0.56f;

		// Subtle drop shadow behind metallic silver letters
		FSlateDrawElement::MakeText(
			OutDrawElements,
			static_cast<uint32>(FMath::Max(0, LayerId)),
			AllottedGeometry.ToPaintGeometry(
				FVector2f(TextW, TextH),
				FSlateLayoutTransform(FVector2f(PosX, PosY + 2.4f * Scale))),
			Text,
			FontInfo,
			ESlateDrawEffect::None,
			BwayPostRoundStyle::Srgb(0, 0, 0, 128));

		// Vertical brushed-silver to gunmetal-slate gradient across the capital glyph height
		const FColor TopSilver(228, 231, 236, 255);
		const FColor MidSilver(168, 172, 178, 255);
		const FColor BotSlate(106, 110, 116, 255);

		const float CapTop = PosY + Metrics.Ascender * 0.13f;
		const float CapBot = PosY + Metrics.Ascender * 1.00f;
		const float CapSpan = FMath::Max(8.0f, CapBot - CapTop);
		constexpr int32 NumBands = 20;

		const FPaintGeometry TextGeom = AllottedGeometry.ToPaintGeometry(
			FVector2f(TextW, TextH),
			FSlateLayoutTransform(FVector2f(PosX, PosY)));
		const uint32 TextLayer = static_cast<uint32>(FMath::Max(0, LayerId + 1));

		for (int32 BandIdx = 0; BandIdx < NumBands; ++BandIdx)
		{
			const float T0 = static_cast<float>(BandIdx) / static_cast<float>(NumBands);
			const float T1 = static_cast<float>(BandIdx + 1) / static_cast<float>(NumBands);
			const float TMid = (T0 + T1) * 0.5f;

			const float ClipTop = (BandIdx == 0) ? (PosY - 6.0f) : (CapTop + CapSpan * T0);
			const float ClipBot = (BandIdx == NumBands - 1) ? (PosY + TextH + 8.0f) : (CapTop + CapSpan * T1);
			const float ClipH = FMath::Max(0.5f, ClipBot - ClipTop);

			const FColor BandColor = (TMid < 0.5f)
				? BwayPostRoundStyle::LerpSrgb(TopSilver, MidSilver, TMid * 2.0f)
				: BwayPostRoundStyle::LerpSrgb(MidSilver, BotSlate, (TMid - 0.5f) * 2.0f);

			const FPaintGeometry ClipGeom = AllottedGeometry.ToPaintGeometry(
				FVector2f(TextW + 24.0f, ClipH),
				FSlateLayoutTransform(FVector2f(PosX - 12.0f, ClipTop)));
			OutDrawElements.PushClip(FSlateClippingZone(ClipGeom));

			FSlateDrawElement::MakeText(
				OutDrawElements,
				TextLayer,
				TextGeom,
				Text,
				FontInfo,
				ESlateDrawEffect::None,
				BwayPostRoundStyle::Srgb(BandColor.R, BandColor.G, BandColor.B, 255));

			OutDrawElements.PopClip();
		}

		return LayerId + 2;
	}

	static void PaintHexagonOuterAura(
		const FSlateRenderTransform& Transform,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FSlateResourceHandle& WhiteHandle,
		const float CenterX,
		const float HexTop,
		const float HexBot,
		const float FlatHalfW,
		const float TipHalfW,
		const float Scale)
	{
		constexpr int32 RowsY = 24; // Even count so i = 12 hits SignedV = 0.0 ('<' and '>' tips)
		constexpr int32 Rings = 5;
		const float OutwardOffsets[Rings] = {
			0.0f,
			5.0f * Scale,
			13.0f * Scale,
			24.0f * Scale,
			38.0f * Scale
		};
		const float RingAlpha[Rings] = { 0.36f, 0.25f, 0.13f, 0.045f, 0.0f };

		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		Verts.Reserve(2 * (RowsY + 1) * Rings);
		Indices.Reserve(2 * RowsY * (Rings - 1) * 6);

		for (int32 Side = 0; Side < 2; ++Side)
		{
			const bool bLeftBlue = (Side == 0);
			const float Dir = bLeftBlue ? -1.0f : 1.0f;
			const SlateIndex SideBase = static_cast<SlateIndex>(Verts.Num());

			for (int32 Row = 0; Row <= RowsY; ++Row)
			{
				const float Ty = static_cast<float>(Row) / static_cast<float>(RowsY);
				const float Y = FMath::Lerp(HexTop, HexBot, Ty);
				const float AbsV = FMath::Abs(Ty * 2.0f - 1.0f);
				const float EdgeX = bLeftBlue
					? FMath::Lerp(CenterX - TipHalfW, CenterX - FlatHalfW, AbsV)
					: FMath::Lerp(CenterX + TipHalfW, CenterX + FlatHalfW, AbsV);

				const float TipWeight = FMath::Pow(1.0f - FMath::Clamp(AbsV * 0.88f, 0.0f, 1.0f), 1.35f);

				for (int32 Ring = 0; Ring < Rings; ++Ring)
				{
					const float RingT = static_cast<float>(Ring) / static_cast<float>(Rings - 1);
					const FColor BaseMist = bLeftBlue
						? BwayPostRoundStyle::LerpSrgb(FColor(180, 208, 248, 255), FColor(224, 232, 246, 255), RingT)
						: BwayPostRoundStyle::LerpSrgb(FColor(246, 190, 186, 255), FColor(240, 224, 222, 255), RingT);
					const uint8 Alpha = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(255.0f * RingAlpha[Ring] * TipWeight), 0, 255));
					const FColor VertColor(BaseMist.R, BaseMist.G, BaseMist.B, Alpha);

					Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
						Transform,
						FVector2f(EdgeX + Dir * OutwardOffsets[Ring], Y),
						FVector2f(0.5f, 0.5f),
						VertColor));
				}
			}

			for (int32 Row = 0; Row < RowsY; ++Row)
			{
				for (int32 Ring = 0; Ring < Rings - 1; ++Ring)
				{
					const SlateIndex I00 = SideBase + static_cast<SlateIndex>(Row * Rings + Ring);
					const SlateIndex I01 = I00 + 1;
					const SlateIndex I10 = SideBase + static_cast<SlateIndex>((Row + 1) * Rings + Ring);
					const SlateIndex I11 = I10 + 1;

					Indices.Add(I00);
					Indices.Add(I01);
					Indices.Add(I11);
					Indices.Add(I00);
					Indices.Add(I11);
					Indices.Add(I10);
				}
			}
		}

		FSlateDrawElement::MakeCustomVerts(
			OutDrawElements,
			static_cast<uint32>(FMath::Max(0, LayerId)),
			WhiteHandle,
			Verts,
			Indices,
			nullptr,
			0,
			0);
	}

	static FColor EvaluateHexagonInteriorColor(const float U, const float AbsV)
	{
		const float TipBoost = 1.0f - 0.28f * AbsV;
		if (U < 0.37f)
		{
			const float W = 1.0f - (U / 0.37f);
			const float Rim = FMath::Pow(W, 3.4f) * TipBoost;
			const float Spread = FMath::Pow(W, 1.70f) * (0.85f + 0.15f * TipBoost);
			const uint8 R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(10.0f * Spread + 8.0f * Rim), 0, 255));
			const uint8 G = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(38.0f * Spread + 36.0f * Rim), 0, 255));
			const uint8 B = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(92.0f * Spread + 84.0f * Rim), 0, 255));
			const uint8 A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(247.0f, 236.0f, Rim)), 0, 255));
			return FColor(R, G, B, A);
		}
		if (U > 0.63f)
		{
			const float W = (U - 0.63f) / 0.37f;
			const float Rim = FMath::Pow(W, 3.4f) * TipBoost;
			const float Spread = FMath::Pow(W, 1.70f) * (0.85f + 0.15f * TipBoost);
			const uint8 R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(88.0f * Spread + 78.0f * Rim), 0, 255));
			const uint8 G = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(12.0f * Spread + 12.0f * Rim), 0, 255));
			const uint8 B = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(13.0f * Spread + 12.0f * Rim), 0, 255));
			const uint8 A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(247.0f, 236.0f, Rim)), 0, 255));
			return FColor(R, G, B, A);
		}
		return FColor(0, 0, 0, 247);
	}

	static void PaintHexagonInteriorMesh(
		const FSlateRenderTransform& Transform,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FSlateResourceHandle& WhiteHandle,
		const float CenterX,
		const float HexTop,
		const float HexBot,
		const float FlatHalfW,
		const float TipHalfW)
	{
		constexpr int32 RowsY = 16; // Even count so Row = 8 lands strictly on HexMidY
		constexpr int32 ColsX = 24;

		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		Verts.Reserve((RowsY + 1) * (ColsX + 1));
		Indices.Reserve(RowsY * ColsX * 6);

		for (int32 Row = 0; Row <= RowsY; ++Row)
		{
			const float Ty = static_cast<float>(Row) / static_cast<float>(RowsY);
			const float Y = FMath::Lerp(HexTop, HexBot, Ty);
			const float AbsV = FMath::Abs(Ty * 2.0f - 1.0f);
			const float LeftX = FMath::Lerp(CenterX - TipHalfW, CenterX - FlatHalfW, AbsV);
			const float RightX = FMath::Lerp(CenterX + TipHalfW, CenterX + FlatHalfW, AbsV);

			for (int32 Col = 0; Col <= ColsX; ++Col)
			{
				const float U = static_cast<float>(Col) / static_cast<float>(ColsX);
				const float X = FMath::Lerp(LeftX, RightX, U);
				const FColor Color = EvaluateHexagonInteriorColor(U, AbsV);
				Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
					Transform,
					FVector2f(X, Y),
					FVector2f(0.5f, 0.5f),
					Color));
			}
		}

		constexpr int32 Stride = ColsX + 1;
		for (int32 Row = 0; Row < RowsY; ++Row)
		{
			for (int32 Col = 0; Col < ColsX; ++Col)
			{
				const SlateIndex I00 = static_cast<SlateIndex>(Row * Stride + Col);
				const SlateIndex I01 = I00 + 1;
				const SlateIndex I10 = static_cast<SlateIndex>((Row + 1) * Stride + Col);
				const SlateIndex I11 = I10 + 1;

				Indices.Add(I00);
				Indices.Add(I01);
				Indices.Add(I11);
				Indices.Add(I00);
				Indices.Add(I11);
				Indices.Add(I10);
			}
		}

		FSlateDrawElement::MakeCustomVerts(
			OutDrawElements,
			static_cast<uint32>(FMath::Max(0, LayerId)),
			WhiteHandle,
			Verts,
			Indices,
			nullptr,
			0,
			0);
	}

	static void PaintHexagonBorders(
		const FSlateRenderTransform& Transform,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FSlateResourceHandle& WhiteHandle,
		const float CenterX,
		const float HexTop,
		const float HexMidY,
		const float HexBot,
		const float FlatHalfW,
		const float TipHalfW,
		const float Scale)
	{
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		Verts.Reserve(40);
		Indices.Reserve(60);

		const float HalfFlat = FMath::Max(1.0f, 1.3f * Scale);
		const FColor FlatMetalColor(126, 128, 134, 236);

		const float TopLeftX = CenterX - FlatHalfW;
		const float TopRightX = CenterX + FlatHalfW;
		const float MidLeftX = CenterX - TipHalfW;
		const float MidRightX = CenterX + TipHalfW;

		// Top & bottom flat metallic grey borders
		AddQuad(
			Verts, Indices, Transform,
			FVector2f(TopLeftX, HexTop - HalfFlat), FlatMetalColor,
			FVector2f(TopRightX, HexTop - HalfFlat), FlatMetalColor,
			FVector2f(TopRightX, HexTop + HalfFlat), FlatMetalColor,
			FVector2f(TopLeftX, HexTop + HalfFlat), FlatMetalColor);

		AddQuad(
			Verts, Indices, Transform,
			FVector2f(TopLeftX, HexBot - HalfFlat), FlatMetalColor,
			FVector2f(TopRightX, HexBot - HalfFlat), FlatMetalColor,
			FVector2f(TopRightX, HexBot + HalfFlat), FlatMetalColor,
			FVector2f(TopLeftX, HexBot + HalfFlat), FlatMetalColor);

		// Pale outer rim highlights along left '<' and right '>' wings
		const float HalfChevron = FMath::Max(1.6f, 2.35f * Scale);
		const float RimWidth = FMath::Max(1.0f, 1.5f * Scale);
		const FColor LeftRimColor(162, 196, 248, 168);
		const FColor RightRimColor(240, 164, 158, 168);

		AddQuad(
			Verts, Indices, Transform,
			FVector2f(TopLeftX - HalfChevron - RimWidth, HexTop - HalfFlat), LeftRimColor,
			FVector2f(TopLeftX - HalfChevron, HexTop - HalfFlat), LeftRimColor,
			FVector2f(MidLeftX - HalfChevron, HexMidY), LeftRimColor,
			FVector2f(MidLeftX - HalfChevron - RimWidth, HexMidY), LeftRimColor);
		AddQuad(
			Verts, Indices, Transform,
			FVector2f(MidLeftX - HalfChevron - RimWidth, HexMidY), LeftRimColor,
			FVector2f(MidLeftX - HalfChevron, HexMidY), LeftRimColor,
			FVector2f(TopLeftX - HalfChevron, HexBot + HalfFlat), LeftRimColor,
			FVector2f(TopLeftX - HalfChevron - RimWidth, HexBot + HalfFlat), LeftRimColor);

		AddQuad(
			Verts, Indices, Transform,
			FVector2f(TopRightX + HalfChevron, HexTop - HalfFlat), RightRimColor,
			FVector2f(TopRightX + HalfChevron + RimWidth, HexTop - HalfFlat), RightRimColor,
			FVector2f(MidRightX + HalfChevron + RimWidth, HexMidY), RightRimColor,
			FVector2f(MidRightX + HalfChevron, HexMidY), RightRimColor);
		AddQuad(
			Verts, Indices, Transform,
			FVector2f(MidRightX + HalfChevron, HexMidY), RightRimColor,
			FVector2f(MidRightX + HalfChevron + RimWidth, HexMidY), RightRimColor,
			FVector2f(TopRightX + HalfChevron + RimWidth, HexBot + HalfFlat), RightRimColor,
			FVector2f(TopRightX + HalfChevron, HexBot + HalfFlat), RightRimColor);

		// Primary glowing Royal Blue '<' and Crimson Red '>' mitered chevron strokes
		const FColor LeftBlueBorder(22, 96, 224, 255);
		const FColor RightRedBorder(208, 36, 36, 255);

		AddQuad(
			Verts, Indices, Transform,
			FVector2f(TopLeftX - HalfChevron, HexTop - HalfFlat), LeftBlueBorder,
			FVector2f(TopLeftX + HalfChevron, HexTop + HalfFlat), LeftBlueBorder,
			FVector2f(MidLeftX + HalfChevron, HexMidY), LeftBlueBorder,
			FVector2f(MidLeftX - HalfChevron, HexMidY), LeftBlueBorder);
		AddQuad(
			Verts, Indices, Transform,
			FVector2f(MidLeftX - HalfChevron, HexMidY), LeftBlueBorder,
			FVector2f(MidLeftX + HalfChevron, HexMidY), LeftBlueBorder,
			FVector2f(TopLeftX + HalfChevron, HexBot - HalfFlat), LeftBlueBorder,
			FVector2f(TopLeftX - HalfChevron, HexBot + HalfFlat), LeftBlueBorder);

		AddQuad(
			Verts, Indices, Transform,
			FVector2f(TopRightX - HalfChevron, HexTop + HalfFlat), RightRedBorder,
			FVector2f(TopRightX + HalfChevron, HexTop - HalfFlat), RightRedBorder,
			FVector2f(MidRightX + HalfChevron, HexMidY), RightRedBorder,
			FVector2f(MidRightX - HalfChevron, HexMidY), RightRedBorder);
		AddQuad(
			Verts, Indices, Transform,
			FVector2f(MidRightX - HalfChevron, HexMidY), RightRedBorder,
			FVector2f(MidRightX + HalfChevron, HexMidY), RightRedBorder,
			FVector2f(TopRightX + HalfChevron, HexBot + HalfFlat), RightRedBorder,
			FVector2f(TopRightX - HalfChevron, HexBot - HalfFlat), RightRedBorder);

		FSlateDrawElement::MakeCustomVerts(
			OutDrawElements,
			static_cast<uint32>(FMath::Max(0, LayerId)),
			WhiteHandle,
			Verts,
			Indices,
			nullptr,
			0,
			0);
	}

	EBwayWinCondition ResolvePipWinCondition(const bool bBlueColumn, const int32 PipIdx) const
	{
		const TArray<uint8>& History = bBlueColumn ? Summary.Team0RoundWinConditions : Summary.Team1RoundWinConditions;
		if (History.IsValidIndex(PipIdx))
		{
			return static_cast<EBwayWinCondition>(History[PipIdx]);
		}

		const int32 ColumnScore = bBlueColumn ? Summary.Team0Score : Summary.Team1Score;
		const bool bColumnWonLatestRound = (Summary.RoundWinningTeam >= 0)
			&& (bBlueColumn ? Summary.bLocalTeamWonRound : !Summary.bLocalTeamWonRound);
		if (bColumnWonLatestRound && PipIdx == ColumnScore - 1 && Summary.WinCondition != 0)
		{
			return static_cast<EBwayWinCondition>(Summary.WinCondition);
		}

		return EBwayWinCondition::GoalScored;
	}

	const FSlateBrush* GetBrushForWinCondition(const EBwayWinCondition Condition) const
	{
		switch (Condition)
		{
		case EBwayWinCondition::TeamEliminated:
			if (BrushIconTeamWipe.GetResourceObject() != nullptr)
			{
				return &BrushIconTeamWipe;
			}
			break;
		case EBwayWinCondition::TimeExpired:
		case EBwayWinCondition::SuddenDeath:
			if (BrushIconTerritory.GetResourceObject() != nullptr)
			{
				return &BrushIconTerritory;
			}
			break;
		case EBwayWinCondition::GoalScored:
		default:
			break;
		}

		return (BrushIconScore.GetResourceObject() != nullptr) ? &BrushIconScore : nullptr;
	}

	static void PaintFallbackPipEmblem(
		const FGeometry& AllottedGeometry,
		const FSlateRenderTransform& Transform,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FSlateResourceHandle& WhiteHandle,
		const FSlateBrush* WhiteBrush,
		const FVector2f& Center,
		const float Radius,
		const FColor& PipBgColor)
	{
		constexpr int32 Segments = 16;
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		Verts.Reserve(Segments + 1);
		Indices.Reserve(Segments * 3);

		const FColor White(248, 250, 252, 250);
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Center, FVector2f(0.5f, 0.5f), White));
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			const float Angle = (2.0f * PI * static_cast<float>(Seg)) / static_cast<float>(Segments);
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				Transform,
				FVector2f(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius),
				FVector2f(0.5f, 0.5f),
				White));
		}
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			Indices.Add(0);
			Indices.Add(static_cast<SlateIndex>(1 + Seg));
			Indices.Add(static_cast<SlateIndex>(1 + ((Seg + 1) % Segments)));
		}

		FSlateDrawElement::MakeCustomVerts(
			OutDrawElements,
			static_cast<uint32>(FMath::Max(0, LayerId)),
			WhiteHandle,
			Verts,
			Indices,
			nullptr,
			0,
			0);

		const float SlitW = FMath::Max(1.5f, Radius * 0.28f);
		const float SlitH = Radius * 1.18f;
		PaintSolidBox(
			AllottedGeometry,
			OutDrawElements,
			LayerId + 1,
			WhiteBrush,
			Center.X - SlitW * 0.5f,
			Center.Y - SlitH * 0.5f,
			SlitW,
			SlitH,
			PipBgColor);
	}

	int32 PaintRoundPipsRow(
		const FGeometry& AllottedGeometry,
		const FSlateRenderTransform& Transform,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FSlateResourceHandle& WhiteHandle,
		const FSlateBrush* WhiteBrush,
		const float CenterX,
		const float HexTop,
		const float Scale) const
	{
		const int32 NumPips = FMath::Clamp(Summary.PointsToWin > 0 ? Summary.PointsToWin : 3, 1, 7);
		const float PipTop = HexTop + 12.5f * Scale;
		const float PipH = 23.0f * Scale;
		const float PipBot = PipTop + PipH;
		const float PipW = 56.0f * Scale;
		const float PipGap = 8.5f * Scale;
		const float CenterGroupGap = 48.0f * Scale;
		const float HalfSlant = 2.3f * Scale; // Total top-to-bottom horizontal slant = 4.6 px at 1080p
		const float IconSize = 19.5f * Scale;

		const FColor BlueLitColor(22, 92, 214, 255);
		const FColor RedLitColor(210, 42, 42, 255);
		const FColor UnlitPipColor(70, 71, 73, 242);

		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		Verts.Reserve(NumPips * 2 * 4);
		Indices.Reserve(NumPips * 2 * 6);

		struct FFilledPipIconDraw
		{
			FVector2f Center;
			EBwayWinCondition Condition;
			FColor PipBgColor;
		};
		TArray<FFilledPipIconDraw> IconsToDraw;

		const float GroupSpan = static_cast<float>(NumPips) * PipW + static_cast<float>(NumPips - 1) * PipGap;
		const float BlueStartLeft = CenterX - CenterGroupGap * 0.5f - GroupSpan;
		const float RedEndRight = CenterX + CenterGroupGap * 0.5f + GroupSpan;

		// Left (Blue) pips: slant '\', filled outside-inward (0 = outermost left, matching Breakaway reference screen)
		for (int32 Idx = 0; Idx < NumPips; ++Idx)
		{
			const float MidX0 = BlueStartLeft + static_cast<float>(Idx) * (PipW + PipGap);
			const float MidX1 = MidX0 + PipW;
			const bool bLit = (Idx < Summary.Team0Score);
			const FColor FillColor = bLit ? BlueLitColor : UnlitPipColor;

			AddQuad(
				Verts, Indices, Transform,
				FVector2f(MidX0 - HalfSlant, PipTop), FillColor,
				FVector2f(MidX1 - HalfSlant, PipTop), FillColor,
				FVector2f(MidX1 + HalfSlant, PipBot), FillColor,
				FVector2f(MidX0 + HalfSlant, PipBot), FillColor);

			if (bLit)
			{
				IconsToDraw.Add({
					FVector2f((MidX0 + MidX1) * 0.5f, (PipTop + PipBot) * 0.5f),
					ResolvePipWinCondition(true, Idx),
					BlueLitColor
				});
			}
		}

		// Right (Red) pips: slant '/', mirror-symmetrical outside-inward (0 = outermost right)
		for (int32 Idx = 0; Idx < NumPips; ++Idx)
		{
			const float MidX1 = RedEndRight - static_cast<float>(Idx) * (PipW + PipGap);
			const float MidX0 = MidX1 - PipW;
			const bool bLit = (Idx < Summary.Team1Score);
			const FColor FillColor = bLit ? RedLitColor : UnlitPipColor;

			AddQuad(
				Verts, Indices, Transform,
				FVector2f(MidX0 + HalfSlant, PipTop), FillColor,
				FVector2f(MidX1 + HalfSlant, PipTop), FillColor,
				FVector2f(MidX1 - HalfSlant, PipBot), FillColor,
				FVector2f(MidX0 - HalfSlant, PipBot), FillColor);

			if (bLit)
			{
				IconsToDraw.Add({
					FVector2f((MidX0 + MidX1) * 0.5f, (PipTop + PipBot) * 0.5f),
					ResolvePipWinCondition(false, Idx),
					RedLitColor
				});
			}
		}

		FSlateDrawElement::MakeCustomVerts(
			OutDrawElements,
			static_cast<uint32>(FMath::Max(0, LayerId)),
			WhiteHandle,
			Verts,
			Indices,
			nullptr,
			0,
			0);

		for (const FFilledPipIconDraw& IconEntry : IconsToDraw)
		{
			if (const FSlateBrush* IconBrush = GetBrushForWinCondition(IconEntry.Condition))
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					static_cast<uint32>(FMath::Max(0, LayerId + 1)),
					AllottedGeometry.ToPaintGeometry(
						FVector2f(IconSize, IconSize),
						FSlateLayoutTransform(FVector2f(
							IconEntry.Center.X - IconSize * 0.5f,
							IconEntry.Center.Y - IconSize * 0.5f))),
					IconBrush,
					ESlateDrawEffect::None,
					FLinearColor(1.0f, 1.0f, 1.0f, 0.98f));
			}
			else
			{
				PaintFallbackPipEmblem(
					AllottedGeometry,
					Transform,
					OutDrawElements,
					LayerId + 1,
					WhiteHandle,
					WhiteBrush,
					IconEntry.Center,
					IconSize * 0.44f,
					IconEntry.PipBgColor);
			}
		}

		return LayerId + 3;
	}

	FBwayPostRoundSummaryData Summary;
	FString HeaderText;
	FString BlueKDAText;
	FString RedKDAText;

	TWeakObjectPtr<UFont> DisplayFont;
	TWeakObjectPtr<UFont> LabelFont;
	FSlateBrush BrushIconScore;
	FSlateBrush BrushIconTeamWipe;
	FSlateBrush BrushIconTerritory;
};

UBwayPostRoundSummaryWidget::UBwayPostRoundSummaryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayNumeralFontAsset = TSoftObjectPtr<UFont>(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Fonts/refrigeratordeluxe-heavy_Font.refrigeratordeluxe-heavy_Font")));
	StatLabelFontAsset = TSoftObjectPtr<UFont>(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Fonts/futura-heavy_Font.futura-heavy_Font")));
	RoundIconScoreAsset = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Art/Match_HUD/icon_round_type_score.icon_round_type_score")));
	RoundIconTeamWipeAsset = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Art/Match_HUD/icon_round_type_team_wipe.icon_round_type_team_wipe")));
	RoundIconTerritoryAsset = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Art/Match_HUD/icon_round_type_territory.icon_round_type_territory")));
}

TSharedRef<SWidget> UBwayPostRoundSummaryWidget::RebuildWidget()
{
	ResolveStyleAssets();
	SuppressLegacyTreeVisuals();

	TSharedRef<SWidget> BaseWidget = Super::RebuildWidget();
	SuppressLegacyTreeVisuals();

	TSharedRef<SOverlay> RootOverlay = SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			BaseWidget
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(PostRoundSlateCanvas, SBwayPostRoundSummaryCanvas)
			.Visibility(EVisibility::HitTestInvisible)
		];

	PushStateToSlateCanvas();
	return RootOverlay;
}

void UBwayPostRoundSummaryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ResolveStyleAssets();
	SuppressLegacyTreeVisuals();
	PushStateToSlateCanvas();
}

void UBwayPostRoundSummaryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveStyleAssets();
	SuppressLegacyTreeVisuals();
	RefreshBoundWidgets();
	PushStateToSlateCanvas();
}

void UBwayPostRoundSummaryWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	if (PostRoundSlateCanvas.IsValid())
	{
		PostRoundSlateCanvas->ClearResourceBrushes();
		PostRoundSlateCanvas.Reset();
	}

	Super::ReleaseSlateResources(bReleaseChildren);
}

void UBwayPostRoundSummaryWidget::ResolveStyleAssets()
{
	if (!ResolvedDisplayFont && !DisplayNumeralFontAsset.IsNull())
	{
		ResolvedDisplayFont = DisplayNumeralFontAsset.LoadSynchronous();
	}
	if (!ResolvedLabelFont && !StatLabelFontAsset.IsNull())
	{
		ResolvedLabelFont = StatLabelFontAsset.LoadSynchronous();
	}
	if (!ResolvedIconScore && !RoundIconScoreAsset.IsNull())
	{
		ResolvedIconScore = RoundIconScoreAsset.LoadSynchronous();
	}
	if (!ResolvedIconTeamWipe && !RoundIconTeamWipeAsset.IsNull())
	{
		ResolvedIconTeamWipe = RoundIconTeamWipeAsset.LoadSynchronous();
	}
	if (!ResolvedIconTerritory && !RoundIconTerritoryAsset.IsNull())
	{
		ResolvedIconTerritory = RoundIconTerritoryAsset.LoadSynchronous();
	}
}

void UBwayPostRoundSummaryWidget::SuppressLegacyTreeVisuals()
{
	if (!WidgetTree)
	{
		return;
	}

	static const FName LegacyWidgetNames[] = {
		TEXT("DimOverlay"),
		TEXT("CenterPanelSize"),
		TEXT("PanelBorder"),
		TEXT("MainVBox")
	};

	for (const FName WidgetName : LegacyWidgetNames)
	{
		if (UWidget* LegacyWidget = WidgetTree->FindWidget(WidgetName))
		{
			LegacyWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UBwayPostRoundSummaryWidget::ApplySummaryData(FBwayPostRoundSummaryData SummaryData)
{
	ResolveStyleAssets();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABwayGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
		{
			if (APlayerState* LocalPS = PC->PlayerState)
			{
				const int32 LocalTeam = GameState->GetPlayerTeam(LocalPS);
				if (LocalTeam == 0 || LocalTeam == 1)
				{
					SummaryData.bLocalTeamWonRound =
						(SummaryData.RoundWinningTeam >= 0 && LocalTeam == SummaryData.RoundWinningTeam);
				}
			}
		}
	}

	const int32 LocalTeam = UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(this);
	CachedSummary = UBwayMatchHUDWidgetBase::RemapPostRoundSummaryForDisplay(SummaryData, LocalTeam);

	RefreshBoundWidgets();
	OnSummaryReady(CachedSummary);
	SuppressLegacyTreeVisuals();
	PushStateToSlateCanvas();
}

FText UBwayPostRoundSummaryWidget::GetRoundOutcomeHeaderText() const
{
	if (CachedSummary.RoundWinningTeam < 0)
	{
		return NSLOCTEXT("PostRound", "RoundDraw", "ROUND DRAW");
	}

	return CachedSummary.bLocalTeamWonRound
		? NSLOCTEXT("PostRound", "BlueRound", "BLUE ROUND")
		: NSLOCTEXT("PostRound", "RedRound", "RED ROUND");
}

FText UBwayPostRoundSummaryWidget::GetWinReasonText() const
{
	const EBwayWinCondition Condition = static_cast<EBwayWinCondition>(CachedSummary.WinCondition);
	switch (Condition)
	{
	case EBwayWinCondition::GoalScored:
	{
		const FString TrimmedScorer = CachedSummary.ScoringPlayerName.TrimStartAndEnd().ToUpper();
		return TrimmedScorer.IsEmpty()
			? NSLOCTEXT("PostRound", "GoalScoredGeneric", "GOAL SCORED")
			: FText::FromString(FString::Printf(TEXT("GOAL SCORED BY %s"), *TrimmedScorer));
	}
	case EBwayWinCondition::TeamEliminated:
		return NSLOCTEXT("PostRound", "TeamEliminated", "ENEMY TEAM ELIMINATED");
	case EBwayWinCondition::TimeExpired:
	case EBwayWinCondition::SuddenDeath:
		return NSLOCTEXT("PostRound", "TerritoryTiebreaker", "TIME EXPIRED — TERRITORY TIEBREAKER");
	default:
		return FText::GetEmpty();
	}
}

FText UBwayPostRoundSummaryWidget::GetSeriesScoreText() const
{
	const int32 TargetWins = FMath::Max(1, CachedSummary.PointsToWin);
	return FText::FromString(FString::Printf(
		TEXT("SERIES SCORE: %d - %d (FIRST TO %d)"),
		CachedSummary.Team0Score,
		CachedSummary.Team1Score,
		TargetWins));
}

FText UBwayPostRoundSummaryWidget::GetTeamKDAFormattedText(const int32 DisplayColumnIndex) const
{
	const FBwayTeamStatAggregate& TeamStats = (DisplayColumnIndex == 0) ? CachedSummary.Team0Stats : CachedSummary.Team1Stats;
	return TeamStats.GetKDAFormattedText();
}

int32 UBwayPostRoundSummaryWidget::GetTeamStatValue(const int32 DisplayColumnIndex, const FName StatName) const
{
	const FBwayPlayerMatchStats& Stats = (DisplayColumnIndex == 0)
		? CachedSummary.Team0Stats.Stats
		: CachedSummary.Team1Stats.Stats;
	return GetStatFieldValue(Stats, StatName);
}

int32 UBwayPostRoundSummaryWidget::GetStatFieldValue(const FBwayPlayerMatchStats& Stats, const FName StatName)
{
	if (StatName == TEXT("GoldEarned"))
	{
		return Stats.GoldEarned;
	}
	if (StatName == TEXT("DamageDealt"))
	{
		return Stats.DamageDealt;
	}
	if (StatName == TEXT("HealingDone"))
	{
		return Stats.HealingDone;
	}
	if (StatName == TEXT("BuildablesDestroyed"))
	{
		return Stats.BuildablesDestroyed;
	}

	return 0;
}

void UBwayPostRoundSummaryWidget::RefreshBoundWidgets()
{
	if (Text_Header)
	{
		Text_Header->SetText(GetRoundOutcomeHeaderText());
	}

	if (Text_WinReason)
	{
		Text_WinReason->SetText(GetWinReasonText());
	}

	if (Text_SeriesScore)
	{
		Text_SeriesScore->SetText(GetSeriesScoreText());
	}

	if (Text_Team0_KDA)
	{
		Text_Team0_KDA->SetText(GetTeamKDAFormattedText(0));
	}
	if (Text_Team1_KDA)
	{
		Text_Team1_KDA->SetText(GetTeamKDAFormattedText(1));
	}

	if (Text_Team0_Gold)
	{
		Text_Team0_Gold->SetText(FText::AsNumber(GetTeamStatValue(0, TEXT("GoldEarned"))));
	}
	if (Text_Team1_Gold)
	{
		Text_Team1_Gold->SetText(FText::AsNumber(GetTeamStatValue(1, TEXT("GoldEarned"))));
	}

	if (Text_Team0_Damage)
	{
		Text_Team0_Damage->SetText(FText::AsNumber(GetTeamStatValue(0, TEXT("DamageDealt"))));
	}
	if (Text_Team1_Damage)
	{
		Text_Team1_Damage->SetText(FText::AsNumber(GetTeamStatValue(1, TEXT("DamageDealt"))));
	}

	if (Text_Team0_Healing)
	{
		Text_Team0_Healing->SetText(FText::AsNumber(GetTeamStatValue(0, TEXT("HealingDone"))));
	}
	if (Text_Team1_Healing)
	{
		Text_Team1_Healing->SetText(FText::AsNumber(GetTeamStatValue(1, TEXT("HealingDone"))));
	}

	if (Text_Team0_Buildables)
	{
		Text_Team0_Buildables->SetText(FText::AsNumber(GetTeamStatValue(0, TEXT("BuildablesDestroyed"))));
	}
	if (Text_Team1_Buildables)
	{
		Text_Team1_Buildables->SetText(FText::AsNumber(GetTeamStatValue(1, TEXT("BuildablesDestroyed"))));
	}
}

void UBwayPostRoundSummaryWidget::PushStateToSlateCanvas()
{
	if (!PostRoundSlateCanvas.IsValid())
	{
		return;
	}

	PostRoundSlateCanvas->SetStyleAssets(
		ResolvedDisplayFont,
		ResolvedLabelFont,
		ResolvedIconScore,
		ResolvedIconTeamWipe,
		ResolvedIconTerritory);

	if (IsDesignTime() && CachedSummary.CompletedRoundNumber <= 0 && CachedSummary.PointsToWin <= 0)
	{
		FBwayPostRoundSummaryData PreviewSummary;
		PreviewSummary.CompletedRoundNumber = 1;
		PreviewSummary.RoundWinningTeam = 0;
		PreviewSummary.bLocalTeamWonRound = true;
		PreviewSummary.Team0Score = 1;
		PreviewSummary.Team1Score = 0;
		PreviewSummary.PointsToWin = 3;
		PreviewSummary.WinCondition = static_cast<uint8>(EBwayWinCondition::GoalScored);
		PreviewSummary.Team0RoundWinConditions.Add(PreviewSummary.WinCondition);
		PreviewSummary.Team1Stats.Stats.Deaths = 1;
		PreviewSummary.Team1Stats.Stats.DamageDealt = 117;
		PostRoundSlateCanvas->SetSummaryData(
			PreviewSummary,
			TEXT("BLUE ROUND"),
			PreviewSummary.Team0Stats.GetKDAFormattedText().ToString(),
			PreviewSummary.Team1Stats.GetKDAFormattedText().ToString());
		return;
	}

	PostRoundSlateCanvas->SetSummaryData(
		CachedSummary,
		GetRoundOutcomeHeaderText().ToString(),
		GetTeamKDAFormattedText(0).ToString(),
		GetTeamKDAFormattedText(1).ToString());
}
