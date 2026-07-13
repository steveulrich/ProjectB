#pragma once
#include "Buildable/BuildableBase.h"
#include "BwayHeroDataAsset.generated.h"

// Forward declarations
class ULyraAbilitySet;
class UAttributeSet;
class UMediaSource;

/**
 * Resolved display information for a single ability in UI widgets.
 * Built at runtime from ability CDO display data + grant InputTag.
 */
USTRUCT(BlueprintType)
struct FAbilityDisplayInfo
{
    GENERATED_BODY()

    /** The name displayed in the UI (e.g., "SIEGE ENGINE") */
    UPROPERTY(BlueprintReadOnly, Category="Display")
    FText AbilityName;

    /** Description text shown when ability is selected */
    UPROPERTY(BlueprintReadOnly, Category="Display")
    FText Description;

    /** Icon texture for the ability bar */
    UPROPERTY(BlueprintReadOnly, Category="Display")
    TSoftObjectPtr<UTexture2D> Icon;

    /** Optional video preview of the ability in action */
    UPROPERTY(BlueprintReadOnly, Category="Display")
    TSoftObjectPtr<UMediaSource> PreviewVideo;

    /** Static image preview (fallback if no video) */
    UPROPERTY(BlueprintReadOnly, Category="Display")
    TSoftObjectPtr<UTexture2D> PreviewImage;

    /** Whether this is the ultimate ability (shown with special styling) */
    UPROPERTY(BlueprintReadOnly, Category="Display")
    bool bIsUltimate = false;

    /** Input tag from the ability set grant (used for keybind display). */
    UPROPERTY(BlueprintReadOnly, Category="Display")
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
    Assassin    UMETA(DisplayName = "Assassin")
};

USTRUCT(BlueprintType)
struct FHeroStats
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float MaxHealth = 500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float BaseDamage = 50.f;

    /** Sheet armor value (e.g. Argus = 3). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float Armor = 0.f;

    /** Sheet speed rating (e.g. Argus = 10 → 600 uu/s). Values > 100 treated as legacy direct uu/s. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float MoveSpeed = 10.f;
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

    /** Hero class type (Fighter, Tank, Support, etc.) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    int32 HeroClassDisplayIndex = 0;

    /** Optional custom class name text (overrides enum if set) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI", meta=(EditCondition="bUseCustomClassName"))
    FText CustomClassName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    bool bUseCustomClassName = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    TObjectPtr<UTexture2D> Portrait;

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

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Buildables")
    UBwayBuildableDataAsset* GetBuildableDataAsset() const { return BuildableDataAsset; }

    /* -------- Audio -------- */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
    TObjectPtr<USoundBase> VoiceBank;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};