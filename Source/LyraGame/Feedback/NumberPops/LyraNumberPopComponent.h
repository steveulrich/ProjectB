// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "LyraNumberPopComponent.generated.h"

#define UE_API LYRAGAME_API

class UObject;
struct FFrame;

USTRUCT(BlueprintType)
struct FLyraNumberPopRequest
{
	GENERATED_BODY()

	// The world location to create the number pop at
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Number Pops")
	FVector WorldLocation;

	// Tags related to the source/cause of the number pop (for determining a style)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Number Pops")
	FGameplayTagContainer SourceTags;

	// Tags related to the target of the number pop (for determining a style)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lyra|Number Pops")
	FGameplayTagContainer TargetTags;

	// The number to display
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Number Pops")
	int32 NumberToDisplay = 0;

	// Whether the number is 'critical' or not (@TODO: move to a tag)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Number Pops")
	bool bIsCriticalDamage = false;

	FLyraNumberPopRequest()
		: WorldLocation(ForceInitToZero)
	{
	}
};


UCLASS(MinimalAPI, Abstract, Blueprintable)
class ULyraNumberPopComponent : public UControllerComponent
{
	GENERATED_BODY()

public:

	UE_API ULyraNumberPopComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Adds a damage number to the damage number list for visualization */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Number Pops")
	UE_API virtual void AddNumberPop(const FLyraNumberPopRequest& NewRequest);
};

#undef UE_API
