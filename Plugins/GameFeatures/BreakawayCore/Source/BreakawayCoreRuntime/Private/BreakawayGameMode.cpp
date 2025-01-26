// Fill out your copyright notice in the Description page of Project Settings.


#include "BreakawayGameMode.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogBreakawayGame);

ABreakawayGameMode::ABreakawayGameMode()
{

}

void ABreakawayGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogBreakawayGame, Warning, TEXT("Breakaway GameMode Loaded"));
}


