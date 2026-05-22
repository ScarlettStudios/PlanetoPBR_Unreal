#include "PlaneToPBRHuggingFaceClient.h"

#include "Async/Async.h"
#include "HAL/PlatformFileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace PlaneToPBRHuggingFace
{
	const FString SpaceBaseUrl = TEXT("https://ascarlettvfx-testpbr2026.hf.space");
	const FString PredictApiName = TEXT("predict");

	FString TruncateResponseBody(const FString& ResponseBody, int32 MaxLength = 150)
	{
		if (ResponseBody.IsEmpty())
		{
			return TEXT("(empty response)");
		}

		if (ResponseBody.Len() <= MaxLength)
		{
			return ResponseBody;
		}

		return ResponseBody.Left(MaxLength) + TEXT("...");
	}

	void CompleteRequest(
		FPlaneToPBRHuggingFaceCallback CompletionCallback,
		bool bSucceeded,
		const FString& Message,
		const FString& RawOutputJson = FString(),
		const TMap<FString, FString>& TexturePaths = TMap<FString, FString>())
	{
		if (!CompletionCallback)
		{
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [CompletionCallback = MoveTemp(CompletionCallback), bSucceeded, Message, RawOutputJson, TexturePaths]()
		{
			FPlaneToPBRHuggingFaceResult Result;
			Result.bSucceeded = bSucceeded;
			Result.Message = Message;
			Result.RawOutputJson = RawOutputJson;
			Result.TexturePaths = TexturePaths;
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
				PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to connect to Hugging Face Space for config. Check your internet connection and try again."));
				return;
			}

			if (Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
			{
				const FString ResponseExcerpt = PlaneToPBRHuggingFace::TruncateResponseBody(Response->GetContentAsString());
				PlaneToPBRHuggingFace::CompleteRequest(
					MoveTemp(CompletionCallback),
					false,
					FString::Printf(TEXT("Hugging Face Space config request failed (HTTP %d): %s. The Space may be down or sleeping."), Response->GetResponseCode(), *ResponseExcerpt));
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
			const FString MimeType = GetMimeTypeForImagePath(Request.ImagePath);
			const TArray<uint8> UploadBody = BuildMultipartUploadBody(Boundary, FileName, MimeType, ImageBytes);

			const TSharedRef<IHttpRequest> UploadRequest = FHttpModule::Get().CreateRequest();
			UploadRequest->SetURL(PlaneToPBRHuggingFace::SpaceBaseUrl / TEXT("gradio_api/upload"));
			UploadRequest->SetVerb(TEXT("POST"));
			UploadRequest->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
			UploadRequest->SetContent(UploadBody);
			UploadRequest->OnProcessRequestComplete().BindLambda(
				[Request, PredictFunctionIndex, ImageSizeBytes = ImageBytes.Num(), FileName, MimeType, CompletionCallback = MoveTemp(CompletionCallback)](
					FHttpRequestPtr UploadRequestPtr,
					FHttpResponsePtr UploadResponse,
					bool bUploadConnectedSuccessfully) mutable
				{
					if (!bUploadConnectedSuccessfully || !UploadResponse.IsValid())
					{
						PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to connect to Hugging Face Space for image upload. Check your internet connection and try again."));
						return;
					}

					if (UploadResponse->GetResponseCode() < 200 || UploadResponse->GetResponseCode() >= 300)
					{
						const FString ResponseExcerpt = PlaneToPBRHuggingFace::TruncateResponseBody(UploadResponse->GetContentAsString());
						PlaneToPBRHuggingFace::CompleteRequest(
							MoveTemp(CompletionCallback),
							false,
							FString::Printf(TEXT("Hugging Face image upload failed (HTTP %d): %s"), UploadResponse->GetResponseCode(), *ResponseExcerpt));
						return;
					}

					FString UploadedPath;
					FString UploadParseErrorMessage;
					if (!TryParseUploadPath(UploadResponse->GetContentAsString(), UploadedPath, UploadParseErrorMessage))
					{
						PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, UploadParseErrorMessage);
						return;
					}

					const FString SessionHash = FGuid::NewGuid().ToString(EGuidFormats::Digits);
					const FString QueueJoinPayload = BuildQueueJoinPayload(
						UploadedPath,
						FileName,
						ImageSizeBytes,
						MimeType,
						Request.HFPrompt,
						PredictFunctionIndex,
						SessionHash);

					const TSharedRef<IHttpRequest> QueueJoinRequest = FHttpModule::Get().CreateRequest();
					QueueJoinRequest->SetURL(PlaneToPBRHuggingFace::SpaceBaseUrl / TEXT("gradio_api/queue/join"));
					QueueJoinRequest->SetVerb(TEXT("POST"));
					QueueJoinRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
					QueueJoinRequest->SetContentAsString(QueueJoinPayload);
					QueueJoinRequest->OnProcessRequestComplete().BindLambda(
						[CompletionCallback = MoveTemp(CompletionCallback), SessionHash](
							FHttpRequestPtr QueueJoinRequestPtr,
							FHttpResponsePtr QueueJoinResponse,
							bool bQueueJoinConnectedSuccessfully) mutable
						{
							if (!bQueueJoinConnectedSuccessfully || !QueueJoinResponse.IsValid())
							{
								PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to connect to Hugging Face Space to join generation queue. Check your internet connection and try again."));
								return;
							}

							if (QueueJoinResponse->GetResponseCode() < 200 || QueueJoinResponse->GetResponseCode() >= 300)
							{
								const FString ResponseExcerpt = PlaneToPBRHuggingFace::TruncateResponseBody(QueueJoinResponse->GetContentAsString());
								PlaneToPBRHuggingFace::CompleteRequest(
									MoveTemp(CompletionCallback),
									false,
									FString::Printf(TEXT("Hugging Face queue join failed (HTTP %d): %s"), QueueJoinResponse->GetResponseCode(), *ResponseExcerpt));
								return;
							}

							FString EventId;
							FString QueueParseErrorMessage;
							if (!TryParseQueueEventId(QueueJoinResponse->GetContentAsString(), EventId, QueueParseErrorMessage))
							{
								PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, QueueParseErrorMessage);
								return;
							}

							const TSharedRef<IHttpRequest> QueuePollRequest = FHttpModule::Get().CreateRequest();
							QueuePollRequest->SetURL(PlaneToPBRHuggingFace::SpaceBaseUrl / TEXT("gradio_api/queue/data?session_hash=") + SessionHash);
							QueuePollRequest->SetVerb(TEXT("GET"));
							QueuePollRequest->OnProcessRequestComplete().BindLambda(
								[CompletionCallback = MoveTemp(CompletionCallback)](
									FHttpRequestPtr QueuePollRequestPtr,
									FHttpResponsePtr QueuePollResponse,
									bool bQueuePollConnectedSuccessfully) mutable
								{
									if (!bQueuePollConnectedSuccessfully || !QueuePollResponse.IsValid())
									{
										PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to connect to Hugging Face Space to poll generation queue. Check your internet connection and try again."));
										return;
									}

									if (QueuePollResponse->GetResponseCode() < 200 || QueuePollResponse->GetResponseCode() >= 300)
									{
										const FString ResponseExcerpt = PlaneToPBRHuggingFace::TruncateResponseBody(QueuePollResponse->GetContentAsString());
										PlaneToPBRHuggingFace::CompleteRequest(
											MoveTemp(CompletionCallback),
											false,
											FString::Printf(TEXT("Hugging Face queue polling failed (HTTP %d): %s"), QueuePollResponse->GetResponseCode(), *ResponseExcerpt));
										return;
									}

									FString RawOutputJson;
									FString QueuePollErrorMessage;
									if (!TryParseQueuePollResponse(QueuePollResponse->GetContentAsString(), RawOutputJson, QueuePollErrorMessage))
									{
										PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, QueuePollErrorMessage);
										return;
									}

									TMap<FString, FString> TextureUrls;
									FString OutputUrlErrorMessage;
									if (!TryParseOutputUrls(RawOutputJson, TextureUrls, OutputUrlErrorMessage))
									{
										PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, OutputUrlErrorMessage, RawOutputJson);
										return;
									}

									FString OutputDirectory;
									FString OutputDirectoryErrorMessage;
									if (!TryCreateOutputDirectory(OutputDirectory, OutputDirectoryErrorMessage))
									{
										PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, OutputDirectoryErrorMessage, RawOutputJson);
										return;
									}

									DownloadOutputTextures(TextureUrls, OutputDirectory, MoveTemp(CompletionCallback));
								});

							if (!QueuePollRequest->ProcessRequest())
							{
								PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to start Hugging Face queue polling request. This is a rare internal HTTP error."));
							}
						});

					if (!QueueJoinRequest->ProcessRequest())
					{
						PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to start Hugging Face queue join request. This is a rare internal HTTP error."));
					}
				});

			if (!UploadRequest->ProcessRequest())
			{
				PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to start Hugging Face image upload request. This is a rare internal HTTP error."));
			}
		});

	if (!ConfigRequest->ProcessRequest())
	{
		PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to start Hugging Face Space config request. This is a rare internal HTTP error."));
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
		OutErrorMessage = TEXT("Hugging Face Space config returned invalid JSON. The API response format may have changed.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
	if (!ConfigObject->TryGetArrayField(TEXT("dependencies"), Dependencies) || !Dependencies)
	{
		OutErrorMessage = TEXT("Hugging Face Space config is missing 'dependencies' field. The API response format may have changed.");
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

	OutErrorMessage = TEXT("Hugging Face Space config does not include the 'predict' API. The Space may be misconfigured or the API format may have changed.");
	return false;
}

bool FPlaneToPBRHuggingFaceClient::TryReadImageFile(const FString& ImagePath, TArray<uint8>& OutImageBytes, FString& OutErrorMessage)
{
	const FString NormalizedImagePath = ImagePath.TrimStartAndEnd();
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*NormalizedImagePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Input image not found: %s. Verify the file path is correct."), *NormalizedImagePath);
		return false;
	}

	if (!FFileHelper::LoadFileToArray(OutImageBytes, *NormalizedImagePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to read input image: %s. Check file permissions and ensure the file is not locked."), *NormalizedImagePath);
		return false;
	}

	return true;
}

FString FPlaneToPBRHuggingFaceClient::GetMimeTypeForImagePath(const FString& ImagePath)
{
	const FString Extension = FPaths::GetExtension(ImagePath).ToLower();
	if (Extension == TEXT("jpg") || Extension == TEXT("jpeg"))
	{
		return TEXT("image/jpeg");
	}

	if (Extension == TEXT("exr"))
	{
		return TEXT("image/x-exr");
	}

	if (Extension == TEXT("png"))
	{
		return TEXT("image/png");
	}

	return TEXT("application/octet-stream");
}

TArray<uint8> FPlaneToPBRHuggingFaceClient::BuildMultipartUploadBody(
	const FString& Boundary,
	const FString& FileName,
	const FString& MimeType,
	const TArray<uint8>& ImageBytes)
{
	const FString Header = FString::Printf(
		TEXT("--%s\r\nContent-Disposition: form-data; name=\"files\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"),
		*Boundary,
		*FileName,
		*MimeType);
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
		OutErrorMessage = TEXT("Hugging Face upload returned invalid JSON. The API response format may have changed.");
		return false;
	}

	if (UploadResponseArray.Num() == 0 || !UploadResponseArray[0].IsValid())
	{
		OutErrorMessage = TEXT("Hugging Face upload returned an empty response. The API response format may have changed.");
		return false;
	}

	OutUploadedPath = UploadResponseArray[0]->AsString();
	if (OutUploadedPath.IsEmpty())
	{
		OutErrorMessage = TEXT("Hugging Face upload response did not include an uploaded path. The API response format may have changed.");
		return false;
	}

	return true;
}

FString FPlaneToPBRHuggingFaceClient::BuildQueueJoinPayload(
	const FString& UploadedPath,
	const FString& OriginalFileName,
	int32 ImageSizeBytes,
	const FString& MimeType,
	const FString& HFPrompt,
	int32 PredictFunctionIndex,
	const FString& SessionHash)
{
	const TSharedRef<FJsonObject> ImageObject = MakeShared<FJsonObject>();
	ImageObject->SetStringField(TEXT("path"), UploadedPath);
	ImageObject->SetStringField(TEXT("orig_name"), OriginalFileName);
	ImageObject->SetNumberField(TEXT("size"), ImageSizeBytes);
	ImageObject->SetStringField(TEXT("mime_type"), MimeType);

	TArray<TSharedPtr<FJsonValue>> DataValues;
	DataValues.Add(MakeShared<FJsonValueObject>(ImageObject));
	DataValues.Add(MakeShared<FJsonValueString>(HFPrompt));

	const TSharedRef<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
	PayloadObject->SetArrayField(TEXT("data"), DataValues);
	PayloadObject->SetField(TEXT("event_data"), MakeShared<FJsonValueNull>());
	PayloadObject->SetNumberField(TEXT("fn_index"), PredictFunctionIndex);
	PayloadObject->SetStringField(TEXT("session_hash"), SessionHash);

	FString SerializedPayload;
	const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&SerializedPayload);
	FJsonSerializer::Serialize(PayloadObject, JsonWriter);
	return SerializedPayload;
}

