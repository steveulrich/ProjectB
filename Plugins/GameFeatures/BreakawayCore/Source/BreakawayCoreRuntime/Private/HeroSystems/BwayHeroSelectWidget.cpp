// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroSelectWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonInputModeTypes.h"
#include "CommonInputSubsystem.h"
#include "CommonTextBlock.h"
#include "CommonUIExtensions.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroAbilityUILibrary.h"
#include "HeroSystems/BwayHeroPreviewActor.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PrimaryGameLayout.h"
#include "RenderingThread.h"
#include "Slate/SceneViewport.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "UI/BwayAbilityPreviewWidget.h"
#include "UI/BwayCommonButton.h"
#include "UI/BwayHeroSlotWidget.h"
#include "UI/Foundation/LyraButtonBase.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroSelectWidget)

namespace
{
	static constexpr const TCHAR* FontHouseOfCardsBoldItalic = TEXT("/BreakawayCore/UI/Fonts/houseofcards_bold_italic_Font.houseofcards_bold_italic_Font");
	static constexpr const TCHAR* FontNeuzeitBoldItalic = TEXT("/BreakawayCore/UI/Fonts/neuzeitgroteskbolditalic_Font.neuzeitgroteskbolditalic_Font");
	static constexpr const TCHAR* FontNeuzeitBold = TEXT("/BreakawayCore/UI/Fonts/neuzeitgro-bol_Font.neuzeitgro-bol_Font");
	static constexpr const TCHAR* FontRefrigeratorHeavy = TEXT("/BreakawayCore/UI/Fonts/refrigeratordeluxe-heavy_Font.refrigeratordeluxe-heavy_Font");
	static constexpr const TCHAR* FontNimbusReg = TEXT("/BreakawayCore/UI/Fonts/nimbussan-reg_Font.nimbussan-reg_Font");

	static constexpr const TCHAR* TexBackdropArgus = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselect_backdrop_argus.charselect_backdrop_argus");
	static constexpr const TCHAR* TexBackdropRawlins = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselect_backdrop_rawlins.charselect_backdrop_rawlins");
	static constexpr const TCHAR* TexBackdropAlona = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselect_backdrop_alona.charselect_backdrop_alona");
	static constexpr const TCHAR* TexBackdropKorryn = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselect_backdrop_korryn.charselect_backdrop_korryn");

	static constexpr const TCHAR* TexLeftSideGradient = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselectleftsidegradient.charselectleftsidegradient");
	static constexpr const TCHAR* TexRightSideGradient = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselectrightsidegradient.charselectrightsidegradient");
	static constexpr const TCHAR* TexAbilityGradient = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselectabilitygradient.charselectabilitygradient");
	static constexpr const TCHAR* TexXPBarFill = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselect_xpbar_fill.charselect_xpbar_fill");
	static constexpr const TCHAR* TexInfoCircle = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselectinfo.charselectinfo");
	static constexpr const TCHAR* TexInfoSymbol = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselect_info_symbol.charselect_info_symbol");
	static constexpr const TCHAR* TexCloseSymbol = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselect_close_symbol.charselect_close_symbol");
	static constexpr const TCHAR* TexLeaveIcon = TEXT("/BreakawayCore/UI/Art/HeroSelect/characterselect_leavematchicon.characterselect_leavematchicon");
	static constexpr const TCHAR* TexSingleCoin = TEXT("/BreakawayCore/UI/Art/HeroSelect/heroselect_hintbar_coin.heroselect_hintbar_coin");
	static constexpr const TCHAR* TexDefaultTaunt = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselect_default_taunt.charselect_default_taunt");
	static constexpr const TCHAR* TexPartyAvatarSlot = TEXT("/BreakawayCore/UI/Art/HeroSelect/heroselect_party_avatar_slot.heroselect_party_avatar_slot");
	static constexpr const TCHAR* TexPartyLeaderDot = TEXT("/BreakawayCore/UI/Art/HeroSelect/heroselect_party_leader_dot.heroselect_party_leader_dot");
	static constexpr const TCHAR* TexPartyInviteSlot = TEXT("/BreakawayCore/UI/Art/HeroSelect/heroselect_party_invite_slot.heroselect_party_invite_slot");
	static constexpr const TCHAR* TexQuickChatBalloon = TEXT("/BreakawayCore/UI/Art/HeroSelect/heroselect_quick_chat_balloon.heroselect_quick_chat_balloon");
	static constexpr const TCHAR* TexHintbarCoin = TEXT("/BreakawayCore/UI/Art/HeroSelect/heroselect_hintbar_coin.heroselect_hintbar_coin");
	static constexpr const TCHAR* TexHintbarSettings = TEXT("/BreakawayCore/UI/Art/HeroSelect/heroselect_hintbar_settings.heroselect_hintbar_settings");

	FSlateFontInfo MakeHeroSelectFont(const UObject* FontObj, int32 PointSize)
	{
		if (FontObj)
		{
			FSlateFontInfo Info(FontObj, PointSize);
			Info.TypefaceFontName = FName(TEXT("Default"));
			return Info;
		}
		return FCoreStyle::GetDefaultFontStyle("Bold", PointSize);
	}

	void ApplyHeroSelectBrush(UImage* Img, const TCHAR* TexPath, const FVector2D& Size, const FLinearColor& Tint = FLinearColor::White)
	{
		if (!Img)
		{
			return;
		}
		if (UTexture2D* Tex = (TexPath && *TexPath) ? LoadObject<UTexture2D>(nullptr, TexPath) : nullptr)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Tex);
			Brush.ImageSize = Size;
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.TintColor = FSlateColor(Tint);
			Img->SetBrush(Brush);
		}
		else
		{
			FSlateBrush Fallback;
			Fallback.DrawAs = ESlateBrushDrawType::NoDrawType;
			Fallback.ImageSize = Size;
			Fallback.TintColor = FSlateColor(FLinearColor::Transparent);
			Img->SetBrush(Fallback);
		}
	}

	void SetTransparentHitButton(UButton* Button)
	{
		if (!Button)
		{
			return;
		}
		FSlateBrush EmptyBrush;
		EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle Style = Button->GetStyle();
		Style.SetNormal(EmptyBrush);
		Style.SetHovered(EmptyBrush);
		Style.SetPressed(EmptyBrush);
		Style.SetDisabled(EmptyBrush);
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::Transparent);
		Button->SetCursor(EMouseCursor::Hand);
	}

	int32 GetCanonicalGoal5HeroSortOrder(const FPrimaryAssetId& HeroId, EHeroClass HeroClass)
	{
		const FString AssetName = HeroId.PrimaryAssetName.ToString();
		if (AssetName.Contains(TEXT("Argus"), ESearchCase::IgnoreCase) || AssetName.Contains(TEXT("Spartacus"), ESearchCase::IgnoreCase))
		{
			return 0;
		}
		if (AssetName.Contains(TEXT("Alona"), ESearchCase::IgnoreCase))
		{
			return 1;
		}
		if (AssetName.Contains(TEXT("Hexweaver"), ESearchCase::IgnoreCase) || AssetName.Contains(TEXT("Korryn"), ESearchCase::IgnoreCase))
		{
			return 2;
		}
		if (AssetName.Contains(TEXT("Gunslinger"), ESearchCase::IgnoreCase) || AssetName.Contains(TEXT("Rawlins"), ESearchCase::IgnoreCase))
		{
			return 3;
		}
		return 10 + static_cast<int32>(HeroClass);
	}

	FLinearColor GetClassBadgeColor(const FText& ClassNameText, EHeroClass HeroClass)
	{
		const FString Upper = ClassNameText.ToString().ToUpper();
		if (Upper.Contains(TEXT("DEBUFF")))
		{
			return FLinearColor(0.68f, 0.36f, 0.96f, 1.0f);
		}
		if (Upper.Contains(TEXT("SUPPORT")) || HeroClass == EHeroClass::Support)
		{
			return FLinearColor(0.18f, 0.84f, 0.68f, 1.0f);
		}
		if (Upper.Contains(TEXT("SHOOTER")) || HeroClass == EHeroClass::Shooter)
		{
			return FLinearColor(0.96f, 0.64f, 0.22f, 1.0f);
		}
		return FLinearColor(0.92f, 0.34f, 0.28f, 1.0f);
	}

	void SafelyDisplaceExistingNamedWidget(UWidgetTree* InWidgetTree, FName ReservedName)
	{
		if (!InWidgetTree)
		{
			return;
		}
		if (UWidget* Existing = InWidgetTree->FindWidget(ReservedName))
		{
			Existing->SetVisibility(ESlateVisibility::Collapsed);
			Existing->Rename(nullptr, InWidgetTree, REN_DontCreateRedirectors | REN_NonTransactional);
		}
	}

	struct FBreakawayHeroSelectArtMapping
	{
		const TCHAR* BackdropTex;
		const TCHAR* SkinTex;
		const TCHAR* WeaponTex;
		const TCHAR* MediaPrefix;
		const TCHAR* AbilityIcons[6];
		const TCHAR* AbilityTitles[6];
		const TCHAR* AbilityDescriptions[6];
	};

	FBreakawayHeroSelectArtMapping ResolveHeroArtMapping(const FPrimaryAssetId& HeroId, const FText& DisplayName)
	{
		const FString Key = (HeroId.PrimaryAssetName.ToString() + TEXT(" ") + DisplayName.ToString()).ToLower();
		if (Key.Contains(TEXT("rawlins")) || Key.Contains(TEXT("gunslinger")))
		{
			return {
				TexBackdropRawlins,
				TEXT("/BreakawayCore/UI/Art/HeroSelect/small_skin_rawlins_default.small_skin_rawlins_default"),
				TEXT("/BreakawayCore/UI/Art/HeroSelect/small_weapon_rwlraretombbreaker_heavenly.small_weapon_rwlraretombbreaker_heavenly"),
				TEXT("rawlins"),
				{
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_rawlins_basicattack.icon_rawlins_basicattack"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_rawlins_defense.icon_rawlins_defense"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_rawlins_ability1.icon_rawlins_ability1"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_rawlins_ability2.icon_rawlins_ability2"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_rawlins_ability3.icon_rawlins_ability3"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_rawlins_buildable1.icon_rawlins_buildable1")
				},
				{
					TEXT("PEACEMAKER SHOT"),
					TEXT("OUTLAW ROLL"),
					TEXT("BUCKSHOT BLAST"),
					TEXT("BLACK POWDER BOMB"),
					TEXT("HIGH NOON BARRAGE"),
					TEXT("JAILHOUSE")
				},
				{
					TEXT("Fires high-caliber rounds from his twin hex-forged pistols."),
					TEXT("Tumbles quickly to evade enemy attacks and reload."),
					TEXT("Fires a close-range scatter blast that knocks back enemies."),
					TEXT("Tosses an explosive charge that detonates on impact, damaging enemies in a radius."),
					TEXT("Unleashes a rapid hail of gunfire across a wide cone in front of Rawlins."),
					TEXT("A frontier cage trap that imprisons and slows enemies caught within its perimeter.")
				}
			};
		}
		if (Key.Contains(TEXT("alona")) || Key.Contains(TEXT("sunpriestess")))
		{
			return {
				TexBackdropAlona,
				TEXT("/BreakawayCore/UI/Art/HeroSelect/small_skin_sunpriestess_default.small_skin_sunpriestess_default"),
				TEXT("/BreakawayCore/UI/Art/HeroSelect/small_weapon_sunpraresunstaff_heavenly.small_weapon_sunpraresunstaff_heavenly"),
				TEXT("sunpriestess"),
				{
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_sunpriestess_basicattack.icon_sunpriestess_basicattack"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_sunpriestess_defense.icon_sunpriestess_defense"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_sunpriestess_ability1.icon_sunpriestess_ability1"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_sunpriestess_ability2.icon_sunpriestess_ability2"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_sunpriestess_ability3.icon_sunpriestess_ability3"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_sunpriestess_buildable1.icon_sunpriestess_buildable1")
				},
				{
					TEXT("SUN STAFF STRIKE"),
					TEXT("SOLAR GLIDE"),
					TEXT("HEALING RAY"),
					TEXT("SOLAR FLARE"),
					TEXT("SUPERNOVA"),
					TEXT("SUN SHRINE")
				},
				{
					TEXT("Strikes enemies with solar-infused staff attacks."),
					TEXT("Dashes quickly in the target direction on a wave of sunlight."),
					TEXT("Projects a beam of solar energy that heals allies and damages enemies."),
					TEXT("Calls down a burst of sunlight that damages and blinds enemies in the area."),
					TEXT("Channels radiant solar power to heal nearby allies and scorch surrounding foes."),
					TEXT("A sacred solar shrine that periodically heals nearby allies and grants vision.")
				}
			};
		}
		if (Key.Contains(TEXT("korryn")) || Key.Contains(TEXT("morgan")) || Key.Contains(TEXT("hexweaver")))
		{
			return {
				TexBackdropKorryn,
				TEXT("/BreakawayCore/UI/Art/HeroSelect/small_skin_morganlefay_default.small_skin_morganlefay_default"),
				TEXT("/BreakawayCore/UI/Art/HeroSelect/small_weapon_mlfrarestaff_heavenly.small_weapon_mlfrarestaff_heavenly"),
				TEXT("morganlefay"),
				{
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_morganlefay_basicattack.icon_morganlefay_basicattack"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_morganlefay_defense.icon_morganlefay_defense"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_morganlefay_ability1.icon_morganlefay_ability1"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_morganlefay_ability2.icon_morganlefay_ability2"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_morganlefay_ability3.icon_morganlefay_ability3"),
					TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_morganlefay_buildable1.icon_morganlefay_buildable1")
				},
				{
					TEXT("HEX BOLT"),
					TEXT("RAVEN STEP"),
					TEXT("CURSED BRIAR"),
					TEXT("DARK VORTEX"),
					TEXT("SOUL STORM"),
					TEXT("CURSED WARD")
				},
				{
					TEXT("Fires bolts of dark sorcery from her cursed staff."),
					TEXT("Blinks through shadow to evade incoming attacks."),
					TEXT("Hurls a creeping briar that damages and slows enemies it passes through."),
					TEXT("Conjures a dark vortex that pulls enemies toward its center and deals damage."),
					TEXT("Unleashes a cataclysmic storm of dark magic across the battlefield."),
					TEXT("A dark totem that curses nearby enemies, reducing their movement speed and defenses.")
				}
			};
		}

		// Default: Argus (Spartacus / Magnus)
		return {
			TexBackdropArgus,
			TEXT("/BreakawayCore/UI/Art/HeroSelect/small_skin_magnus_darkgreen.small_skin_magnus_darkgreen"),
			TEXT("/BreakawayCore/UI/Art/HeroSelect/small_weapon_magrarechainsaw_heavenly.small_weapon_magrarechainsaw_heavenly"),
			TEXT("magnus"),
			{
				TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_magnus_basicattack.icon_magnus_basicattack"),
				TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_magnus_defense.icon_magnus_defense"),
				TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_magnus_ability1.icon_magnus_ability1"),
				TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_magnus_ability2.icon_magnus_ability2"),
				TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_magnus_ability3.icon_magnus_ability3"),
				TEXT("/BreakawayCore/UI/Art/HeroSelect/icon_magnus_buildable1.icon_magnus_buildable1")
			},
			{
				TEXT("CHAINSAW CLEAVE"),
				TEXT("JUGGERNAUT CHARGE"),
				TEXT("FLAME WALL"),
				TEXT("EMPOWERED LEAP"),
				TEXT("CATACLYSM"),
				TEXT("SIEGE ENGINE")
			},
			{
				TEXT("Cleaves enemies in front of Argus with his motorized greatblade."),
				TEXT("Charges forward, knocking back enemies in his path."),
				TEXT("Slams the ground to erupt a line of searing energy forward."),
				TEXT("Leaps into the air and slams down, damaging and slowing nearby enemies."),
				TEXT("Unleashes a devastating shockwave that heavily damages and launches nearby enemies."),
				TEXT("A large siege weapon which moves forward and deals damage to enemy buildables in its path.")
			}
		};
	}
}

