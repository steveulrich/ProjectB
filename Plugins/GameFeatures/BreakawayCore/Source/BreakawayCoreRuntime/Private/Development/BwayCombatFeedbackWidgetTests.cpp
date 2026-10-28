// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCombatFeedbackWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayDamageDirectionTest,
	"Breakaway.CombatPolish.HUD.DamageDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayDamageDirectionTest::RunTest(const FString& Parameters)
{
	const FVector Origin(50, 100, 200);
	TestTrue(TEXT("Front source points above reticle"),
		UBwayCombatFeedbackWidget::ResolveDamageDirection(Origin + FVector(100, 0, 30), Origin, 0).Equals(FVector2D(0, -1)));
	TestTrue(TEXT("Right source points right"),
		UBwayCombatFeedbackWidget::ResolveDamageDirection(Origin + FVector(0, 100, 0), Origin, 0).Equals(FVector2D(1, 0)));
	TestTrue(TEXT("Back source points below reticle"),
		UBwayCombatFeedbackWidget::ResolveDamageDirection(Origin + FVector(-100, 0, 0), Origin, 0).Equals(FVector2D(0, 1)));
	TestTrue(TEXT("Turning view right moves world-front source to left"),
		UBwayCombatFeedbackWidget::ResolveDamageDirection(Origin + FVector(100, 0, 0), Origin, 90).Equals(FVector2D(-1, 0), 0.001));
	TestTrue(TEXT("Coincident source uses nondirectional fallback"),
		UBwayCombatFeedbackWidget::ResolveDamageDirection(Origin, Origin, 45).IsNearlyZero());
	TestTrue(TEXT("World origin is a valid source, not a missing-position sentinel"),
		UBwayCombatFeedbackWidget::ResolveDamageDirection(FVector::ZeroVector, FVector(100, 0, 0), 0).Equals(FVector2D(0, 1)));
	return true;
}
#endif
