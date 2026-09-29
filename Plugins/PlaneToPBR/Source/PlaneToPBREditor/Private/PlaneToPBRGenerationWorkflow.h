#pragma once

#include "CoreMinimal.h"
#include "PlaneToPBRHuggingFaceClient.h"

/**
 * High-level coordinator managing the complete end-to-end pipeline:
 * 1. Requesting texture generation from Hugging Face Space
 * 2. Importing downloaded textures into the project
 * 3. Constructing the PBR material graph
 * 4. Generating the displaced StaticMesh and spawning it into the active level
 */
class FPlaneToPBRGenerationWorkflow
{
public:
	/** Status update callback reporting progress messages to the editor UI. */
	using FStatusCallback = TFunction<void(const FText& StatusText)>;

	/** Final completion callback signaled when the workflow reaches success or error termination. */
	using FCompletionCallback = TFunction<void()>;

	/**
	 * Initiates the asynchronous generation, import, material creation, and mesh spawning workflow.
	 *
	 * @param Request Input parameters including source image path and material prompt.
	 * @param StatusCallback Delegate reporting step-by-step progress to UI.
	 * @param CompletionCallback Delegate invoked when generation finishes.
	 */
	void GeneratePBRPlane(
		const FPlaneToPBRHuggingFaceRequest& Request,
		FStatusCallback StatusCallback,
		FCompletionCallback CompletionCallback);
};
