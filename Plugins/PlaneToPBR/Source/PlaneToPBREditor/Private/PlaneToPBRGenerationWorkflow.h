#pragma once

#include "CoreMinimal.h"
#include "PlaneToPBRHuggingFaceClient.h"

class FPlaneToPBRGenerationWorkflow
{
public:
	using FStatusCallback = TFunction<void(const FText& StatusText)>;
	using FCompletionCallback = TFunction<void()>;

	void GeneratePBRPlane(
		const FPlaneToPBRHuggingFaceRequest& Request,
		FStatusCallback StatusCallback,
		FCompletionCallback CompletionCallback);
};
