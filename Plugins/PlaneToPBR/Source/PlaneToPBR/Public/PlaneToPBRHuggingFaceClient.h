#pragma once

#include "CoreMinimal.h"

struct FPlaneToPBRHuggingFaceRequest
{
	FString ImagePath;
	FString HFPrompt;
};

struct FPlaneToPBRHuggingFaceResult
{
	bool bSucceeded = false;
	FString Message;
	FString RawOutputJson;
	TMap<FString, FString> TexturePaths;
};

using FPlaneToPBRHuggingFaceCallback = TFunction<void(const FPlaneToPBRHuggingFaceResult& Result)>;

class PLANETOPBR_API FPlaneToPBRHuggingFaceClient
{
public:
	void GeneratePBRTexturesAsync(
		const FPlaneToPBRHuggingFaceRequest& Request,
		FPlaneToPBRHuggingFaceCallback CompletionCallback);
};