bool FPlaneToPBRHuggingFaceClient::TryParseQueueEventId(const FString& QueueJoinJson, FString& OutEventId, FString& OutErrorMessage)
{
	TSharedPtr<FJsonObject> QueueJoinObject;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(QueueJoinJson);
	if (!FJsonSerializer::Deserialize(JsonReader, QueueJoinObject) || !QueueJoinObject.IsValid())
	{
		OutErrorMessage = TEXT("Hugging Face queue join returned invalid JSON. The API response format may have changed.");
		return false;
	}

	if (!QueueJoinObject->TryGetStringField(TEXT("event_id"), OutEventId) || OutEventId.IsEmpty())
	{
		OutErrorMessage = TEXT("Hugging Face queue join response did not include an 'event_id'. The API response format may have changed.");
		return false;
	}

	return true;
}

bool FPlaneToPBRHuggingFaceClient::TryParseQueuePollResponse(const FString& QueuePollText, FString& OutRawOutputJson, FString& OutErrorMessage)
{
	TArray<FString> Lines;
	QueuePollText.ParseIntoArrayLines(Lines);

	for (FString Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (!Line.StartsWith(TEXT("data:")))
		{
			continue;
		}

		FString EventJson = Line.RightChop(5).TrimStartAndEnd();
		if (EventJson.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> EventObject;
		const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(EventJson);
		if (!FJsonSerializer::Deserialize(JsonReader, EventObject) || !EventObject.IsValid())
		{
			OutErrorMessage = TEXT("Hugging Face queue polling returned invalid event JSON. The API response format may have changed.");
			return false;
		}

		FString MessageType;
		if (!EventObject->TryGetStringField(TEXT("msg"), MessageType))
		{
			continue;
		}

		if (MessageType == TEXT("process_failed"))
		{
			// Try to extract a meaningful error message from the output field
			FString ErrorDetail;
			const TSharedPtr<FJsonObject>* OutputObject = nullptr;
			if (EventObject->TryGetObjectField(TEXT("output"), OutputObject) && OutputObject && OutputObject->IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* ErrorArray = nullptr;
				if ((*OutputObject)->TryGetArrayField(TEXT("error"), ErrorArray) && ErrorArray && ErrorArray->Num() > 0)
				{
					ErrorDetail = (*ErrorArray)[0]->AsString();
				}
				else if ((*OutputObject)->HasField(TEXT("error")))
				{
					ErrorDetail = (*OutputObject)->GetStringField(TEXT("error"));
				}
			}

			if (ErrorDetail.IsEmpty())
			{
				// Fallback to truncated raw JSON if no structured error found
				ErrorDetail = PlaneToPBRHuggingFace::TruncateResponseBody(EventJson, 100);
			}

			OutErrorMessage = FString::Printf(TEXT("Hugging Face generation failed: %s. Check your input image and prompt."), *ErrorDetail);
			return false;
		}

		if (MessageType != TEXT("process_completed"))
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* OutputObject = nullptr;
		if (!EventObject->TryGetObjectField(TEXT("output"), OutputObject) || !OutputObject || !OutputObject->IsValid())
		{
			OutErrorMessage = TEXT("Hugging Face completion event is missing 'output' metadata. The API response format may have changed.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OutputData = nullptr;
		if (!(*OutputObject)->TryGetArrayField(TEXT("data"), OutputData) || !OutputData)
		{
			OutErrorMessage = TEXT("Hugging Face completion event is missing 'data' array. The API response format may have changed.");
			return false;
		}

		if (OutputData->Num() < 4)
		{
			OutErrorMessage = FString::Printf(TEXT("Hugging Face completion returned %d output files; expected 4. The API may have changed."), OutputData->Num());
			return false;
		}

		FString SerializedOutputData;
		const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&SerializedOutputData);
		FJsonSerializer::Serialize(*OutputData, JsonWriter);
		OutRawOutputJson = SerializedOutputData;
		return true;
	}

	OutErrorMessage = TEXT("Hugging Face queue polling finished without a completion event. The generation may have timed out or the API response format may have changed.");
	return false;
}

bool FPlaneToPBRHuggingFaceClient::TryParseOutputUrls(const FString& RawOutputJson, TMap<FString, FString>& OutTextureUrls, FString& OutErrorMessage)
{
	TArray<TSharedPtr<FJsonValue>> OutputData;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(RawOutputJson);
	if (!FJsonSerializer::Deserialize(JsonReader, OutputData))
	{
		OutErrorMessage = TEXT("Hugging Face output metadata returned invalid JSON. The API response format may have changed.");
		return false;
	}

	static const TArray<FString> TextureKeys = { TEXT("depth"), TEXT("normal"), TEXT("roughness"), TEXT("mask") };
	if (OutputData.Num() < TextureKeys.Num())
	{
		OutErrorMessage = FString::Printf(TEXT("Hugging Face output metadata included %d files; expected 4. The API may have changed."), OutputData.Num());
		return false;
	}

	for (int32 TextureIndex = 0; TextureIndex < TextureKeys.Num(); ++TextureIndex)
	{
		const TSharedPtr<FJsonObject> TextureObject = OutputData[TextureIndex]->AsObject();
		if (!TextureObject.IsValid())
		{
			OutErrorMessage = FString::Printf(TEXT("Hugging Face output metadata for %s texture is invalid. The API response format may have changed."), *TextureKeys[TextureIndex]);
			return false;
		}

		FString TextureUrl;
		if (!TextureObject->TryGetStringField(TEXT("url"), TextureUrl) || TextureUrl.IsEmpty())
		{
			OutErrorMessage = FString::Printf(TEXT("Hugging Face output metadata for %s texture is missing a download URL. The API response format may have changed."), *TextureKeys[TextureIndex]);
			return false;
		}

		OutTextureUrls.Add(TextureKeys[TextureIndex], TextureUrl);
	}

	return true;
}

bool FPlaneToPBRHuggingFaceClient::TryCreateOutputDirectory(FString& OutOutputDirectory, FString& OutErrorMessage)
{
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	OutOutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PlaneToPBR"), TEXT("Generated"), Timestamp);

	if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*OutOutputDirectory))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to create PlaneToPBR output directory: %s. Check write permissions for the Saved folder."), *OutOutputDirectory);
		return false;
	}

	return true;
}

