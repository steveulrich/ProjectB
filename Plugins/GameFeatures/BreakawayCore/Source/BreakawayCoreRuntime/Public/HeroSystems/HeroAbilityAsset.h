// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HeroAbilityAsset.generated.h"

UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UAbilityDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		FText DescriptionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		int32 Cooldown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "AbilityData")
		TArray<TObjectPtr<UHeroAbilityData>> AbilityDataSet;
};

UCLASS(EditInlineNew, DefaultToInstanced)
class BREAKAWAYCORERUNTIME_API UHeroAbilityData : public UObject
{
	GENERATED_BODY()
};

UCLASS(DisplayName = "Base Damage")
class BREAKAWAYCORERUNTIME_API UBaseDamageAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		int32 BaseDamage;
};

UCLASS(DisplayName = "Environment Damage")
class BREAKAWAYCORERUNTIME_API UEnvironmentDamageAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		int32 EnvDamage;
};

UCLASS(DisplayName = "Power Scaling Ratio")
class BREAKAWAYCORERUNTIME_API UPowerScalingRatioAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		float PowerScalingRatio;
};

UCLASS(DisplayName = "Power Scaling Ratio Max")
class BREAKAWAYCORERUNTIME_API UPowerScalingRatioMaxAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		float PowerScalingRatioMax;
};

UCLASS(DisplayName = "Range")
class BREAKAWAYCORERUNTIME_API URangeAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		int32 Range;
};

UCLASS(DisplayName = "Radius")
class BREAKAWAYCORERUNTIME_API URadiusAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		float Radius;
};

UCLASS(DisplayName = "Apply Force")
class BREAKAWAYCORERUNTIME_API UApplyForceAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		float Force;
};

UCLASS(DisplayName = "Duration")
class BREAKAWAYCORERUNTIME_API UDurationAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		int32 Duration;
};

UCLASS(DisplayName = "Iterations")
class BREAKAWAYCORERUNTIME_API UIterationsAbilityDataAsset : public UHeroAbilityData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityData")
		int32 Iterations;
};