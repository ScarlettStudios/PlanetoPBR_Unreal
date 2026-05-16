#include "PlaneToPBRHuggingFaceClient.h"

#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace PlaneToPBRHuggingFace
{
	const FString SpaceBaseUrl = TEXT("https://ascarlettvfx-testpbr2026.hf.space");
	const FString PredictApiName = TEXT("predict");

	void CompleteRequest(FPlaneToPBRHuggingFaceCallback CompletionCallback, bool bSucceeded, const FString& Message)
	{
		if (!CompletionCallback)
		{
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [CompletionCallback = MoveTemp(CompletionCallback), bSucceeded, Message]()
		{
			FPlaneToPBRHuggingFaceResult Result;
			Result.bSucceeded = bSucceeded;
			Result.Message = Message;
			CompletionCallback(Result);
		});
	}
}

void FPlaneToPBRHuggingFaceClient::GeneratePBRTexturesAsync(
	const FPlaneToPBRHuggingFaceRequest& Request,
	FPlaneToPBRHuggingFaceCallback CompletionCallback)
{
	FString ErrorMessage;
	if (!ValidateRequest(Request, ErrorMessage))
	{
		PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, ErrorMessage);
		return;
	}

	const TSharedRef<IHttpRequest> ConfigRequest = FHttpModule::Get().CreateRequest();
	ConfigRequest->SetURL(PlaneToPBRHuggingFace::SpaceBaseUrl / TEXT("config"));
	ConfigRequest->SetVerb(TEXT("GET"));
	ConfigRequest->OnProcessRequestComplete().BindLambda(
		[CompletionCallback = MoveTemp(CompletionCallback)](
			FHttpRequestPtr RequestPtr,
			FHttpResponsePtr Response,
			bool bConnectedSuccessfully) mutable
		{
			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to fetch Hugging Face Space config."));
				return;
			}

			if (Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
			{
				PlaneToPBRHuggingFace::CompleteRequest(
					MoveTemp(CompletionCallback),
					false,
					FString::Printf(TEXT("Hugging Face Space config request failed with HTTP %d."), Response->GetResponseCode()));
				return;
			}

			int32 PredictFunctionIndex = INDEX_NONE;
			FString ResolveErrorMessage;
			if (!ResolvePredictFunctionIndex(Response->GetContentAsString(), PredictFunctionIndex, ResolveErrorMessage))
			{
				PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, ResolveErrorMessage);
				return;
			}

			PlaneToPBRHuggingFace::CompleteRequest(
				MoveTemp(CompletionCallback),
				true,
				FString::Printf(TEXT("Resolved Hugging Face predict function index: %d."), PredictFunctionIndex));
		});

	if (!ConfigRequest->ProcessRequest())
	{
		PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to start Hugging Face Space config request."));
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

bool FPlaneToPBRHuggingFaceClient::ResolvePredictFunctionIndex(const FString& ConfigJson, int32& OutFunctionIndex, FString& OutErrorMessage)
{
	TSharedPtr<FJsonObject> ConfigObject;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ConfigJson);
	if (!FJsonSerializer::Deserialize(JsonReader, ConfigObject) || !ConfigObject.IsValid())
	{
		OutErrorMessage = TEXT("Hugging Face Space config returned invalid JSON.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
	if (!ConfigObject->TryGetArrayField(TEXT("dependencies"), Dependencies) || !Dependencies)
	{
		OutErrorMessage = TEXT("Hugging Face Space config is missing dependencies.");
		return false;
	}

	for (int32 DependencyIndex = 0; DependencyIndex < Dependencies->Num(); ++DependencyIndex)
	{
		const TSharedPtr<FJsonObject> DependencyObject = (*Dependencies)[DependencyIndex]->AsObject();
		if (!DependencyObject.IsValid())
		{
			continue;
		}

		FString ApiName;
		if (DependencyObject->TryGetStringField(TEXT("api_name"), ApiName) && ApiName == PlaneToPBRHuggingFace::PredictApiName)
		{
			OutFunctionIndex = DependencyIndex;
			return true;
		}
	}

	OutErrorMessage = TEXT("Hugging Face Space config does not include the predict API.");
	return false;
}
