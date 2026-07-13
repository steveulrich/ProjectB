#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "BwayGameplayAbility.generated.h"

class UMediaSource;
class UTexture2D;

/**
 * Authoring-time UI metadata stored on the ability CDO.
 * Resolved at runtime into FAbilityDisplayInfo for widgets.
 */
USTRUCT(BlueprintType)
struct FBwayAbilityUIData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText AbilityName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UMediaSource> PreviewVideo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> PreviewImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	bool bIsUltimate = false;

	bool HasContent() const
	{
		return !AbilityName.IsEmpty() || !Description.IsEmpty() || !Icon.IsNull() || !PreviewVideo.IsNull() || !PreviewImage.IsNull();
	}
};

/**
 * Breakaway gameplay ability root — shared UI display metadata for hero select and match HUD.
 * Default CancelAbilitiesWithTag cancels Ability.Buildable.PlacementSession on activate (GAS PreActivate).
 * Placement confirm/cancel/start abilities clear that cancel tag in their constructors.
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	const FBwayAbilityUIData& GetDisplayData() const { return DisplayData; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool HasDisplayData() const { return DisplayData.HasContent(); }

	/** Read display data from an ability class CDO (returns false if not a UBwayGameplayAbility). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	static bool GetDisplayDataFromAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass, FBwayAbilityUIData& OutDisplayData);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FBwayAbilityUIData DisplayData;
};
