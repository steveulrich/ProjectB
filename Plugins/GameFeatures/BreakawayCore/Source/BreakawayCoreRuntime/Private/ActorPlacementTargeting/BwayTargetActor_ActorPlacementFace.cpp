// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorPlacementTargeting/BwayTargetActor_ActorPlacementFace.h"
#include "ActorPlacementTargeting/BwayWorldReticle_ActorVisualization.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Buildable/BwayPlacementValidation.h"
#include "Buildable/BuildableBase.h"
#include "Engine/World.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "Engine/EngineTypes.h"
#include "Abilities/GameplayAbilityWorldReticle_ActorVisualization.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayTargetActor_ActorPlacementFace)

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	AGATargetActor_ActorPlacementFace
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

ABwayTargetActor_ActorPlacementFace::ABwayTargetActor_ActorPlacementFace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true; // Enable tick for continuous validation feedback
	// MaxPlacementAngleDegrees, ValidPlacementMaterial, InvalidPlacementMaterial will be set on instance or via ExposeOnSpawn
}

void ABwayTargetActor_ActorPlacementFace::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle->Destroy();
		ActorVisualizationReticle = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayTargetActor_ActorPlacementFace::StartTargeting(UGameplayAbility* InAbility)
{
	if (!BuildableDataAsset && InAbility)
	{
		if (APlayerController* PC = InAbility->GetCurrentActorInfo()->PlayerController.Get())
		{
			BuildableDataAsset = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(GetWorld(), PC);
		}
	}

	if (!PlacedActorClass)
	{
		if (BuildableDataAsset)
		{
			PlacedActorClass = BuildableDataAsset->BuildableActorClass;
		}
		else if (InAbility)
		{
			if (APlayerController* PC = InAbility->GetCurrentActorInfo()->PlayerController.Get())
			{
				if (UBwayBuildableDataAsset* ResolvedBuildableData = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(GetWorld(), PC))
				{
					BuildableDataAsset = ResolvedBuildableData;
					PlacedActorClass = ResolvedBuildableData->BuildableActorClass;
				}
			}
		}
	}

	Super::StartTargeting(InAbility); // This will create ReticleActor (often a decal)

	// WaitTargetData only forwards LocalInputConfirm/Cancel when ShouldProduceTargetData() is true,
	// which for the base target actor requires PrimaryPC->IsLocalController(). Repair if missing.
	if (!PrimaryPC)
	{
		if (APawn* SourcePawn = Cast<APawn>(SourceActor.Get()))
		{
			PrimaryPC = Cast<APlayerController>(SourcePawn->GetController());
		}
		if (!PrimaryPC && InAbility && InAbility->GetCurrentActorInfo())
		{
			PrimaryPC = InAbility->GetCurrentActorInfo()->PlayerController.Get();
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("BwayPlacementTarget StartTargeting PrimaryPC=%s SourceActor=%s ShouldProduce=%d"),
		*GetNameSafe(PrimaryPC),
		*GetNameSafe(SourceActor.Get()),
		ShouldProduceTargetData() ? 1 : 0);

	InitializePlacementVisualization(InAbility);
}

bool ABwayTargetActor_ActorPlacementFace::ShouldProduceTargetData() const
{
	if (PrimaryPC && PrimaryPC->IsLocalController())
	{
		return true;
	}

	if (const APawn* SourcePawn = Cast<APawn>(SourceActor.Get()))
	{
		if (const APlayerController* PC = Cast<APlayerController>(SourcePawn->GetController()))
		{
			return PC->IsLocalController();
		}
	}

	return Super::ShouldProduceTargetData();
}

void ABwayTargetActor_ActorPlacementFace::ConfirmTargeting()
{
	UE_LOG(LogTemp, Log,
		TEXT("BwayPlacementTarget ConfirmTargeting ShouldProduce=%d PrimaryPC=%s SourceActor=%s bLastValid=%d"),
		ShouldProduceTargetData() ? 1 : 0,
		*GetNameSafe(PrimaryPC),
		*GetNameSafe(SourceActor.Get()),
		bLastTickPlacementValid ? 1 : 0);

	Super::ConfirmTargeting();
}

void ABwayTargetActor_ActorPlacementFace::CancelTargeting()
{
	UE_LOG(LogTemp, Log, TEXT("BwayPlacementTarget CancelTargeting ShouldProduce=%d"), ShouldProduceTargetData() ? 1 : 0);
	Super::CancelTargeting();
}

void ABwayTargetActor_ActorPlacementFace::InitializePlacementVisualization(UGameplayAbility* InAbility)
{
	if (!TryInitializeVisualizationFromBuildableData(InAbility))
	{
		if (!TryInitializeVisualizationFromActorClass())
		{
			UE_LOG(LogTemp, Warning, TEXT("ABwayTargetActor_ActorPlacementFace: No preview mesh available. Set PrimaryBuildableMesh on the buildable data asset or mesh on %s."),
				PlacedActorClass ? *PlacedActorClass->GetName() : TEXT("(null class)"));
		}
	}

	bLastTickPlacementValid = false;
	if (ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle->UpdatePlacementVisuals(bLastTickPlacementValid);
	}
}

bool ABwayTargetActor_ActorPlacementFace::TryInitializeVisualizationFromBuildableData(UGameplayAbility* InAbility)
{
	APlayerController* PC = InAbility ? InAbility->GetCurrentActorInfo()->PlayerController.Get() : nullptr;
	USkeletalMesh* PreviewMesh = UBwayBuildablePlacementLibrary::ResolvePreviewMeshForPlayer(GetWorld(), PC, BuildableDataAsset);
	if (!PreviewMesh)
	{
		return false;
	}

	if (!ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle = GetWorld()->SpawnActor<ABwayWorldReticle_ActorVisualization>();
	}

	if (!ActorVisualizationReticle.IsValid())
	{
		return false;
	}

	ActorVisualizationReticle->InitializeReticleVisualizationFromPreviewMesh(SourceActor.Get(), PreviewMesh, ValidPlacementMaterial, InvalidPlacementMaterial);

	if (AGameplayAbilityWorldReticle* ParentReticle = ReticleActor.Get())
	{
		ActorVisualizationReticle->AttachToActor(ParentReticle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		ActorVisualizationReticle->SetIsReticleVisible(true);
	}
	else
	{
		ReticleActor = ActorVisualizationReticle;
		ActorVisualizationReticle->SetIsReticleVisible(true);
	}

	return true;
}

bool ABwayTargetActor_ActorPlacementFace::TryInitializeVisualizationFromActorClass()
{
	if (!PlacedActorClass)
	{
		return false;
	}

	AActor* VisualizationActor = GetWorld()->SpawnActor(PlacedActorClass);
	if (!VisualizationActor)
	{
		return false;
	}

	if (!ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle = GetWorld()->SpawnActor<ABwayWorldReticle_ActorVisualization>();
	}

	if (ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle->InitializeReticleVisualizationInformation(SourceActor.Get(), VisualizationActor, ValidPlacementMaterial, InvalidPlacementMaterial);

		if (AGameplayAbilityWorldReticle* ParentReticle = ReticleActor.Get())
		{
			ActorVisualizationReticle->AttachToActor(ParentReticle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			ActorVisualizationReticle->SetIsReticleVisible(true);
		}
		else
		{
			ReticleActor = ActorVisualizationReticle;
			ActorVisualizationReticle->SetIsReticleVisible(true);
		}
	}

	GetWorld()->DestroyActor(VisualizationActor);
	return ActorVisualizationReticle.IsValid();
}

FHitResult ABwayTargetActor_ActorPlacementFace::PerformTrace(AActor* InSourceActor)
{
	// Get the initial ground trace from the parent
	FHitResult OriginalGroundHit = Super::PerformTrace(InSourceActor);

	// Perform our detailed validation
	LastValidatedPlacementResult = PerformPlacementValidation(OriginalGroundHit);

	bLastTickPlacementValid = LastValidatedPlacementResult.bIsValid;
	
	if (bLastTickPlacementValid)
	{
		// If valid, return a hit result that uses our adjusted location.
		// The base class's reticle (decal) will snap to this.
		FHitResult AdjustedHit = OriginalGroundHit;
		AdjustedHit.Location = LastValidatedPlacementResult.AdjustedLocation;
		AdjustedHit.ImpactPoint = LastValidatedPlacementResult.AdjustedLocation;
		// Note: ImpactNormal might ideally be re-evaluated at AdjustedLocation if it moved significantly.
		// For now, we assume OriginalGroundHit.ImpactNormal is sufficient for decal orientation.
		return AdjustedHit;
	}
	
	// If not valid by our rules, return the original hit. The decal will be there, but our visual will be "invalid".
	return OriginalGroundHit;
}

void ABwayTargetActor_ActorPlacementFace::ConfirmTargetingAndContinue()
{
	check(ShouldProduceTargetData());
	if (SourceActor)
	{
		bDebug = false;
		// Confirm from fresh ground, not the already pivot-lifted visual location.
		const FHitResult TraceHit = Super::PerformTrace(SourceActor);
		const FGameplayAbilityTargetDataHandle Handle = CreateTargetData(TraceHit);
		if (Handle.IsValid(0))
		{
			UE_LOG(LogTemp, Log, TEXT("BwayPlacementTarget ConfirmTargetingAndContinue VALID — broadcasting target data"));
			TargetDataReadyDelegate.Broadcast(Handle);
		}
		else
		{
			// Invalid confirm: do not broadcast — WaitTargetData stays active so the player can reposition and retry.
			UE_LOG(LogTemp, Warning,
				TEXT("BwayPlacementTarget ConfirmTargetingAndContinue INVALID — no broadcast (preview stays). bLastValid=%d Hit=%d Actor=%s"),
				bLastTickPlacementValid ? 1 : 0,
				TraceHit.bBlockingHit ? 1 : 0,
				*GetNameSafe(TraceHit.GetActor()));
		}
	}
}

// Override Tick to update the material AFTER Super::Tick() has positioned the reticle based on NewTraceResult's output
void ABwayTargetActor_ActorPlacementFace::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); // This will call AimWithPlayerController -> NewTraceResult, and position ReticleActor

	if (ActorVisualizationReticle.IsValid())
	{
		// bLastTickPlacementValid was set by our NewTraceResult override.
		// Also check if the parent reticle itself is considered valid by the trace logic.
		ActorVisualizationReticle->UpdatePlacementVisuals(bLastTickPlacementValid);
	}
}

FGameplayAbilityTargetDataHandle ABwayTargetActor_ActorPlacementFace::CreateTargetData(
	const FHitResult& InHitResult) const
{
	// Re-validate at confirm time (do not trust only the last tick cache).
	const FPlacementValidationResult Validation = PerformPlacementValidation(InHitResult);
	bLastTickPlacementValid = Validation.bIsValid;
	LastValidatedPlacementResult = Validation;

	if (Validation.bIsValid && Validation.OriginalGroundHit.bBlockingHit)
	{
		FGameplayAbilityTargetData_SingleTargetHit* ReturnData = new FGameplayAbilityTargetData_SingleTargetHit();

		FHitResult FinalHit = Validation.OriginalGroundHit;
		// Submit the ground intent. Authority computes its own geometry and pivot lift.
		FinalHit.Location = Validation.OriginalGroundHit.ImpactPoint;
		FinalHit.ImpactPoint = Validation.OriginalGroundHit.ImpactPoint;
		ReturnData->HitResult = FinalHit;

		return FGameplayAbilityTargetDataHandle(ReturnData);
	}

	// Invalid confirm: empty handle — WaitTargetData stays alive (see ConfirmTargetingAndContinue).
	return FGameplayAbilityTargetDataHandle();
}

FBwayPlacementQuery ABwayTargetActor_ActorPlacementFace::MakePlacementQuery(AActor* InSourceActor, const APlayerController* PC) const
{
    FBwayPlacementQuery Query;
    Query.World = InSourceActor ? InSourceActor->GetWorld() : nullptr;
    Query.Source = InSourceActor;
    Query.PlacedClass = PlacedActorClass;
    Query.AuthoredBounds = BuildableDataAsset ? BuildableDataAsset->GetPlacementLocalBounds() : FBox(ForceInit);
    Query.AimDirection = PC ? PC->GetControlRotation().Vector() : (InSourceActor ? InSourceActor->GetActorForwardVector() : FVector::ForwardVector);
    Query.CollisionChannel = CollisionChannel;
    Query.MaxRange = MaxRange;
    Query.MaxSlopeDegrees = MaxPlacementAngleDegrees;
    Query.MaxFootprintZDifference = MaxFootprintZDifference;
    Query.TraceUp = FootprintTraceUpOffset;
    Query.TraceDown = FootprintTraceDownLength;
    Query.GroundClearance = ObstructionGroundClearance;
    if (!HasAnyFlags(RF_ClassDefaultObject)) Query.IgnoredActors.Add(this);
    if (ActorVisualizationReticle.IsValid()) Query.IgnoredActors.Add(ActorVisualizationReticle.Get());
    if (ReticleActor.IsValid()) Query.IgnoredActors.Add(ReticleActor.Get());
    return Query;
}

FPlacementValidationResult ABwayTargetActor_ActorPlacementFace::PerformPlacementValidation(const FHitResult& GroundHit) const
{
    const APlayerController* PC = OwningAbility && OwningAbility->GetCurrentActorInfo()
        ? OwningAbility->GetCurrentActorInfo()->PlayerController.Get() : nullptr;
    return MakePlacementQuery(SourceActor.Get(), PC).ValidateGroundHit(GroundHit);
}
