// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayCharacterWithAbilities.h"
#include "Camera/LyraCameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "BwayCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "AbilitySystemGlobals.h"
#include "BwayGameState.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameFramework/PlayerState.h"
#include "BwayPlayerState.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroStatsLibrary.h"
#include "Movement/BwayMovementFeelConfig.h"
#include "Animation/AnimInstance.h"
#include "Player/LyraPlayerState.h"
#include "Teams/LyraTeamDisplayAsset.h"
#include "Teams/LyraTeamSubsystem.h"
#include "Net/UnrealNetwork.h"


ABwayCharacterWithAbilities::ABwayCharacterWithAbilities(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UBwayCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	BwayCharacterMovementComponent = Cast<UBwayCharacterMovementComponent>(GetCharacterMovement());
	BwayCharacterMovementComponent->SetIsReplicated(true);

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = bUseControllerRotationYaw = bUseControllerRotationRoll = false;

}

void ABwayCharacterWithAbilities::OnAbilitySystemInitialized()
{
	Super::OnAbilitySystemInitialized();

	if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
	{
		MoveComp->BindAbilitySystem(GetAbilitySystemComponent());
	}
}

void ABwayCharacterWithAbilities::OnAbilitySystemUninitialized()
{
	if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
	{
		MoveComp->UnbindAbilitySystem();
	}

	if (HasAuthority())
	{
		if (ABwayPlayerState* BwayPS = GetPlayerState<ABwayPlayerState>())
		{
			if (ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent())
			{
				BwayPS->GetHeroAbilityGrantedHandles().TakeFromAbilitySystem(ASC);
			}
		}
	}

	Super::OnAbilitySystemUninitialized();
}

void ABwayCharacterWithAbilities::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwayCharacterWithAbilities, ReplicatedHeroId);
}

void ABwayCharacterWithAbilities::OnDeathStarted(AActor* OwningActor)
{
	Super::OnDeathStarted(OwningActor);
	if (HasAuthority())
	{
		if (ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>())
		{
			if (UBwayRoundManagementComponent* RoundMgmt = GS->FindComponentByClass<UBwayRoundManagementComponent>())
			{
				AController* VictimController = GetController();

				// Determine killer from the last damage instigator
				AController* KillerController = nullptr;
				if (LastDamageInstigator.IsValid())
				{
					if (APawn* InstigatorPawn = Cast<APawn>(LastDamageInstigator.Get()))
					{
						KillerController = InstigatorPawn->GetController();
					}
				}

				// Record stats on PlayerStates
				if (VictimController)
				{
					if (ABwayPlayerState* VictimPS = VictimController->GetPlayerState<ABwayPlayerState>())
					{
						VictimPS->AddDeath();
					}
				}
				if (KillerController && KillerController != VictimController)
				{
					if (ABwayPlayerState* KillerPS = KillerController->GetPlayerState<ABwayPlayerState>())
					{
						KillerPS->AddKill();
					}
				}

				// TODO: Assist tracking - need damage contribution system
				// For now, only direct killer gets credit

				RoundMgmt->OnPlayerDied(VictimController, KillerController);
			}

			if (UBwayRelicManagerComponent* RelicMgr = GS->FindComponentByClass<UBwayRelicManagerComponent>())
			{
				RelicMgr->DropRelicIfCarriedBy(this);
			}
		}
	}
}

void ABwayCharacterWithAbilities::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
	{
		ULyraCameraComponent* CameraComp = ULyraCameraComponent::FindCameraComponent(this);
		if (CameraComp)
		{
			// --- FOV Scaling ---
			const float TargetFOVOffset = MoveComp->IsSliding() ? (MoveComp->GetSlideIntensity() * MoveComp->SlideFOVOffsetMax) : 0.0f;
			CurrentSlideFOVOffset = FMath::FInterpTo(CurrentSlideFOVOffset, TargetFOVOffset, DeltaSeconds, MoveComp->SlideFOVInterpSpeed);

			if (FMath::Abs(CurrentSlideFOVOffset) > KINDA_SMALL_NUMBER)
			{
				CameraComp->AddFieldOfViewOffset(CurrentSlideFOVOffset);
			}

			// --- Camera Shake ---
			if (MoveComp->IsSliding() && MoveComp->SlideCameraShakeClass)
			{
				if (APlayerController* PC = Cast<APlayerController>(GetController()))
				{
					if (PC->PlayerCameraManager && !ActiveSlideCameraShake.IsValid())
					{
						// Scale the shake intensity by the slide intensity
						ActiveSlideCameraShake = PC->PlayerCameraManager->StartCameraShake(MoveComp->SlideCameraShakeClass, MoveComp->GetSlideIntensity());
					}
				}
			}
			else
			{
				ActiveSlideCameraShake.Reset();
			}
		}
	}
}

FCollisionQueryParams ABwayCharacterWithAbilities::GetIgnoreCharacterParams() const
{
	FCollisionQueryParams Params;

	TArray<AActor*> CharacterChildren;
	GetAllChildActors(CharacterChildren);
	Params.AddIgnoredActors(CharacterChildren);
	Params.AddIgnoredActor(this);

	return Params;
}

