#include "Buildable/BwayBuildablePlacementLibrary.h"

#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/KismetMathLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBuildablePlacementLibrary)

ABuildableActor* UBwayBuildablePlacementLibrary::SpawnBuildableForPlayer(const UObject* WorldContextObject, APlayerController* PlayerController, UBwayBuildableDataAsset* BuildableData, const FTransform& SpawnTransform)
{
	FText FailureReason;
	if (!CanPlayerPlaceBuildable(WorldContextObject, PlayerController, BuildableData, FailureReason))
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !World->GetAuthGameMode())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerController;
	SpawnParams.Instigator = PlayerController ? PlayerController->GetPawn() : nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	ABuildableActor* Buildable = World->SpawnActor<ABuildableActor>(BuildableData->BuildableActorClass, SpawnTransform, SpawnParams);
	if (!Buildable)
	{
		return nullptr;
	}

	int32 TeamIndex = -1;
	if (const ABwayGameState* BwayGS = World->GetGameState<ABwayGameState>())
	{
		TeamIndex = BwayGS->GetPlayerTeam(PlayerController ? PlayerController->PlayerState : nullptr);
	}

	ApplyResolvedBuildableMesh(Buildable, BuildableData, GetBuildableCosmeticIndexForPlayer(PlayerController));

	Buildable->InitializeBuildable(PlayerController, TeamIndex);

	if (ABwayPlayerState* BwayPS = PlayerController ? PlayerController->GetPlayerState<ABwayPlayerState>() : nullptr)
	{
		BwayPS->MarkBuildablePlacedThisRound();
	}

	return Buildable;
}

bool UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable(const UObject* WorldContextObject, const APlayerController* PlayerController, const UBwayBuildableDataAsset* BuildableData, FText& OutFailureReason)
{
	if (!PlayerController || !BuildableData || !BuildableData->BuildableActorClass)
	{
		OutFailureReason = NSLOCTEXT("BreakawayBuildables", "InvalidPlacementData", "Invalid buildable placement data.");
		return false;
	}

	if (ABwayPlayerState* BwayPS = PlayerController->GetPlayerState<ABwayPlayerState>())
	{
		if (BwayPS->HasPlacedBuildableThisRound())
		{
			OutFailureReason = NSLOCTEXT("BreakawayBuildables", "AlreadyPlacedThisRound", "You already placed a buildable this round.");
			return false;
		}
	}
	else
	{
		OutFailureReason = NSLOCTEXT("BreakawayBuildables", "PlayerStateMissing", "Player state is not initialized.");
		return false;
	}

	if (BuildableData->MaxActiveBuildablesPerPlayer > 0 && CountActiveBuildablesForPlayer(WorldContextObject, PlayerController) >= BuildableData->MaxActiveBuildablesPerPlayer)
	{
		OutFailureReason = NSLOCTEXT("BreakawayBuildables", "BuildableCapReached", "Maximum active buildables reached.");
		return false;
	}

	OutFailureReason = FText::GetEmpty();
	return true;
}

int32 UBwayBuildablePlacementLibrary::CountActiveBuildablesForPlayer(const UObject* WorldContextObject, const APlayerController* PlayerController)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !PlayerController)
	{
		return 0;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		if (const UBwayBuildableRegistryComponent* Registry = GameState->BuildableRegistryComponent)
		{
			return Registry->GetBuildableCountForPlayer(PlayerController->PlayerState);
		}
	}

	int32 Count = 0;
	for (TActorIterator<ABuildableActor> It(World); It; ++It)
	{
		const ABuildableActor* Buildable = *It;
		if (Buildable && Buildable->GetOwner() == PlayerController && !Buildable->IsActorBeingDestroyed())
		{
			++Count;
		}
	}
	return Count;
}

UBwayBuildableDataAsset* UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(const UObject* WorldContextObject, const APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return nullptr;
	}

	const ABwayPlayerState* BwayPS = PlayerController->GetPlayerState<ABwayPlayerState>();
	if (!BwayPS)
	{
		return nullptr;
	}

	const FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		return nullptr;
	}

	const UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
	if (!HeroData)
	{
		return nullptr;
	}

	return HeroData->GetBuildableDataAsset();
}

int32 UBwayBuildablePlacementLibrary::GetBuildableCosmeticIndexForPlayer(const APlayerController* PlayerController)
{
	// TODO: read selected buildable cosmetic from ABwayPlayerState when loadout/cosmetic UI lands.
	return 0;
}

USkeletalMesh* UBwayBuildablePlacementLibrary::ResolvePreviewMeshForPlayer(const UObject* WorldContextObject, const APlayerController* PlayerController, const UBwayBuildableDataAsset* BuildableData)
{
	if (!BuildableData)
	{
		return nullptr;
	}

	return BuildableData->ResolvePreviewMesh(GetBuildableCosmeticIndexForPlayer(PlayerController));
}

void UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(ABuildableActor* Buildable, const UBwayBuildableDataAsset* BuildableData, int32 CosmeticIndex)
{
	if (!Buildable || !BuildableData)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = Buildable->GetMesh();
	if (!MeshComponent)
	{
		return;
	}

	USkeletalMesh* MeshToApply = MeshComponent->GetSkeletalMeshAsset();
	if (!MeshToApply)
	{
		MeshToApply = BuildableData->ResolvePreviewMesh(CosmeticIndex);
		if (MeshToApply)
		{
			MeshComponent->SetSkeletalMesh(MeshToApply);
		}
	}

	if (MeshToApply && Buildable->HasAuthority())
	{
		Buildable->CommitReplicatedVisualMesh(MeshToApply);
	}
}

bool UBwayBuildablePlacementLibrary::MakePlacementTransformFromHit(const FHitResult& HitResult, const APlayerController* PlayerController, FTransform& OutTransform)
{
	if (!HitResult.bBlockingHit || !PlayerController)
	{
		return false;
	}

	FVector Forward = PlayerController->GetControlRotation().Vector();
	Forward = FVector::VectorPlaneProject(Forward, HitResult.ImpactNormal);
	if (Forward.IsNearlyZero())
	{
		Forward = PlayerController->GetPawn() ? PlayerController->GetPawn()->GetActorForwardVector() : FVector::ForwardVector;
		Forward = FVector::VectorPlaneProject(Forward, HitResult.ImpactNormal);
	}

	const FRotator PlacementRotation = UKismetMathLibrary::MakeRotFromZX(HitResult.ImpactNormal, Forward.GetSafeNormal());
	OutTransform = FTransform(PlacementRotation, HitResult.Location);
	return true;
}
