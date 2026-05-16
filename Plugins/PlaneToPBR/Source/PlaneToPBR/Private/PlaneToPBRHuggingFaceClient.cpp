#include "PlaneToPBRHuggingFaceClient.h"

#include "Async/Async.h"

void FPlaneToPBRHuggingFaceClient::GeneratePBRTexturesAsync(
	const FPlaneToPBRHuggingFaceRequest& Request,
	FPlaneToPBRHuggingFaceCallback CompletionCallback)
{
	FString ErrorMessage;
	if (!ValidateRequest(Request, ErrorMessage))
	{
		if (CompletionCallback)
		{
			AsyncTask(ENamedThreads::GameThread, [CompletionCallback = MoveTemp(CompletionCallback), ErrorMessage]()
			{
				FPlaneToPBRHuggingFaceResult Result;
				Result.bSucceeded = false;
				Result.Message = ErrorMessage;
				CompletionCallback(Result);
			});
		}
		return;
	}

	if (CompletionCallback)
	{
		AsyncTask(ENamedThreads::GameThread, [CompletionCallback = MoveTemp(CompletionCallback), Request]()
		{
			FPlaneToPBRHuggingFaceResult Result;
			Result.bSucceeded = true;
			Result.Message = Request.HFPrompt.TrimStartAndEnd().IsEmpty()
				? FString::Printf(TEXT("Hugging Face generation request accepted for image: %s"), *Request.ImagePath)
				: FString::Printf(TEXT("Hugging Face generation request accepted for image: %s with prompt: %s"), *Request.ImagePath, *Request.HFPrompt);
			CompletionCallback(Result);
		});
	}
}

bool FPlaneToPBRHuggingFaceClient::ValidateRequest(const FPlaneToPBRHuggingFaceRequest& Request, FString& OutErrorMessage)
{
	if (Request.ImagePath.TrimStartAndEnd().IsEmpty())
	{
		OutErrorMessage = TEXT("Image path is required.");
		return false;
	}

	return true;
}