// Optional method to update character appearance based on team
void ABwayCharacterWithAbilities::UpdateAppearanceForTeam()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>();
	if (!TeamSubsystem)
	{
		return;
	}

	bool bIsPartOfTeam = false;
	int32 TeamId = INDEX_NONE;
	TeamSubsystem->FindTeamFromActor(this, bIsPartOfTeam, TeamId);
	if (!bIsPartOfTeam || TeamId == INDEX_NONE)
	{
		if (const ALyraPlayerState* LPS = GetPlayerState<ALyraPlayerState>())
		{
			TeamId = LPS->GetTeamId();
		}
	}

	if (TeamId == INDEX_NONE)
	{
		return;
	}

	// Prefer Lyra team display assets (Green=1 / Blue=2 for B_BW_TeamSetup_TwoTeams)
	// instead of hardcoded Blue/Red Breakaway indices that fought the nameplate colors.
	if (ULyraTeamDisplayAsset* DisplayAsset = TeamSubsystem->GetTeamDisplayAsset(TeamId, INDEX_NONE))
	{
		DisplayAsset->ApplyToMeshComponent(MeshComp);
	}
}

void ABwayCharacterWithAbilities::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindTeamAppearanceListener();
	UpdateAppearanceForTeam();
}

void ABwayCharacterWithAbilities::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindTeamAppearanceListener();
	UpdateAppearanceForTeam();
}

void ABwayCharacterWithAbilities::BindTeamAppearanceListener()
{
	if (bBoundTeamAppearanceListener)
	{
		return;
	}

	GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::HandleTeamAppearanceChanged);
	bBoundTeamAppearanceListener = true;
}

void ABwayCharacterWithAbilities::HandleTeamAppearanceChanged(UObject* /*ObjectChangingTeam*/, int32 /*OldTeamID*/, int32 /*NewTeamID*/)
{
	UpdateAppearanceForTeam();
}

void ABwayCharacterWithAbilities::TryPickupOverlappingRelic()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	// Use the character's capsule or another relevant component for overlap checks
	GetCapsuleComponent()->GetOverlappingActors(OverlappingActors, ARelicActor::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		ARelicActor* Relic = Cast<ARelicActor>(OverlappingActor);
		if (Relic && Relic->CanBePickedUpBy(this))
		{
			APlayerState* MyPlayerState = GetPlayerState();
			if (MyPlayerState)
			{
				ULyraAbilitySystemComponent* PlayerStateASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MyPlayerState));
				const URelicSettings* Settings = Relic->GetRelicSettings(); // Assuming a getter for RelicSettings
				const FGameplayTag PickupEventTag = (Settings && Settings->PickupEventTag.IsValid())
					? Settings->PickupEventTag
					: FGameplayTag::RequestGameplayTag(FName("Event.Interaction.PickupRelic"), /*ErrorIfNotFound*/ false);

				if (PlayerStateASC && PickupEventTag.IsValid())
				{
					FGameplayEventData Payload;
					Payload.EventTag = PickupEventTag;
					Payload.Instigator = this;
					Payload.Target = Relic; // Target is the relic

					PlayerStateASC->HandleGameplayEvent(Payload.EventTag, &Payload);
					UE_LOG(LogTemp, Log, TEXT("Server: Sent PickupRelic event from Character overlap check to PlayerState ASC of %s for Relic %s"), *GetNameSafe(MyPlayerState), *GetNameSafe(Relic));

					// Found a valid relic and sent the event, stop checking
					return;
				}
			}
		}
	}
}

