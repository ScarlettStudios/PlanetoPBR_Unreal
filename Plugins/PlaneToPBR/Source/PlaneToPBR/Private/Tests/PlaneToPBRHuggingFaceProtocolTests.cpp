#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PlaneToPBRHuggingFaceProtocol.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceTimeoutTest,
	"PlaneToPBR.HuggingFace.TimeoutClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceTimeoutTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("Elapsed time below the timeout window does not count as a timeout"),
		FPlaneToPBRHuggingFaceProtocol::DidQueuePollReachTimeout(298.0, 300.0));

	TestTrue(
		TEXT("Elapsed time inside the final one-second timeout window counts as a timeout"),
		FPlaneToPBRHuggingFaceProtocol::DidQueuePollReachTimeout(299.0, 300.0));

	TestTrue(
		TEXT("Elapsed time beyond the configured timeout counts as a timeout"),
		FPlaneToPBRHuggingFaceProtocol::DidQueuePollReachTimeout(301.0, 300.0));

	TestFalse(
		TEXT("Non-positive timeout values are treated as disabled"),
		FPlaneToPBRHuggingFaceProtocol::DidQueuePollReachTimeout(301.0, 0.0));

	return true;
}

#endif
