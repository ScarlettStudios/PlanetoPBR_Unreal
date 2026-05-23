#pragma once

#include "CoreMinimal.h"

struct FPlaneToPBRPanelState
{
	FString HFPrompt;
	FString ImagePath;

	bool CanGeneratePBRPlane(bool bGenerationInProgress) const
	{
		return !bGenerationInProgress && !ImagePath.TrimStartAndEnd().IsEmpty();
	}
};