void ABwayCharacterWithAbilities::InitializeHeroData(const UBwayHeroDataAsset* HeroData)
{
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: BEGIN for character %s"), *GetName());
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	
	check(HeroData);

	UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: HeroData = %s (DisplayName: %s)"), 
		*GetNameSafe(HeroData), *HeroData->DisplayName.ToString());

	HeroDataAsset = HeroData;

	// Set the replicated hero ID so clients can apply visuals via OnRep
	if (HasAuthority())
	{
		ReplicatedHeroId = HeroData->GetPrimaryAssetId();
	}

	// Apply visuals on the server (clients will apply via OnRep_ReplicatedHeroId)
	ApplyHeroVisuals(HeroData);

	// Abilities are server-only
	if (HasAuthority())
	{
		ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent();
		if (!ASC)
		{
			UE_LOG(LogTemp, Error, TEXT("InitializeHeroData: LyraAbilitySystemComponent is null for %s."), *GetName());
			UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: END (FAILED - No ASC)"));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: ASC = %s"), *GetNameSafe(ASC));

		// Ability Sets
		if (!HeroData->AbilitySets.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: Processing %d AbilitySets"), HeroData->AbilitySets.Num());

			ABwayPlayerState* BwayPS = GetPlayerState<ABwayPlayerState>();
			if (!BwayPS)
			{
				UE_LOG(LogTemp, Error, TEXT("InitializeHeroData: BwayPlayerState is null for %s — cannot track hero ability grants."), *GetName());
				UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: END (FAILED - No PlayerState)"));
				return;
			}

			FLyraAbilitySet_GrantedHandles& HeroAbilityGrantedHandles = BwayPS->GetHeroAbilityGrantedHandles();

			// Clear any previously granted hero abilities on the persistent ASC (survives pawn respawn).
			HeroAbilityGrantedHandles.TakeFromAbilitySystem(ASC);
			
			for (int32 i = 0; i < HeroData->AbilitySets.Num(); ++i)
			{
				const ULyraAbilitySet* Set = HeroData->AbilitySets[i];
				if (Set)
				{
					UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: Granting AbilitySet[%d] = %s"), i, *GetNameSafe(Set));

					// Grant the ability set (tracked on PlayerState for cleanup)
					Set->GiveToAbilitySystem(ASC, &HeroAbilityGrantedHandles);
					
					UE_LOG(LogTemp, Log, TEXT("InitializeHeroData: AbilitySet[%d] granted successfully"), i);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: AbilitySet[%d] is NULL!"), i);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: HeroData has NO AbilitySets configured!"));
		}

		// Log summary
		TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();
		UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: SUMMARY - Total activatable abilities on ASC: %d"), ActivatableAbilities.Num());
		
		for (const FGameplayAbilitySpec& Spec : ActivatableAbilities)
		{
			if (Spec.Ability)
			{
				FGameplayTagContainer DynamicTags = Spec.GetDynamicSpecSourceTags();
				FString TagsStr = DynamicTags.ToStringSimple();
				UE_LOG(LogTemp, Log, TEXT("  - %s [InputTags: %s]"), 
					*Spec.Ability->GetClass()->GetName(), 
					TagsStr.IsEmpty() ? TEXT("None") : *TagsStr);
			}
		}
	}

	UBwayHeroStatsLibrary::ApplyHeroStatsFromDataAsset(this, HeroData);

	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("InitializeHeroData: END for character %s"), *GetName());
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

void ABwayCharacterWithAbilities::ApplyHeroVisuals(const UBwayHeroDataAsset* HeroData)
{
	if (!HeroData)
	{
		return;
	}

	USkeletalMeshComponent* MainMesh = GetMesh();

	TArray<USkeletalMeshComponent*> MeshComponents;
	GetComponents<USkeletalMeshComponent>(MeshComponents);
	for (USkeletalMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp || MeshComp == MainMesh)
		{
			continue;
		}

		MeshComp->SetHiddenInGame(true);
		MeshComp->SetVisibility(false, true);
	}

	if (MainMesh)
	{
		if (HeroData->HeroMesh)
		{
			UE_LOG(LogTemp, Log, TEXT("ApplyHeroVisuals: Setting HeroMesh = %s on %s"), *GetNameSafe(HeroData->HeroMesh), *GetName());
			MainMesh->SetHiddenInGame(false);
			MainMesh->SetVisibility(true);
			MainMesh->SetSkeletalMesh(HeroData->HeroMesh);
			MainMesh->SetRelativeTransform(HeroData->HeroMeshRelativeTransform);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyHeroVisuals: HeroMesh is null for %s — keeping experience pawn mesh"), *GetNameSafe(HeroData));
		}

		if (HeroData->AnimationClass)
		{
			UE_LOG(LogTemp, Log, TEXT("ApplyHeroVisuals: Setting AnimInstanceClass = %s"), *GetNameSafe(HeroData->AnimationClass.Get()));
			MainMesh->SetAnimInstanceClass(HeroData->AnimationClass);
			MainMesh->InitAnim(true);
			if (HeroData->AnimationLayerClass)
			{
				MainMesh->LinkAnimClassLayers(HeroData->AnimationLayerClass);
			}
		}
		else if (HeroData->HeroMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyHeroVisuals: AnimationClass is null for %s — mesh may T-pose"), *GetNameSafe(HeroData));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyHeroVisuals: MeshComponent is NULL on %s!"), *GetName());
	}

	UpdateAppearanceForTeam();

	if (!HeroData->MovementFeelConfig.IsNull())
	{
		if (const UBwayMovementFeelConfig* FeelConfig = HeroData->MovementFeelConfig.LoadSynchronous())
		{
			if (UBwayCharacterMovementComponent* MoveComp = GetBwayCharacterMovement())
			{
				MoveComp->ApplyMovementFeelConfig(FeelConfig);
			}
		}
	}
}

void ABwayCharacterWithAbilities::OnRep_ReplicatedHeroId()
{
	if (!ReplicatedHeroId.IsValid())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("OnRep_ReplicatedHeroId: Applying hero visuals for %s on %s"), 
		*ReplicatedHeroId.ToString(), *GetName());

	// Load hero data from registry and apply visuals
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(ReplicatedHeroId);
	if (HeroData)
	{
		HeroDataAsset = HeroData;
		ApplyHeroVisuals(HeroData);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRep_ReplicatedHeroId: Could not load hero data for %s"), *ReplicatedHeroId.ToString());
	}
}
