// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "LyraTeamDisplayAsset.generated.h"

#define UE_API LYRAGAME_API

struct FPropertyChangedEvent;

class UMaterialInstanceDynamic;
class UMeshComponent;
class UNiagaraComponent;
class AActor;
class UTexture;

// Represents the display information for team definitions (e.g., colors, display names, textures, etc...)
UCLASS(MinimalAPI, BlueprintType)
class ULyraTeamDisplayAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, float> ScalarParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FLinearColor> ColorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, TObjectPtr<UTexture>> TextureParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText TeamShortName;

public:
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API void ApplyToMaterial(UMaterialInstanceDynamic* Material);

	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API void ApplyToMeshComponent(UMeshComponent* MeshComponent);

	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API void ApplyToNiagaraComponent(UNiagaraComponent* NiagaraComponent);

	UFUNCTION(BlueprintCallable, Category=Teams, meta=(DefaultToSelf="TargetActor"))
	UE_API void ApplyToActor(AActor* TargetActor, bool bIncludeChildActors = true);

public:

	//~UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End of UObject interface
};

#undef UE_API
