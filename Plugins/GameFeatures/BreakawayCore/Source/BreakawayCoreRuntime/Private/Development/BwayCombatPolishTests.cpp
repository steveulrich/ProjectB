// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "BwayCharacterMovementComponent.h"
#include "Combat/BwayCombatFeedbackTags.h"
#include "Combat/BwayCombatNumberPopComponent.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemGlobals.h"
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "Buildable/BuildableBase.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Combat/BwayCombatFeedbackRouterComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayCues/GCN_BwayCombatFeedback.h"
#include "GameplayCueManager.h"
#include "Misc/ScopeExit.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwaySlideJumpLaunchVelocityTest,
	"Breakaway.CombatPolish.SlideJump.LaunchVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwaySlideJumpLaunchVelocityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FVector Out = FVector::ZeroVector;
	const bool bOk = UBwayCharacterMovementComponent::ComputeSlideJumpLaunchVelocity(
		FVector(500.f, 0.f, 0.f),
		600.f,
		1.2f,
		400.f,
		2000.f,
		Out);

	TestTrue(TEXT("ComputeSlideJumpLaunchVelocity succeeds"), bOk);
	TestTrue(TEXT("Z uses JumpZ"), FMath::IsNearlyEqual(Out.Z, 600.f));
	TestTrue(TEXT("Horizontal boosted"), Out.Size2D() > 500.f);
	TestTrue(TEXT("Horizontal within max"), Out.Size2D() <= 2000.f + KINDA_SMALL_NUMBER);

	FVector Clamped = FVector::ZeroVector;
	UBwayCharacterMovementComponent::ComputeSlideJumpLaunchVelocity(
		FVector(50.f, 0.f, 0.f),
		0.f,
		1.0f,
		400.f,
		2000.f,
		Clamped);
	TestTrue(TEXT("Min horizontal clamp"), FMath::IsNearlyEqual(Clamped.Size2D(), 400.f));

	FVector Invalid = FVector::ZeroVector;
	const bool bInvalid = UBwayCharacterMovementComponent::ComputeSlideJumpLaunchVelocity(
		FVector(500.f, 0.f, 0.f),
		0.f,
		1.0f,
		2000.f,
		400.f,
		Invalid);
	TestFalse(TEXT("MinH > MaxH fails"), bInvalid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayNameplateScaleTest,
	"Breakaway.CombatPolish.Nameplates.ScaleEvaluation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayNameplateScaleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UBwayCombatReadabilityConfig* Config = NewObject<UBwayCombatReadabilityConfig>();
	Config->NameplateScaleDistanceStart = 0.f;
	Config->NameplateScaleDistanceEnd = 1000.f;
	Config->MinNameplateScale = 0.5f;
	Config->MaxNameplateScale = 1.0f;
	Config->CombatFeedbackScaleMultiplier = 1.0f;

	TestTrue(TEXT("Near scale"), FMath::IsNearlyEqual(Config->EvaluateNameplateScale(0.f), 1.0f));
	TestTrue(TEXT("Far scale"), FMath::IsNearlyEqual(Config->EvaluateNameplateScale(1000.f), 0.5f));
	TestTrue(TEXT("Mid between"), Config->EvaluateNameplateScale(500.f) > 0.5f && Config->EvaluateNameplateScale(500.f) < 1.0f);

	Config->CombatFeedbackScaleMultiplier = 2.0f;
	TestTrue(TEXT("Accessibility multiplier"), FMath::IsNearlyEqual(Config->EvaluateNameplateScale(0.f), 2.0f));

	Config->bUseColorblindPalette = true;
	Config->ColorblindIncomingDamageColor = FLinearColor(0.1f, 0.2f, 0.3f);
	const FLinearColor Resolved = Config->ResolveIncomingDamageColor();
	TestTrue(TEXT("Colorblind damage R"), FMath::IsNearlyEqual(Resolved.R, 0.1f));
	TestTrue(TEXT("Colorblind damage G"), FMath::IsNearlyEqual(Resolved.G, 0.2f));
	TestTrue(TEXT("Colorblind damage B"), FMath::IsNearlyEqual(Resolved.B, 0.3f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayCooldownPercentMathTest,
	"Breakaway.CombatPolish.Cooldown.PercentMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCooldownPercentMathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	auto CalcPercent = [](float Remaining, float Duration) -> float
	{
		if (Remaining <= KINDA_SMALL_NUMBER || Duration <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		return FMath::Clamp(1.f - (Remaining / Duration), 0.f, 1.f);
	};

	TestTrue(TEXT("Just started"), FMath::IsNearlyEqual(CalcPercent(10.f, 10.f), 0.f));
	TestTrue(TEXT("Halfway"), FMath::IsNearlyEqual(CalcPercent(5.f, 10.f), 0.5f));
	TestTrue(TEXT("Almost ready treated as ready"), FMath::IsNearlyEqual(CalcPercent(0.f, 10.f), 0.f));
	TestTrue(TEXT("Near end fills"), CalcPercent(1.f, 10.f) > 0.85f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayCombatNumberLocationHelperTest,
	"Breakaway.CombatPolish.Numbers.LocationHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatNumberLocationHelperTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FVector Offset(1.f, 2.f, 90.f);
	TestTrue(TEXT("Null target uses offset"),
		UBwayCombatReadabilityLibrary::ResolveNumberPopWorldLocation(nullptr, Offset).Equals(Offset));

	// UObject itself is abstract in UE 5.8; use a concrete transient object so
	// this non-actor fallback test does not trigger a CoreUObject ensure.
	UBwayCombatReadabilityConfig* Dummy = NewObject<UBwayCombatReadabilityConfig>();
	TestTrue(TEXT("Non-actor uses offset"),
		UBwayCombatReadabilityLibrary::ResolveNumberPopWorldLocation(Dummy, Offset).Equals(Offset));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayCombatNumberColorTagsTest,
	"Breakaway.CombatPolish.Numbers.ColorTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatNumberColorTagsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UBwayCombatReadabilityConfig* Config = NewObject<UBwayCombatReadabilityConfig>();
	Config->IncomingDamageColor = FLinearColor(1.f, 0.f, 0.f);
	Config->OutgoingDamageColor = FLinearColor::White;
	Config->IncomingHealColor = FLinearColor(0.f, 1.f, 0.f);
	Config->bUseColorblindPalette = false;

	TestTrue(TEXT("Incoming damage red"), Config->ResolveIncomingDamageColor().Equals(FLinearColor(1.f, 0.f, 0.f)));
	TestTrue(TEXT("Outgoing damage white"), Config->ResolveOutgoingDamageColor().Equals(FLinearColor::White));
	TestTrue(TEXT("Incoming heal green"), Config->ResolveIncomingHealColor().Equals(FLinearColor(0.f, 1.f, 0.f)));

	TestTrue(TEXT("Incoming damage tag registered"), BwayCombatFeedbackTags::Number_IncomingDamage.GetTag().IsValid());
	TestTrue(TEXT("Outgoing damage tag registered"), BwayCombatFeedbackTags::Number_OutgoingDamage.GetTag().IsValid());
	TestTrue(TEXT("Incoming heal tag registered"), BwayCombatFeedbackTags::Number_IncomingHeal.GetTag().IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBwayCombatNumberPopAnimationTest,
	"Breakaway.CombatPolish.Numbers.StaggeredPopAnimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatNumberPopAnimationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FVector Base(0.f, 0.f, 100.f);
	const FVector RightDir = FVector::RightVector;
	const FVector LeftDir = -FVector::RightVector;
	const float Lifespan = 1.f;
	const float LateralOffset = 35.f;
	const float OutwardDistance = 45.f;
	const float RiseDistance = 80.f;
	const float EndScale = 0.f;

	FVector RightStart = FVector::ZeroVector;
	FVector RightEnd = FVector::ZeroVector;
	FVector LeftStart = FVector::ZeroVector;
	FVector MidLocation = FVector::ZeroVector;
	float ScaleStart = -1.f;
	float ScaleMid = -1.f;
	float ScaleEnd = -1.f;
	float UnusedScale = 0.f;

	UBwayCombatNumberPopComponent::EvaluateNumberPopAnimation(
		0.f, Lifespan, Base, RightDir, LateralOffset, OutwardDistance, RiseDistance, EndScale, RightStart, ScaleStart);
	UBwayCombatNumberPopComponent::EvaluateNumberPopAnimation(
		Lifespan, Lifespan, Base, RightDir, LateralOffset, OutwardDistance, RiseDistance, EndScale, RightEnd, ScaleEnd);
	UBwayCombatNumberPopComponent::EvaluateNumberPopAnimation(
		0.f, Lifespan, Base, LeftDir, LateralOffset, OutwardDistance, RiseDistance, EndScale, LeftStart, UnusedScale);
	UBwayCombatNumberPopComponent::EvaluateNumberPopAnimation(
		0.5f, Lifespan, Base, RightDir, LateralOffset, OutwardDistance, RiseDistance, EndScale, MidLocation, ScaleMid);

	TestTrue(TEXT("Spawn scale is 1"), FMath::IsNearlyEqual(ScaleStart, 1.f));
	TestTrue(TEXT("End scale matches config"), FMath::IsNearlyEqual(ScaleEnd, EndScale));
	TestTrue(TEXT("Mid scale is between start and end"), ScaleMid > EndScale && ScaleMid < 1.f);
	TestTrue(TEXT("Right spawn is offset +Y"), RightStart.Y > Base.Y + 1.f);
	TestTrue(TEXT("Left spawn is offset -Y"), LeftStart.Y < Base.Y - 1.f);
	TestTrue(TEXT("Opposite sides at spawn"), RightStart.Y > 0.f && LeftStart.Y < 0.f);
	TestTrue(TEXT("Right flies further out"), RightEnd.Y > RightStart.Y);
	TestTrue(TEXT("Rises over lifespan"), RightEnd.Z > RightStart.Z);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCombatNumberBurstLayoutTest,
	"Breakaway.CombatPolish.Numbers.BurstLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatNumberBurstLayoutTest::RunTest(const FString&)
{
	// Exercise the allocator used by AddNumberPop with both an RPC burst and a full Barrage cadence.
	for (const float WorldSize : {18.f, 72.f})
	for (const float Interval : {0.f, 0.08f})
	{
		TArray<FBwayLiveCombatNumberPop> Pops;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			FBwayLiveCombatNumberPop Pop;
			Pop.BaseLocation = FVector(1000.f, 0.f, 90.f);
			Pop.SpawnWorldTime = Index * Interval;
			UBwayCombatNumberPopComponent::ArrangeNumberPop(Pop, Pops, FVector::RightVector,
				WorldSize, WorldSize * 2.5f, 35.f, 45.f, 80.f, 1.f);
			Pops.Add(Pop);
		}
		for (float Now = 0.f; Now < 2.f; Now += 0.04f)
		{
			for (int32 A = 0; A < Pops.Num(); ++A)
			{
				const float AgeA = Now - Pops[A].SpawnWorldTime;
				if (AgeA < 0.f || AgeA >= 1.f) continue;
				FVector PosA; float ScaleA;
				UBwayCombatNumberPopComponent::EvaluateNumberPopAnimation(AgeA, 1.f,
					Pops[A].BaseLocation + Pops[A].LayoutOffset, Pops[A].OutwardDirection,
					35.f, 45.f, 80.f, 0.f, PosA, ScaleA);
				for (int32 B = 0; B < A; ++B)
				{
					const float AgeB = Now - Pops[B].SpawnWorldTime;
					if (AgeB < 0.f || AgeB >= 1.f) continue;
					FVector PosB; float ScaleB;
					UBwayCombatNumberPopComponent::EvaluateNumberPopAnimation(AgeB, 1.f,
						Pops[B].BaseLocation + Pops[B].LayoutOffset, Pops[B].OutwardDirection,
						35.f, 45.f, 80.f, 0.f, PosB, ScaleB);
					const bool bSeparate = FMath::Abs(PosA.Y - PosB.Y) >= WorldSize * 1.25f * (ScaleA + ScaleB)
						|| FMath::Abs(PosA.Z - PosB.Z) >= WorldSize * 0.5f * (ScaleA + ScaleB);
					if (!TestTrue(FString::Printf(TEXT("Burst interval %.2f pair %d/%d at %.2f has readable bounds"),
						Interval, A, B, Now), bSeparate)) return false;
				}
			}
		}
		FBwayLiveCombatNumberPop Reused;
		Reused.BaseLocation = Pops[0].BaseLocation;
		Reused.SpawnWorldTime = 3.f;
		UBwayCombatNumberPopComponent::ArrangeNumberPop(Reused, Pops, FVector::RightVector,
			WorldSize, WorldSize * 2.5f, 35.f, 45.f, 80.f, 1.f);
		TestTrue(TEXT("Expired numbers release their first lane"), Reused.LayoutOffset.Equals(Pops[0].LayoutOffset));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCombatFeedbackRecipientsTest,
	"Breakaway.CombatPolish.Routing.PlayerAndBuildableRecipients",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatFeedbackRecipientsTest::RunTest(const FString&)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Settings);
	if (!TestNotNull(TEXT("Recipient fixture world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	ABwayPlayerState* Source = World->SpawnActor<ABwayPlayerState>();
	ABwayPlayerState* Target = World->SpawnActor<ABwayPlayerState>();
	APawn* SourcePawn = World->SpawnActor<APawn>();
	APawn* TargetPawn = World->SpawnActor<APawn>();
	ABuildableActor* Buildable = World->SpawnActor<ABuildableActor>();
	if (!TestNotNull(TEXT("GameState"), GS) || !TestNotNull(TEXT("Source player"), Source)
		|| !TestNotNull(TEXT("Target player"), Target) || !TestNotNull(TEXT("Source pawn"), SourcePawn)
		|| !TestNotNull(TEXT("Target pawn"), TargetPawn) || !TestNotNull(TEXT("Buildable"), Buildable)) return false;
	SourcePawn->SetPlayerState(Source);
	TargetPawn->SetPlayerState(Target);
	Buildable->SetInstigator(TargetPawn);
	FArrayProperty* TeamsProperty = FindFProperty<FArrayProperty>(ABwayGameState::StaticClass(), TEXT("Teams"));
	if (!TestNotNull(TEXT("Team fixture property"), TeamsProperty)) return false;
	TeamsProperty->ContainerPtrToValuePtr<TArray<FTeamInfo>>(GS)->SetNum(2);
	GS->AddPlayerToTeam(Source, 0);
	GS->AddPlayerToTeam(Target, 1);
	auto Recipients = UBwayCombatFeedbackRouterComponent::ResolveDamageRecipients(Source, Target, GS);
	TestTrue(TEXT("Enemy player damage is incoming to victim"), Recipients.IncomingPlayer == Target);
	TestTrue(TEXT("Enemy player damage is outgoing to attacker"), Recipients.OutgoingPlayer == Source);
	Recipients = UBwayCombatFeedbackRouterComponent::ResolveDamageRecipients(Source, Buildable, GS);
	TestNull(TEXT("A damaged buildable is not incoming owner health damage"), Recipients.IncomingPlayer);
	TestTrue(TEXT("Enemy buildable still confirms outgoing damage"), Recipients.OutgoingPlayer == Source);
	Recipients = UBwayCombatFeedbackRouterComponent::ResolveDamageRecipients(Buildable, Source, GS);
	TestTrue(TEXT("Buildable source retains owner confirmation"), Recipients.OutgoingPlayer == Target);
	TestTrue(TEXT("Buildable source retains real player victim"), Recipients.IncomingPlayer == Source);
	GS->AddPlayerToTeam(Target, 0);
	Recipients = UBwayCombatFeedbackRouterComponent::ResolveDamageRecipients(Source, Buildable, GS);
	TestNull(TEXT("Friendly structure cannot produce an enemy confirmation"), Recipients.OutgoingPlayer);
	Recipients = UBwayCombatFeedbackRouterComponent::ResolveDamageRecipients(Source, SourcePawn, GS);
	TestNull(TEXT("Self damage cannot confirm an enemy hit"), Recipients.OutgoingPlayer);
	FLyraVerbMessage Message;
	Message.Instigator = Source; Message.Target = Target;
	TestTrue(TEXT("Source receives outgoing local channel"), UBwayCombatNumberPopComponent::IsMessageForPlayer(
		BwayCombatFeedbackTags::Number_OutgoingDamage, Message, Source));
	TestFalse(TEXT("Other local player does not receive outgoing channel"), UBwayCombatNumberPopComponent::IsMessageForPlayer(
		BwayCombatFeedbackTags::Number_OutgoingDamage, Message, Target));
	TestTrue(TEXT("Healed player receives heal channel"), UBwayCombatNumberPopComponent::IsMessageForPlayer(
		BwayCombatFeedbackTags::Number_IncomingHeal, Message, Target));
	TestFalse(TEXT("Healer does not receive target heal channel"), UBwayCombatNumberPopComponent::IsMessageForPlayer(
		BwayCombatFeedbackTags::Number_IncomingHeal, Message, Source));
	Message.Target = Buildable;
	TestFalse(TEXT("Local filter also rejects false buildable-owner incoming damage"), UBwayCombatNumberPopComponent::IsMessageForPlayer(
		BwayCombatFeedbackTags::Number_IncomingDamage, Message, Target));
	TestFalse(TEXT("Queued feedback cannot present while unpossessed"), UBwayCombatNumberPopComponent::IsPawnPresentationActive(nullptr));
	ULyraHealthComponent* Health = NewObject<ULyraHealthComponent>(SourcePawn);
	SourcePawn->AddInstanceComponent(Health);
	Health->RegisterComponent();
	TestTrue(TEXT("Living pawn accepts number presentation"), UBwayCombatNumberPopComponent::IsPawnPresentationActive(SourcePawn));
	Health->StartDeath();
	TestFalse(TEXT("Queued feedback after death cannot recreate cleared number presentation"), UBwayCombatNumberPopComponent::IsPawnPresentationActive(SourcePawn));
	TestTrue(TEXT("A living replacement accepts new feedback"), UBwayCombatNumberPopComponent::IsPawnPresentationActive(TargetPawn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCombatNumberEventPositionTest,
	"Breakaway.CombatPolish.Numbers.EventPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatNumberEventPositionTest::RunTest(const FString&)
{
	UBwayCombatNumberPopComponent* Component = NewObject<UBwayCombatNumberPopComponent>();
	Component->CachedConfig = NewObject<UBwayCombatReadabilityConfig>();
	FLyraVerbMessage Message;
	TestTrue(TEXT("Legacy message retains target/offset fallback"), Component->ResolvePopLocation(Message).Equals(FVector(0.f, 0.f, 90.f)));
	Message.bHasWorldLocation = true;
	Message.WorldLocation = FVector::ZeroVector;
	TestTrue(TEXT("A valid event at the world origin is not mistaken for missing position"), Component->ResolvePopLocation(Message).IsZero());
	Message.WorldLocation = FVector(1200.f, -300.f, 180.f);
	Message.bHasInstigatorWorldLocation = true;
	Message.InstigatorWorldLocation = FVector(1500.f, -100.f, 90.f);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive WriteArchive(Writer, false);
	FLyraVerbMessage::StaticStruct()->SerializeItem(WriteArchive, &Message, nullptr);
	FLyraVerbMessage Received;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive ReadArchive(Reader, false);
	FLyraVerbMessage::StaticStruct()->SerializeItem(ReadArchive, &Received, nullptr);
	TestTrue(TEXT("Reflected transport preserves target event position with no target actor"),
		Received.bHasWorldLocation && Component->ResolvePopLocation(Received).Equals(Message.WorldLocation));
	TestTrue(TEXT("Reflected transport preserves directional source position"),
		Received.bHasInstigatorWorldLocation && Received.InstigatorWorldLocation.Equals(Message.InstigatorWorldLocation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCombatCueAudioOwnershipTest,
	"Breakaway.CombatPolish.Cues.AudioOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatCueAudioOwnershipTest::RunTest(const FString&)
{
	UGCN_BwayCombatFeedback* Cue = NewObject<UGCN_BwayCombatFeedback>();
	Cue->GameplayCueTag = BwayGameplayCueTags::CombatHitConfirm;
	TestTrue(TEXT("Attacker hears personal hit confirmation"), Cue->ShouldPlayFeedbackSound(true));
	TestFalse(TEXT("Proxy does not hear another player's personal hit confirmation"), Cue->ShouldPlayFeedbackSound(false));
	Cue->GameplayCueTag = BwayGameplayCueTags::CombatHit;
	TestFalse(TEXT("Legacy hit-confirmation tag also remains personal"), Cue->ShouldPlayFeedbackSound(false));
	for (FGameplayTag Tag : {BwayGameplayCueTags::MovementSlide.GetTag(), BwayGameplayCueTags::ScoringGoal.GetTag(),
		BwayGameplayCueTags::AlonaPrimaryFire.GetTag(), BwayGameplayCueTags::RawlinsBarrageFire.GetTag()})
	{
		Cue->GameplayCueTag = Tag;
		TestTrue(TEXT("Spatial cue remains audible to observing peers"), Cue->ShouldPlayFeedbackSound(false));
	}
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCombatNumberMaterialPoolingTest,
    "Breakaway.CombatPolish.Numbers.MaterialPooling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatNumberMaterialPoolingTest::RunTest(const FString&)
{
    // Use the same real local-controller/world presentation path as the
    // fractional-heal fixture, without introducing unrelated damage or GAS state.
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone();
    UWorld* World = Instance->GetWorld();
    if (!TestNotNull(TEXT("Number material fixture world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    APlayerController* PC = World->SpawnActor<APlayerController>();
    APawn* Pawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("Local controller"), PC) || !TestNotNull(TEXT("Living pawn"), Pawn)) return false;
    PC->SetAsLocalPlayerController();
    PC->Possess(Pawn);
    if (!TestTrue(TEXT("Fixture controller is local"), PC->IsLocalPlayerController())
        || !TestTrue(TEXT("Fixture controller possesses the living pawn"), PC->GetPawn() == Pawn)) return false;
    UBwayCombatNumberPopComponent* Numbers = NewObject<UBwayCombatNumberPopComponent>(PC);
    PC->AddInstanceComponent(Numbers);
    Numbers->RegisterComponent();
    UBwayCombatReadabilityConfig* Config = NewObject<UBwayCombatReadabilityConfig>(Numbers);
    Numbers->CachedConfig = Config;
    UMaterialInterface* DefaultMaterial = GetDefault<UTextRenderComponent>()->GetMaterial(0);
    if (!TestNotNull(TEXT("Engine default text material"), DefaultMaterial)) return false;
    UMaterialInstanceDynamic* FirstMaterial = UMaterialInstanceDynamic::Create(DefaultMaterial, Numbers);
    UMaterialInstanceDynamic* SecondMaterial = UMaterialInstanceDynamic::Create(DefaultMaterial, Numbers);
    if (!TestNotNull(TEXT("First replaceable material"), FirstMaterial)
        || !TestNotNull(TEXT("Second replaceable material"), SecondMaterial)) return false;
    TestTrue(TEXT("Override instances are distinct"), FirstMaterial != SecondMaterial);
    FLyraNumberPopRequest Request;
    Request.NumberToDisplay = 7;
    Request.WorldLocation = FVector(0.f, 0.f, 100.f);
    const auto Present = [&]() -> UTextRenderComponent*
    {
        Numbers->AddNumberPop(Request);
        if (!TestEqual(TEXT("Public number API creates one actual pop"), Numbers->LivePops.Num(), 1)) return nullptr;
        UTextRenderComponent* Text = Numbers->LivePops[0].TextComponent;
        TestEqual(TEXT("Material changes preserve number text"), Text->Text.ToString(), FText::AsNumber(7).ToString());
        return Text;
    };
    UTextRenderComponent* Original = Present();
    if (!TestNotNull(TEXT("Unconfigured number component"), Original)) return false;
    TestTrue(TEXT("Unset config preserves the engine text material"), Original->GetMaterial(0) == DefaultMaterial);
    Numbers->ClearLocalFeedback();
    TestEqual(TEXT("Real cleanup recycles the text component"), Numbers->PooledTextComponents.Num(), 1);
    Config->NumberPopMaterial = FirstMaterial;
    UTextRenderComponent* Reused = Present();
    if (!TestNotNull(TEXT("First configured pooled number"), Reused)) return false;
    TestTrue(TEXT("First override reuses the real pooled component"), Reused == Original);
    TestTrue(TEXT("First assigned material is applied"), Reused->GetMaterial(0) == FirstMaterial);
    Numbers->ClearLocalFeedback();
    Config->NumberPopMaterial = SecondMaterial;
    Reused = Present();
    if (!TestNotNull(TEXT("Changed configured pooled number"), Reused)) return false;
    TestTrue(TEXT("Changed override still reuses the same component"), Reused == Original);
    TestTrue(TEXT("Pool acquisition follows the changed material"), Reused->GetMaterial(0) == SecondMaterial);
    Numbers->ClearLocalFeedback();
    Config->NumberPopMaterial = nullptr;
    Reused = Present();
    if (!TestNotNull(TEXT("Cleared override pooled number"), Reused)) return false;
    TestTrue(TEXT("Clearing override does not bypass pooling"), Reused == Original);
    TestTrue(TEXT("Clearing config restores engine default instead of stale override"), Reused->GetMaterial(0) == DefaultMaterial);
    TestEqual(TEXT("Acquisition consumes the single pooled component"), Numbers->PooledTextComponents.Num(), 0);
    Numbers->ClearLocalFeedback();
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCombatNumberCameraDepthTest,
    "Breakaway.CombatPolish.Numbers.CameraDepthScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatNumberCameraDepthTest::RunTest(const FString&)
{
    const FVector Camera(-69.2762, 0.0, 5056.8589);
    const FRotator Rotation(68.3503, 0.0, 0.0);
    const FVector Forward = Rotation.Vector();
    const FVector Right = Rotation.Quaternion().GetRightVector();
    const auto Scale = [&](const FVector& Position)
    {
        return UBwayCombatNumberPopComponent::EvaluateNumberPopDepthScale(Position, Camera, Forward, 10.f);
    };
    TestTrue(TEXT("Pitched nearby heal scales below the old 72uu minimum"),
        FMath::IsNearlyEqual(Scale(Camera + Forward * 100.0), .125f));
    TestTrue(TEXT("Large off-axis radial distance does not enlarge shallow-depth text"),
        FMath::IsNearlyEqual(Scale(Camera + Forward * 100.0 + Right * 1600.0), .125f));
    TestTrue(TEXT("Near projected height stays equal at two forward depths"),
        FMath::IsNearlyEqual(Scale(Camera + Forward * 100.0) / 100.f,
            Scale(Camera + Forward * 400.0) / 400.f));
    TestTrue(TEXT("Centered 800uu appearance is unchanged"), FMath::IsNearlyEqual(Scale(Camera + Forward * 800.0), 1.f));
    TestTrue(TEXT("Centered far appearance is unchanged"), FMath::IsNearlyEqual(Scale(Camera + Forward * 1600.0), 2.f));
    TestEqual(TEXT("Existing 8x far cap is preserved"), Scale(Camera + Forward * 10000.0), 8.f);
    TestTrue(TEXT("No size jump at the former 800uu minimum"),
        FMath::Abs(Scale(Camera + Forward * 800.1) - Scale(Camera + Forward * 799.9)) < .001f);
    TestEqual(TEXT("Behind-camera text is hidden"), Scale(Camera - Forward * 100.0), 0.f);
    TestEqual(TEXT("Camera-plane text is hidden"), Scale(Camera), 0.f);
    TestEqual(TEXT("Text inside the actual near plane is hidden"), Scale(Camera + Forward * 9.0), 0.f);
    TestEqual(TEXT("Nonfinite position is hidden"), Scale(FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0)), 0.f);
    TestEqual(TEXT("Infinite camera is hidden"), UBwayCombatNumberPopComponent::EvaluateNumberPopDepthScale(
        FVector::ZeroVector, FVector(std::numeric_limits<double>::infinity(), 0, 0), Forward, 10.f), 0.f);
    TestEqual(TEXT("Invalid view direction is hidden"), UBwayCombatNumberPopComponent::EvaluateNumberPopDepthScale(
        Camera + Forward * 100.0, Camera, FVector::ZeroVector, 10.f), 0.f);
    TestEqual(TEXT("Invalid near plane is hidden"), UBwayCombatNumberPopComponent::EvaluateNumberPopDepthScale(
        Camera + Forward * 100.0, Camera, Forward, std::numeric_limits<float>::quiet_NaN()), 0.f);

    // Exercise acquisition, real component updates and reuse, not a second implementation of the math.
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone();
    UWorld* World = Instance->GetWorld();
    if (!TestNotNull(TEXT("Number depth fixture world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    APlayerController* PC = World->SpawnActor<APlayerController>();
    APawn* Pawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("Depth local controller"), PC) || !TestNotNull(TEXT("Depth living pawn"), Pawn)) return false;
    PC->SetAsLocalPlayerController();
    PC->Possess(Pawn);
    if (!TestTrue(TEXT("Depth fixture has an actual local owner"), PC->IsLocalPlayerController())
        || !TestNotNull(TEXT("Depth fixture camera manager"), PC->PlayerCameraManager.Get())) return false;
    UBwayCombatNumberPopComponent* Numbers = NewObject<UBwayCombatNumberPopComponent>(PC);
    PC->AddInstanceComponent(Numbers);
    Numbers->RegisterComponent();
    UBwayCombatReadabilityConfig* Config = NewObject<UBwayCombatReadabilityConfig>(Numbers);
    Numbers->CachedConfig = Config;
    FMinimalViewInfo View;
    View.Location = FVector::ZeroVector;
    View.Rotation = FRotator::ZeroRotator;
    View.PerspectiveNearClipPlane = 10.f;
    PC->PlayerCameraManager->SetCameraCachePOV(View);
    FLyraNumberPopRequest Request;
    Request.NumberToDisplay = 7;
    Request.WorldLocation = FVector(200.f, 0.f, 100.f);
    Numbers->AddNumberPop(Request);
    if (!TestEqual(TEXT("Public API creates a real near-camera number"), Numbers->LivePops.Num(), 1)) return false;
    UTextRenderComponent* Text = Numbers->LivePops[0].TextComponent;
    if (!TestNotNull(TEXT("Real near text component"), Text)) return false;
    TestTrue(TEXT("Acquisition reserves near-camera font height"), FMath::IsNearlyEqual(Numbers->LivePops[0].LayoutHeight, 18.f));
    TestTrue(TEXT("Initial actual component uses forward depth"), FMath::IsNearlyEqual(Text->WorldSize, 18.f));
    const FVector InitialLocation = Text->GetComponentLocation();
    View.Location.X = 150.f;
    PC->PlayerCameraManager->SetCameraCachePOV(View);
    Numbers->TickComponent(0.f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Existing number shrinks when the camera approaches"), FMath::IsNearlyEqual(Text->WorldSize, 4.5f));
    TestTrue(TEXT("Sizing preserves the animated world position"), Text->GetComponentLocation().Equals(InitialLocation));
    View.Location.X = 201.f;
    PC->PlayerCameraManager->SetCameraCachePOV(View);
    Numbers->TickComponent(0.f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Existing number hides behind the camera"), Text->IsVisible());
    View.Location.X = 0.f;
    PC->PlayerCameraManager->SetCameraCachePOV(View);
    Numbers->TickComponent(0.f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Still-live number reappears when in front again"), Text->IsVisible());
    TestTrue(TEXT("Reappearing number restores finite depth size"), FMath::IsNearlyEqual(Text->WorldSize, 18.f));
    Numbers->ClearLocalFeedback();
    View.Location.X = 100.f;
    PC->PlayerCameraManager->SetCameraCachePOV(View);
    Numbers->AddNumberPop(Request);
    if (!TestEqual(TEXT("Pooled depth pop exists"), Numbers->LivePops.Num(), 1)) return false;
    TestTrue(TEXT("Depth fix preserves real component pooling"), Numbers->LivePops[0].TextComponent == Text);
    TestTrue(TEXT("Pooled component receives new camera depth"), FMath::IsNearlyEqual(Text->WorldSize, 9.f));
    TestTrue(TEXT("Required lateral offset remains 35uu"), FMath::IsNearlyEqual(Config->NumberPopLateralOffset, 35.f));
    TestTrue(TEXT("Required rise remains 80uu"), FMath::IsNearlyEqual(Config->NumberPopRiseDistance, 80.f));
    Numbers->ClearLocalFeedback();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCombatFractionalNumberFormatTest,
	"Breakaway.CombatPolish.Numbers.FractionalAmounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatFractionalNumberFormatTest::RunTest(const FString&)
{
	auto Format = [](double Amount) { return UBwayCombatNumberPopComponent::FormatCombatNumber(Amount).ToString(); };
	TestEqual(TEXT("A small leech amount remains fractional"), Format(0.4), FText::AsNumber(0.4).ToString());
	TestEqual(TEXT("At most one decimal is shown with half-away-from-zero rounding"), Format(1.25), FText::AsNumber(1.3).ToString());
	TestEqual(TEXT("A positive near-full clamp never displays zero or a fabricated one"), Format(0.01),
		FString(TEXT("<")) + FText::AsNumber(0.1).ToString());
	TestEqual(TEXT("The display precision boundary is inclusive"), Format(0.1), FText::AsNumber(0.1).ToString());
	TestEqual(TEXT("Whole amounts retain whole-number display"), Format(12.0), FText::AsNumber(12).ToString());
	for (double Invalid : {0.0, -0.4, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
	{
		TestTrue(TEXT("Invalid/nonpositive amounts have no display"), UBwayCombatNumberPopComponent::FormatCombatNumber(Invalid).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCombatFractionalHealRoutingTest,
	"Breakaway.CombatPolish.Routing.FractionalHealing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCombatFractionalHealRoutingTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Fractional feedback world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	World->InitializeActorsForPlay(FURL());
	ABwayGameState* GS = World->SpawnActor<ABwayGameState>();
	ABwayPlayerState* Source = World->SpawnActor<ABwayPlayerState>();
	ABwayPlayerState* Target = World->SpawnActor<ABwayPlayerState>();
	APlayerController* PC = World->SpawnActor<APlayerController>();
	APawn* Pawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("GameState"), GS) || !TestNotNull(TEXT("Healer"), Source)
		|| !TestNotNull(TEXT("Healed player"), Target) || !TestNotNull(TEXT("Local controller"), PC)
		|| !TestNotNull(TEXT("Living pawn"), Pawn)) return false;
	// No GameMode login runs in this isolated world. Establish the same local
	// controller intent as SetPlayer(ULocalPlayer), without viewport/input setup.
	PC->SetAsLocalPlayerController();
	PC->SetPlayerState(Target);
	Target->SetOwner(PC);
	Pawn->SetPlayerState(Target);
	PC->Possess(Pawn);
	if (!TestTrue(TEXT("Fixture controller is local"), PC->IsLocalPlayerController())
		|| !TestTrue(TEXT("Fixture controller possesses the living pawn"), PC->GetPawn() == Pawn)
		|| !TestTrue(TEXT("Fixture recipient resolves its controller"), Target->GetPlayerController() == PC)) return false;
	UBwayCombatFeedbackRouterComponent* Router = NewObject<UBwayCombatFeedbackRouterComponent>(GS);
	GS->AddInstanceComponent(Router);
	Router->RegisterComponent();
	UBwayCombatNumberPopComponent* Numbers = NewObject<UBwayCombatNumberPopComponent>(PC);
	PC->AddInstanceComponent(Numbers);
	Numbers->RegisterComponent();
	Numbers->CachedConfig = NewObject<UBwayCombatReadabilityConfig>();
	int32 HealEvents = 0;
	int32 DamageEvents = 0;
	int32 HitConfirmCues = 0;
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!TestNotNull(TEXT("Gameplay cue observer"), CueManager)) return false;
	const FDelegateHandle CueListener = CueManager->OnGameplayCueRouted().AddLambda(
		[&](AActor*, FGameplayTag Tag, EGameplayCueEvent::Type, const FGameplayCueParameters&, EGameplayCueExecutionOptions)
		{
			if (Tag == BwayGameplayCueTags::CombatHitConfirm || Tag == BwayGameplayCueTags::CombatHit) ++HitConfirmCues;
		});
	ON_SCOPE_EXIT { CueManager->OnGameplayCueRouted().Remove(CueListener); };
	FLyraVerbMessage Delivered;
	UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(World);
	FGameplayMessageListenerHandle HealListener = Messages.RegisterListener<FLyraVerbMessage>(
		BwayCombatFeedbackTags::Number_IncomingHeal, [&](FGameplayTag Channel, const FLyraVerbMessage& Message)
		{
			++HealEvents;
			Delivered = Message;
			Numbers->OnCombatNumberMessage(Channel, Message);
		});
	FGameplayMessageListenerHandle OutgoingListener = Messages.RegisterListener<FLyraVerbMessage>(
		BwayCombatFeedbackTags::Number_OutgoingDamage, [&](FGameplayTag, const FLyraVerbMessage&) { ++DamageEvents; });
	FGameplayMessageListenerHandle IncomingListener = Messages.RegisterListener<FLyraVerbMessage>(
		BwayCombatFeedbackTags::Number_IncomingDamage, [&](FGameplayTag, const FLyraVerbMessage&) { ++DamageEvents; });
	ON_SCOPE_EXIT { HealListener.Unregister(); OutgoingListener.Unregister(); IncomingListener.Unregister(); };
	FLyraVerbMessage Message;
	Message.Instigator = Source;
	Message.Target = Target;
	Message.Magnitude = 0.4;
	Router->RouteHealFeedback(Message);
	TestEqual(TEXT("Sub-half healing reaches the local production route once"), HealEvents, 1);
	TestEqual(TEXT("Router preserves effective fractional magnitude"), Delivered.Magnitude, 0.4);
	TestEqual(TEXT("Fractional display does not change integer healing stats"), Source->GetHealingDone(), 0);
	TestEqual(TEXT("Actual number component creates one fractional pop"), Numbers->LivePops.Num(), 1);
	if (Numbers->LivePops.Num() == 1)
	{
		TestEqual(TEXT("Rendered text retains the fraction"), Numbers->LivePops[0].TextComponent->Text.ToString(), FText::AsNumber(0.4).ToString());
		TestTrue(TEXT("Fractional healing retains green palette"), Numbers->LivePops[0].BaseColor.Equals(Numbers->CachedConfig->ResolveIncomingHealColor()));
	}
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive WriteArchive(Writer, false);
	FLyraVerbMessage::StaticStruct()->SerializeItem(WriteArchive, &Delivered, nullptr);
	FLyraVerbMessage Received;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive ReadArchive(Reader, false);
	FLyraVerbMessage::StaticStruct()->SerializeItem(ReadArchive, &Received, nullptr);
	TestEqual(TEXT("Reflected owner transport preserves raw 0.4"), Received.Magnitude, 0.4);
	TestTrue(TEXT("Reflected transport retains the heal-only channel"), Received.Verb == BwayCombatFeedbackTags::Number_IncomingHeal);
	Message.Magnitude = 1.25;
	Router->RouteHealFeedback(Message);
	TestEqual(TEXT("Whole-number match stat behavior remains rounded independently"), Source->GetHealingDone(), 1);
	Message.Magnitude = 0.01;
	Router->RouteHealFeedback(Message);
	TestEqual(TEXT("Near-full effective healing still delivers a pop"), HealEvents, 3);
	if (Numbers->LivePops.Num() == 3)
	{
		TestEqual(TEXT("Actual tiny-heal pop shows less than display precision"), Numbers->LivePops.Last().TextComponent->Text.ToString(),
			FString(TEXT("<")) + FText::AsNumber(0.1).ToString());
	}
	for (double Invalid : {0.0, -0.4, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
	{
		Message.Magnitude = Invalid;
		Router->RouteHealFeedback(Message);
		Numbers->PresentCombatNumber(BwayCombatFeedbackTags::Number_IncomingHeal, Message);
	}
	TestEqual(TEXT("Invalid heals cannot add routed events"), HealEvents, 3);
	TestEqual(TEXT("Invalid heals cannot add local pops"), Numbers->LivePops.Num(), 3);
	TestEqual(TEXT("Healing never enters damage or outgoing hit-confirm message routes"), DamageEvents, 0);
	TestEqual(TEXT("Healing never executes a hit-confirm gameplay cue"), HitConfirmCues, 0);
	FLyraNumberPopRequest Legacy;
	Legacy.NumberToDisplay = 7;
	Legacy.TargetTags.AddTag(BwayCombatFeedbackTags::Number_IncomingHeal);
	Numbers->AddNumberPop(Legacy);
	TestEqual(TEXT("The original integer AddNumberPop API still presents"), Numbers->LivePops.Num(), 4);
	if (Numbers->LivePops.Num() == 4)
	{
		TestEqual(TEXT("Legacy integer text stays unchanged"), Numbers->LivePops.Last().TextComponent->Text.ToString(), FText::AsNumber(7).ToString());
	}
	Numbers->ClearLocalFeedback();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