UBwayHeroSelectWidget::UBwayHeroSelectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TOptional<FUIInputConfig> UBwayHeroSelectWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UBwayHeroSelectWidget::BuildOrBindWidgetHierarchy()
{
	if (!WidgetTree)
	{
		return;
	}

	// If already built on this instance, nothing to do
	if (bUsingGoal5Layout && LockInButton && Text_HeroDetails && SelectionTimerWidget && HeroSlotWidgets.Num() >= 4 && HeroBackdropImage)
	{
		return;
	}

	// Check if already persisted in WidgetTree (from bway.Editor.PopulateHeroSelectWidgetBlueprint)
	if (UCanvasPanel* ExistingRootCanvas = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("HeroSelectRootCanvas"))))
	{
		(void)ExistingRootCanvas;
		HeroBackdropImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_HeroBackdrop")));
		InspectedHeroNameText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_InspectedHeroName")));
		InspectedHeroClassText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_InspectedHeroClass")));
		InspectedHeroClassBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("InspectedHeroClassBorder")));
		InspectedStatsText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_InspectedStats")));
		Text_HeroDetails = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_HeroDetails")));
		HeroDetailsBannerBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("HeroDetailsBannerBorder")));
		SelectionTimerWidget = WidgetTree->FindWidget(TEXT("Widget_Timer"));
		CountdownTimerText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_CountdownTimer")));
		ReadyCountText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_ReadyCount")));
		PhaseTitleText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_PhaseTitle")));
		LocalTeamBadgeBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("LocalTeamBadgeBorder")));
		LocalTeamBadgeText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_LocalTeamBadge")));
		SharedMovementText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_SharedMovement")));
		KitPreviewScrollBox = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("KitPreviewScrollBox")));
		HeroSlotsRow = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("HeroRosterSlotsRow")));
		LockInButton = Cast<UBwayCommonButton>(WidgetTree->FindWidget(TEXT("Button_Lock")));
		TravelConfirmButton = LockInButton;
		LockHintText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_LockHint")));
		InspectionPanelBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("InspectionPanelBorder")));

		AbilityGradientImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_AbilityGradient")));
		AbilityVideoFrameBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_AbilityVideoFrame")));
		AbilityVideoPreviewImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_AbilityVideoPreview")));
		SelectedAbilityTitleText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_SelectedAbilityTitle")));
		SelectedAbilityDescText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_SelectedAbilityDesc")));
		for (int32 i = 0; i < 6; ++i)
		{
			AbilityBarButtons[i] = Cast<UButton>(WidgetTree->FindWidget(*FString::Printf(TEXT("Button_AbilityBar_%d"), i)));
			AbilityBarOuterBorders[i] = Cast<UBorder>(WidgetTree->FindWidget(*FString::Printf(TEXT("Border_AbilityBarOuter_%d"), i)));
			AbilityBarIconImages[i] = Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("Image_AbilityBarIcon_%d"), i)));
		}
		AbilityCollapseButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_AbilityCollapse")));
		AbilityCollapseBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_AbilityCollapse")));
		LoadoutSkinImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_LoadoutSkin")));
		LoadoutTauntImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_LoadoutTaunt")));
		LoadoutWeaponImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_LoadoutWeapon")));
		BuyHeroButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_BuyHero")));
		BuyHeroPriceText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_BuyPrice")));
		ProgressionInfoButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_ProgressionInfo")));
		LeaveButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_Leave")));
		BottomSettingsButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_BottomSettings")));
		SoftCurrencyText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_SoftCurrency")));

		HeroSlotWidgets.Reset();
		for (int32 i = 0; i < 4; ++i)
		{
			if (UBwayHeroSlotWidget* SlotW = Cast<UBwayHeroSlotWidget>(WidgetTree->FindWidget(*FString::Printf(TEXT("HeroSlotCard_%d"), i))))
			{
				SlotW->EnsureVisualTreeConstructed();
				HeroSlotWidgets.Add(SlotW);
			}
		}

		AbilityPreviewWidgets.Reset();
		for (int32 i = 0; i < 8; ++i)
		{
			if (UBwayAbilityPreviewWidget* AbRow = Cast<UBwayAbilityPreviewWidget>(WidgetTree->FindWidget(*FString::Printf(TEXT("AbilityPreviewRow_%d"), i))))
			{
				AbilityPreviewWidgets.Add(AbRow);
			}
		}

		BlueTeamHeaderText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_BlueTeamSidebarHeader")));
		RedTeamHeaderText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("Text_RedTeamSidebarHeader")));
		for (int32 i = 0; i < 4; ++i)
		{
			BlueTeamSlots[i].OuterBorder = Cast<UBorder>(WidgetTree->FindWidget(*FString::Printf(TEXT("BlueTeamSlotOuter_%d"), i)));
			BlueTeamSlots[i].PortraitBorder = Cast<UBorder>(WidgetTree->FindWidget(*FString::Printf(TEXT("BlueTeamSlotPortraitBorder_%d"), i)));
			BlueTeamSlots[i].PortraitImage = Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("BlueTeamSlotPortraitImg_%d"), i)));
			BlueTeamSlots[i].PlayerNameText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(*FString::Printf(TEXT("BlueTeamSlotPlayerName_%d"), i)));
			BlueTeamSlots[i].HeroStatusText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(*FString::Printf(TEXT("BlueTeamSlotHeroStatus_%d"), i)));

			RedTeamSlots[i].OuterBorder = Cast<UBorder>(WidgetTree->FindWidget(*FString::Printf(TEXT("RedTeamSlotOuter_%d"), i)));
			RedTeamSlots[i].PortraitBorder = Cast<UBorder>(WidgetTree->FindWidget(*FString::Printf(TEXT("RedTeamSlotPortraitBorder_%d"), i)));
			RedTeamSlots[i].PortraitImage = Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("RedTeamSlotPortraitImg_%d"), i)));
			RedTeamSlots[i].PlayerNameText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(*FString::Printf(TEXT("RedTeamSlotPlayerName_%d"), i)));
			RedTeamSlots[i].HeroStatusText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(*FString::Printf(TEXT("RedTeamSlotHeroStatus_%d"), i)));
		}

		if (LockInButton && Text_HeroDetails && SelectionTimerWidget && HeroSlotWidgets.Num() >= 4 && AbilityPreviewWidgets.Num() >= 6)
		{
			bUsingGoal5Layout = true;
			return;
		}
	}

	// Displace any legacy widgets sharing reserved contract names
	SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("HeroSelectRootOverlay"));
	SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("HeroSelectRootCanvas"));
	SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("Button_Lock"));
	SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("Text_HeroDetails"));
	SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("Widget_Timer"));

	bUsingGoal5Layout = true;
	HeroSlotWidgets.Reset();
	AbilityPreviewWidgets.Reset();

	const UObject* HouseOfCardsItalicFont = LoadObject<UFont>(nullptr, FontHouseOfCardsBoldItalic);
	const UObject* NeuzeitBoldItalicFont = LoadObject<UFont>(nullptr, FontNeuzeitBoldItalic);
	const UObject* NeuzeitBoldFont = LoadObject<UFont>(nullptr, FontNeuzeitBold);
	const UObject* RefrigeratorFont = LoadObject<UFont>(nullptr, FontRefrigeratorHeavy);
	const UObject* NimbusFont = LoadObject<UFont>(nullptr, FontNimbusReg);

	auto MakeTextWithFont = [this](FName Name, const FText& Txt, const UObject* FontObj, int32 PtSize, const FLinearColor& Col, ETextJustify::Type Justify = ETextJustify::Left) -> UCommonTextBlock*
	{
		UCommonTextBlock* Block = Name.IsNone()
			? WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass())
			: WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), Name);
		Block->SetText(Txt);
		Block->SetColorAndOpacity(FSlateColor(Col));
		Block->SetJustification(Justify);
		Block->SetFont(MakeHeroSelectFont(FontObj, PtSize));
		Block->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Block;
	};

	// =========================================================================
	// ROOT VIEWPORT CANVAS:
	//   Layer 0: BackdropScaleBox (ScaleToFill 1920x1080 hero showcase & temple portal)
	//   Layer 1: MainScaleBox (ScaleToFit 1920x1080 Breakaway Character Select UI)
	// =========================================================================
	UCanvasPanel* ViewportCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HeroSelectViewportCanvas"));
	WidgetTree->RootWidget = ViewportCanvas;

	UScaleBox* BackdropScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("BackdropScaleBox"));
	BackdropScaleBox->SetStretch(EStretch::ScaleToFill);
	BackdropScaleBox->SetStretchDirection(EStretchDirection::Both);
	BackdropScaleBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* BgScaleSlot = ViewportCanvas->AddChildToCanvas(BackdropScaleBox))
	{
		BgScaleSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgScaleSlot->SetOffsets(FMargin(0.f));
		BgScaleSlot->SetZOrder(0);
	}

	HeroBackdropImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_HeroBackdrop"));
	ApplyHeroSelectBrush(HeroBackdropImage, TexBackdropArgus, FVector2D(1920.f, 1080.f));
	HeroBackdropImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BackdropScaleBox->AddChild(HeroBackdropImage);

	UScaleBox* MainScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("MainUIScaleBox"));
	MainScaleBox->SetStretch(EStretch::ScaleToFit);
	MainScaleBox->SetStretchDirection(EStretchDirection::Both);
	if (UCanvasPanelSlot* MainScaleSlot = ViewportCanvas->AddChildToCanvas(MainScaleBox))
	{
		MainScaleSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		MainScaleSlot->SetOffsets(FMargin(0.f));
		MainScaleSlot->SetZOrder(1);
	}

	USizeBox* MainSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MainUISizeBox"));
	MainSizeBox->SetWidthOverride(1920.f);
	MainSizeBox->SetHeightOverride(1080.f);
	MainScaleBox->AddChild(MainSizeBox);

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HeroSelectRootCanvas"));
	MainSizeBox->AddChild(RootCanvas);

	auto PlaceOnCanvas = [](UCanvasPanel* Canvas, UWidget* Child, float X, float Y, float W, float H, int32 ZOrder) -> UCanvasPanelSlot*
	{
		UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Child);
		if (CSlot)
		{
			CSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			CSlot->SetAlignment(FVector2D(0.f, 0.f));
			CSlot->SetPosition(FVector2D(X, Y));
			CSlot->SetSize(FVector2D(W, H));
			CSlot->SetZOrder(ZOrder);
		}
		return CSlot;
	};

	// =========================================================================
	// 1. LEFT SLANTED GLASS PANE (#lgGlassPane)
	// =========================================================================
	UBorder* LeftGlassOuter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_LeftGlassOuter"));
	LeftGlassOuter->SetPadding(FMargin(1.0f));
	LeftGlassOuter->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.04f));
	LeftGlassOuter->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	LeftGlassOuter->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
	LeftGlassOuter->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(RootCanvas, LeftGlassOuter, 239.f, -10.f, 494.f, 987.f, 2);

	UBorder* LeftGlassInner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_LeftGlassInner"));
	LeftGlassInner->SetBrushColor(FLinearColor(0.006f, 0.008f, 0.012f, 0.28f));
	LeftGlassOuter->SetContent(LeftGlassInner);

	// =========================================================================
	// 2. CHARACTER INFO HEADER (#characterInfoPane: ARGUS, XP Bar, FIGHTER, MAX LEVEL 99, Info Btn)
	// =========================================================================
	InspectedHeroNameText = MakeTextWithFont(
		TEXT("Text_InspectedHeroName"),
		NSLOCTEXT("BwayHeroSelect", "DefaultHeroTitle", "ARGUS"),
		HouseOfCardsItalicFont,
		54,
		FLinearColor::FromSRGBColor(FColor(250, 193, 3, 255)));
	InspectedHeroNameText->SetShadowOffset(FVector2D(2.f, 2.f));
	InspectedHeroNameText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	PlaceOnCanvas(RootCanvas, InspectedHeroNameText, 127.f, 100.f, 560.f, 76.f, 5);

	UBorder* XPBarOuter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_XPBarOuter"));
	XPBarOuter->SetPadding(FMargin(1.0f));
	XPBarOuter->SetBrushColor(FLinearColor::FromSRGBColor(FColor(115, 115, 115, 255)));
	XPBarOuter->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	XPBarOuter->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
	XPBarOuter->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(RootCanvas, XPBarOuter, 130.f, 175.f, 532.f, 14.f, 5);

	UImage* XPBarFillImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_XPBarFill"));
	ApplyHeroSelectBrush(XPBarFillImg, TexXPBarFill, FVector2D(530.f, 12.f));
	XPBarOuter->SetContent(XPBarFillImg);

	InspectedHeroClassText = MakeTextWithFont(
		TEXT("Text_InspectedHeroClass"),
		NSLOCTEXT("BwayHeroSelect", "DefaultClassTitle", "FIGHTER"),
		NeuzeitBoldItalicFont,
		16,
		FLinearColor::White);
	InspectedHeroClassText->SetShadowOffset(FVector2D(1.f, 1.f));
	InspectedHeroClassText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.80f));
	PlaceOnCanvas(RootCanvas, InspectedHeroClassText, 133.f, 190.f, 280.f, 26.f, 5);

	UCommonTextBlock* LevelLabelText = MakeTextWithFont(
		TEXT("Text_CharacterLevelLabel"),
		NSLOCTEXT("BwayHeroSelect", "MaxLevelLabel", "MAX LEVEL"),
		NeuzeitBoldItalicFont,
		14,
		FLinearColor::White,
		ETextJustify::Right);
	LevelLabelText->SetShadowOffset(FVector2D(1.f, 1.f));
	LevelLabelText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.80f));
	PlaceOnCanvas(RootCanvas, LevelLabelText, 498.f, 192.f, 160.f, 24.f, 5);

	UCommonTextBlock* LevelNumText = MakeTextWithFont(
		TEXT("Text_CharacterLevelNumber"),
		FText::FromString(TEXT("99")),
		RefrigeratorFont,
		34,
		FLinearColor::White,
		ETextJustify::Left);
	LevelNumText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	LevelNumText->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
	LevelNumText->SetShadowOffset(FVector2D(1.5f, 2.f));
	LevelNumText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	PlaceOnCanvas(RootCanvas, LevelNumText, 662.f, 165.f, 64.f, 52.f, 5);

	ProgressionInfoButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_ProgressionInfo"));
	SetTransparentHitButton(ProgressionInfoButton);
	PlaceOnCanvas(RootCanvas, ProgressionInfoButton, 722.f, 180.f, 36.f, 36.f, 6);

	UCanvasPanel* InfoBtnCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_ProgressionInfo"));
	ProgressionInfoButton->AddChild(InfoBtnCanvas);

	UImage* InfoCircleImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_ProgressionInfoCircle"));
	ApplyHeroSelectBrush(InfoCircleImg, TexInfoCircle, FVector2D(36.f, 36.f));
	PlaceOnCanvas(InfoBtnCanvas, InfoCircleImg, 0.f, 0.f, 36.f, 36.f, 0);

	UImage* InfoSymImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_ProgressionInfoSymbol"));
	ApplyHeroSelectBrush(InfoSymImg, TexInfoSymbol, FVector2D(6.f, 16.f));
	PlaceOnCanvas(InfoBtnCanvas, InfoSymImg, 15.f, 10.f, 6.f, 16.f, 1);

	// =========================================================================
	// 3. 16-SLOT SKEWED 4x4 HERO SELECTION GRID (#gridHolder.thin)
	//    Populated slots:
	//      Cell 0  (Row 0, Col 0): Argus (Spartacus)  -> HeroSlotWidgets[0]
	//      Cell 9  (Row 2, Col 1): Rawlins            -> HeroSlotWidgets[3]
	//      Cell 12 (Row 3, Col 0): Alona              -> HeroSlotWidgets[1]
	//      Cell 13 (Row 3, Col 1): Korryn             -> HeroSlotWidgets[2]
	//    Remaining 12 cells are empty dark skewed slots matching screenshot cells 7 & 11.
	// =========================================================================
	for (int32 SlotIdx = 0; SlotIdx < 4; ++SlotIdx)
	{
		UBwayHeroSlotWidget* SlotWidget = WidgetTree->ConstructWidget<UBwayHeroSlotWidget>(
			UBwayHeroSlotWidget::StaticClass(),
			*FString::Printf(TEXT("HeroSlotCard_%d"), SlotIdx));
		SlotWidget->EnsureVisualTreeConstructed();
		SlotWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SlotWidget->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
		HeroSlotWidgets.Add(SlotWidget);
	}

	const int32 HeroCellPositions[4] = { 0, 12, 13, 9 }; // Canonical index 0=Argus->Cell 0, 1=Alona->Cell 12, 2=Korryn->Cell 13, 3=Rawlins->Cell 9
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			const int32 CellIdx = Row * 4 + Col;
			const float CellX = 207.0f + Row * 40.0f + Col * 109.0f;
			const float CellY = 246.0f + Row * 149.0f;

			int32 CanonicalHeroSlotIdx = INDEX_NONE;
			for (int32 h = 0; h < 4; ++h)
			{
				if (HeroCellPositions[h] == CellIdx)
				{
					CanonicalHeroSlotIdx = h;
					break;
				}
			}

			if (CanonicalHeroSlotIdx != INDEX_NONE && HeroSlotWidgets.IsValidIndex(CanonicalHeroSlotIdx))
			{
				PlaceOnCanvas(RootCanvas, HeroSlotWidgets[CanonicalHeroSlotIdx], CellX, CellY, 97.f, 143.f, 6);
			}
			else
			{
				UBorder* EmptyOuter = WidgetTree->ConstructWidget<UBorder>(
					UBorder::StaticClass(),
					*FString::Printf(TEXT("GridEmptyCell_%d"), CellIdx));
				EmptyOuter->SetPadding(FMargin(1.0f));
				EmptyOuter->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.045f));
				EmptyOuter->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				EmptyOuter->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
				EmptyOuter->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				PlaceOnCanvas(RootCanvas, EmptyOuter, CellX, CellY, 97.f, 143.f, 5);

				UBorder* EmptyInner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				EmptyInner->SetBrushColor(FLinearColor(0.004f, 0.005f, 0.007f, 0.58f));
				EmptyOuter->SetContent(EmptyInner);
			}
		}
	}

	// =========================================================================
	// 4. BOTTOM-LEFT ACTION BUTTONS (#buyBtnSmall '400' & #readyBtnSmall 'TRY' / Button_Lock)
	// =========================================================================
	BuyHeroButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_BuyHero"));
	SetTransparentHitButton(BuyHeroButton);
	BuyHeroButton->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	BuyHeroButton->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
	PlaceOnCanvas(RootCanvas, BuyHeroButton, 388.f, 899.f, 214.f, 47.f, 6);

	UBorder* BuyBtnBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_BuyHero"));
	BuyBtnBorder->SetBrushColor(FLinearColor::FromSRGBColor(FColor(116, 27, 29, 255)));
	BuyBtnBorder->SetHorizontalAlignment(HAlign_Center);
	BuyBtnBorder->SetVerticalAlignment(VAlign_Center);
	BuyHeroButton->AddChild(BuyBtnBorder);

	UHorizontalBox* BuyBtnHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HBox_BuyHero"));
	BuyBtnHBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	BuyBtnHBox->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
	BuyBtnBorder->SetContent(BuyBtnHBox);

	UImage* BuyCoinImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_BuyCoinIcon"));
	ApplyHeroSelectBrush(BuyCoinImg, TexSingleCoin, FVector2D(28.f, 28.f));
	if (UHorizontalBoxSlot* CoinHSlot = BuyBtnHBox->AddChildToHorizontalBox(BuyCoinImg))
	{
		CoinHSlot->SetVerticalAlignment(VAlign_Center);
		CoinHSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	BuyHeroPriceText = MakeTextWithFont(
		TEXT("Text_BuyPrice"),
		FText::FromString(TEXT("400")),
		RefrigeratorFont,
		26,
		FLinearColor::White,
		ETextJustify::Center);
	if (UHorizontalBoxSlot* PriceHSlot = BuyBtnHBox->AddChildToHorizontalBox(BuyHeroPriceText))
	{
		PriceHSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Button_Lock ('TRY' / 'LOCK IN' / 'Retry travel')
	LockInButton = WidgetTree->ConstructWidget<UBwayCommonButton>(UBwayCommonButton::StaticClass(), TEXT("Button_Lock"));
	LockInButton->SetLabelText(NSLOCTEXT("BwayHeroSelect", "TryButtonLabel", "TRY"));
	LockInButton->SetFontSize(26);
	LockInButton->SetContentPadding(FMargin(8.f, 4.f));
	const FLinearColor TryGoldColor = FLinearColor::FromSRGBColor(FColor(253, 185, 0, 255));
	LockInButton->SetButtonColors(TryGoldColor, TryGoldColor, TryGoldColor, FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));
	LockInButton->SetHighlightedState(true);
	LockInButton->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	LockInButton->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
	if (UCommonTextBlock* LockLbl = LockInButton->GetLabelWidget())
	{
		LockLbl->SetFont(MakeHeroSelectFont(RefrigeratorFont, 26));
		LockLbl->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		LockLbl->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
	}
	PlaceOnCanvas(RootCanvas, LockInButton, 608.f, 899.f, 215.f, 47.f, 6);
	TravelConfirmButton = LockInButton;

	// =========================================================================
	// 5. RIGHT-SIDE ABILITY PREVIEW PANEL (#abilityPane: 16:9 Media Frame, Title, Desc, 6 Icons + X)
	// =========================================================================
	AbilityGradientImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_AbilityGradient"));
	ApplyHeroSelectBrush(AbilityGradientImage, TexAbilityGradient, FVector2D(839.f, 426.f), FLinearColor(1.f, 1.f, 1.f, 0.85f));
	AbilityGradientImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(RootCanvas, AbilityGradientImage, 1063.f, 433.f, 839.f, 426.f, 3);

	AbilityVideoFrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_AbilityVideoFrame"));
	AbilityVideoFrameBorder->SetPadding(FMargin(0.f));
	AbilityVideoFrameBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f));
	AbilityVideoFrameBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(RootCanvas, AbilityVideoFrameBorder, 1153.f, 360.f, 520.f, 292.f, 4);

	AbilityVideoPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_AbilityVideoPreview"));
	ApplyHeroSelectBrush(
		AbilityVideoPreviewImage,
		TEXT("/BreakawayCore/UI/Art/HeroSelect/ability_magnus_buildable1.ability_magnus_buildable1"),
		FVector2D(520.f, 292.f));
	AbilityVideoFrameBorder->SetContent(AbilityVideoPreviewImage);

	SelectedAbilityTitleText = MakeTextWithFont(
		TEXT("Text_SelectedAbilityTitle"),
		FText::FromString(TEXT("SIEGE ENGINE")),
		HouseOfCardsItalicFont,
		24,
		FLinearColor::FromSRGBColor(FColor(255, 167, 28, 255)));
	SelectedAbilityTitleText->SetShadowOffset(FVector2D(1.5f, 2.f));
	SelectedAbilityTitleText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	PlaceOnCanvas(RootCanvas, SelectedAbilityTitleText, 1160.f, 663.f, 500.f, 36.f, 5);

	SelectedAbilityDescText = MakeTextWithFont(
		TEXT("Text_SelectedAbilityDesc"),
		FText::FromString(TEXT("A large siege weapon which moves forward and deals damage to enemy buildables in its path.")),
		NeuzeitBoldFont,
		15,
		FLinearColor::White);
	SelectedAbilityDescText->SetAutoWrapText(true);
	SelectedAbilityDescText->SetShadowOffset(FVector2D(1.f, 1.5f));
	SelectedAbilityDescText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	PlaceOnCanvas(RootCanvas, SelectedAbilityDescText, 1160.f, 703.f, 508.f, 68.f, 5);

	const FBreakawayHeroSelectArtMapping DefaultArgusArt = ResolveHeroArtMapping(FPrimaryAssetId(), FText::FromString(TEXT("Argus")));
	for (int32 AbIdx = 0; AbIdx < 6; ++AbIdx)
	{
		UButton* AbBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("Button_AbilityBar_%d"), AbIdx));
		SetTransparentHitButton(AbBtn);
		AbBtn->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		AbBtn->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
		PlaceOnCanvas(RootCanvas, AbBtn, 1184.f + AbIdx * 80.f, 779.f, 77.f, 77.f, 6);
		AbilityBarButtons[AbIdx] = AbBtn;

		const bool bIsSelectedAb = (AbIdx == SelectedAbilityPreviewSlot);
		UBorder* AbOuter = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("Border_AbilityBarOuter_%d"), AbIdx));
		AbOuter->SetPadding(FMargin(bIsSelectedAb ? 3.0f : 1.0f));
		AbOuter->SetBrushColor(bIsSelectedAb
			? FLinearColor::FromSRGBColor(FColor(255, 162, 0, 255))
			: FLinearColor(1.0f, 1.0f, 1.0f, 0.08f));
		AbBtn->AddChild(AbOuter);
		AbilityBarOuterBorders[AbIdx] = AbOuter;

		UBorder* AbInner = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("Border_AbilityBarInner_%d"), AbIdx));
		AbInner->SetPadding(FMargin(4.f));
		AbInner->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.02f, 0.92f));
		AbInner->SetHorizontalAlignment(HAlign_Center);
		AbInner->SetVerticalAlignment(VAlign_Center);
		AbOuter->SetContent(AbInner);

		UImage* AbIcon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("Image_AbilityBarIcon_%d"), AbIdx));
		ApplyHeroSelectBrush(AbIcon, DefaultArgusArt.AbilityIcons[AbIdx], FVector2D(62.f, 62.f));
		AbIcon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		AbIcon->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
		AbInner->SetContent(AbIcon);
		AbilityBarIconImages[AbIdx] = AbIcon;
	}

	AbilityCollapseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_AbilityCollapse"));
	SetTransparentHitButton(AbilityCollapseButton);
	AbilityCollapseButton->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	AbilityCollapseButton->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
	PlaceOnCanvas(RootCanvas, AbilityCollapseButton, 1665.f, 797.f, 56.f, 51.f, 6);

	AbilityCollapseBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_AbilityCollapse"));
	AbilityCollapseBorder->SetBrushColor(FLinearColor::FromSRGBColor(FColor(255, 162, 0, 255)));
	AbilityCollapseBorder->SetHorizontalAlignment(HAlign_Center);
	AbilityCollapseBorder->SetVerticalAlignment(VAlign_Center);
	AbilityCollapseButton->AddChild(AbilityCollapseBorder);

	UImage* CollapseSymImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_AbilityCollapseSymbol"));
	ApplyHeroSelectBrush(CollapseSymImg, TexCloseSymbol, FVector2D(15.f, 13.f), FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));
	CollapseSymImg->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	CollapseSymImg->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
	AbilityCollapseBorder->SetContent(CollapseSymImg);

	// =========================================================================
	// 6. BOTTOM-RIGHT LOADOUT PANE (#smallGlassPane + SKINS, TAUNTS, WEAPONS cards)
	// =========================================================================
	UBorder* LoadoutGlassOuter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_LoadoutGlassOuter"));
	LoadoutGlassOuter->SetPadding(FMargin(1.0f));
	LoadoutGlassOuter->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.035f));
	LoadoutGlassOuter->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	LoadoutGlassOuter->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
	LoadoutGlassOuter->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(RootCanvas, LoadoutGlassOuter, 1233.f, 854.f, 554.f, 175.f, 3);

	UBorder* LoadoutGlassInner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_LoadoutGlassInner"));
	LoadoutGlassInner->SetBrushColor(FLinearColor(0.004f, 0.005f, 0.007f, 0.48f));
	LoadoutGlassOuter->SetContent(LoadoutGlassInner);

	const TCHAR* LoadoutCardLabels[3] = { TEXT("SKINS"), TEXT("TAUNTS"), TEXT("WEAPONS") };
	const TCHAR* LoadoutImgNames[3] = { TEXT("Image_LoadoutSkin"), TEXT("Image_LoadoutTaunt"), TEXT("Image_LoadoutWeapon") };
	const TCHAR* LoadoutDefaultTex[3] = { DefaultArgusArt.SkinTex, TexDefaultTaunt, DefaultArgusArt.WeaponTex };
	TObjectPtr<UImage>* LoadoutImgPtrs[3] = { &LoadoutSkinImage, &LoadoutTauntImage, &LoadoutWeaponImage };

	for (int32 c = 0; c < 3; ++c)
	{
		UBorder* CardOuter = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("Card_Loadout_%d"), c));
		CardOuter->SetPadding(FMargin(1.0f));
		CardOuter->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.08f));
		CardOuter->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		CardOuter->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(15.0f, 0.0f), 0.0f));
		CardOuter->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PlaceOnCanvas(RootCanvas, CardOuter, 1296.f + c * 129.f, 873.f, 123.f, 138.f, 5);

		UBorder* CardInner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		CardInner->SetPadding(FMargin(0.f));
		CardInner->SetBrushColor(FLinearColor(0.008f, 0.01f, 0.014f, 0.90f));
		CardInner->SetClipping(EWidgetClipping::ClipToBounds);
		CardOuter->SetContent(CardInner);

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		CardCanvas->SetClipping(EWidgetClipping::ClipToBounds);
		CardInner->SetContent(CardCanvas);

		UImage* CardThumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), LoadoutImgNames[c]);
		CardThumb->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (c == 1)
		{
			CardThumb->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
			ApplyHeroSelectBrush(CardThumb, LoadoutDefaultTex[c], FVector2D(92.f, 92.f), FLinearColor(0.65f, 0.65f, 0.65f, 0.85f));
			PlaceOnCanvas(CardCanvas, CardThumb, 15.f, 10.f, 92.f, 92.f, 1);
		}
		else
		{
			CardThumb->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D::ZeroVector, 0.0f));
			ApplyHeroSelectBrush(CardThumb, LoadoutDefaultTex[c], FVector2D(121.f, 112.f));
			PlaceOnCanvas(CardCanvas, CardThumb, 0.f, 0.f, 121.f, 112.f, 1);
		}
		*LoadoutImgPtrs[c] = CardThumb;

		UBorder* CardFooter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		CardFooter->SetBrushColor(FLinearColor::FromSRGBColor(FColor(16, 16, 16, 245)));
		CardFooter->SetHorizontalAlignment(HAlign_Center);
		CardFooter->SetVerticalAlignment(VAlign_Center);
		PlaceOnCanvas(CardCanvas, CardFooter, 0.f, 112.f, 121.f, 24.f, 2);

		UCommonTextBlock* CardLbl = MakeTextWithFont(
			NAME_None,
			FText::FromString(LoadoutCardLabels[c]),
			NeuzeitBoldFont,
			11,
			FLinearColor::FromSRGBColor(FColor(184, 184, 184, 255)),
			ETextJustify::Center);
		CardLbl->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		CardLbl->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
		CardFooter->SetContent(CardLbl);
	}

	// =========================================================================
	// 7. RIGHT-EDGE PARTY SOCIAL BAR (social_component.html: 4 circular slots + '...' pill)
	// =========================================================================
	UCanvasPanel* PartyCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PartySocialCanvas"));
	PlaceOnCanvas(RootCanvas, PartyCanvas, 1770.f, 175.f, 150.f, 312.f, 6);

	UBorder* PartyBacking = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PartyBackingBorder"));
	PartyBacking->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.55f));
	PartyBacking->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(PartyCanvas, PartyBacking, 56.f, 5.f, 94.f, 304.f, 0);

	UImage* LeaderAvatarImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_PartyLeaderAvatar"));
	ApplyHeroSelectBrush(LeaderAvatarImg, TexPartyAvatarSlot, FVector2D(66.f, 66.f));
	PlaceOnCanvas(PartyCanvas, LeaderAvatarImg, 61.f, 2.f, 66.f, 66.f, 2);

	UImage* LeaderDotImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_PartyLeaderDot"));
	ApplyHeroSelectBrush(LeaderDotImg, TexPartyLeaderDot, FVector2D(10.f, 10.f));
	PlaceOnCanvas(PartyCanvas, LeaderDotImg, 89.f, 59.f, 10.f, 10.f, 3);

	UImage* QuickChatImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_QuickChatBalloon"));
	ApplyHeroSelectBrush(QuickChatImg, TexQuickChatBalloon, FVector2D(42.f, 37.f));
	PlaceOnCanvas(PartyCanvas, QuickChatImg, 4.f, 22.f, 42.f, 37.f, 3);

	const float InviteSlotYs[3] = { 78.f, 154.f, 230.f };
	for (int32 idx = 0; idx < 3; ++idx)
	{
		UImage* InvImg = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("Image_PartyInviteSlot%d"), idx + 1));
		ApplyHeroSelectBrush(InvImg, TexPartyInviteSlot, FVector2D(66.f, 66.f));
		PlaceOnCanvas(PartyCanvas, InvImg, 61.f, InviteSlotYs[idx], 66.f, 66.f, 2);
	}

	// =========================================================================
	// 8. BOTTOM HINT / STATUS BAR (bottombar2.html: < LEAVE, Build Watermark, 0 + Coin, Settings)
	// =========================================================================
	UCanvasPanel* BottomCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BottomHintBarCanvas"));
	PlaceOnCanvas(RootCanvas, BottomCanvas, 0.f, 1026.f, 1920.f, 54.f, 7);

	UBorder* BottomFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomBarFillBorder"));
	BottomFill->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
	BottomFill->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(BottomCanvas, BottomFill, 0.f, 0.f, 1920.f, 54.f, 0);

	UBorder* BottomTopLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomBarTopLine"));
	BottomTopLine->SetBrushColor(FLinearColor::FromSRGBColor(FColor(135, 90, 13, 255)));
	BottomTopLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(BottomCanvas, BottomTopLine, 0.f, 0.f, 1920.f, 2.2f, 1);

	LeaveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Leave"));
	SetTransparentHitButton(LeaveButton);
	PlaceOnCanvas(BottomCanvas, LeaveButton, 18.f, 8.f, 150.f, 38.f, 3);

	UHorizontalBox* LeaveHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HBox_LeaveButton"));
	LeaveButton->AddChild(LeaveHBox);

	UImage* LeaveIconImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_LeaveIcon"));
	ApplyHeroSelectBrush(LeaveIconImg, TexLeaveIcon, FVector2D(34.f, 34.f));
	if (UHorizontalBoxSlot* LeaveIconSlot = LeaveHBox->AddChildToHorizontalBox(LeaveIconImg))
	{
		LeaveIconSlot->SetVerticalAlignment(VAlign_Center);
		LeaveIconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	UCommonTextBlock* LeaveLbl = MakeTextWithFont(
		TEXT("Text_LeaveLabel"),
		NSLOCTEXT("BwayHeroSelect", "LeaveBtnLabel", "LEAVE"),
		NeuzeitBoldFont,
		15,
		FLinearColor::White);
	if (UHorizontalBoxSlot* LeaveTxtSlot = LeaveHBox->AddChildToHorizontalBox(LeaveLbl))
	{
		LeaveTxtSlot->SetVerticalAlignment(VAlign_Center);
	}

	UCommonTextBlock* BuildWatermarkText = MakeTextWithFont(
		TEXT("Text_BuildWatermark"),
		FText::FromString(TEXT("Breakaway Build # 0.0.0.0 218026")),
		NimbusFont,
		10,
		FLinearColor::FromSRGBColor(FColor(45, 205, 45, 230)),
		ETextJustify::Right);
	PlaceOnCanvas(BottomCanvas, BuildWatermarkText, 1526.f, 9.f, 340.f, 20.f, 3);

	SoftCurrencyText = MakeTextWithFont(
		TEXT("Text_SoftCurrency"),
		FText::FromString(TEXT("0")),
		RefrigeratorFont,
		25,
		FLinearColor::FromSRGBColor(FColor(255, 167, 28, 255)),
		ETextJustify::Right);
	PlaceOnCanvas(BottomCanvas, SoftCurrencyText, 1644.f, 12.f, 150.f, 40.f, 2);

	UImage* CoinImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_CurrencyCoin"));
	ApplyHeroSelectBrush(CoinImage, TexHintbarCoin, FVector2D(52.f, 52.f));
	PlaceOnCanvas(BottomCanvas, CoinImage, 1796.f, 1.f, 52.f, 52.f, 2);

	BottomSettingsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_BottomSettings"));
	SetTransparentHitButton(BottomSettingsButton);
	PlaceOnCanvas(BottomCanvas, BottomSettingsButton, 1842.f, 1.f, 52.f, 52.f, 3);

	UImage* SettingsImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_BottomSettings"));
	ApplyHeroSelectBrush(SettingsImg, TexHintbarSettings, FVector2D(52.f, 52.f));
	BottomSettingsButton->AddChild(SettingsImg);

	// =========================================================================
	// 9. COLLAPSED COMPATIBILITY CONTAINER (Preserves Goal 5 Automated Test Contracts)
	// =========================================================================
	InspectionPanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InspectionPanelBorder"));
	InspectionPanelBorder->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(RootCanvas, InspectionPanelBorder, 0.f, 0.f, 1.f, 1.f, 0);

	UVerticalBox* CompatVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CompatContractsVBox"));
	InspectionPanelBorder->SetContent(CompatVBox);

	PhaseTitleText = MakeTextWithFont(TEXT("Text_PhaseTitle"), FText::FromString(TEXT("HERO SELECT")), NeuzeitBoldFont, 14, FLinearColor::White);
	CompatVBox->AddChildToVerticalBox(PhaseTitleText);

	LocalTeamBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LocalTeamBadgeBorder"));
	LocalTeamBadgeText = MakeTextWithFont(TEXT("Text_LocalTeamBadge"), FText::FromString(TEXT("BLUE TEAM")), NeuzeitBoldFont, 11, FLinearColor::White);
	LocalTeamBadgeBorder->SetContent(LocalTeamBadgeText);
	CompatVBox->AddChildToVerticalBox(LocalTeamBadgeBorder);

	ReadyCountText = MakeTextWithFont(TEXT("Text_ReadyCount"), FText::FromString(TEXT("Ready (0/0)")), NeuzeitBoldFont, 12, FLinearColor::White);
	CompatVBox->AddChildToVerticalBox(ReadyCountText);

	UBorder* TimerFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Widget_Timer"));
	CountdownTimerText = MakeTextWithFont(TEXT("Text_CountdownTimer"), FText::FromString(TEXT("ASSEMBLE TEAM   99")), NeuzeitBoldFont, 14, FLinearColor::White);
	TimerFrame->SetContent(CountdownTimerText);
	SelectionTimerWidget = TimerFrame;
	CompatVBox->AddChildToVerticalBox(TimerFrame);

	InspectedHeroClassBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InspectedHeroClassBorder"));
	CompatVBox->AddChildToVerticalBox(InspectedHeroClassBorder);

	InspectedStatsText = MakeTextWithFont(TEXT("Text_InspectedStats"), FText::FromString(TEXT("HP 0 | ATK 0 | ARM 0 | SPD 0")), NeuzeitBoldFont, 10, FLinearColor::White);
	CompatVBox->AddChildToVerticalBox(InspectedStatsText);

	HeroDetailsBannerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HeroDetailsBannerBorder"));
	Text_HeroDetails = MakeTextWithFont(TEXT("Text_HeroDetails"), FText::FromString(TEXT("Choose a hero.")), NeuzeitBoldFont, 10, FLinearColor::White);
	HeroDetailsBannerBorder->SetContent(Text_HeroDetails);
	CompatVBox->AddChildToVerticalBox(HeroDetailsBannerBorder);

	SharedMovementText = MakeTextWithFont(TEXT("Text_SharedMovement"), UBwayHeroAbilityUILibrary::ResolveSharedMovementSummaryText(nullptr), NeuzeitBoldFont, 10, FLinearColor::White);
	CompatVBox->AddChildToVerticalBox(SharedMovementText);

	LockHintText = MakeTextWithFont(TEXT("Text_LockHint"), FText::FromString(TEXT("Unique heroes per team")), NeuzeitBoldFont, 10, FLinearColor::White);
	CompatVBox->AddChildToVerticalBox(LockHintText);

	HeroSlotsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeroRosterSlotsRow"));
	CompatVBox->AddChildToVerticalBox(HeroSlotsRow);

	KitPreviewScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("KitPreviewScrollBox"));
	CompatVBox->AddChildToVerticalBox(KitPreviewScrollBox);
	for (int32 AbilIdx = 0; AbilIdx < 8; ++AbilIdx)
	{
		UBwayAbilityPreviewWidget* PreviewRow = WidgetTree->ConstructWidget<UBwayAbilityPreviewWidget>(
			UBwayAbilityPreviewWidget::StaticClass(),
			*FString::Printf(TEXT("AbilityPreviewRow_%d"), AbilIdx));
		PreviewRow->ClearPreview();
		AbilityPreviewWidgets.Add(PreviewRow);
		KitPreviewScrollBox->AddChild(PreviewRow);
	}

	BlueTeamHeaderText = MakeTextWithFont(TEXT("Text_BlueTeamSidebarHeader"), FText::FromString(TEXT("BLUE TEAM (0/4)")), NeuzeitBoldFont, 10, FLinearColor::White);
	RedTeamHeaderText = MakeTextWithFont(TEXT("Text_RedTeamSidebarHeader"), FText::FromString(TEXT("RED TEAM (0/4)")), NeuzeitBoldFont, 10, FLinearColor::White);
	CompatVBox->AddChildToVerticalBox(BlueTeamHeaderText);
	CompatVBox->AddChildToVerticalBox(RedTeamHeaderText);

	for (int32 i = 0; i < 4; ++i)
	{
		BlueTeamSlots[i].OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("BlueTeamSlotOuter_%d"), i));
		BlueTeamSlots[i].PortraitBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("BlueTeamSlotPortraitBorder_%d"), i));
		BlueTeamSlots[i].PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("BlueTeamSlotPortraitImg_%d"), i));
		BlueTeamSlots[i].PlayerNameText = MakeTextWithFont(*FString::Printf(TEXT("BlueTeamSlotPlayerName_%d"), i), FText::GetEmpty(), NeuzeitBoldFont, 9, FLinearColor::White);
		BlueTeamSlots[i].HeroStatusText = MakeTextWithFont(*FString::Printf(TEXT("BlueTeamSlotHeroStatus_%d"), i), FText::GetEmpty(), NeuzeitBoldFont, 9, FLinearColor::White);
		CompatVBox->AddChildToVerticalBox(BlueTeamSlots[i].OuterBorder);
		CompatVBox->AddChildToVerticalBox(BlueTeamSlots[i].PortraitBorder);
		CompatVBox->AddChildToVerticalBox(BlueTeamSlots[i].PortraitImage);
		CompatVBox->AddChildToVerticalBox(BlueTeamSlots[i].PlayerNameText);
		CompatVBox->AddChildToVerticalBox(BlueTeamSlots[i].HeroStatusText);

		RedTeamSlots[i].OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("RedTeamSlotOuter_%d"), i));
		RedTeamSlots[i].PortraitBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("RedTeamSlotPortraitBorder_%d"), i));
		RedTeamSlots[i].PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("RedTeamSlotPortraitImg_%d"), i));
		RedTeamSlots[i].PlayerNameText = MakeTextWithFont(*FString::Printf(TEXT("RedTeamSlotPlayerName_%d"), i), FText::GetEmpty(), NeuzeitBoldFont, 9, FLinearColor::White);
		RedTeamSlots[i].HeroStatusText = MakeTextWithFont(*FString::Printf(TEXT("RedTeamSlotHeroStatus_%d"), i), FText::GetEmpty(), NeuzeitBoldFont, 9, FLinearColor::White);
		CompatVBox->AddChildToVerticalBox(RedTeamSlots[i].OuterBorder);
		CompatVBox->AddChildToVerticalBox(RedTeamSlots[i].PortraitBorder);
		CompatVBox->AddChildToVerticalBox(RedTeamSlots[i].PortraitImage);
		CompatVBox->AddChildToVerticalBox(RedTeamSlots[i].PlayerNameText);
		CompatVBox->AddChildToVerticalBox(RedTeamSlots[i].HeroStatusText);
	}
}

