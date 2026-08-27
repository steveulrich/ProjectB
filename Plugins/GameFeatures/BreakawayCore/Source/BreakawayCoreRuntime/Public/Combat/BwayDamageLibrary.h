#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayDamageLibrary.generated.h"

class AActor;
class ABwayCharacterWithAbilities;
class UAbilitySystemComponent;
struct FGameplayEffectContextHandle;

/**
 * Shared Breakaway damage application.
 * Applies sheet armor (1 Armor = 1% reduction; negative Armor increases damage)
 * and IncomingDamageMultiplier, then applies UGE_Bway_Damage_SetByCaller
 * (pure SetByCaller→Damage — not Lyra's execution-based damage GE).
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayDamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Final = RawDamage * (1 - Armor*0.01) * IncomingDamageMultiplier, floored at 0. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Combat")
	static float ResolveFinalDamage(float RawDamage, const UAbilitySystemComponent* TargetASC);

	/** Apply SetByCaller damage to TargetActor using SourceASC (server authoritative). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Combat")
	static bool ApplyDamageFromSource(
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor,
		float RawDamage,
		AActor* EffectCauser = nullptr,
		AActor* InstigatorActor = nullptr);

	/** Convenience: resolve ASC from InstigatorCharacter and apply to Enemy. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Combat")
	static bool ApplyDamageToEnemy(
		ABwayCharacterWithAbilities* InstigatorCharacter,
		ABwayCharacterWithAbilities* Enemy,
		float RawDamage,
		AActor* EffectCauser = nullptr);
};
