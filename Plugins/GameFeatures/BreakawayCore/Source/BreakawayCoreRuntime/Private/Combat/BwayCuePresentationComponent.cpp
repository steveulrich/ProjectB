#include "Combat/BwayCuePresentationComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Buildable/BuildableBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GameplayPrediction.h"
#include "GameplayCueManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCuePresentationComponent)

namespace
{
	struct FSharedLoop
	{
		int32 Owners = 0;
		bool bAddedByPresentation = false;
	};
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, TMap<FGameplayTag, FSharedLoop>> SharedLoops;

	bool AcquireLoop(UAbilitySystemComponent* ASC, FGameplayTag Tag)
	{
		for (auto It = SharedLoops.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
		FSharedLoop& Loop = SharedLoops.FindOrAdd(ASC).FindOrAdd(Tag);
		if (++Loop.Owners != 1) return false;
		Loop.bAddedByPresentation = !ASC->IsGameplayCueActive(Tag);
		return Loop.bAddedByPresentation;
	}

	bool ReleaseLoop(TWeakObjectPtr<UAbilitySystemComponent> ASC, FGameplayTag Tag)
	{
		TMap<FGameplayTag, FSharedLoop>* Loops = SharedLoops.Find(ASC);
		FSharedLoop* Loop = Loops ? Loops->Find(Tag) : nullptr;
		if (!Loop || --Loop->Owners > 0) return false;
		const bool bRemove = Loop->bAddedByPresentation;
		Loops->Remove(Tag);
		if (Loops->IsEmpty()) SharedLoops.Remove(ASC);
		return bRemove;
	}

	FGameplayCueParameters MakePresentationParameters(UAbilitySystemComponent* ASC, AActor* Avatar, UObject* Source, const FVector& Location, USceneComponent* AttachComponent)
	{
		FGameplayCueParameters Params;
		Params.Location = Location;
		Params.Instigator = Avatar;
		Params.EffectCauser = Avatar;
		Params.SourceObject = Source;
		Params.TargetAttachComponent = AttachComponent;
		if (ASC)
		{
			Params.EffectContext = ASC->MakeEffectContext();
			Params.EffectContext.AddSourceObject(Source);
		}
		return Params;
	}
}

bool FBwayCuePresentationState::Start(UAbilitySystemComponent* ASC, AActor* Avatar, UObject* Source,
	FGameplayTag StartCue, FGameplayTag ActiveCue, FGameplayTag EndCue, USceneComponent* AttachComponent)
{
	if (bStarted || !IsValid(ASC) || !IsValid(Avatar) || !Avatar->HasAuthority()) return false;
	bStarted = true;
	OwningASC = ASC;
	OriginalAvatar = Avatar;
	SourceObject = Source;
	AttachmentRoot = AttachComponent;
	LastLocation = Avatar->GetActorLocation();
	OwnedActiveCue = ActiveCue;
	OwnedEndCue = EndCue;
	const FGameplayCueParameters Params = MakePresentationParameters(ASC, Avatar, Source, LastLocation, AttachComponent);
	// These cues are authority-only. Clearing the prediction key prevents the owning
	// client from discarding a server event that it never predicted locally.
	FScopedPredictionWindow ConfirmedPresentation(ASC, FPredictionKey(), false);
	if (OwnedActiveCue.IsValid() && AcquireLoop(ASC, OwnedActiveCue)) ASC->AddGameplayCue(OwnedActiveCue, Params);
	if (!bEnded && StartCue.IsValid()) ASC->ExecuteGameplayCue(StartCue, Params);
	return true;
}

void FBwayCuePresentationState::Stop(bool bPlayEnd)
{
	if (!bStarted || bEnded) return;
	bEnded = true;
	// RemoveGameplayCue removes every occurrence of a tag in UE5.8. Share one
	// helper-owned cue until the last concurrent presentation lifetime releases it.
	const bool bRemoveLoop = OwnedActiveCue.IsValid() && ReleaseLoop(OwningASC, OwnedActiveCue);
	UAbilitySystemComponent* ASC = OwningASC.Get();
	if (!ASC) return;
	AActor* Avatar = OriginalAvatar.Get();
	if (Avatar) LastLocation = Avatar->GetActorLocation();
	FScopedPredictionWindow ConfirmedPresentation(ASC, FPredictionKey(), false);
	if (bRemoveLoop) ASC->RemoveGameplayCue(OwnedActiveCue);
	if (bPlayEnd && OwnedEndCue.IsValid())
	{
		ASC->ExecuteGameplayCue(OwnedEndCue, MakePresentationParameters(ASC, Avatar, SourceObject.Get(), LastLocation, AttachmentRoot.Get()));
	}
}

void FBwayCuePresentationState::Reset()
{
	Stop(false);
	*this = FBwayCuePresentationState();
}

UBwayCuePresentationComponent::UBwayCuePresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBwayCuePresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeVisual();
	if (bFollowOwnerLifetime) StartOwnerLifetimeCue();
}

