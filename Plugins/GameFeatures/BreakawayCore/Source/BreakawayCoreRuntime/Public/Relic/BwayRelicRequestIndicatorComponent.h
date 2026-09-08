#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "BwayRelicRequestIndicatorComponent.generated.h"

class APlayerController;
class UAbilitySystemComponent;
class ULocalPlayer;
class UWidgetComponent;

/** Local presentation of a teammate's request; GAS remains the source of gameplay state. */
UCLASS(meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayRelicRequestIndicatorComponent : public UActorComponent
{

	GENERATED_BODY()

public:
	UBwayRelicRequestIndicatorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Uses the same match configuration as request activation and relic pickup. */
	static FGameplayTag ResolveRequestTag(const AActor* Actor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The existing world-space widget may be restyled without changing request gameplay. */
	UPROPERTY(EditDefaultsOnly, Category = "Relic|Presentation", meta = (UseComponentPicker, AllowedClasses = "/Script/UMG.WidgetComponent"))
	FComponentReference RequestWidget;

private:
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();
	void HandleTagChanged(FGameplayTag Tag, int32 Count);
	void SetViewer(APlayerController* Controller);
	void RefreshVisibility();

	UFUNCTION()
	void HandleTeamChanged(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UWidgetComponent> IndicatorWidget;
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;
	TWeakObjectPtr<APlayerController> Viewer;
	FGameplayTag RequestTag;
	FDelegateHandle RequestTagHandle;
	FDelegateHandle DeathTagHandle;
	FDelegateHandle ControllerChangedHandle;
	bool bEndingPlay = false;
};
