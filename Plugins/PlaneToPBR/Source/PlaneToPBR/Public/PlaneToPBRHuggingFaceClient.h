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
};

using FPlaneToPBRHuggingFaceCallback = TFunction<void(const FPlaneToPBRHuggingFaceResult& Result)>;

class PLANETOPBR_API FPlaneToPBRHuggingFaceClient
{
public:
	void GeneratePBRTexturesAsync(
		const FPlaneToPBRHuggingFaceRequest& Request,
		FPlaneToPBRHuggingFaceCallback CompletionCallback);

private:
	static bool ValidateRequest(const FPlaneToPBRHuggingFaceRequest& Request, FString& OutErrorMessage);
	static bool ResolvePredictFunctionIndex(const FString& ConfigJson, int32& OutFunctionIndex, FString& OutErrorMessage);
};
