// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/BwayGoldAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGoldAttributeSet)

UBwayGoldAttributeSet::UBwayGoldAttributeSet()
{
	InitCurrentGold(0.0f);
	InitMaxGold(500.0f);
	InitGoldPerSecond(2.0f);
}

void UBwayGoldAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBwayGoldAttributeSet, CurrentGold, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBwayGoldAttributeSet, MaxGold, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBwayGoldAttributeSet, GoldPerSecond, COND_None, REPNOTIFY_Always);
}

void UBwayGoldAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp CurrentGold between 0 and MaxGold
	if (Attribute == GetCurrentGoldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxGold());
	}
}

void UBwayGoldAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetCurrentGoldAttribute())
	{
		// Clamp final value
		SetCurrentGold(FMath::Clamp(GetCurrentGold(), 0.0f, GetMaxGold()));
	}
}

void UBwayGoldAttributeSet::OnRep_CurrentGold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBwayGoldAttributeSet, CurrentGold, OldValue);
}

void UBwayGoldAttributeSet::OnRep_MaxGold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBwayGoldAttributeSet, MaxGold, OldValue);
}

void UBwayGoldAttributeSet::OnRep_GoldPerSecond(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBwayGoldAttributeSet, GoldPerSecond, OldValue);
}
