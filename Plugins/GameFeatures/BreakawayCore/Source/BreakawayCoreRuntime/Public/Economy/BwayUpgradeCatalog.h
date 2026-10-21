#pragma once

#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "BwayUpgradeCatalog.generated.h"

/** Reversible, non-stacking attribute modifiers used for an owned upgrade rank. */
UCLASS(Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayUpgradeEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UBwayUpgradeEffect();
	bool IsSupportedUpgradeEffect() const;
	static const FName MagnitudeName;
};

/** A data-configured magnitude added to the hero's attack strength. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayUpgradeEffect_AttackStrength : public UBwayUpgradeEffect
{
	GENERATED_BODY()
public:
	UBwayUpgradeEffect_AttackStrength();
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayUpgradeEffect_Armor : public UBwayUpgradeEffect
{
	GENERATED_BODY()
public:
	UBwayUpgradeEffect_Armor();
};

/** Additive fraction of normal movement speed; 0.10 adds ten percent before slow modifiers. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayUpgradeEffect_MoveSpeed : public UBwayUpgradeEffect
{
	GENERATED_BODY()
public:
	UBwayUpgradeEffect_MoveSpeed();
};

/** Additive fraction removed from future finite cooldown durations. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayUpgradeEffect_CooldownReduction : public UBwayUpgradeEffect
{
	GENERATED_BODY()
public:
	UBwayUpgradeEffect_CooldownReduction();
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayUpgradeEffect_LifeLeech : public UBwayUpgradeEffect
{
	GENERATED_BODY()
public:
	UBwayUpgradeEffect_LifeLeech();
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayUpgradeEffect_RespawnTimeReduction : public UBwayUpgradeEffect
{
	GENERATED_BODY()
public:
	UBwayUpgradeEffect_RespawnTimeReduction();
};

UENUM(BlueprintType)
enum class EBwayUpgradeCategory : uint8
{
	Attack	UMETA(DisplayName = "ATTACK"),
	Defense	UMETA(DisplayName = "DEFENSE"),
	Misc	UMETA(DisplayName = "MISC")
};

USTRUCT(BlueprintType)
struct FBwayUpgradeRank
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0"))
	int32 Cost = 0;
	/** Total bonus at this rank, replacing the previous rank's effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Magnitude = 0.0f;
};

USTRUCT(BlueprintType)
struct FBwayUpgradeDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine="true"))
	FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UBwayUpgradeEffect> EffectClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FBwayUpgradeRank> Ranks;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EBwayUpgradeCategory Category = EBwayUpgradeCategory::Attack;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bHasExplicitCategory = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsRecommended = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bHasExplicitRecommended = false;

	EBwayUpgradeCategory GetEffectiveCategory() const;
	bool IsEffectiveRecommended() const;
};

/** Immutable match catalog. A hard reference from the owning component retains/cooks definitions. */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwayUpgradeCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1"))
	int32 MaxOwnedUpgrades = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FBwayUpgradeDefinition> Upgrades;
	const FBwayUpgradeDefinition* FindUpgrade(FName Id) const;
	bool ValidateCatalog(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
