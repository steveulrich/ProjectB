#pragma once
#include "Buildable/BuildableBase.h"
#include "BwayHeroDataAsset.generated.h"

// Forward declarations
class UAbilitySet;
class UAttributeSet;
class ABuildableBase;

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
    /* -------- Presentation -------- */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    FText DisplayName;

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

    /* -------- Audio -------- */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
    TObjectPtr<USoundBase> VoiceBank;
};