TSharedRef<SWidget> UBwayHeroSelectWidget::RebuildWidget()
{
	BuildOrBindWidgetHierarchy();
	return Super::RebuildWidget();
}

void UBwayHeroSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildOrBindWidgetHierarchy();

	if (!SelectionTimerWidget)
	{
		SelectionTimerWidget = GetWidgetFromName(TEXT("Widget_Timer"));
	}
	if (!TravelConfirmButton)
	{
		TravelConfirmButton = Cast<UCommonButtonBase>(GetWidgetFromName(TEXT("Button_Lock")));
	}

	if (LockInButton)
	{
		LockInButton->OnClicked().RemoveAll(this);
		LockInButton->OnClicked().AddUObject(this, &ThisClass::HandleLockInButtonClicked);
	}

	if (AbilityBarButtons[0]) { AbilityBarButtons[0]->OnClicked.RemoveAll(this); AbilityBarButtons[0]->OnClicked.AddDynamic(this, &ThisClass::HandleAbilitySlot0Clicked); }
	if (AbilityBarButtons[1]) { AbilityBarButtons[1]->OnClicked.RemoveAll(this); AbilityBarButtons[1]->OnClicked.AddDynamic(this, &ThisClass::HandleAbilitySlot1Clicked); }
	if (AbilityBarButtons[2]) { AbilityBarButtons[2]->OnClicked.RemoveAll(this); AbilityBarButtons[2]->OnClicked.AddDynamic(this, &ThisClass::HandleAbilitySlot2Clicked); }
	if (AbilityBarButtons[3]) { AbilityBarButtons[3]->OnClicked.RemoveAll(this); AbilityBarButtons[3]->OnClicked.AddDynamic(this, &ThisClass::HandleAbilitySlot3Clicked); }
	if (AbilityBarButtons[4]) { AbilityBarButtons[4]->OnClicked.RemoveAll(this); AbilityBarButtons[4]->OnClicked.AddDynamic(this, &ThisClass::HandleAbilitySlot4Clicked); }
	if (AbilityBarButtons[5]) { AbilityBarButtons[5]->OnClicked.RemoveAll(this); AbilityBarButtons[5]->OnClicked.AddDynamic(this, &ThisClass::HandleAbilitySlot5Clicked); }

	if (AbilityCollapseButton)
	{
		AbilityCollapseButton->OnClicked.RemoveAll(this);
		AbilityCollapseButton->OnClicked.AddDynamic(this, &ThisClass::HandleAbilityCollapseClicked);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked.RemoveAll(this);
		LeaveButton->OnClicked.AddDynamic(this, &ThisClass::HandleLeaveButtonClicked);
	}
	if (BuyHeroButton)
	{
		BuyHeroButton->OnClicked.RemoveAll(this);
		BuyHeroButton->OnClicked.AddDynamic(this, &ThisClass::HandleBuyHeroButtonClicked);
	}
	if (ProgressionInfoButton)
	{
		ProgressionInfoButton->OnClicked.RemoveAll(this);
		ProgressionInfoButton->OnClicked.AddDynamic(this, &ThisClass::HandleProgressionInfoClicked);
	}
	if (BottomSettingsButton)
	{
		BottomSettingsButton->OnClicked.RemoveAll(this);
		BottomSettingsButton->OnClicked.AddDynamic(this, &ThisClass::HandleBottomSettingsClicked);
	}

	for (UBwayHeroSlotWidget* SlotWidget : HeroSlotWidgets)
	{
		if (!SlotWidget)
		{
			continue;
		}
		SlotWidget->OnHeroSlotClicked.RemoveDynamic(this, &ThisClass::HandleHeroSlotClicked);
		SlotWidget->OnHeroSlotClicked.AddDynamic(this, &ThisClass::HandleHeroSlotClicked);
		SlotWidget->OnHeroSlotFocusedOrHovered.RemoveDynamic(this, &ThisClass::HandleHeroSlotFocusedOrHovered);
		SlotWidget->OnHeroSlotFocusedOrHovered.AddDynamic(this, &ThisClass::HandleHeroSlotFocusedOrHovered);
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		LocalPlayerState = PC->GetPlayerState<ABwayPlayerState>();
	}

	if (UWorld* World = GetWorld())
	{
		HeroRegistry = UBwayHeroRegistry::Get(World);
		if (AGameStateBase* GameState = World->GetGameState())
		{
			SelectionManager = GameState->FindComponentByClass<UBwayHeroSelectionManager>();
		}
	}

	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		FrontendSubsystem = LP->GetSubsystem<UBwayFrontendStateSubsystem>();
	}

	RefreshHeroDetails();
	RefreshGoal5HeroSelectLayout();
}

