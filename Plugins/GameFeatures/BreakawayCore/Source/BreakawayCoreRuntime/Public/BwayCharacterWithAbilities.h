// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Character/LyraCharacter.h"
#include "Net/UnrealNetwork.h"
#include "BwayCharacterWithAbilities.generated.h"

class UCameraShakeBase;
class UAnimInstance;
class UAnimMontage;
class UStaticMeshComponent;
class APlayerCameraManager;
class APlayerController;
class UBwayRelicRequestIndicatorComponent;
class ABwayPlayerState;
class ULyraHealthSet;
struct FGameplayEffectSpec;

UCLASS(config=Game)
class BREAKAWAYCORERUNTIME_API ABwayCharacterWithAbilities : public ALyraCharacter
{
	GENERATED_BODY()
#if !UE_BUILD_SHIPPING
	friend class ABwayAllKitCombatProbe;
#endif
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Movement)
	TObjectPtr<UBwayCharacterMovementComponent> BwayCharacterMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic")
	TObjectPtr<UBwayRelicRequestIndicatorComponent> RelicRequestIndicatorComponent;
public:
	explicit ABwayCharacterWithAbilities(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION(BlueprintPure) FORCEINLINE UBwayCharacterMovementComponent* GetBwayCharacterMovement() const { return BwayCharacterMovementComponent; }
	FCollisionQueryParams GetIgnoreCharacterParams() const;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team")
	FGameplayTag TeamTag;
    
	UFUNCTION(BlueprintCallable, Category = "Team")
	void UpdateAppearanceForTeam();

	// Method to try and pickup an overlapping relic
	UFUNCTION(BlueprintCallable, Category = "Relic")
	void TryPickupOverlappingRelic();

	UFUNCTION(BlueprintCallable)
	void InitializeHeroData(const UBwayHeroDataAsset* HeroData);

	/** Cosmetic one-shot playback for an already delivered cue; the caller supplies its replaceable montage asset. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Presentation")
	bool PlayPrimaryActionMontage(UAnimMontage* Montage, float PlayRate = 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Data")
	TObjectPtr<const UBwayHeroDataAsset> HeroDataAsset;

	virtual void Tick(float DeltaSeconds) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;

	virtual void OnDeathStarted(AActor* OwningActor) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Legacy presentation hint. Combat awards are derived from authoritative health changes. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetLastDamageInstigator(AActor* InInstigator) { LastDamageInstigator = InInstigator; }

	/** Consume this pawn's death once. Called by the active round before its results snapshot. */
	bool ConsumeCombatCredit(ABwayPlayerState*& OutKiller, TArray<ABwayPlayerState*>& OutAssisters);
#if !UE_BUILD_SHIPPING
	int32 GetCombatContributionCountForAudit() const { return CombatContributors.Num(); }
	int32 GetConsumedAssisterCountForAudit() const { return ConsumedAssisterCountForAudit; }
private:
	int32 ConsumedAssisterCountForAudit = INDEX_NONE;
public:
#endif

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UCameraShakeBase> ActiveSlideCameraShake;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerCameraManager> SlideCameraShakeManager;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> SlideCameraShakeController;

	bool bSlideCameraShakeStopping = false;

	/** Current FOV offset being applied. Interpolates towards the target FOV based on slide speed. */
	float CurrentSlideFOVOffset = 0.0f;

	/** Legacy damage source hint; never sufficient to award credit. */
	UPROPERTY()
	TWeakObjectPtr<AActor> LastDamageInstigator;

	/** Replicated hero ID — clients use this to apply visuals */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedHeroId)
	FPrimaryAssetId ReplicatedHeroId;

	UFUNCTION()
	void OnRep_ReplicatedHeroId();

	/** Apply visual-only hero data (mesh/anim). Called on both server and clients. */
	void ApplyHeroVisuals(const UBwayHeroDataAsset* HeroData);

	void BindTeamAppearanceListener();
	void ClearRelicRequestForCurrentPawn();

	UFUNCTION()
	void HandleTeamAppearanceChanged(UObject* ObjectChangingTeam, int32 OldTeamID, int32 NewTeamID);

	bool bBoundTeamAppearanceListener = false;

	virtual void OnAbilitySystemInitialized() override;
	virtual void OnAbilitySystemUninitialized() override;

private:
	void StopPrimaryActionMontage(bool bImmediately, const TCHAR* Reason);

	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimInstance> PrimaryActionAnimInstance;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimMontage> PrimaryActionMontage;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeroEquipmentMeshComponent;

	int32 PrimaryActionMontageInstanceId = INDEX_NONE;
	bool bStartingPrimaryActionMontage = false;

	void UpdateSlideCameraShake();
	void StopSlideCameraShake(bool bImmediately);

	struct FCombatContribution
	{
		double LastDamageTime = 0.0;
		int32 Team = INDEX_NONE;
	};
	TMap<TWeakObjectPtr<ABwayPlayerState>, FCombatContribution> CombatContributors;
	TWeakObjectPtr<ABwayPlayerState> LastDamagePlayerState;
	TWeakObjectPtr<const ULyraHealthSet> CombatHealthSet;
	FDelegateHandle CombatHealthChangedHandle;
	int32 CombatRound = INDEX_NONE;
	int32 CombatVictimTeam = INDEX_NONE;
	bool bLastDamageWasLethal = false;
	bool bCombatDeathConsumed = false;

	void UnbindCombatHealth();
	void HandleCombatHealthChanged(AActor* DamageInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
		float Magnitude, float OldHealth, float NewHealth);
};
