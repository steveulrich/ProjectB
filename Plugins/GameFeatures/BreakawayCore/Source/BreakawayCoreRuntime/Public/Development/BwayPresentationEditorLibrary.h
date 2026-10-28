#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayPresentationEditorLibrary.generated.h"

class UNiagaraSystem;

/** Narrow editor authoring helpers. Never saves packages or changes runtime instances. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayPresentationEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Reads an existing User.* float. Returns false outside the editor or during PIE. */
	UFUNCTION(BlueprintCallable, Category="Breakaway|Editor|Presentation")
	static bool ReadExistingNiagaraUserFloat(UNiagaraSystem* System, FName ParameterName, float& OutValue);

	/** Changes only an existing User.* float, joins the caller's transaction, and marks its package dirty. */
	UFUNCTION(BlueprintCallable, Category="Breakaway|Editor|Presentation")
	static bool SetExistingNiagaraUserFloat(UNiagaraSystem* System, FName ParameterName, float Value,
		float& OutPreviousValue, float& OutReadbackValue);

	/** Audits one exact compact-hit candidate; optional apply sets only .12s lifetime, 12cm size and burst count 3.
	 * Never saves or binds consumers. Apply requests asynchronous compilation; a later audit must confirm it finished.
	 */
	UFUNCTION(BlueprintCallable, Category="Breakaway|Editor|Presentation")
	static bool ConfigureCompactHitConfirm(UNiagaraSystem* System, bool bApply, FString& OutReport);

	/** Creates only the absent compact-hit candidate through Niagara's factory and template-copy utilities; never saves. */
	UFUNCTION(BlueprintCallable, Category="Breakaway|Editor|Presentation")
	static UNiagaraSystem* CreateCompactHitConfirmCandidate(FString& OutReport);
};
