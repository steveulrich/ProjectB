#pragma once
#include "Buildable/BuildableBase.h"
#include "BwayHeroDataAsset.generated.h"

// Forward declarations
class UAbilitySet;
class UAttributeSet;
class ABuildableBase;
class UMediaSource;

/**
 * Display information for a single ability in the character select UI
 */
USTRUCT(BlueprintType)
struct FAbilityDisplayInfo
{
    GENERATED_BODY()

    /** The name displayed in the UI (e.g., "SIEGE ENGINE") */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display")
    FText AbilityName;

    /** Description text shown when ability is selected */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display", meta=(MultiLine=true))
    FText Description;

    /** Icon texture for the ability bar */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display")
    TSoftObjectPtr<UTexture2D> Icon;

    /** Optional video preview of the ability in action */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display")
    TSoftObjectPtr<UMediaSource> PreviewVideo;

    /** Static image preview (fallback if no video) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display")
    TSoftObjectPtr<UTexture2D> PreviewImage;

    /** Whether this is the ultimate ability (shown with special styling) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display")
    bool bIsUltimate = false;

    /** Input action tag for displaying keybind (optional) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display")
    FGameplayTag InputActionTag;
};

/**
 * Hero class type for display in character select
 */
UENUM(BlueprintType)
enum class EHeroClass : uint8
{
    Fighter     UMETA(DisplayName = "Fighter"),
    Tank        UMETA(DisplayName = "Tank"),
    Support     UMETA(DisplayName = "Support"),
    Assassin    UMETA(DisplayName = "Assassin"),
    Mage        UMETA(DisplayName = "Mage"),
    Ranger      UMETA(DisplayName = "Ranger")
};

USTRUCT(BlueprintType)
struct FHeroStats
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float MaxHealth = 200.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float BaseDamage = 25.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float MoveSpeed = 600.f;
};

UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwayHeroDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Must override this to return a valid type
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("HeroDataAsset", GetFName());
    }

    /** Get the display text for the hero's class */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="UI")
    FText GetClassDisplayName() const
    {
        if (bUseCustomClassName && !CustomClassName.IsEmpty())
        {
            return CustomClassName;
        }
        
        // Convert enum to display text
        const UEnum* EnumPtr = StaticEnum<EHeroClass>();
        if (EnumPtr)
        {
            return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(HeroClass));
        }
        return FText::FromString(TEXT("Unknown"));
    }
    
    /* -------- Presentation -------- */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    FText DisplayName;

    /** Hero class type (Fighter, Tank, Support, etc.) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    EHeroClass HeroClass = EHeroClass::Fighter;

    /** Optional custom class name text (overrides enum if set) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI", meta=(EditCondition="bUseCustomClassName"))
    FText CustomClassName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    bool bUseCustomClassName = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    TObjectPtr<UTexture2D> Portrait;

    /** Abilities to display in character select (order matters: slot 0-5) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    TArray<FAbilityDisplayInfo> AbilityDisplayInfos;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mesh")
    TObjectPtr<USkeletalMesh> HeroMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mesh")
    TObjectPtr<UAnimBlueprint> AnimationBP;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    FHeroStats HeroStats;
    
    /* -------- GAS References -------- */
    // Attribute-set class the character uses
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
    TSubclassOf<UAttributeSet> AttributeSetClass;

    // Set of abilities granted on spawn (primary, secondary, ult, etc.)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
    TArray<TObjectPtr<const ULyraAbilitySet>> AbilitySets;

    /* -------- Buildables -------- */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buildables")
    TObjectPtr<UBwayBuildableDataAsset> BuildableDataAsset;

    /* -------- Audio -------- */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
    TObjectPtr<USoundBase> VoiceBank;
};