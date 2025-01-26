// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BwayThrowable.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BREAKAWAYCORERUNTIME_API UBwayThrowable : public UActorComponent
{
	GENERATED_BODY()

public:
	UBwayThrowable();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual void Throw(FVector ThrowDirection, float Strength);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
	float DefaultThrowStrength = 1000.0f;

protected:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerThrow(FVector ThrowDirection, float Strength);
};