void UBwayHeroSelectWidget::NativeDestruct()
{
	CleanupPreviewActor();
	UnbindInputMethodListener();
	UnbindFromPlayerState();
	UnbindFromSelectionManager();
	UnbindFromSelectionPhase();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	Super::NativeDestruct();
}

void UBwayHeroSelectWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UWorld* World = GetWorld(); World && !HeroRegistry)
	{
		HeroRegistry = UBwayHeroRegistry::Get(World);
	}

	RefreshSelectionManager();
	RefreshLocalPlayerState();
	BindToPlayerState();
	BindToSelectionManager();
	BindInputMethodListener();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SelectionTimerHandle,
			this,
			&UBwayHeroSelectWidget::UpdateSelectionTimer,
			0.1f,
			true);
	}

	// IMPORTANT: Never call SelectHero() automatically on activation — players start with an
	// empty authoritative selection until they explicitly pick, preserving EmptyLockRejected
	// and duplicate-selection invariants. Instead, locally inspect the selected or first hero.
	const FPrimaryAssetId ExistingSelection = GetSelectedHeroId();
	if (ExistingSelection.IsValid())
	{
		InspectedHeroId = ExistingSelection;
		OnLocalSelectionChanged(ExistingSelection);
		EnsureAndUpdatePreviewActor(ExistingSelection);
	}
	else
	{
		const TArray<FHeroDisplayInfo> Available = GetAvailableHeroes();
		for (const FHeroDisplayInfo& Info : Available)
		{
			if (Info.HeroId.IsValid())
			{
				InspectedHeroId = Info.HeroId;
				EnsureAndUpdatePreviewActor(Info.HeroId);
				break;
			}
		}
	}

	bCachedLockState = IsSelectionLocked();
	if (bCachedLockState)
	{
		OnLocalSelectionLocked(ExistingSelection);
	}
	else
	{
		OnLocalSelectionUnlocked();
	}

	OnHeroListChanged();
	RefreshHeroDetails();
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();

	bIsInitialized = true;
	RequestRefreshFocus();
}

void UBwayHeroSelectWidget::NativeOnDeactivated()
{
	CleanupPreviewActor();
	UnbindInputMethodListener();
	UnbindFromPlayerState();
	UnbindFromSelectionManager();
	UnbindFromSelectionPhase();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	bIsInitialized = false;
	Super::NativeOnDeactivated();
}

UWidget* UBwayHeroSelectWidget::NativeGetDesiredFocusTarget() const
{
	if (bTravelRetryAvailable && TravelConfirmButton && TravelConfirmButton->GetIsEnabled())
	{
		return TravelConfirmButton;
	}

	if (!IsSelectionLocked())
	{
		const FPrimaryAssetId TargetId = GetSelectedHeroId().IsValid() ? GetSelectedHeroId() : InspectedHeroId;
		for (UBwayHeroSlotWidget* SlotWidget : HeroSlotWidgets)
		{
			if (SlotWidget && SlotWidget->GetVisibility() == ESlateVisibility::Visible && SlotWidget->GetIsEnabled()
				&& SlotWidget->GetHeroId() == TargetId)
			{
				return SlotWidget;
			}
		}
		for (UBwayHeroSlotWidget* SlotWidget : HeroSlotWidgets)
		{
			if (SlotWidget && SlotWidget->GetVisibility() == ESlateVisibility::Visible && SlotWidget->GetIsEnabled())
			{
				return SlotWidget;
			}
		}
	}

	if (TravelConfirmButton && TravelConfirmButton->GetIsEnabled())
	{
		return TravelConfirmButton;
	}

	return Super::NativeGetDesiredFocusTarget();
}

// ========== PUBLIC API ==========

TArray<FHeroDisplayInfo> UBwayHeroSelectWidget::GetAvailableHeroes()
{
	TArray<FHeroDisplayInfo> DisplayInfos;

	if (!HeroRegistry && GetWorld())
	{
		HeroRegistry = UBwayHeroRegistry::Get(GetWorld());
	}
	if (!HeroRegistry)
	{
		return DisplayInfos;
	}

	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> HeroSoftRefs = HeroRegistry->GetAllHeroSoftObjects();

	const int32 LocalTeam = GetLocalPlayerTeam();
	const FPrimaryAssetId CurrentSelection = GetSelectedHeroId();
	const ULocalPlayer* OwningLP = GetOwningLocalPlayer();
	const TArray<FPlayerHeroSelectionState> PlayerSelections =
		SelectionManager ? SelectionManager->GetAllPlayerSelections() : TArray<FPlayerHeroSelectionState>();

	for (const TSoftObjectPtr<UBwayHeroDataAsset>& SoftRef : HeroSoftRefs)
	{
		UBwayHeroDataAsset* HeroData = HeroRegistry->LoadHeroSync(SoftRef);
		if (!HeroData)
		{
			continue;
		}

		FHeroDisplayInfo DisplayInfo;
		DisplayInfo.HeroId = HeroData->GetPrimaryAssetId();
		DisplayInfo.DisplayName = HeroData->DisplayName;
		DisplayInfo.Portrait = HeroData->Portrait;
		DisplayInfo.Stats = HeroData->HeroStats;
		DisplayInfo.HeroClass = HeroData->HeroClass;
		DisplayInfo.ClassName = HeroData->GetClassDisplayName();
		DisplayInfo.Abilities = UBwayHeroAbilityUILibrary::ResolveAbilityBarForHero(HeroData);

		if (OwningLP)
		{
			for (FAbilityDisplayInfo& AbilityInfo : DisplayInfo.Abilities)
			{
				if (AbilityInfo.InputActionTag.IsValid())
				{
					AbilityInfo.InputKeyLabel = UBwayHeroAbilityUILibrary::ResolveKeyLabelForInputTag(AbilityInfo.InputActionTag, OwningLP);
				}
			}
		}

		if (SelectionManager)
		{
			DisplayInfo.bIsAvailable = (DisplayInfo.HeroId == CurrentSelection) ||
				SelectionManager->IsHeroAvailableForTeam(DisplayInfo.HeroId, LocalTeam);
		}
		else
		{
			DisplayInfo.bIsAvailable = true;
		}

		DisplayInfo.bIsSelected = (DisplayInfo.HeroId == CurrentSelection);
		DisplayInfo.bIsLocked = IsSelectionLocked() && DisplayInfo.bIsSelected;

		DisplayInfo.SelectedByPlayerIndex = -1;
		for (int32 SelectionIndex = 0; SelectionIndex < PlayerSelections.Num(); ++SelectionIndex)
		{
			const FPlayerHeroSelectionState& Selection = PlayerSelections[SelectionIndex];
			if (Selection.SelectedHeroId == DisplayInfo.HeroId && (LocalTeam < 0 || Selection.TeamIndex == LocalTeam))
			{
				if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
				{
					DisplayInfo.SelectedByPlayerIndex = FMath::Max(0, BwayPS->GetPlayerNum());
				}
				else
				{
					DisplayInfo.SelectedByPlayerIndex = SelectionIndex;
				}
				break;
			}
		}

		DisplayInfos.Add(MoveTemp(DisplayInfo));
	}

	DisplayInfos.Sort([](const FHeroDisplayInfo& A, const FHeroDisplayInfo& B)
	{
		const int32 OrderA = GetCanonicalGoal5HeroSortOrder(A.HeroId, A.HeroClass);
		const int32 OrderB = GetCanonicalGoal5HeroSortOrder(B.HeroId, B.HeroClass);
		if (OrderA != OrderB)
		{
			return OrderA < OrderB;
		}
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});

	TMap<uint8, int32> ClassCounts;
	for (FHeroDisplayInfo& Info : DisplayInfos)
	{
		int32& Count = ClassCounts.FindOrAdd(static_cast<uint8>(Info.HeroClass));
		Info.HeroClassIndex = Count++;
	}

	return DisplayInfos;
}

FPrimaryAssetId UBwayHeroSelectWidget::GetSelectedHeroId() const
{
	if (LocalPlayerState)
	{
		return LocalPlayerState->GetSelectedHeroId();
	}
	if (FrontendSubsystem)
	{
		return FrontendSubsystem->GetSelectedHeroId();
	}
	return FPrimaryAssetId();
}

FPrimaryAssetId UBwayHeroSelectWidget::GetInspectedHeroId() const
{
	if (InspectedHeroId.IsValid())
	{
		return InspectedHeroId;
	}
	return GetSelectedHeroId();
}

