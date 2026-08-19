// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Character/LyraCharacter.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "Net/UnrealNetwork.h"
#include "BwayCharacterWithAbilities.generated.h"

class UCameraShakeBase;

UCLASS(config=Game)
class BREAKAWAYCORERUNTIME_API ABwayCharacterWithAbilities : public ALyraCharacter
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Movement)
	TObjectPtr<UBwayCharacterMovementComponent> BwayCharacterMovementComponent;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Data")
	TObjectPtr<const UBwayHeroDataAsset> HeroDataAsset;

	virtual void Tick(float DeltaSeconds) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual void OnDeathStarted(AActor* OwningActor) override;

	/** Set the last actor that dealt damage to this character. Called from health component callbacks. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetLastDamageInstigator(AActor* InInstigator) { LastDamageInstigator = InInstigator; }

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraShakeBase> ActiveSlideCameraShake;

	/** Current FOV offset being applied. Interpolates towards the target FOV based on slide speed. */
	float CurrentSlideFOVOffset = 0.0f;

	/** The last actor that dealt damage to us — used for kill attribution */
	UPROPERTY()
	TWeakObjectPtr<AActor> LastDamageInstigator;

	/** Handles for hero ability sets granted via InitializeHeroData (cleared on death to prevent double-grant) */
	FLyraAbilitySet_GrantedHandles HeroAbilityGrantedHandles;

	/** Replicated hero ID — clients use this to apply visuals */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedHeroId)
	FPrimaryAssetId ReplicatedHeroId;

	UFUNCTION()
	void OnRep_ReplicatedHeroId();

	/** Apply visual-only hero data (mesh/anim). Called on both server and clients. */
	void ApplyHeroVisuals(const UBwayHeroDataAsset* HeroData);

	void BindTeamAppearanceListener();

	UFUNCTION()
	void HandleTeamAppearanceChanged(UObject* ObjectChangingTeam, int32 OldTeamID, int32 NewTeamID);

	bool bBoundTeamAppearanceListener = false;

	virtual void OnAbilitySystemInitialized() override;
	virtual void OnAbilitySystemUninitialized() override;
};
