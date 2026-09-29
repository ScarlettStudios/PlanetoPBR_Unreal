#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PlaneToPBRPanelState.h"

/**
 * Automation test validating button enablement and input validation rules in the panel state model.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRPanelStateGenerateReadinessTest,
	"PlaneToPBR.Editor.PanelState.GenerateReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRPanelStateGenerateReadinessTest::RunTest(const FString& Parameters)
{
	FPlaneToPBRPanelState PanelState;

	TestFalse(
		TEXT("Generation is disabled when image path is empty"),
		PanelState.CanGeneratePBRPlane(false));

	PanelState.ImagePath = TEXT("   ");
	TestFalse(
		TEXT("Generation is disabled when image path is only whitespace"),
		PanelState.CanGeneratePBRPlane(false));

	PanelState.ImagePath = TEXT("C:/Textures/Input.png");
	TestTrue(
		TEXT("Generation is enabled when image path is present and generation is idle"),
		PanelState.CanGeneratePBRPlane(false));

	TestFalse(
		TEXT("Generation is disabled while generation is already in progress"),
		PanelState.CanGeneratePBRPlane(true));

	return true;
}

#endif
