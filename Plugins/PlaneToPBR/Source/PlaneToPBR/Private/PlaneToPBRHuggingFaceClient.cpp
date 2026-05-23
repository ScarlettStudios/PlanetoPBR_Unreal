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
#include "PlaneToPBRHuggingFaceProtocol.h"

namespace PlaneToPBRHuggingFace
{
	const FString SpaceBaseUrl = TEXT("https://ascarlettvfx-testpbr2026.hf.space");
	const float GenerationTimeoutSeconds = 300.0f;

	void DownloadOutputTextures(
		const TMap<FString, FString>& TextureUrls,
		const FString& OutputDirectory,
		FPlaneToPBRHuggingFaceCallback CompletionCallback);
	void DownloadNextOutputTexture(
		TArray<TPair<FString, FString>> PendingDownloads,
		TMap<FString, FString> DownloadedTexturePaths,
		const FString& OutputDirectory,
		FPlaneToPBRHuggingFaceCallback CompletionCallback);

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

	bool ValidateRequest(const FPlaneToPBRHuggingFaceRequest& Request, FString& OutErrorMessage)
	{
		if (Request.ImagePath.TrimStartAndEnd().IsEmpty())
		{
			OutErrorMessage = TEXT("Image path is required.");
			return false;
		}

		return true;
	}

	bool TryReadImageFile(const FString& ImagePath, TArray<uint8>& OutImageBytes, FString& OutErrorMessage)
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

	bool TryCreateOutputDirectory(FString& OutOutputDirectory, FString& OutErrorMessage)
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
}