void UBwayHeroSelectWidget::SetInspectedHero(FPrimaryAssetId HeroId)
{
	if (!HeroId.IsValid())
	{
		return;
	}
	const bool bChanged = (InspectedHeroId != HeroId);
	InspectedHeroId = HeroId;
	if (bChanged)
	{
		EnsureAndUpdatePreviewActor(HeroId);
	}
	RefreshHeroDetails();
	RefreshHeroRosterGrid();
	RefreshTeamRosterSidebars();
	if (bShowingTravelFailure)
	{
		RefreshTravelFailurePresentation();
	}
}

bool UBwayHeroSelectWidget::IsSelectionLocked() const
{
	if (LocalPlayerState)
	{
		return LocalPlayerState->IsHeroLocked();
	}
	return false;
}

bool UBwayHeroSelectWidget::SelectHero(FPrimaryAssetId HeroId)
{
	RefreshSelectionManager();
	RefreshLocalPlayerState();
	if ((!LocalPlayerState || !SelectionManager) && GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		return false;
	}
	if (IsSelectionLocked())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Cannot select hero - selection is locked"));
		return false;
	}
	if (!IsHeroAvailable(HeroId))
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Hero %s is not available"), *HeroId.ToString());
		return false;
	}

	InspectedHeroId = HeroId;

	if (LocalPlayerState)
	{
		if (FrontendSubsystem)
		{
			FrontendSubsystem->SetSelectedHeroId(HeroId);
		}

		LocalPlayerState->ServerSetSelectedHeroId(HeroId);
		EnsureAndUpdatePreviewActor(HeroId);
		RefreshHeroDetails();
		RefreshGoal5HeroSelectLayout();
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Selected hero %s (Server Route)"), *HeroId.ToString());
	}
	else if (FrontendSubsystem)
	{
		FrontendSubsystem->SetSelectedHeroId(HeroId);
		OnLocalSelectionChanged(HeroId);
		EnsureAndUpdatePreviewActor(HeroId);
		RefreshHeroDetails();
		OnHeroListChanged();
		RefreshGoal5HeroSelectLayout();
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Selected hero %s (Frontend Route)"), *HeroId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: Cannot select hero - neither player state nor frontend subsystem found"));
		return false;
	}

	return true;
}

bool UBwayHeroSelectWidget::LockSelection()
{
	RefreshSelectionManager();
	RefreshLocalPlayerState();
	if (SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed() && IsSelectionLocked())
	{
		if (auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer()))
		{
			Controller->RetryHeroSelectionTravel();
		}
		RefreshTravelFailurePresentation();
		return false;
	}
	if ((!LocalPlayerState || !SelectionManager) && GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		return false;
	}
	if (!LocalPlayerState)
	{
		if (FrontendSubsystem)
		{
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Locked selection (Frontend Route)"));
			return true;
		}
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: Cannot lock selection - no player state"));
		return false;
	}

	if (IsSelectionLocked())
	{
		return true;
	}

	const FPrimaryAssetId SelectedHero = GetSelectedHeroId();
	if (!SelectedHero.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Cannot lock - no hero selected"));
		return false;
	}

	LocalPlayerState->ServerLockHeroSelection();

	if (UBwayHeroDataAsset* HeroData = GetHeroDataAsset(SelectedHero))
	{
		if (HeroData->VoiceBank)
		{
			UGameplayStatics::PlaySound2D(this, HeroData->VoiceBank);
		}
	}
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Locked selection"));
	return true;
}

void UBwayHeroSelectWidget::ConfirmSelection()
{
	RefreshSelectionManager();
	RefreshLocalPlayerState();
	if (SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed() && IsSelectionLocked())
	{
		if (auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer()))
		{
			Controller->RetryHeroSelectionTravel();
		}
		RefreshTravelFailurePresentation();
		return;
	}

	FPrimaryAssetId SelectedHero = GetSelectedHeroId();
	if (!SelectedHero.IsValid() && InspectedHeroId.IsValid() && IsHeroAvailable(InspectedHeroId))
	{
		SelectHero(InspectedHeroId);
		SelectedHero = GetSelectedHeroId();
	}
	if (!SelectedHero.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Cannot confirm - no hero selected"));
		return;
	}

	const bool bAwaitPhaseAdvance = SelectionManager && (SelectionManager->IsSelectionActive()
		|| (LocalPlayerState && GetOwningPlayer() && !GetOwningPlayer()->GetPawn()));

	if (!IsSelectionLocked())
	{
		if (!LockSelection())
		{
			return;
		}
	}

	if (bAwaitPhaseAdvance)
	{
		return;
	}

	OnHeroConfirmed.Broadcast();
	DeactivateWidget();
}

void UBwayHeroSelectWidget::RefreshSelectionLockPresentation()
{
	const bool bNowLocked = IsSelectionLocked();
	if (bNowLocked != bCachedLockState)
	{
		bCachedLockState = bNowLocked;
		if (bNowLocked)
		{
			OnLocalSelectionLocked(GetSelectedHeroId());
		}
		else
		{
			OnLocalSelectionUnlocked();
		}
		RefreshGoal5HeroSelectLayout();
		RefreshTravelFailurePresentation();
		RequestRefreshFocus();
	}
}

int32 UBwayHeroSelectWidget::GetLocalPlayerTeam() const
{
	if (LocalPlayerState)
	{
		if (const ABwayGameState* BwayGS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
		{
			return BwayGS->GetPlayerTeam(LocalPlayerState);
		}
	}
	return -1;
}

bool UBwayHeroSelectWidget::IsHeroAvailable(FPrimaryAssetId HeroId) const
{
	if (!HeroId.IsValid())
	{
		return false;
	}
	if (HeroId == GetSelectedHeroId())
	{
		return true;
	}
	if (!SelectionManager)
	{
		return true;
	}
	return SelectionManager->IsHeroAvailableForTeam(HeroId, GetLocalPlayerTeam());
}

float UBwayHeroSelectWidget::GetRemainingSelectionTime() const
{
	if (!SelectionManager || !SelectionManager->IsSelectionActive() || SelectionManager->SelectionTimeLimit <= 0.0f)
	{
		return -1.0f;
	}
	return SelectionManager->GetSelectionTimeRemaining();
}

void UBwayHeroSelectWidget::GetReadyPlayerCount(int32& OutReady, int32& OutTotal) const
{
	if (SelectionManager)
	{
		OutReady = SelectionManager->GetNumPlayersReady();
		OutTotal = SelectionManager->GetTotalPlayers();
	}
	else
	{
		OutReady = 0;
		OutTotal = 0;
	}
}

UBwayHeroDataAsset* UBwayHeroSelectWidget::GetHeroDataAsset(FPrimaryAssetId HeroId) const
{
	if (!HeroId.IsValid())
	{
		return nullptr;
	}
	return UBwayHeroRegistry::GetHeroDataById(HeroId);
}

bool UBwayHeroSelectWidget::GetHeroDisplayInfo(FPrimaryAssetId HeroId, FHeroDisplayInfo& OutDisplayInfo) const
{
	UBwayHeroDataAsset* HeroData = GetHeroDataAsset(HeroId);
	if (!HeroData)
	{
		return false;
	}

	OutDisplayInfo.HeroId = HeroData->GetPrimaryAssetId();
	OutDisplayInfo.DisplayName = HeroData->DisplayName;
	OutDisplayInfo.Portrait = HeroData->Portrait;
	OutDisplayInfo.Stats = HeroData->HeroStats;
	OutDisplayInfo.HeroClass = HeroData->HeroClass;
	OutDisplayInfo.ClassName = HeroData->GetClassDisplayName();
	OutDisplayInfo.Abilities = UBwayHeroAbilityUILibrary::ResolveAbilityBarForHero(HeroData);
	OutDisplayInfo.HeroClassIndex = 0;

	if (const ULocalPlayer* OwningLP = GetOwningLocalPlayer())
	{
		for (FAbilityDisplayInfo& AbilityInfo : OutDisplayInfo.Abilities)
		{
			if (AbilityInfo.InputActionTag.IsValid())
			{
				AbilityInfo.InputKeyLabel = UBwayHeroAbilityUILibrary::ResolveKeyLabelForInputTag(AbilityInfo.InputActionTag, OwningLP);
			}
		}
	}

	const int32 LocalTeam = GetLocalPlayerTeam();
	const FPrimaryAssetId CurrentSelection = GetSelectedHeroId();

	if (SelectionManager)
	{
		OutDisplayInfo.bIsAvailable = (OutDisplayInfo.HeroId == CurrentSelection) ||
			SelectionManager->IsHeroAvailableForTeam(OutDisplayInfo.HeroId, LocalTeam);
	}
	else
	{
		OutDisplayInfo.bIsAvailable = true;
	}

	OutDisplayInfo.bIsSelected = (OutDisplayInfo.HeroId == CurrentSelection);
	OutDisplayInfo.bIsLocked = IsSelectionLocked() && OutDisplayInfo.bIsSelected;
	OutDisplayInfo.SelectedByPlayerIndex = -1;

	if (SelectionManager)
	{
		const TArray<FPlayerHeroSelectionState> PlayerSelections = SelectionManager->GetAllPlayerSelections();
		for (int32 SelectionIndex = 0; SelectionIndex < PlayerSelections.Num(); ++SelectionIndex)
		{
			const FPlayerHeroSelectionState& Selection = PlayerSelections[SelectionIndex];
			if (Selection.SelectedHeroId == OutDisplayInfo.HeroId && (LocalTeam < 0 || Selection.TeamIndex == LocalTeam))
			{
				if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
				{
					OutDisplayInfo.SelectedByPlayerIndex = FMath::Max(0, BwayPS->GetPlayerNum());
				}
				else
				{
					OutDisplayInfo.SelectedByPlayerIndex = SelectionIndex;
				}
				break;
			}
		}
	}

	return true;
}

bool UBwayHeroSelectWidget::GetSelectedHeroDisplayInfo(FHeroDisplayInfo& OutDisplayInfo) const
{
	const FPrimaryAssetId SelectedId = GetSelectedHeroId();
	return SelectedId.IsValid() && GetHeroDisplayInfo(SelectedId, OutDisplayInfo);
}

bool UBwayHeroSelectWidget::GetInspectedHeroDisplayInfo(FHeroDisplayInfo& OutDisplayInfo) const
{
	const FPrimaryAssetId TargetId = GetInspectedHeroId();
	return TargetId.IsValid() && GetHeroDisplayInfo(TargetId, OutDisplayInfo);
}

TArray<FBwayTeamRosterSlotViewModel> UBwayHeroSelectWidget::GetTeamRosterSlotViewModels(int32 TeamIndex) const
{
	TArray<FBwayTeamRosterSlotViewModel> Slots;
	Slots.Reserve(4);

	const ABwayGameState* BwayGS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;

	// 1. Collect human & active PlayerState entries assigned to TeamIndex
	if (SelectionManager)
	{
		const TArray<FPlayerHeroSelectionState> AllPlayers = SelectionManager->GetAllPlayerSelections();
		for (const FPlayerHeroSelectionState& Entry : AllPlayers)
		{
			if (!Entry.PlayerState || Entry.PlayerState->IsInactive() || Entry.PlayerState->IsOnlyASpectator())
			{
				continue;
			}

			int32 ResolvedTeam = Entry.TeamIndex;
			if (ResolvedTeam < 0 && BwayGS)
			{
				ResolvedTeam = BwayGS->GetPlayerTeam(Entry.PlayerState);
			}
			if (ResolvedTeam != TeamIndex || Slots.Num() >= 4)
			{
				continue;
			}

			FBwayTeamRosterSlotViewModel VM;
			VM.TeamIndex = TeamIndex;
			VM.SlotIndex = Slots.Num();
			VM.bIsOccupied = true;
			VM.bIsBot = Entry.PlayerState->IsABot();
			VM.bIsLocalPlayer = (LocalPlayerState && Entry.PlayerState == LocalPlayerState);
			VM.bIsLocked = Entry.bIsLocked;

			FString BaseName = Entry.PlayerState->GetPlayerName();
			if (BaseName.TrimStartAndEnd().IsEmpty())
			{
				BaseName = FString::Printf(TEXT("Player %d"), Entry.PlayerState->GetPlayerId());
			}
			VM.PlayerName = VM.bIsLocalPlayer ? FString::Printf(TEXT("%s (YOU)"), *BaseName) : BaseName;

			FPrimaryAssetId EffectiveHero = Entry.SelectedHeroId;
			bool bHoverOnly = false;
			if (!EffectiveHero.IsValid() && VM.bIsLocalPlayer && InspectedHeroId.IsValid())
			{
				EffectiveHero = InspectedHeroId;
				bHoverOnly = true;
			}

			VM.HeroId = EffectiveHero;
			if (UBwayHeroDataAsset* HeroData = GetHeroDataAsset(EffectiveHero))
			{
				VM.HeroDisplayName = HeroData->DisplayName;
				VM.Portrait = HeroData->Portrait;
				if (Entry.bIsLocked)
				{
					VM.StatusText = FText::FromString(FString::Printf(TEXT("%s • LOCKED"), *HeroData->DisplayName.ToString()));
				}
				else if (bHoverOnly)
				{
					VM.StatusText = FText::FromString(FString::Printf(TEXT("%s • HOVERING"), *HeroData->DisplayName.ToString()));
				}
				else
				{
					VM.StatusText = FText::FromString(FString::Printf(TEXT("%s • SELECTING"), *HeroData->DisplayName.ToString()));
				}
			}
			else
			{
				VM.StatusText = NSLOCTEXT("BwayHeroSelect", "StatusSelecting", "SELECTING...");
			}

			Slots.Add(MoveTemp(VM));
		}
	}

	// 2. Append pre-assigned staging bot entries for TeamIndex (live bot portraits carrying into Dorado)
	if (SelectionManager && Slots.Num() < 4)
	{
		TArray<FBwayStagingBotSelection> TeamBots;
		for (const FBwayStagingBotSelection& BotSel : SelectionManager->GetStagingBotSelections())
		{
			if (BotSel.TeamIndex == TeamIndex)
			{
				TeamBots.Add(BotSel);
			}
		}
		TeamBots.Sort([](const FBwayStagingBotSelection& A, const FBwayStagingBotSelection& B)
		{
			return A.BotSlotIndex < B.BotSlotIndex;
		});

		for (const FBwayStagingBotSelection& BotSel : TeamBots)
		{
			if (Slots.Num() >= 4)
			{
				break;
			}

			FBwayTeamRosterSlotViewModel VM;
			VM.TeamIndex = TeamIndex;
			VM.SlotIndex = Slots.Num();
			VM.bIsOccupied = true;
			VM.bIsBot = true;
			VM.bIsLocked = BotSel.bIsLocked;
			VM.PlayerName = BotSel.BotDisplayName.IsEmpty()
				? FString::Printf(TEXT("Bot %d"), VM.SlotIndex + 1)
				: BotSel.BotDisplayName;
			VM.HeroId = BotSel.SelectedHeroId;

			if (UBwayHeroDataAsset* HeroData = GetHeroDataAsset(BotSel.SelectedHeroId))
			{
				VM.HeroDisplayName = HeroData->DisplayName;
				VM.Portrait = HeroData->Portrait;
				VM.StatusText = FText::FromString(FString::Printf(TEXT("%s • BOT READY"), *HeroData->DisplayName.ToString()));
			}
			else
			{
				VM.StatusText = NSLOCTEXT("BwayHeroSelect", "BotPending", "BOT READY");
			}

			Slots.Add(MoveTemp(VM));
		}
	}

	// 3. Pad up to 4 slots per team
	while (Slots.Num() < 4)
	{
		FBwayTeamRosterSlotViewModel EmptyVM;
		EmptyVM.TeamIndex = TeamIndex;
		EmptyVM.SlotIndex = Slots.Num();
		EmptyVM.bIsOccupied = false;
		EmptyVM.PlayerName = FString::Printf(TEXT("%s Slot %d"), TeamIndex == 0 ? TEXT("Blue") : TEXT("Red"), EmptyVM.SlotIndex + 1);
		EmptyVM.StatusText = NSLOCTEXT("BwayHeroSelect", "OpenTeamSlot", "OPEN SLOT");
		Slots.Add(MoveTemp(EmptyVM));
	}

	return Slots;
}

void UBwayHeroSelectWidget::ToggleAbilitiesPanel()
{
	bAbilitiesPanelVisible = !bAbilitiesPanelVisible;
	if (InspectionPanelBorder)
	{
		InspectionPanelBorder->SetVisibility(
			bAbilitiesPanelVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshBreakawayAbilityPreviewPanel();
}

void UBwayHeroSelectWidget::SelectAbilityPreviewSlot(int32 SlotIndex)
{
	SelectedAbilityPreviewSlot = FMath::Clamp(SlotIndex, 0, 5);
	bAbilitiesPanelVisible = true;
	RefreshBreakawayAbilityPreviewPanel();
}

FText UBwayHeroSelectWidget::GetSelectedAbilityTitleText() const
{
	return SelectedAbilityTitleText ? SelectedAbilityTitleText->GetText() : FText::GetEmpty();
}

FText UBwayHeroSelectWidget::GetSelectedAbilityDescriptionText() const
{
	return SelectedAbilityDescText ? SelectedAbilityDescText->GetText() : FText::GetEmpty();
}

void UBwayHeroSelectWidget::ClickLeaveButton()
{
	HandleLeaveButtonClicked();
}

bool UBwayHeroSelectWidget::SelectHeroByIndex(int32 Index)
{
	const TArray<FHeroDisplayInfo> Heroes = GetAvailableHeroes();
	if (!Heroes.IsValidIndex(Index))
	{
		return false;
	}
	return SelectHero(Heroes[Index].HeroId);
}

FText UBwayHeroSelectWidget::GetReadyHeaderText() const
{
	return ReadyCountText ? ReadyCountText->GetText() : FText::GetEmpty();
}

FText UBwayHeroSelectWidget::GetCountdownHeaderText() const
{
	return CountdownTimerText ? CountdownTimerText->GetText() : FText::GetEmpty();
}

FText UBwayHeroSelectWidget::GetSharedMovementDisplayText() const
{
	return SharedMovementText ? SharedMovementText->GetText() : FText::GetEmpty();
}

FText UBwayHeroSelectWidget::GetInspectedStatsDisplayText() const
{
	return InspectedStatsText ? InspectedStatsText->GetText() : FText::GetEmpty();
}

// ========== BINDING & INPUT DEVICE UPDATES ==========

void UBwayHeroSelectWidget::BindInputMethodListener()
{
	UnbindInputMethodListener();
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(LP))
		{
			InputMethodChangedHandle = InputSubsystem->OnInputMethodChangedNative.AddUObject(
				this,
				&ThisClass::HandleInputMethodChanged);
		}
	}
}

void UBwayHeroSelectWidget::UnbindInputMethodListener()
{
	if (InputMethodChangedHandle.IsValid())
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(LP))
			{
				InputSubsystem->OnInputMethodChangedNative.Remove(InputMethodChangedHandle);
			}
		}
		InputMethodChangedHandle.Reset();
	}
}

