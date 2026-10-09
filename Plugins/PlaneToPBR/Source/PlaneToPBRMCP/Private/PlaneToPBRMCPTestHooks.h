#pragma once

#include "PlaneToPBRGenerationWorkflow.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace PlaneToPBRMCPTests
{
	using FWorkflowRunner = TFunction<void(const FPlaneToPBRHuggingFaceRequest&, const FPlaneToPBRMeshOptions&,
		TFunction<void(const FPlaneToPBRGenerationResult&)>)>;
	extern FWorkflowRunner WorkflowRunner;
}
#endif