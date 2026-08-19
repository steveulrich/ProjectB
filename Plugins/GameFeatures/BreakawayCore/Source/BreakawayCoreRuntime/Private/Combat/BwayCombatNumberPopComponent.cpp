// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayCombatNumberPopComponent.h"

#include "Combat/BwayCombatFeedbackTags.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCombatNumberPopComponent)

UBwayCombatNumberPopComponent::UBwayCombatNumberPopComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumberNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/Effects/Particles/Impacts/NS_DamageNumbers.NS_DamageNumbers")));
}

void UBwayCombatNumberPopComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	EnsureConfig();

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	IncomingDamageHandle = MessageSubsystem.RegisterListener(
		BwayCombatFeedbackTags::Number_IncomingDamage, this, &ThisClass::OnCombatNumberMessage);
	OutgoingDamageHandle = MessageSubsystem.RegisterListener(
		BwayCombatFeedbackTags::Number_OutgoingDamage, this, &ThisClass::OnCombatNumberMessage);
	IncomingHealHandle = MessageSubsystem.RegisterListener(
		BwayCombatFeedbackTags::Number_IncomingHeal, this, &ThisClass::OnCombatNumberMessage);
}

void UBwayCombatNumberPopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.UnregisterListener(IncomingDamageHandle);
		MessageSubsystem.UnregisterListener(OutgoingDamageHandle);
		MessageSubsystem.UnregisterListener(IncomingHealHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UBwayCombatNumberPopComponent::EnsureConfig() const
{
	if (!CachedConfig)
	{
		CachedConfig = UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(this);
	}
}

FVector UBwayCombatNumberPopComponent::ResolvePopLocation(const FLyraVerbMessage& Payload) const
{
	FVector Offset = CachedConfig ? CachedConfig->NumberWorldOffset : FVector(0.f, 0.f, 90.f);

	if (const AActor* TargetActor = Cast<AActor>(Payload.Target))
	{
		return TargetActor->GetActorLocation() + Offset;
	}

	if (const APlayerState* PS = Cast<APlayerState>(Payload.Target))
	{
		if (const APawn* Pawn = PS->GetPawn())
		{
			return Pawn->GetActorLocation() + Offset;
		}
	}

	return Offset;
}

FLinearColor UBwayCombatNumberPopComponent::ResolveColor(const FGameplayTagContainer& TargetTags) const
{
	EnsureConfig();
	if (!CachedConfig)
	{
		return FLinearColor::White;
	}

	if (TargetTags.HasTag(BwayCombatFeedbackTags::Number_IncomingDamage))
	{
		return CachedConfig->ResolveIncomingDamageColor();
	}
	if (TargetTags.HasTag(BwayCombatFeedbackTags::Number_IncomingHeal))
	{
		return CachedConfig->ResolveIncomingHealColor();
	}
	if (TargetTags.HasTag(BwayCombatFeedbackTags::Number_OutgoingDamage))
	{
		return CachedConfig->ResolveOutgoingDamageColor();
	}

	return CachedConfig->ResolveOutgoingDamageColor();
}

UNiagaraComponent* UBwayCombatNumberPopComponent::GetOrCreateNiagaraComponent()
{
	if (NiagaraComp)
	{
		return NiagaraComp;
	}

	UNiagaraSystem* System = NumberNiagaraSystem.LoadSynchronous();
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBwayCombatNumberPopComponent: Failed to load Niagara system %s"),
			*NumberNiagaraSystem.ToString());
		return nullptr;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	NiagaraComp = NewObject<UNiagaraComponent>(OwnerActor);
	NiagaraComp->SetAsset(System);
	NiagaraComp->bAutoActivate = false;
	NiagaraComp->SetupAttachment(nullptr);
	NiagaraComp->RegisterComponent();
	return NiagaraComp;
}

void UBwayCombatNumberPopComponent::AddNumberPop(const FLyraNumberPopRequest& NewRequest)
{
	APlayerController* PC = GetController<APlayerController>();
	if (PC && !PC->IsLocalController())
	{
		return;
	}

	UNiagaraComponent* Comp = GetOrCreateNiagaraComponent();
	if (!Comp)
	{
		return;
	}

	const FLinearColor Color = ResolveColor(NewRequest.TargetTags);
	Comp->SetVariableLinearColor(NiagaraColorParamName, Color);
	Comp->SetWorldLocation(NewRequest.WorldLocation);
	Comp->Activate(false);

	int32 LocalDamage = NewRequest.NumberToDisplay;
	if (NewRequest.bIsCriticalDamage)
	{
		LocalDamage *= -1;
	}

	TArray<FVector4> DamageList =
		UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayVector4(Comp, NiagaraArrayName);
	DamageList.Add(FVector4(
		NewRequest.WorldLocation.X,
		NewRequest.WorldLocation.Y,
		NewRequest.WorldLocation.Z,
		static_cast<float>(LocalDamage)));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Comp, NiagaraArrayName, DamageList);
}

void UBwayCombatNumberPopComponent::OnCombatNumberMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	const int32 Magnitude = FMath::RoundToInt(static_cast<float>(Payload.Magnitude));
	if (Magnitude <= 0)
	{
		return;
	}

	EnsureConfig();

	FLyraNumberPopRequest Request;
	Request.WorldLocation = ResolvePopLocation(Payload);
	Request.NumberToDisplay = Magnitude;
	Request.bIsCriticalDamage = false;
	Request.TargetTags.AddTag(Channel);
	Request.SourceTags = Payload.InstigatorTags;

	AddNumberPop(Request);
}