void UBwayHeroSelectWidget::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	RefreshHeroDetails();
	if (bShowingTravelFailure)
	{
		RefreshTravelFailurePresentation();
	}
	if (NewInputType == ECommonInputType::Gamepad && !HasUserFocusedDescendants(GetOwningPlayer()))
	{
		RequestRefreshFocus();
	}
}

void UBwayHeroSelectWidget::RefreshSelectionManager()
{
	RefreshSelectionPhase();
	AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	UBwayHeroSelectionManager* CurrentManager = GS ? GS->FindComponentByClass<UBwayHeroSelectionManager>() : nullptr;
	if (SelectionManager == CurrentManager)
	{
		return;
	}
	UnbindFromSelectionManager();
	SelectionManager = CurrentManager;
	BindToSelectionManager();
	OnHeroListChanged();
	RefreshGoal5HeroSelectLayout();
	RequestRefreshFocus();
}

void UBwayHeroSelectWidget::RefreshSelectionPhase()
{
	const auto* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	auto* CurrentPhase = GS ? GS->HeroSelectionPhaseComponent.Get() : nullptr;
	if (SelectionPhase == CurrentPhase)
	{
		return;
	}
	UnbindFromSelectionPhase();
	SelectionPhase = CurrentPhase;
	if (SelectionPhase)
	{
		SelectionPhase->OnTravelFailureChanged.AddUniqueDynamic(this, &ThisClass::RefreshTravelFailurePresentation);
	}
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::UnbindFromSelectionPhase()
{
	if (SelectionPhase)
	{
		SelectionPhase->OnTravelFailureChanged.RemoveDynamic(this, &ThisClass::RefreshTravelFailurePresentation);
	}
	SelectionPhase = nullptr;
}

void UBwayHeroSelectWidget::RefreshTravelFailurePresentation()
{
	const bool bFailed = SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed();
	if (!bFailed)
	{
		if (bShowingTravelFailure)
		{
			bShowingTravelFailure = false;
			bTravelRetryAvailable = false;
			if (HeroDetailsBannerBorder)
			{
				HeroDetailsBannerBorder->SetBrushColor(FLinearColor(0.05f, 0.10f, 0.18f, 0.94f));
			}
			if (Text_HeroDetails)
			{
				Text_HeroDetails->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.90f, 0.98f, 1.0f)));
			}
			if (IsSelectionLocked())
			{
				OnLocalSelectionLocked(GetSelectedHeroId());
			}
			else
			{
				OnLocalSelectionUnlocked();
			}
			RefreshHeroDetails();
			RefreshLockButtonPresentation();
			ConfigureExplicitNavigationRules();
		}
		return;
	}

	const auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer());
	const bool bHost = Controller && Controller->HasAuthority() && Controller->IsLocalController() && Controller->GetLocalPlayer();
	const bool bCanRetry = Controller && Controller->CanRetryHeroSelectionTravel();

	if (IsSelectionLocked())
	{
		const FText RetryLabel = bHost
			? NSLOCTEXT("BwayHeroSelect", "RetryTravel", "Retry travel")
			: NSLOCTEXT("BwayHeroSelect", "WaitForHostRetry", "Waiting for host");

		if (LockInButton)
		{
			LockInButton->SetLabelText(RetryLabel);
			LockInButton->SetButtonColors(
				FLinearColor(0.68f, 0.28f, 0.12f, 0.98f),
				FLinearColor(0.88f, 0.38f, 0.14f, 1.00f),
				FLinearColor(0.98f, 0.84f, 0.30f, 1.00f),
				FLinearColor::White);
			LockInButton->SetIsEnabled(bCanRetry);
			LockInButton->SetIsInteractionEnabled(bCanRetry);
		}
		else if (TravelConfirmButton)
		{
			if (ULyraButtonBase* LyraBtn = Cast<ULyraButtonBase>(TravelConfirmButton))
			{
				LyraBtn->SetButtonText(RetryLabel);
			}
			TravelConfirmButton->SetIsEnabled(bCanRetry);
			TravelConfirmButton->SetIsInteractionEnabled(bCanRetry);
		}
	}

	if (HeroDetailsBannerBorder)
	{
		HeroDetailsBannerBorder->SetBrushColor(FLinearColor(0.32f, 0.12f, 0.08f, 0.97f));
	}
	if (Text_HeroDetails)
	{
		Text_HeroDetails->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.88f, 0.64f, 1.0f)));
		Text_HeroDetails->SetText(!IsSelectionLocked()
			? NSLOCTEXT("BwayHeroSelect", "TravelFailedSelect", "Match travel failed. Choose and lock your hero; the host can retry when everyone is ready.")
			: bCanRetry ? NSLOCTEXT("BwayHeroSelect", "TravelFailedRetry", "Match travel failed. Your selections are saved. Choose Retry travel to try again.")
			: bHost ? NSLOCTEXT("BwayHeroSelect", "TravelFailedPlayers", "Match travel failed. Waiting for all players to lock before you can retry.")
			: NSLOCTEXT("BwayHeroSelect", "TravelFailedWait", "Match travel failed. Your selection is saved. Waiting for the host to retry."));
	}
	if (LockHintText)
	{
		LockHintText->SetText(bHost
			? NSLOCTEXT("BwayHeroSelect", "TravelRetryHostHint", "Host recovery active")
			: NSLOCTEXT("BwayHeroSelect", "TravelRetryClientHint", "Selections preserved"));
	}

	const bool bFocusRetry = bCanRetry && !bTravelRetryAvailable;
	bShowingTravelFailure = true;
	bTravelRetryAvailable = bCanRetry;
	ConfigureExplicitNavigationRules();
	if (bFocusRetry)
	{
		RequestRefreshFocus();
	}
}

void UBwayHeroSelectWidget::RefreshLocalPlayerState()
{
	APlayerController* PC = GetOwningPlayer();
	ABwayPlayerState* CurrentState = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (CurrentState == LocalPlayerState)
	{
		RefreshSelectionLockPresentation();
		return;
	}

	UnbindFromPlayerState();
	LocalPlayerState = CurrentState;
	BindToPlayerState();

	if (LocalPlayerState)
	{
		const FPrimaryAssetId SelectedId = GetSelectedHeroId();
		if (SelectedId.IsValid())
		{
			InspectedHeroId = SelectedId;
			OnLocalSelectionChanged(SelectedId);
			EnsureAndUpdatePreviewActor(SelectedId);
		}
		bCachedLockState = IsSelectionLocked();
		if (bCachedLockState)
		{
			OnLocalSelectionLocked(SelectedId);
		}
		else
		{
			OnLocalSelectionUnlocked();
		}
		RefreshHeroDetails();
		OnHeroListChanged();
		RefreshGoal5HeroSelectLayout();
		RefreshTravelFailurePresentation();
		RequestRefreshFocus();
	}
}

void UBwayHeroSelectWidget::BindToPlayerState()
{
	if (!LocalPlayerState)
	{
		return;
	}
	LocalPlayerState->OnSelectedHeroChanged.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);
	LocalPlayerState->OnHeroLocked.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandlePlayerHeroLocked);
}

void UBwayHeroSelectWidget::UnbindFromPlayerState()
{
	if (!LocalPlayerState)
	{
		return;
	}
	LocalPlayerState->OnSelectedHeroChanged.RemoveDynamic(this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);
	LocalPlayerState->OnHeroLocked.RemoveDynamic(this, &UBwayHeroSelectWidget::HandlePlayerHeroLocked);
}

void UBwayHeroSelectWidget::BindToSelectionManager()
{
	if (!SelectionManager)
	{
		return;
	}
	SelectionManager->OnPlayerHeroSelectionChanged.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
	SelectionManager->OnPlayerHeroLocked.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerHeroLocked);
	SelectionManager->OnStagingBotSelectionsChanged.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandleStagingBotSelectionsChanged);
	SelectionManager->OnAllPlayersReady.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandleAllPlayersReady);
}

void UBwayHeroSelectWidget::UnbindFromSelectionManager()
{
	if (!SelectionManager)
	{
		return;
	}
	SelectionManager->OnPlayerHeroSelectionChanged.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
	SelectionManager->OnPlayerHeroLocked.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerHeroLocked);
	SelectionManager->OnStagingBotSelectionsChanged.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleStagingBotSelectionsChanged);
	SelectionManager->OnAllPlayersReady.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleAllPlayersReady);
}

// ========== DELEGATE HANDLERS ==========

