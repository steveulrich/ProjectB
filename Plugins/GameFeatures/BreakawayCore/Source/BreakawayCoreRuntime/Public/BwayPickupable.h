// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BwayPickupable.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BREAKAWAYCORERUNTIME_API UBwayPickupable : public UActorComponent
{
	GENERATED_BODY()

	public:
		UBwayPickupable();

		UFUNCTION(BlueprintCallable, Category = "Interaction")
		virtual void PickUp(AActor* PickupActor);

		UFUNCTION(BlueprintCallable, Category = "Interaction")
		virtual void Drop();

		UPROPERTY(ReplicatedUsing = OnRep_HoldingActor)
		AActor* HoldingActor;

		UFUNCTION()
		virtual void OnRep_HoldingActor();

		virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	FName AttachSocketName;
	};