void UBwayCuePresentationComponent::InitializeVisual()
{
	AActor* Owner = GetOwner();
	if (VisualComponent || !Owner || !CueAttachComponent || Owner->GetNetMode() == NM_DedicatedServer || VisualMesh.IsNull()) return;
	UStaticMesh* Mesh = VisualMesh.LoadSynchronous();
	if (!Mesh) return;
	// Local, nonreplicated geometry: each peer reads its class defaults. Static mesh
	// component registration is also supported by NullRHI; it makes no direct RHI calls.
	VisualComponent = NewObject<UStaticMeshComponent>(Owner);
	VisualComponent->SetIsReplicated(false);
	VisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualComponent->SetGenerateOverlapEvents(false);
	VisualComponent->SetCanEverAffectNavigation(false);
	VisualComponent->SetMobility(EComponentMobility::Movable);
	VisualComponent->SetStaticMesh(Mesh);
	VisualComponent->SetupAttachment(CueAttachComponent);
	VisualComponent->SetRelativeTransform(VisualRelativeTransform);
	VisualComponent->RegisterComponent();
	if (ABuildableActor* Buildable = Cast<ABuildableActor>(Owner))
	{
		ReplacedMesh = Buildable->GetMesh();
		if (USkeletalMeshComponent* Original = ReplacedMesh.Get())
		{
			bReplacedMeshWasVisible = Original->IsVisible();
			Original->SetVisibility(false, false);
		}
	}
}

void UBwayCuePresentationComponent::ClearVisual()
{
	if (VisualComponent) VisualComponent->DestroyComponent();
	VisualComponent = nullptr;
	if (USkeletalMeshComponent* Original = ReplacedMesh.Get()) Original->SetVisibility(bReplacedMeshWasVisible, false);
	ReplacedMesh.Reset();
}

void UBwayCuePresentationComponent::StartPresentation()
{
	if (bFollowOwnerLifetime) { StartOwnerLifetimeCue(); return; }
	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Presentation.Start(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner), Owner, Owner,
			Cue_Start, Cue_Active, Cue_End, CueAttachComponent);
	}
}

void UBwayCuePresentationComponent::StopPresentation(bool bPlayEnd)
{
	if (bFollowOwnerLifetime) { StopOwnerLifetimeCue(); return; }
	Presentation.Stop(bPlayEnd);
}

void UBwayCuePresentationComponent::StartOwnerLifetimeCue()
{
	AActor* Owner = GetOwner();
	if (bOwnerLifetimeCueActive || !Owner || Owner->IsActorBeingDestroyed() || !Cue_Active.IsValid() || Owner->GetNetMode() == NM_DedicatedServer) return;
	bOwnerLifetimeCueActive = true;
	// The replicated actor is the current state. A new observer plays WhileActive
	// only, without replaying a cast, deployment, or expiration one-shot.
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(Owner, Cue_Active, EGameplayCueEvent::WhileActive,
		MakePresentationParameters(nullptr, Owner, Owner, Owner->GetActorLocation(), CueAttachComponent));
}

void UBwayCuePresentationComponent::StopOwnerLifetimeCue()
{
	if (!bOwnerLifetimeCueActive) return;
	bOwnerLifetimeCueActive = false;
	if (AActor* Owner = GetOwner())
	{
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(Owner, Cue_Active, EGameplayCueEvent::Removed,
			MakePresentationParameters(nullptr, Owner, Owner, Owner->GetActorLocation(), CueAttachComponent));
	}
}

void UBwayCuePresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Relevance, round teardown and map travel are not gameplay destruction events.
	StopPresentation(false);
	ClearVisual();
	Super::EndPlay(EndPlayReason);
}
