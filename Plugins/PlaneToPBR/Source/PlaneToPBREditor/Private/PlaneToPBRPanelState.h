#pragma once

#include "CoreMinimal.h"

/**
 * Encapsulates the UI input state and validation logic for the PlaneToPBR editor panel.
 */
struct FPlaneToPBRPanelState
{
	/** Optional material mask prompt entered by the user. */
	FString HFPrompt;

	/** Path to the source image chosen by the user. */
	FString ImagePath;

	/**
	 * Determines whether the Generate button should be active and clickable.
	 *
	 * @param bGenerationInProgress True if an async generation process is currently running.
	 * @return True if generation is idle and a valid non-empty image path has been provided.
	 */
	bool CanGeneratePBRPlane(bool bGenerationInProgress) const
	{
		return !bGenerationInProgress && !ImagePath.TrimStartAndEnd().IsEmpty();
	}
};