void FPlaneToPBRHuggingFaceClient::DownloadOutputTextures(
	const TMap<FString, FString>& TextureUrls,
	const FString& OutputDirectory,
	FPlaneToPBRHuggingFaceCallback CompletionCallback)
{
	TArray<TPair<FString, FString>> PendingDownloads;
	for (const TPair<FString, FString>& TextureUrl : TextureUrls)
	{
		PendingDownloads.Add(TextureUrl);
	}

	DownloadNextOutputTexture(MoveTemp(PendingDownloads), TMap<FString, FString>(), OutputDirectory, MoveTemp(CompletionCallback));
}

void FPlaneToPBRHuggingFaceClient::DownloadNextOutputTexture(
	TArray<TPair<FString, FString>> PendingDownloads,
	TMap<FString, FString> DownloadedTexturePaths,
	const FString& OutputDirectory,
	FPlaneToPBRHuggingFaceCallback CompletionCallback)
{
	if (PendingDownloads.Num() == 0)
	{
		PlaneToPBRHuggingFace::CompleteRequest(
			MoveTemp(CompletionCallback),
			true,
			FString::Printf(TEXT("Downloaded Hugging Face textures to: %s"), *OutputDirectory),
			FString(),
			DownloadedTexturePaths);
		return;
	}

	const TPair<FString, FString> CurrentDownload = PendingDownloads.Pop(EAllowShrinking::No);
	const TSharedRef<IHttpRequest> DownloadRequest = FHttpModule::Get().CreateRequest();
	DownloadRequest->SetURL(CurrentDownload.Value);
	DownloadRequest->SetVerb(TEXT("GET"));
	DownloadRequest->OnProcessRequestComplete().BindLambda(
		[PendingDownloads = MoveTemp(PendingDownloads),
		 DownloadedTexturePaths = MoveTemp(DownloadedTexturePaths),
		 OutputDirectory,
		 CurrentDownload,
		 CompletionCallback = MoveTemp(CompletionCallback)](
			FHttpRequestPtr DownloadRequestPtr,
			FHttpResponsePtr DownloadResponse,
			bool bDownloadConnectedSuccessfully) mutable
		{
			if (!bDownloadConnectedSuccessfully || !DownloadResponse.IsValid())
			{
				PlaneToPBRHuggingFace::CompleteRequest(
					MoveTemp(CompletionCallback),
					false,
					FString::Printf(TEXT("Failed to connect to Hugging Face Space to download %s texture. Check your internet connection and try again."), *CurrentDownload.Key));
				return;
			}

			if (DownloadResponse->GetResponseCode() < 200 || DownloadResponse->GetResponseCode() >= 300)
			{
				const FString ResponseExcerpt = PlaneToPBRHuggingFace::TruncateResponseBody(DownloadResponse->GetContentAsString());
				PlaneToPBRHuggingFace::CompleteRequest(
					MoveTemp(CompletionCallback),
					false,
					FString::Printf(TEXT("Hugging Face %s texture download failed (HTTP %d): %s"), *CurrentDownload.Key, DownloadResponse->GetResponseCode(), *ResponseExcerpt));
				return;
			}

			const FString OutputPath = FPaths::Combine(OutputDirectory, CurrentDownload.Key + TEXT(".png"));
			if (!FFileHelper::SaveArrayToFile(DownloadResponse->GetContent(), *OutputPath))
			{
				PlaneToPBRHuggingFace::CompleteRequest(
					MoveTemp(CompletionCallback),
					false,
					FString::Printf(TEXT("Failed to save Hugging Face %s texture to: %s. Check write permissions for the output directory."), *CurrentDownload.Key, *OutputPath));
				return;
			}

			DownloadedTexturePaths.Add(CurrentDownload.Key, OutputPath);
			DownloadNextOutputTexture(MoveTemp(PendingDownloads), MoveTemp(DownloadedTexturePaths), OutputDirectory, MoveTemp(CompletionCallback));
		});

	if (!DownloadRequest->ProcessRequest())
	{
		PlaneToPBRHuggingFace::CompleteRequest(
			MoveTemp(CompletionCallback),
			false,
			FString::Printf(TEXT("Failed to start Hugging Face %s texture download. This is a rare internal HTTP error."), *CurrentDownload.Key));
	}
}
