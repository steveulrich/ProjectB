#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BwayCuePresentationComponent.generated.h"

class UAbilitySystemComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USceneComponent;

/** One cosmetic lifetime. Owns only the loop it added, on the original ASC. */
struct BREAKAWAYCORERUNTIME_API FBwayCuePresentationState
{
	bool Start(UAbilitySystemComponent* ASC, AActor* Avatar, UObject* Source,
		FGameplayTag StartCue, FGameplayTag ActiveCue, FGameplayTag EndCue, USceneComponent* AttachComponent = nullptr);
	void Stop(bool bPlayEnd);
	void Reset();
	bool IsActive() const { return bStarted && !bEnded; }

private:
	TWeakObjectPtr<UAbilitySystemComponent> OwningASC;
	TWeakObjectPtr<AActor> OriginalAvatar;
	TWeakObjectPtr<UObject> SourceObject;
	TWeakObjectPtr<USceneComponent> AttachmentRoot;
	FGameplayTag OwnedActiveCue;
	FGameplayTag OwnedEndCue;
	FVector LastLocation = FVector::ZeroVector;
	bool bStarted = false;
	bool bEnded = false;
};

/** Optional ASC-owned start/active/end cues for a structure's existing gameplay lifetime. */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayCuePresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBwayCuePresentationComponent();
	void StartPresentation();
	void StopPresentation(bool bPlayEnd = true);
	/** Stable default subobject below a replicated root; never a runtime-created attachment. */
	UPROPERTY(VisibleAnywhere, Category = "Presentation")
	TObjectPtr<USceneComponent> CueAttachComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag Cue_Start;
	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag Cue_Active;
	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag Cue_End;
	/** Optional local cosmetic replacement; never changes the structure's gameplay collision. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Visual")
	TSoftObjectPtr<UStaticMesh> VisualMesh;
	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Visual")
	FTransform VisualRelativeTransform = FTransform::Identity;
	/** For replicated zones with no ASC: current loop follows local actor existence on each peer. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Cues")
	bool bFollowOwnerLifetime = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	friend class FBwayCuePresentationLifecycleTest;
	void InitializeVisual();
	void ClearVisual();
	void StartOwnerLifetimeCue();
	void StopOwnerLifetimeCue();
	FBwayCuePresentationState Presentation;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> VisualComponent;
	TWeakObjectPtr<USkeletalMeshComponent> ReplacedMesh;
	bool bReplacedMeshWasVisible = true;
	bool bOwnerLifetimeCueActive = false;
};
