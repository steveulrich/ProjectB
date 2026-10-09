#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BwayRespawnHUDWidget.generated.h"

class ABwayPlayerState;
class UTextBlock;

/** Local, non-interactive presentation of the authoritative respawn schedule. */
UCLASS(Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayRespawnHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UBwayRespawnHUDWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void RefreshPlayerState();

	UFUNCTION(BlueprintPure, Category="Respawn")
	int32 GetDisplayedSeconds() const { return DisplayedSeconds; }

	UFUNCTION(BlueprintPure, Category="Respawn")
	FText GetDisplayedStatus() const;
	bool IsSuppressedByMenu() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** A designer-authored skin can replace the native placeholder tree. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Respawn")
	TObjectPtr<UTextBlock> Text_RespawnStatus;

	UFUNCTION(BlueprintImplementableEvent, Category="Respawn")
	void OnCountdownChanged(int32 SecondsRemaining, bool bShown);

private:
	UFUNCTION()
	void HandleScheduleChanged();
	void RefreshDisplay();
	void UnbindPlayerState();
	TWeakObjectPtr<ABwayPlayerState> BoundPlayerState;
	FTimerHandle CountdownTimer;
	int32 DisplayedSeconds = -1;
};