void UBwayHeroSelectWidget::HandlePlayerSelectionChanged(FPrimaryAssetId NewHeroId)
{
	if (NewHeroId.IsValid())
	{
		InspectedHeroId = NewHeroId;
		EnsureAndUpdatePreviewActor(NewHeroId);
	}
	OnLocalSelectionChanged(NewHeroId);
	RefreshHeroDetails();
	OnHeroListChanged();
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandlePlayerHeroLocked(FPrimaryAssetId LockedHeroId)
{
	bCachedLockState = true;
	if (LockedHeroId.IsValid())
	{
		InspectedHeroId = LockedHeroId;
	}
	OnLocalSelectionLocked(LockedHeroId);
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
	RequestRefreshFocus();
}

void UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged(APlayerState* PlayerState, FPrimaryAssetId HeroId, int32 TeamIndex)
{
	if (TeamIndex == GetLocalPlayerTeam())
	{
		OnHeroListChanged();
	}
	OnOtherPlayerSelectionChanged(PlayerState, HeroId, TeamIndex);
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandleOtherPlayerHeroLocked(APlayerState* PlayerState, FPrimaryAssetId LockedHeroId)
{
	int32 PeerTeam = -1;
	if (const ABwayGameState* BwayGS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
	{
		PeerTeam = BwayGS->GetPlayerTeam(PlayerState);
	}
	OnOtherPlayerSelectionChanged(PlayerState, LockedHeroId, PeerTeam);
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandleStagingBotSelectionsChanged()
{
	RefreshTeamRosterSidebars();
}

void UBwayHeroSelectWidget::HandleAllPlayersReady()
{
	OnAllPlayersReady();
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandleHeroSlotClicked(FPrimaryAssetId HeroId)
{
	if (HeroId.IsValid())
	{
		SelectHero(HeroId);
	}
}

void UBwayHeroSelectWidget::HandleHeroSlotFocusedOrHovered(FPrimaryAssetId HeroId)
{
	if (HeroId.IsValid())
	{
		SetInspectedHero(HeroId);
	}
}

void UBwayHeroSelectWidget::HandleLockInButtonClicked()
{
	ConfirmSelection();
}

void UBwayHeroSelectWidget::HandleAbilitySlot0Clicked()
{
	SelectAbilityPreviewSlot(0);
}

void UBwayHeroSelectWidget::HandleAbilitySlot1Clicked()
{
	SelectAbilityPreviewSlot(1);
}

void UBwayHeroSelectWidget::HandleAbilitySlot2Clicked()
{
	SelectAbilityPreviewSlot(2);
}

void UBwayHeroSelectWidget::HandleAbilitySlot3Clicked()
{
	SelectAbilityPreviewSlot(3);
}

void UBwayHeroSelectWidget::HandleAbilitySlot4Clicked()
{
	SelectAbilityPreviewSlot(4);
}

void UBwayHeroSelectWidget::HandleAbilitySlot5Clicked()
{
	SelectAbilityPreviewSlot(5);
}

void UBwayHeroSelectWidget::HandleAbilityCollapseClicked()
{
	ToggleAbilitiesPanel();
}

void UBwayHeroSelectWidget::HandleLeaveButtonClicked()
{
	DeactivateWidget();
}

void UBwayHeroSelectWidget::HandleBuyHeroButtonClicked()
{
	const FPrimaryAssetId TargetId = GetInspectedHeroId();
	if (TargetId.IsValid() && !IsSelectionLocked())
	{
		SelectHero(TargetId);
	}
}

void UBwayHeroSelectWidget::HandleProgressionInfoClicked()
{
	ToggleAbilitiesPanel();
}

void UBwayHeroSelectWidget::HandleBottomSettingsClicked()
{
	ToggleAbilitiesPanel();
}

void UBwayHeroSelectWidget::UpdateSelectionTimer()
{
	RefreshSelectionManager();
	RefreshLocalPlayerState();

	ABwayGameState* State = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	UBwayRoundManagementComponent* Rounds = State ? State->GetRoundManagement() : nullptr;
	APlayerController* Controller = GetOwningPlayer();
	if (LocalPlayerState && LocalPlayerState->IsHeroLocked() && Controller && Controller->GetPawn() && Rounds
		&& (Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
			|| Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound))
	{
		if (ABwayPlayerController* BwayController = Cast<ABwayPlayerController>(Controller))
		{
			BwayController->Client_HideHeroSelection();
		}
		else
		{
			DeactivateWidget();
		}
		return;
	}

	const float RemainingTime = GetRemainingSelectionTime();
	OnSelectionTimerUpdated(RemainingTime);

	if (SelectionTimerWidget)
	{
		SelectionTimerWidget->SetVisibility(RemainingTime >= 0.0f
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (CountdownTimerText)
	{
		const int32 DisplaySecs = RemainingTime >= 0.0f ? FMath::CeilToInt(RemainingTime) : 0;
		CountdownTimerText->SetText(FText::Format(
			NSLOCTEXT("BwayHeroSelect", "AssembleTeamFmt", "ASSEMBLE TEAM   {0}"),
			FText::AsNumber(DisplaySecs)));
	}

	int32 ReadyCount = 0;
	int32 TotalCount = 0;
	GetReadyPlayerCount(ReadyCount, TotalCount);
	if (ReadyCountText)
	{
		ReadyCountText->SetText(FText::Format(
			NSLOCTEXT("BwayHeroSelect", "ReadyCounterFmt", "Ready ({0}/{1})"),
			FText::AsNumber(ReadyCount),
			FText::AsNumber(TotalCount)));
	}

	RefreshTeamRosterSidebars();
}

void UBwayHeroSelectWidget::RefreshHeroDetails()
{
	FHeroDisplayInfo Hero;
	const bool bHasHero = GetInspectedHeroDisplayInfo(Hero);

	if (SharedMovementText)
	{
		SharedMovementText->SetText(UBwayHeroAbilityUILibrary::ResolveSharedMovementSummaryText(GetOwningLocalPlayer()));
	}

	if (!bHasHero)
	{
		if (!bShowingTravelFailure && Text_HeroDetails)
		{
			Text_HeroDetails->SetText(NSLOCTEXT("BwayHeroSelect", "ChooseHero", "Choose a hero to view base stats, kit actions, shared movement, and signature buildable."));
		}
		for (UBwayAbilityPreviewWidget* PreviewWidget : AbilityPreviewWidgets)
		{
			if (PreviewWidget)
			{
				PreviewWidget->ClearPreview();
			}
		}
		RefreshBreakawayAbilityPreviewPanel();
		return;
	}

	const FLinearColor ClassAccent = GetClassBadgeColor(Hero.ClassName, Hero.HeroClass);

	if (InspectedHeroNameText)
	{
		InspectedHeroNameText->SetText(FText::FromString(Hero.DisplayName.ToString().ToUpper()));
	}
	if (InspectedHeroClassText)
	{
		InspectedHeroClassText->SetText(FText::FromString(Hero.ClassName.ToString().ToUpper()));
	}
	if (InspectedHeroClassBorder)
	{
		InspectedHeroClassBorder->SetBrushColor(FLinearColor(ClassAccent.R * 0.45f, ClassAccent.G * 0.45f, ClassAccent.B * 0.45f, 0.96f));
	}
	if (InspectedStatsText)
	{
		InspectedStatsText->SetText(FText::FromString(FString::Printf(
			TEXT("HP %d  |  ATK %d  |  ARMOR %d  |  SPEED %d"),
			FMath::RoundToInt(Hero.Stats.MaxHealth),
			FMath::RoundToInt(Hero.Stats.BaseDamage),
			FMath::RoundToInt(Hero.Stats.Armor),
			FMath::RoundToInt(Hero.Stats.MoveSpeed))));
	}

	FString BuildableName;
	for (const FAbilityDisplayInfo& AbilityInfo : Hero.Abilities)
	{
		if (AbilityInfo.bIsBuildable && !AbilityInfo.AbilityName.IsEmpty())
		{
			BuildableName = AbilityInfo.AbilityName.ToString();
			break;
		}
	}

	if (!bShowingTravelFailure && Text_HeroDetails)
	{
		const FString SummaryLine = BuildableName.IsEmpty()
			? FString::Printf(
				TEXT("%s (%s) — Health: %d  Attack: %d  Armor: %d  Speed: %d"),
				*Hero.DisplayName.ToString(),
				*Hero.ClassName.ToString(),
				FMath::RoundToInt(Hero.Stats.MaxHealth),
				FMath::RoundToInt(Hero.Stats.BaseDamage),
				FMath::RoundToInt(Hero.Stats.Armor),
				FMath::RoundToInt(Hero.Stats.MoveSpeed))
			: FString::Printf(
				TEXT("%s (%s) — Health: %d  Attack: %d  Armor: %d  Speed: %d  |  Signature Buildable: %s"),
				*Hero.DisplayName.ToString(),
				*Hero.ClassName.ToString(),
				FMath::RoundToInt(Hero.Stats.MaxHealth),
				FMath::RoundToInt(Hero.Stats.BaseDamage),
				FMath::RoundToInt(Hero.Stats.Armor),
				FMath::RoundToInt(Hero.Stats.MoveSpeed),
				*BuildableName);
		Text_HeroDetails->SetText(FText::FromString(SummaryLine));
	}

	for (int32 RowIdx = 0; RowIdx < AbilityPreviewWidgets.Num(); ++RowIdx)
	{
		UBwayAbilityPreviewWidget* PreviewWidget = AbilityPreviewWidgets[RowIdx];
		if (!PreviewWidget)
		{
			continue;
		}
		if (Hero.Abilities.IsValidIndex(RowIdx))
		{
			PreviewWidget->SetAbilityPreview(Hero.Abilities[RowIdx]);
		}
		else
		{
			PreviewWidget->ClearPreview();
		}
	}

	RefreshBreakawayAbilityPreviewPanel();
}

void UBwayHeroSelectWidget::RefreshBreakawayAbilityPreviewPanel()
{
	FHeroDisplayInfo Hero;
	const bool bHasHero = GetInspectedHeroDisplayInfo(Hero);
	const FPrimaryAssetId EffectiveHeroId = bHasHero ? Hero.HeroId : GetInspectedHeroId();
	const FText EffectiveHeroName = bHasHero ? Hero.DisplayName : FText::FromString(TEXT("Argus"));
	const FBreakawayHeroSelectArtMapping ArtMap = ResolveHeroArtMapping(EffectiveHeroId, EffectiveHeroName);

	if (HeroBackdropImage)
	{
		ApplyHeroSelectBrush(HeroBackdropImage, ArtMap.BackdropTex, FVector2D(1920.f, 1080.f));
	}
	if (LoadoutSkinImage)
	{
		ApplyHeroSelectBrush(LoadoutSkinImage, ArtMap.SkinTex, FVector2D(121.f, 112.f));
	}
	if (LoadoutTauntImage)
	{
		ApplyHeroSelectBrush(LoadoutTauntImage, TexDefaultTaunt, FVector2D(92.f, 92.f), FLinearColor(0.65f, 0.65f, 0.65f, 0.85f));
	}
	if (LoadoutWeaponImage)
	{
		ApplyHeroSelectBrush(LoadoutWeaponImage, ArtMap.WeaponTex, FVector2D(121.f, 112.f));
	}

	const int32 ActiveSlot = FMath::Clamp(SelectedAbilityPreviewSlot, 0, 5);
	static const TCHAR* AbilityMediaSuffixes[6] = {
		TEXT("basicattack"),
		TEXT("defense"),
		TEXT("ability1"),
		TEXT("ability2"),
		TEXT("ability3"),
		TEXT("buildable1")
	};

	for (int32 Idx = 0; Idx < 6; ++Idx)
	{
		if (AbilityBarIconImages[Idx])
		{
			ApplyHeroSelectBrush(AbilityBarIconImages[Idx], ArtMap.AbilityIcons[Idx], FVector2D(62.f, 62.f));
		}
		if (AbilityBarOuterBorders[Idx])
		{
			const bool bIsSelectedSlot = (Idx == ActiveSlot) && bAbilitiesPanelVisible;
			AbilityBarOuterBorders[Idx]->SetPadding(FMargin(bIsSelectedSlot ? 3.f : 1.f));
			AbilityBarOuterBorders[Idx]->SetBrushColor(bIsSelectedSlot
				? FLinearColor::FromSRGBColor(FColor(255, 162, 0, 255))
				: FLinearColor(1.0f, 1.0f, 1.0f, 0.08f));
		}
		if (AbilityBarInnerBorders[Idx])
		{
			AbilityBarInnerBorders[Idx]->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.02f, 0.92f));
		}
	}

	const ESlateVisibility PanelVis = bAbilitiesPanelVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (AbilityGradientImage)
	{
		AbilityGradientImage->SetVisibility(PanelVis);
	}
	if (AbilityVideoFrameBorder)
	{
		AbilityVideoFrameBorder->SetVisibility(PanelVis);
	}
	if (SelectedAbilityTitleText)
	{
		SelectedAbilityTitleText->SetVisibility(PanelVis);
	}
	if (SelectedAbilityDescText)
	{
		SelectedAbilityDescText->SetVisibility(PanelVis);
	}
	if (AbilityCollapseButton)
	{
		AbilityCollapseButton->SetVisibility(bAbilitiesPanelVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	const FString MediaTexPath = FString::Printf(
		TEXT("/BreakawayCore/UI/Art/HeroSelect/ability_%s_%s.ability_%s_%s"),
		ArtMap.MediaPrefix,
		AbilityMediaSuffixes[ActiveSlot],
		ArtMap.MediaPrefix,
		AbilityMediaSuffixes[ActiveSlot]);
	if (AbilityVideoPreviewImage)
	{
		ApplyHeroSelectBrush(AbilityVideoPreviewImage, *MediaTexPath, FVector2D(520.f, 293.f));
	}

	FString AbilityTitle = ArtMap.AbilityTitles[ActiveSlot];
	FString AbilityDesc = ArtMap.AbilityDescriptions[ActiveSlot];
	if (AbilityTitle.IsEmpty() && bHasHero && Hero.Abilities.IsValidIndex(ActiveSlot))
	{
		const FAbilityDisplayInfo& AbilInfo = Hero.Abilities[ActiveSlot];
		if (!AbilInfo.AbilityName.IsEmpty())
		{
			AbilityTitle = AbilInfo.AbilityName.ToString().ToUpper();
		}
		if (!AbilInfo.Description.IsEmpty())
		{
			AbilityDesc = AbilInfo.Description.ToString();
		}
	}

	if (SelectedAbilityTitleText)
	{
		SelectedAbilityTitleText->SetText(FText::FromString(AbilityTitle));
	}
	if (SelectedAbilityDescText)
	{
		SelectedAbilityDescText->SetText(FText::FromString(AbilityDesc));
	}
}

void UBwayHeroSelectWidget::RefreshGoal5HeroSelectLayout()
{
	if (!bUsingGoal5Layout)
	{
		return;
	}

	const int32 LocalTeam = GetLocalPlayerTeam();
	if (LocalTeamBadgeBorder && LocalTeamBadgeText)
	{
		if (LocalTeam == 1)
		{
			LocalTeamBadgeText->SetText(NSLOCTEXT("BwayHeroSelect", "RedTeamBadge", "RED TEAM"));
			LocalTeamBadgeBorder->SetBrushColor(FLinearColor(0.46f, 0.14f, 0.14f, 0.95f));
		}
		else if (LocalTeam == 0)
		{
			LocalTeamBadgeText->SetText(NSLOCTEXT("BwayHeroSelect", "BlueTeamBadge", "BLUE TEAM"));
			LocalTeamBadgeBorder->SetBrushColor(FLinearColor(0.10f, 0.28f, 0.50f, 0.95f));
		}
		else
		{
			LocalTeamBadgeText->SetText(NSLOCTEXT("BwayHeroSelect", "StagingLobbyBadge", "STAGING"));
			LocalTeamBadgeBorder->SetBrushColor(FLinearColor(0.16f, 0.22f, 0.32f, 0.92f));
		}
	}

	int32 ReadyCount = 0;
	int32 TotalCount = 0;
	GetReadyPlayerCount(ReadyCount, TotalCount);
	if (ReadyCountText)
	{
		ReadyCountText->SetText(FText::Format(
			NSLOCTEXT("BwayHeroSelect", "ReadyCounterFmt", "Ready ({0}/{1})"),
			FText::AsNumber(ReadyCount),
			FText::AsNumber(TotalCount)));
	}

	RefreshHeroRosterGrid();
	RefreshTeamRosterSidebars();
	RefreshLockButtonPresentation();
	ConfigureExplicitNavigationRules();
}

void UBwayHeroSelectWidget::RefreshHeroRosterGrid()
{
	const TArray<FHeroDisplayInfo> Heroes = GetAvailableHeroes();
	const bool bLocalLocked = IsSelectionLocked();
	const FPrimaryAssetId CurrentInspected = GetInspectedHeroId();

	if (HeroSlotsRow)
	{
		while (HeroSlotWidgets.Num() < Heroes.Num())
		{
			const int32 NewIdx = HeroSlotWidgets.Num();
			UBwayHeroSlotWidget* ExtraSlot = WidgetTree->ConstructWidget<UBwayHeroSlotWidget>(
				UBwayHeroSlotWidget::StaticClass(),
				*FString::Printf(TEXT("HeroSlotCard_%d"), NewIdx));
			ExtraSlot->OnHeroSlotClicked.AddDynamic(this, &ThisClass::HandleHeroSlotClicked);
			ExtraSlot->OnHeroSlotFocusedOrHovered.AddDynamic(this, &ThisClass::HandleHeroSlotFocusedOrHovered);
			HeroSlotWidgets.Add(ExtraSlot);
			UHorizontalBoxSlot* CardHSlot = HeroSlotsRow->AddChildToHorizontalBox(ExtraSlot);
			CardHSlot->SetPadding(FMargin(5.f, 0.f));
			CardHSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	for (int32 SlotIdx = 0; SlotIdx < HeroSlotWidgets.Num(); ++SlotIdx)
	{
		UBwayHeroSlotWidget* SlotWidget = HeroSlotWidgets[SlotIdx];
		if (!SlotWidget)
		{
			continue;
		}
		if (Heroes.IsValidIndex(SlotIdx))
		{
			SlotWidget->SetVisibility(ESlateVisibility::Visible);
			SlotWidget->SetHeroData(Heroes[SlotIdx]);
			SlotWidget->SetInspectedState(Heroes[SlotIdx].HeroId == CurrentInspected);
			SlotWidget->SetLocalPhaseLockedState(bLocalLocked);
		}
		else
		{
			SlotWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UBwayHeroSelectWidget::RefreshTeamRosterSidebars()
{
	if (!bUsingGoal5Layout)
	{
		return;
	}

	auto UpdateSidebar = [](
		const TArray<FBwayTeamRosterSlotViewModel>& ViewModels,
		FTeamSidebarSlotRow Rows[4],
		UCommonTextBlock* HeaderText,
		bool bIsBlueTeam)
	{
		const FLinearColor TeamAccent = bIsBlueTeam
			? FLinearColor(0.20f, 0.62f, 0.98f, 1.0f)
			: FLinearColor(0.94f, 0.32f, 0.30f, 1.0f);

		int32 OccupiedCount = 0;
		for (int32 Idx = 0; Idx < 4; ++Idx)
		{
			if (!ViewModels.IsValidIndex(Idx))
			{
				continue;
			}
			const FBwayTeamRosterSlotViewModel& VM = ViewModels[Idx];
			if (VM.bIsOccupied)
			{
				++OccupiedCount;
			}

			FTeamSidebarSlotRow& Row = Rows[Idx];
			if (Row.PlayerNameText)
			{
				Row.PlayerNameText->SetText(FText::FromString(VM.PlayerName));
				Row.PlayerNameText->SetColorAndOpacity(FSlateColor(
					VM.bIsLocalPlayer
						? FLinearColor(0.98f, 0.84f, 0.32f, 1.0f)
						: (VM.bIsOccupied ? FLinearColor::White : FLinearColor(0.48f, 0.54f, 0.64f, 0.85f))));
			}
			if (Row.HeroStatusText)
			{
				Row.HeroStatusText->SetText(VM.StatusText);
				Row.HeroStatusText->SetColorAndOpacity(FSlateColor(
					VM.bIsLocked
						? FLinearColor(0.30f, 0.94f, 0.54f, 1.0f)
						: (VM.bIsBot
							? FLinearColor(0.56f, 0.80f, 0.98f, 0.95f)
							: FLinearColor(0.74f, 0.82f, 0.92f, 0.95f))));
			}
			if (Row.OuterBorder)
			{
				if (VM.bIsLocalPlayer)
				{
					Row.OuterBorder->SetBrushColor(FLinearColor(0.11f, 0.20f, 0.34f, 0.97f));
				}
				else if (VM.bIsLocked)
				{
					Row.OuterBorder->SetBrushColor(FLinearColor(0.06f, 0.16f, 0.14f, 0.95f));
				}
				else
				{
					Row.OuterBorder->SetBrushColor(FLinearColor(0.05f, 0.09f, 0.16f, 0.90f));
				}
			}
			if (Row.PortraitBorder)
			{
				Row.PortraitBorder->SetBrushColor(VM.bIsLocked
					? FLinearColor(0.26f, 0.90f, 0.50f, 1.0f)
					: FLinearColor(TeamAccent.R * 0.45f, TeamAccent.G * 0.45f, TeamAccent.B * 0.45f, 0.90f));
			}
			if (Row.PortraitImage)
			{
				if (VM.Portrait)
				{
					Row.PortraitImage->SetBrushFromTexture(VM.Portrait, false);
					const float Opacity = VM.bIsLocked ? 1.0f : 0.78f;
					Row.PortraitImage->SetColorAndOpacity(FLinearColor(Opacity, Opacity, Opacity, 1.0f));
				}
				else
				{
					Row.PortraitImage->SetBrushFromTexture(nullptr, false);
					Row.PortraitImage->SetBrushTintColor(FSlateColor(
						VM.bIsOccupied
							? FLinearColor(TeamAccent.R * 0.35f, TeamAccent.G * 0.35f, TeamAccent.B * 0.35f, 0.90f)
							: FLinearColor(0.08f, 0.11f, 0.17f, 0.70f)));
				}
			}
		}

		if (HeaderText)
		{
			HeaderText->SetText(FText::FromString(FString::Printf(
				TEXT("%s (%d/4)"),
				bIsBlueTeam ? TEXT("BLUE TEAM") : TEXT("RED TEAM"),
				OccupiedCount)));
		}
	};

	UpdateSidebar(GetTeamRosterSlotViewModels(0), BlueTeamSlots, BlueTeamHeaderText, true);
	UpdateSidebar(GetTeamRosterSlotViewModels(1), RedTeamSlots, RedTeamHeaderText, false);
}

void UBwayHeroSelectWidget::RefreshLockButtonPresentation()
{
	if (bShowingTravelFailure)
	{
		return;
	}

	const bool bLocked = IsSelectionLocked();
	const bool bHasSelection = GetSelectedHeroId().IsValid();

	if (LockInButton)
	{
		const UObject* RefrigFont = LoadObject<UFont>(nullptr, FontRefrigeratorHeavy);
		if (bLocked)
		{
			LockInButton->SetLabelText(NSLOCTEXT("BwayHeroSelect", "LockedInBtn", "LOCKED IN"));
			LockInButton->SetButtonColors(
				FLinearColor(0.14f, 0.48f, 0.26f, 0.96f),
				FLinearColor(0.18f, 0.58f, 0.32f, 1.00f),
				FLinearColor(0.32f, 0.92f, 0.54f, 1.00f),
				FLinearColor::White);
			LockInButton->SetIsEnabled(false);
			LockInButton->SetIsInteractionEnabled(false);
			if (UCommonTextBlock* Lbl = LockInButton->GetLabelWidget())
			{
				Lbl->SetFont(MakeHeroSelectFont(RefrigFont, 21));
				Lbl->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				Lbl->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D(-15.f, 0.f), 0.f));
			}
		}
		else
		{
			LockInButton->SetLabelText(NSLOCTEXT("BwayHeroSelect", "TryHeroBtn", "TRY"));
			LockInButton->SetButtonColors(
				FLinearColor::FromSRGBColor(FColor(253, 185, 0, 255)),
				FLinearColor::FromSRGBColor(FColor(255, 200, 40, 255)),
				FLinearColor::FromSRGBColor(FColor(253, 185, 0, 255)),
				FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));
			LockInButton->SetIsEnabled(true);
			LockInButton->SetIsInteractionEnabled(true);
			if (LockInButton->WidgetTree)
			{
				if (UBorder* OuterBorder = Cast<UBorder>(LockInButton->WidgetTree->RootWidget))
				{
					OuterBorder->SetPadding(FMargin(0.f));
					OuterBorder->SetBrushColor(FLinearColor::FromSRGBColor(FColor(253, 185, 0, 255)));
				}
			}
			if (UCommonTextBlock* Lbl = LockInButton->GetLabelWidget())
			{
				Lbl->SetFont(MakeHeroSelectFont(RefrigFont, 24));
				Lbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)));
				Lbl->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				Lbl->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D(-15.f, 0.f), 0.f));
			}
		}
	}

	if (LockHintText)
	{
		if (bLocked)
		{
			LockHintText->SetText(NSLOCTEXT("BwayHeroSelect", "HintWaitingForPeers", "Waiting for all players"));
		}
		else if (bHasSelection)
		{
			LockHintText->SetText(NSLOCTEXT("BwayHeroSelect", "HintConfirmLock", "Confirm warrior to lock in"));
		}
		else
		{
			LockHintText->SetText(NSLOCTEXT("BwayHeroSelect", "HintChooseWarrior", "Unique heroes per team"));
		}
	}
}

void UBwayHeroSelectWidget::ConfigureExplicitNavigationRules()
{
	TArray<UBwayHeroSlotWidget*> VisibleSlots;
	for (UBwayHeroSlotWidget* SlotWidget : HeroSlotWidgets)
	{
		if (SlotWidget && SlotWidget->GetVisibility() == ESlateVisibility::Visible && SlotWidget->GetIsEnabled())
		{
			VisibleSlots.Add(SlotWidget);
		}
	}

	const bool bLockFocusable = TravelConfirmButton && TravelConfirmButton->GetIsEnabled();

	for (int32 Idx = 0; Idx < VisibleSlots.Num(); ++Idx)
	{
		UBwayHeroSlotWidget* Current = VisibleSlots[Idx];
		if (Idx > 0)
		{
			Current->SetNavigationRuleExplicit(EUINavigation::Left, VisibleSlots[Idx - 1]);
		}
		else
		{
			Current->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Stop);
		}

		if (Idx + 1 < VisibleSlots.Num())
		{
			Current->SetNavigationRuleExplicit(EUINavigation::Right, VisibleSlots[Idx + 1]);
		}
		else if (bLockFocusable)
		{
			Current->SetNavigationRuleExplicit(EUINavigation::Right, TravelConfirmButton);
		}
		else
		{
			Current->SetNavigationRuleBase(EUINavigation::Right, EUINavigationRule::Stop);
		}

		if (bLockFocusable)
		{
			Current->SetNavigationRuleExplicit(EUINavigation::Down, TravelConfirmButton);
		}
		else
		{
			Current->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
		}
		Current->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
	}

	if (TravelConfirmButton)
	{
		if (!VisibleSlots.IsEmpty())
		{
			UBwayHeroSlotWidget* ReturnSlot = VisibleSlots.Last();
			const FPrimaryAssetId SelectedId = GetSelectedHeroId();
			for (UBwayHeroSlotWidget* SlotWidget : VisibleSlots)
			{
				if (SlotWidget->GetHeroId() == SelectedId)
				{
					ReturnSlot = SlotWidget;
					break;
				}
			}
			TravelConfirmButton->SetNavigationRuleExplicit(EUINavigation::Left, ReturnSlot);
			TravelConfirmButton->SetNavigationRuleExplicit(EUINavigation::Up, ReturnSlot);
		}
		else
		{
			TravelConfirmButton->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Stop);
			TravelConfirmButton->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
		}
		TravelConfirmButton->SetNavigationRuleBase(EUINavigation::Right, EUINavigationRule::Stop);
		TravelConfirmButton->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
	}
}

void UBwayHeroSelectWidget::EnsureAndUpdatePreviewActor(FPrimaryAssetId HeroId)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	UBwayHeroDataAsset* HeroData = GetHeroDataAsset(HeroId);
	if (!PreviewActor || !IsValid(PreviewActor))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		PreviewActor = World->SpawnActor<ABwayHeroPreviewActor>(
			ABwayHeroPreviewActor::StaticClass(),
			FVector(0.0f, 0.0f, 1450.0f),
			FRotator(0.0f, -20.0f, 0.0f),
			SpawnParams);
	}

	if (PreviewActor)
	{
		PreviewActor->UpdatePreviewHero(HeroData);
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (!PC->GetPawn() && PC->GetViewTarget() != PreviewActor)
			{
				PC->SetViewTargetWithBlend(PreviewActor, 0.25f);
			}
		}
	}
}

