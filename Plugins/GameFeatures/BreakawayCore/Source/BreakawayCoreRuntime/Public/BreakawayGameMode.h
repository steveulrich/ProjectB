// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BreakawayGameMode.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBreakawayGame, Log, All);

/**
 * 
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABreakawayGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ABreakawayGameMode();
	virtual void BeginPlay() override;
	
	
};
