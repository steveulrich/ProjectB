// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "BwayCharacterMovementComponent.h"
#include "Combat/BwayCombatFeedbackTags.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

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

	UObject* Dummy = NewObject<UObject>();
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

#endif // WITH_DEV_AUTOMATION_TESTS
