#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

class FPlaneToPBRHuggingFaceRequestBuilder;
class FPlaneToPBRHuggingFaceResponseParser;

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
	FPlaneToPBRHuggingFaceClient();
	~FPlaneToPBRHuggingFaceClient();

	void GeneratePBRTexturesAsync(
		const FPlaneToPBRHuggingFaceRequest& Request,
		FPlaneToPBRHuggingFaceCallback CompletionCallback);

private:
	TUniquePtr<FPlaneToPBRHuggingFaceRequestBuilder> RequestBuilder;
	TUniquePtr<FPlaneToPBRHuggingFaceResponseParser> ResponseParser;
};
