// Public/Interfaces/InteractionInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractionInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

class INTERACTIVEOBJECT_API IInteractionInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	bool CanInteractWith(AActor* InteractingActor) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnInteractionStarted(AActor* InteractingActor);

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnInteractionEnded(AActor* InteractingActor);
};