void UBwayHeroSelectWidget::CleanupPreviewActor()
{
	if (PreviewActor && IsValid(PreviewActor))
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (PC->GetViewTarget() == PreviewActor && PC->GetPawn())
			{
				PC->SetViewTarget(PC->GetPawn());
			}
		}
		PreviewActor->Destroy();
	}
	PreviewActor = nullptr;
}

#if !UE_BUILD_SHIPPING
namespace BwayHeroSelectVerifyHarness
{
	static FTSTicker::FDelegateHandle GHeroSelectVerifyTicker;

	static UWorld* FindActivePieOrGameWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* W = Context.World();
			if (W && (W->WorldType == EWorldType::PIE || W->WorldType == EWorldType::Game) && !W->bIsTearingDown)
			{
				return W;
			}
		}
		return nullptr;
	}

	static bool CaptureHeroSelectViewportPng(UWorld* World, const FString& FileName, FIntPoint& OutSize)
	{
		OutSize = FIntPoint::ZeroValue;
		if (!World || !FSlateApplication::IsInitialized())
		{
			return false;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		UGameViewportClient* ViewportClient = (PC && PC->GetLocalPlayer()) ? PC->GetLocalPlayer()->ViewportClient.Get() : World->GetGameViewport();
		if (!ViewportClient || !ViewportClient->Viewport)
		{
			return false;
		}

		FSlateApplication& Slate = FSlateApplication::Get();
		const TSharedPtr<SViewport> ViewWidget = ViewportClient->GetGameViewportWidget();
		TSharedPtr<SWindow> Window = ViewportClient->GetWindow();
		if (!Window.IsValid() && ViewWidget.IsValid())
		{
			Window = Slate.FindWidgetWindow(ViewWidget.ToSharedRef());
		}
		if (!Window.IsValid())
		{
			Window = Slate.GetActiveTopLevelWindow();
		}
		if (Window.IsValid())
		{
			Window->SlatePrepass(Window->GetDPIScaleFactor());
		}
		ViewportClient->Viewport->Draw(false);

		TArray<FColor> Pixels;
		FIntVector SlateSize = FIntVector::ZeroValue;
		bool bCaptured = false;
		if (ViewWidget.IsValid())
		{
			FWidgetPath ViewPath;
			if (Slate.GeneratePathToWidgetUnchecked(ViewWidget.ToSharedRef(), ViewPath))
			{
				bCaptured = Slate.TakeScreenshot(ViewWidget.ToSharedRef(), Pixels, SlateSize);
			}
		}
		if ((!bCaptured || SlateSize.X <= 0 || SlateSize.Y <= 0) && Window.IsValid())
		{
			Pixels.Reset();
			bCaptured = Slate.TakeScreenshot(Window.ToSharedRef(), Pixels, SlateSize);
		}
		if (!bCaptured || SlateSize.X <= 0 || SlateSize.Y <= 0 || int64(SlateSize.X) * SlateSize.Y != Pixels.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectVerify: TakeScreenshot failed bCaptured=%d size=%dx%d pixels=%d"),
				bCaptured ? 1 : 0, SlateSize.X, SlateSize.Y, Pixels.Num());
			return false;
		}

		for (FColor& Pixel : Pixels)
		{
			Pixel.A = 255;
		}

		TArray64<uint8> CompressedData;
		FImageUtils::PNGCompressImageArray(
			SlateSize.X,
			SlateSize.Y,
			TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()),
			CompressedData);
		if (CompressedData.IsEmpty())
		{
			return false;
		}

		const FString OutDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("heroselect-ui-verify"));
		IFileManager::Get().MakeDirectory(*OutDir, true);
		const FString OutPath = OutDir / FileName;
		if (!FFileHelper::SaveArrayToFile(CompressedData, *OutPath))
		{
			return false;
		}
		OutSize = FIntPoint(SlateSize.X, SlateSize.Y);
		UE_LOG(LogTemp, Display, TEXT("BwayHeroSelectVerify: SavedScreenshot path=%s size=%dx%d bytes=%lld"),
			*OutPath, OutSize.X, OutSize.Y, static_cast<long long>(CompressedData.Num()));
		return true;
	}

	static void ResizeHeroSelectViewport(UWorld* World, int32 Width, int32 Height)
	{
		UGameViewportClient* GVC = World ? World->GetGameViewport() : nullptr;
		if (!GVC)
		{
			return;
		}
		TSharedPtr<SWindow> Window = GVC->GetWindow();
		if (!Window.IsValid() && FSlateApplication::IsInitialized() && GVC->GetGameViewportWidget().IsValid())
		{
			Window = FSlateApplication::Get().FindWidgetWindow(GVC->GetGameViewportWidget().ToSharedRef());
		}
		if (Window.IsValid())
		{
			Window->Resize(FVector2D(static_cast<float>(Width), static_cast<float>(Height)));
		}
	}

	static FAutoConsoleCommand VerifyHeroSelectUICmd(
		TEXT("bway.Test.VerifyHeroSelectUI"),
		TEXT("Dispatches guard=0 PIE verification of W_HeroSelection at 1920x1080, 1024x573, 1280x800, and interactive ability/hero states."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (GHeroSelectVerifyTicker.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(GHeroSelectVerifyTicker);
				GHeroSelectVerifyTicker.Reset();
			}

			struct FHeroSelectVerifyState
			{
				int32 Step = 0;
				double StepStartTime = 0.0;
				TWeakObjectPtr<UBwayHeroSelectWidget> HeroSelect;
			};

			TSharedRef<FHeroSelectVerifyState> State = MakeShared<FHeroSelectVerifyState>();
			State->StepStartTime = FPlatformTime::Seconds();

			GHeroSelectVerifyTicker = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([State](float) -> bool
				{
					UWorld* World = FindActivePieOrGameWorld();
					if (!World)
					{
						UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectVerify: FAIL reason=no_pie_world"));
						return false;
					}

					APlayerController* PC = World->GetFirstPlayerController();
					if (!PC || !PC->GetLocalPlayer())
					{
						return true;
					}

					const double Now = FPlatformTime::Seconds();

					if (State->Step == 0)
					{
						if (Now - State->StepStartTime < 4.0)
						{
							return true;
						}
						UBwayHeroSelectWidget* Widget = nullptr;
						for (TObjectIterator<UBwayHeroSelectWidget> It; It; ++It)
						{
							if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated())
							{
								Widget = *It;
								break;
							}
						}
						if (!Widget)
						{
							UClass* WidgetClass = LoadClass<UBwayHeroSelectWidget>(
								nullptr,
								TEXT("/BreakawayCore/UI/HeroSelect/W_HeroSelection.W_HeroSelection_C"));
							if (!WidgetClass)
							{
								WidgetClass = UBwayHeroSelectWidget::StaticClass();
							}
							const FGameplayTag LayerTag = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
							Widget = Cast<UBwayHeroSelectWidget>(
								UCommonUIExtensions::PushContentToLayer_ForPlayer(PC->GetLocalPlayer(), LayerTag, WidgetClass));
							if (!Widget)
							{
								Widget = CreateWidget<UBwayHeroSelectWidget>(PC, WidgetClass);
								if (Widget)
								{
									Widget->AddToViewport(100);
									Widget->ActivateWidget();
								}
							}
						}
						if (!Widget || !Widget->IsActivated())
						{
							if (Now - State->StepStartTime > 15.0)
							{
								UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectVerify: FAIL reason=hero_select_not_active"));
								return false;
							}
							return true;
						}

						State->HeroSelect = Widget;
						const TArray<FHeroDisplayInfo> Heroes = Widget->GetAvailableHeroes();
						if (!Heroes.IsEmpty())
						{
							Widget->SetInspectedHero(Heroes[0].HeroId);
						}
						Widget->SelectAbilityPreviewSlot(5); // Siege Engine default matching screenshot
						ResizeHeroSelectViewport(World, 1920, 1080);
						State->Step = 1;
						State->StepStartTime = Now;
						return true;
					}

					if (State->Step == 1)
					{
						if (Now - State->StepStartTime < 1.2)
						{
							return true;
						}
						FIntPoint CapSize;
						if (!CaptureHeroSelectViewportPng(World, TEXT("heroselect-1080p-reference.png"), CapSize))
						{
							UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectVerify: FAIL reason=capture_1080p_reference_failed"));
							return false;
						}
						ResizeHeroSelectViewport(World, 1024, 573);
						State->Step = 2;
						State->StepStartTime = Now;
						return true;
					}

					if (State->Step == 2)
					{
						if (Now - State->StepStartTime < 0.8)
						{
							return true;
						}
						FIntPoint CapSize;
						if (!CaptureHeroSelectViewportPng(World, TEXT("heroselect-573p-reference.png"), CapSize))
						{
							UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectVerify: FAIL reason=capture_573p_reference_failed"));
							return false;
						}
						ResizeHeroSelectViewport(World, 1920, 1080);
						if (UBwayHeroSelectWidget* Widget = State->HeroSelect.Get())
						{
							Widget->SelectAbilityPreviewSlot(2); // Switch ability preview to Flame Wall (Ability 1)
						}
						State->Step = 3;
						State->StepStartTime = Now;
						return true;
					}

					if (State->Step == 3)
					{
						if (Now - State->StepStartTime < 0.8)
						{
							return true;
						}
						FIntPoint CapSize;
						if (!CaptureHeroSelectViewportPng(World, TEXT("heroselect-1080p-ability-switch.png"), CapSize))
						{
							UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectVerify: FAIL reason=capture_ability_switch_failed"));
							return false;
						}
						if (UBwayHeroSelectWidget* Widget = State->HeroSelect.Get())
						{
							const TArray<FHeroDisplayInfo> Heroes = Widget->GetAvailableHeroes();
							if (Heroes.Num() >= 4)
							{
								Widget->SelectHero(Heroes[3].HeroId); // Select Rawlins (Cell 9)
								Widget->SelectAbilityPreviewSlot(5);  // Jailhouse buildable
							}
						}
						State->Step = 4;
						State->StepStartTime = Now;
						return true;
					}

					if (State->Step == 4)
					{
						if (Now - State->StepStartTime < 0.8)
						{
							return true;
						}
						FIntPoint CapSize;
						if (!CaptureHeroSelectViewportPng(World, TEXT("heroselect-1080p-hero-switch.png"), CapSize))
						{
							UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectVerify: FAIL reason=capture_hero_switch_failed"));
							return false;
						}
						if (UBwayHeroSelectWidget* Widget = State->HeroSelect.Get())
						{
							const TArray<FHeroDisplayInfo> Heroes = Widget->GetAvailableHeroes();
							if (!Heroes.IsEmpty())
							{
								Widget->SelectHero(Heroes[0].HeroId); // Back to Argus
								Widget->SelectAbilityPreviewSlot(5);
							}
						}
						ResizeHeroSelectViewport(World, 1280, 800);
						State->Step = 5;
						State->StepStartTime = Now;
						return true;
					}

					if (State->Step == 5)
					{
						if (Now - State->StepStartTime < 0.8)
						{
							return true;
						}
						FIntPoint CapSize;
						if (!CaptureHeroSelectViewportPng(World, TEXT("heroselect-1280x800-aspect.png"), CapSize))
						{
							UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectVerify: FAIL reason=capture_1280x800_failed"));
							return false;
						}
						ResizeHeroSelectViewport(World, 1920, 1080);
						UBwayHeroSelectWidget* Widget = State->HeroSelect.Get();
						FHeroDisplayInfo InspectedHeroInfo;
						const bool bHasHeroInfo = Widget && Widget->GetInspectedHeroDisplayInfo(InspectedHeroInfo);
						UE_LOG(LogTemp, Display, TEXT("BwayHeroSelectVerify: PASS hero=%s class=%s ability=%s slots=%d"),
							bHasHeroInfo ? *InspectedHeroInfo.DisplayName.ToString() : TEXT("None"),
							bHasHeroInfo ? *InspectedHeroInfo.ClassName.ToString() : TEXT("None"),
							Widget ? *Widget->GetSelectedAbilityTitleText().ToString() : TEXT("None"),
							Widget ? Widget->GetHeroSlotWidgets().Num() : 0);
						return false;
					}

					return false;
				}),
				0.1f);
		}));
} // namespace BwayHeroSelectVerifyHarness
#endif

#if WITH_EDITOR
namespace BwayHeroSelectEditorBuilder
{
	static void PopulateSingleHeroSelectBlueprint(const TCHAR* AssetPath, bool bIsHeroSlotBlueprint)
	{
		UBlueprint* WBP = LoadObject<UBlueprint>(nullptr, AssetPath);
		if (!WBP)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectEditorBuilder: Failed to load %s"), AssetPath);
			return;
		}

		FObjectProperty* TreeProp = FindFProperty<FObjectProperty>(WBP->GetClass(), TEXT("WidgetTree"));
		UWidgetTree* BpWidgetTree = TreeProp ? Cast<UWidgetTree>(TreeProp->GetObjectPropertyValue_InContainer(WBP)) : nullptr;
		if (!BpWidgetTree)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectEditorBuilder: No WidgetTree property on %s"), AssetPath);
			return;
		}

		WBP->Modify();
		BpWidgetTree->Modify();

		TArray<UWidget*> OldWidgets;
		BpWidgetTree->GetAllWidgets(OldWidgets);
		BpWidgetTree->RootWidget = nullptr;
		for (UWidget* OldW : OldWidgets)
		{
			if (OldW)
			{
				OldW->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			}
		}

		if (bIsHeroSlotBlueprint)
		{
			UBwayHeroSlotWidget* SlotBuilder = NewObject<UBwayHeroSlotWidget>(GetTransientPackage(), UBwayHeroSlotWidget::StaticClass(), NAME_None, RF_Transient);
			SlotBuilder->WidgetTree = BpWidgetTree;
			SlotBuilder->EnsureVisualTreeConstructed();
			SlotBuilder->WidgetTree = nullptr;
		}
		else
		{
			UBwayHeroSelectWidget* SelectBuilder = NewObject<UBwayHeroSelectWidget>(GetTransientPackage(), UBwayHeroSelectWidget::StaticClass(), NAME_None, RF_Transient);
			SelectBuilder->WidgetTree = BpWidgetTree;
			SelectBuilder->BuildOrBindWidgetHierarchy();
			SelectBuilder->WidgetTree = nullptr;
		}

		TArray<UWidget*> NewWidgets;
		BpWidgetTree->GetAllWidgets(NewWidgets);
		for (UWidget* W : NewWidgets)
		{
			if (W)
			{
				W->bIsVariable = false;
			}
		}

		if (FMapProperty* GuidMapProp = FindFProperty<FMapProperty>(WBP->GetClass(), TEXT("WidgetVariableNameToGuidMap")))
		{
			FScriptMapHelper MapHelper(GuidMapProp, GuidMapProp->ContainerPtrToValuePtr<void>(WBP));
			MapHelper.EmptyValues();
			for (UWidget* W : NewWidgets)
			{
				if (W)
				{
					const FName WName = W->GetFName();
					const FGuid WGuid = FGuid::NewGuid();
					MapHelper.AddPair(&WName, &WGuid);
				}
			}
		}

		// Clear any stale Blueprint event/function graph nodes that referenced deleted legacy WidgetTree variables
		for (UEdGraph* Graph : WBP->UbergraphPages)
		{
			if (Graph)
			{
				Graph->Nodes.Empty();
			}
		}
		for (UEdGraph* Graph : WBP->FunctionGraphs)
		{
			if (Graph)
			{
				Graph->Nodes.Empty();
			}
		}
		WBP->FunctionGraphs.Empty();
		WBP->NewVariables.Empty();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
		FKismetEditorUtilities::CompileBlueprint(WBP);

		UPackage* Pkg = WBP->GetOutermost();
		Pkg->MarkPackageDirty();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		SaveArgs.Error = GError;
		const FSavePackageResultStruct SaveRes = UPackage::Save(Pkg, WBP, *PackageFileName, SaveArgs);

		UE_LOG(LogTemp, Display, TEXT("BwayHeroSelectEditorBuilder: PASS asset=%s root=%s widgets=%d status=%d saved=%d path=%s"),
			AssetPath,
			*GetNameSafe(BpWidgetTree->RootWidget),
			NewWidgets.Num(),
			int32(WBP->Status),
			SaveRes.IsSuccessful() ? 1 : 0,
			*PackageFileName);
	}

	static FAutoConsoleCommand PopulateHeroSelectWidgetBlueprintCmd(
		TEXT("bway.Editor.PopulateHeroSelectWidgetBlueprint"),
		TEXT("Populates WBP_BW_HeroSlot, W_HeroSelection, and WBP_BW_HeroSelect WidgetTrees with the Breakaway Character Select UMG hierarchy, compiles, and saves the assets."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			PopulateSingleHeroSelectBlueprint(TEXT("/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSlot.WBP_BW_HeroSlot"), true);
			PopulateSingleHeroSelectBlueprint(TEXT("/BreakawayCore/UI/HeroSelect/W_HeroSelection.W_HeroSelection"), false);
			PopulateSingleHeroSelectBlueprint(TEXT("/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect.WBP_BW_HeroSelect"), false);
		}));
} // namespace BwayHeroSelectEditorBuilder
#endif
