#include "PlaneToPBRHuggingFaceClient.h"

#include "Async/Async.h"
#include "HAL/PlatformFileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
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
		[Request, CompletionCallback = MoveTemp(CompletionCallback)](
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

			TArray<uint8> ImageBytes;
			FString ReadErrorMessage;
			if (!TryReadImageFile(Request.ImagePath, ImageBytes, ReadErrorMessage))
			{
				PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, ReadErrorMessage);
				return;
			}

			const FString Boundary = TEXT("----Boundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			const FString FileName = FPaths::GetCleanFilename(Request.ImagePath);
			const TArray<uint8> UploadBody = BuildMultipartUploadBody(Boundary, FileName, ImageBytes);

			const TSharedRef<IHttpRequest> UploadRequest = FHttpModule::Get().CreateRequest();
			UploadRequest->SetURL(PlaneToPBRHuggingFace::SpaceBaseUrl / TEXT("gradio_api/upload"));
			UploadRequest->SetVerb(TEXT("POST"));
			UploadRequest->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
			UploadRequest->SetContent(UploadBody);
			UploadRequest->OnProcessRequestComplete().BindLambda(
				[CompletionCallback = MoveTemp(CompletionCallback)](
					FHttpRequestPtr UploadRequestPtr,
					FHttpResponsePtr UploadResponse,
					bool bUploadConnectedSuccessfully) mutable
				{
					if (!bUploadConnectedSuccessfully || !UploadResponse.IsValid())
					{
						PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to upload image to Hugging Face Space."));
						return;
					}

					if (UploadResponse->GetResponseCode() < 200 || UploadResponse->GetResponseCode() >= 300)
					{
						PlaneToPBRHuggingFace::CompleteRequest(
							MoveTemp(CompletionCallback),
							false,
							FString::Printf(TEXT("Hugging Face image upload failed with HTTP %d."), UploadResponse->GetResponseCode()));
						return;
					}

					FString UploadedPath;
					FString UploadParseErrorMessage;
					if (!TryParseUploadPath(UploadResponse->GetContentAsString(), UploadedPath, UploadParseErrorMessage))
					{
						PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, UploadParseErrorMessage);
						return;
					}

					PlaneToPBRHuggingFace::CompleteRequest(
						MoveTemp(CompletionCallback),
						true,
						FString::Printf(TEXT("Uploaded image to Hugging Face path: %s"), *UploadedPath));
				});

			if (!UploadRequest->ProcessRequest())
			{
				PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to start Hugging Face image upload request."));
			}
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

bool FPlaneToPBRHuggingFaceClient::TryReadImageFile(const FString& ImagePath, TArray<uint8>& OutImageBytes, FString& OutErrorMessage)
{
	const FString NormalizedImagePath = ImagePath.TrimStartAndEnd();
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*NormalizedImagePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Input image not found: %s"), *NormalizedImagePath);
		return false;
	}

	if (!FFileHelper::LoadFileToArray(OutImageBytes, *NormalizedImagePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to read input image: %s"), *NormalizedImagePath);
		return false;
	}

	return true;
}

TArray<uint8> FPlaneToPBRHuggingFaceClient::BuildMultipartUploadBody(
	const FString& Boundary,
	const FString& FileName,
	const TArray<uint8>& ImageBytes)
{
	const FString Header = FString::Printf(
		TEXT("--%s\r\nContent-Disposition: form-data; name=\"files\"; filename=\"%s\"\r\nContent-Type: image/png\r\n\r\n"),
		*Boundary,
		*FileName);
	const FString Footer = FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary);

	TArray<uint8> Body;
	FTCHARToUTF8 HeaderUtf8(*Header);
	Body.Append(reinterpret_cast<const uint8*>(HeaderUtf8.Get()), HeaderUtf8.Length());
	Body.Append(ImageBytes);
	FTCHARToUTF8 FooterUtf8(*Footer);
	Body.Append(reinterpret_cast<const uint8*>(FooterUtf8.Get()), FooterUtf8.Length());

	return Body;
}

bool FPlaneToPBRHuggingFaceClient::TryParseUploadPath(const FString& UploadJson, FString& OutUploadedPath, FString& OutErrorMessage)
{
	TArray<TSharedPtr<FJsonValue>> UploadResponseArray;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(UploadJson);
	if (!FJsonSerializer::Deserialize(JsonReader, UploadResponseArray))
	{
		OutErrorMessage = TEXT("Hugging Face upload returned invalid JSON.");
		return false;
	}

	if (UploadResponseArray.Num() == 0 || !UploadResponseArray[0].IsValid())
	{
		OutErrorMessage = TEXT("Hugging Face upload returned an empty response.");
		return false;
	}

	OutUploadedPath = UploadResponseArray[0]->AsString();
	if (OutUploadedPath.IsEmpty())
	{
		OutErrorMessage = TEXT("Hugging Face upload response did not include an uploaded path.");
		return false;
	}

	return true;
}