void FPlaneToPBRHuggingFaceClient::GeneratePBRTexturesAsync(
	const FPlaneToPBRHuggingFaceRequest& Request,
	FPlaneToPBRHuggingFaceCallback CompletionCallback)
{
	FString ErrorMessage;
	if (!PlaneToPBRHuggingFace::ValidateRequest(Request, ErrorMessage))
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
				const FString ResponseExcerpt = FPlaneToPBRHuggingFaceProtocol::TruncateResponseBody(Response->GetContentAsString());
				PlaneToPBRHuggingFace::CompleteRequest(
					MoveTemp(CompletionCallback),
					false,
					FString::Printf(TEXT("Hugging Face Space config request failed (HTTP %d): %s. The Space may be down or sleeping."), Response->GetResponseCode(), *ResponseExcerpt));
				return;
			}

			int32 PredictFunctionIndex = INDEX_NONE;
			FString ResolveErrorMessage;
			if (!FPlaneToPBRHuggingFaceProtocol::ResolvePredictFunctionIndex(Response->GetContentAsString(), PredictFunctionIndex, ResolveErrorMessage))
			{
				PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, ResolveErrorMessage);
				return;
			}

			TArray<uint8> ImageBytes;
			FString ReadErrorMessage;
			if (!PlaneToPBRHuggingFace::TryReadImageFile(Request.ImagePath, ImageBytes, ReadErrorMessage))
			{
				PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, ReadErrorMessage);
				return;
			}

			const FString Boundary = TEXT("----Boundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			const FString FileName = FPaths::GetCleanFilename(Request.ImagePath);
			const FString MimeType = FPlaneToPBRHuggingFaceProtocol::GetMimeTypeForImagePath(Request.ImagePath);
			const TArray<uint8> UploadBody = FPlaneToPBRHuggingFaceProtocol::BuildMultipartUploadBody(Boundary, FileName, MimeType, ImageBytes);

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
						const FString ResponseExcerpt = FPlaneToPBRHuggingFaceProtocol::TruncateResponseBody(UploadResponse->GetContentAsString());
						PlaneToPBRHuggingFace::CompleteRequest(
							MoveTemp(CompletionCallback),
							false,
							FString::Printf(TEXT("Hugging Face image upload failed (HTTP %d): %s"), UploadResponse->GetResponseCode(), *ResponseExcerpt));
						return;
					}

					FString UploadedPath;
					FString UploadParseErrorMessage;
					if (!FPlaneToPBRHuggingFaceProtocol::TryParseUploadPath(UploadResponse->GetContentAsString(), UploadedPath, UploadParseErrorMessage))
					{
						PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, UploadParseErrorMessage);
						return;
					}

					const FString SessionHash = FGuid::NewGuid().ToString(EGuidFormats::Digits);
					const FString QueueJoinPayload = FPlaneToPBRHuggingFaceProtocol::BuildQueueJoinPayload(
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
								const FString ResponseExcerpt = FPlaneToPBRHuggingFaceProtocol::TruncateResponseBody(QueueJoinResponse->GetContentAsString());
								PlaneToPBRHuggingFace::CompleteRequest(
									MoveTemp(CompletionCallback),
									false,
									FString::Printf(TEXT("Hugging Face queue join failed (HTTP %d): %s"), QueueJoinResponse->GetResponseCode(), *ResponseExcerpt));
								return;
							}

							FString EventId;
							FString QueueParseErrorMessage;
							if (!FPlaneToPBRHuggingFaceProtocol::TryParseQueueEventId(QueueJoinResponse->GetContentAsString(), EventId, QueueParseErrorMessage))
							{
								PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, QueueParseErrorMessage);
								return;
							}

							const TSharedRef<IHttpRequest> QueuePollRequest = FHttpModule::Get().CreateRequest();
							QueuePollRequest->SetURL(PlaneToPBRHuggingFace::SpaceBaseUrl / TEXT("gradio_api/queue/data?session_hash=") + SessionHash);
							QueuePollRequest->SetVerb(TEXT("GET"));
							QueuePollRequest->SetTimeout(PlaneToPBRHuggingFace::GenerationTimeoutSeconds);
							QueuePollRequest->OnProcessRequestComplete().BindLambda(
								[CompletionCallback = MoveTemp(CompletionCallback), QueuePollStartTime = FPlatformTime::Seconds()](
									FHttpRequestPtr QueuePollRequestPtr,
									FHttpResponsePtr QueuePollResponse,
									bool bQueuePollConnectedSuccessfully) mutable
								{
									if (!bQueuePollConnectedSuccessfully || !QueuePollResponse.IsValid())
									{
										const double ElapsedSeconds = FPlatformTime::Seconds() - QueuePollStartTime;
										if (FPlaneToPBRHuggingFaceProtocol::DidQueuePollReachTimeout(ElapsedSeconds, PlaneToPBRHuggingFace::GenerationTimeoutSeconds))
										{
											PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Hugging Face generation timed out after 5 minutes. The Space may be busy or asleep; try again."));
											return;
										}

										PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, TEXT("Failed to connect to Hugging Face Space to poll generation queue. Check your internet connection and try again."));
										return;
									}

									if (QueuePollResponse->GetResponseCode() < 200 || QueuePollResponse->GetResponseCode() >= 300)
									{
										const FString ResponseExcerpt = FPlaneToPBRHuggingFaceProtocol::TruncateResponseBody(QueuePollResponse->GetContentAsString());
										PlaneToPBRHuggingFace::CompleteRequest(
											MoveTemp(CompletionCallback),
											false,
											FString::Printf(TEXT("Hugging Face queue polling failed (HTTP %d): %s"), QueuePollResponse->GetResponseCode(), *ResponseExcerpt));
										return;
									}

									FString RawOutputJson;
									FString QueuePollErrorMessage;
									if (!FPlaneToPBRHuggingFaceProtocol::TryParseQueuePollResponse(QueuePollResponse->GetContentAsString(), RawOutputJson, QueuePollErrorMessage))
									{
										PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, QueuePollErrorMessage);
										return;
									}

									TMap<FString, FString> TextureUrls;
									FString OutputUrlErrorMessage;
									if (!FPlaneToPBRHuggingFaceProtocol::TryParseOutputUrls(RawOutputJson, TextureUrls, OutputUrlErrorMessage))
									{
										PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, OutputUrlErrorMessage, RawOutputJson);
										return;
									}

									FString OutputDirectory;
									FString OutputDirectoryErrorMessage;
									if (!PlaneToPBRHuggingFace::TryCreateOutputDirectory(OutputDirectory, OutputDirectoryErrorMessage))
									{
										PlaneToPBRHuggingFace::CompleteRequest(MoveTemp(CompletionCallback), false, OutputDirectoryErrorMessage, RawOutputJson);
										return;
									}

									PlaneToPBRHuggingFace::DownloadOutputTextures(TextureUrls, OutputDirectory, MoveTemp(CompletionCallback));
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

void PlaneToPBRHuggingFace::DownloadOutputTextures(
	const TMap<FString, FString>& TextureUrls,
	const FString& OutputDirectory,
	FPlaneToPBRHuggingFaceCallback CompletionCallback)
{
	TArray<TPair<FString, FString>> PendingDownloads;
	for (const TPair<FString, FString>& TextureUrl : TextureUrls)
	{
		PendingDownloads.Add(TextureUrl);
	}

	PlaneToPBRHuggingFace::DownloadNextOutputTexture(MoveTemp(PendingDownloads), TMap<FString, FString>(), OutputDirectory, MoveTemp(CompletionCallback));
}

void PlaneToPBRHuggingFace::DownloadNextOutputTexture(
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
				const FString ResponseExcerpt = FPlaneToPBRHuggingFaceProtocol::TruncateResponseBody(DownloadResponse->GetContentAsString());
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
			PlaneToPBRHuggingFace::DownloadNextOutputTexture(MoveTemp(PendingDownloads), MoveTemp(DownloadedTexturePaths), OutputDirectory, MoveTemp(CompletionCallback));
		});

	if (!DownloadRequest->ProcessRequest())
	{
		PlaneToPBRHuggingFace::CompleteRequest(
			MoveTemp(CompletionCallback),
			false,
			FString::Printf(TEXT("Failed to start Hugging Face %s texture download. This is a rare internal HTTP error."), *CurrentDownload.Key));
	}
}
