// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacterWithAbilities.h"
#include "BwayCharacterWithAbilities.generated.h"

UCLASS(config=Game)
class BREAKAWAYCORERUNTIME_API ABwayCharacterWithAbilities : public ALyraCharacterWithAbilities
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Movement) TObjectPtr<UBwayCharacterMovementComponent> BwayCharacterMovementComponent;
public:
	explicit ABwayCharacterWithAbilities(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure) FORCEINLINE UBwayCharacterMovementComponent* GetBwayCharacterMovement() const { return BwayCharacterMovementComponent; }
	FCollisionQueryParams GetIgnoreCharacterParams() const;
	
	// Team functionality
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Team")
	int32 TeamId;
    
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team")
	FGameplayTag TeamTag;
    
	// Get team ID
	UFUNCTION(BlueprintPure, Category = "Team")
	int32 GetTeamId() const { return TeamId; }
    
	// Set team ID
	UFUNCTION(BlueprintCallable, Category = "Team")
	void SetTeamId(int32 NewTeamId);

	UFUNCTION(BlueprintCallable, Category = "Team")
	void UpdateAppearanceForTeam();

	
};
