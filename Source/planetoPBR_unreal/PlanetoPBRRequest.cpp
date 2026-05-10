// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlanetoPBRRequest.h"

#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlanetoPBRPlaneActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetoPBR, Log, All);

const FString UPlanetoPBRRequest::SpaceBaseUrl = TEXT("https://ascarlettvfx-testpbr2026.hf.space");

UPlanetoPBRRequest* UPlanetoPBRRequest::GeneratePBRPlaneFromImage(UObject* InWorldContextObject, const FString& ImagePath, const FString& Prompt, UMaterialInterface* InMasterMaterial)
{
	UPlanetoPBRRequest* Request = NewObject<UPlanetoPBRRequest>();
	Request->AddToRoot();
	Request->WorldContextObject = InWorldContextObject;
	Request->SourceImagePath = ImagePath;
	Request->PromptText = Prompt;
	Request->MasterMaterial = InMasterMaterial;
	UE_LOG(LogPlanetoPBR, Display, TEXT("GeneratePBRPlaneFromImage called. ImagePath='%s', Prompt='%s', MasterMaterial='%s'"),
		*ImagePath,
		*Prompt,
		*GetNameSafe(InMasterMaterial));
	Request->Start();
	return Request;
}

void UPlanetoPBRRequest::Start()
{
	if (!WorldContextObject)
	{
		UE_LOG(LogPlanetoPBR, Error, TEXT("Generation failed before request start: invalid world context."));
		Fail(TEXT("PlaneToPBR requires a valid world context."));
		return;
	}

	if (!FPaths::FileExists(SourceImagePath))
	{
		UE_LOG(LogPlanetoPBR, Error, TEXT("Generation failed before request start: input image not found: %s"), *SourceImagePath);
		Fail(FString::Printf(TEXT("Input image not found: %s"), *SourceImagePath));
		return;
	}

	if (!FFileHelper::LoadFileToArray(SourceImageBytes, *SourceImagePath))
	{
		UE_LOG(LogPlanetoPBR, Error, TEXT("Generation failed before request start: failed to read image: %s"), *SourceImagePath);
		Fail(FString::Printf(TEXT("Failed to read input image: %s"), *SourceImagePath));
		return;
	}

	SourceFileName = FPaths::GetCleanFilename(SourceImagePath);
	Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	SessionHash = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	OutputDirectory = FPaths::ProjectSavedDir() / TEXT("PlaneToPBR_textures");
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);

	UE_LOG(LogPlanetoPBR, Display, TEXT("Starting Hugging Face generation. OutputDirectory='%s', SourceBytes=%d"), *OutputDirectory, SourceImageBytes.Num());
	ResolveFunctionIndex();
}

void UPlanetoPBRRequest::ResolveFunctionIndex()
{
	UE_LOG(LogPlanetoPBR, Display, TEXT("Fetching Gradio config: %s"), *(SpaceBaseUrl / TEXT("config")));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(SpaceBaseUrl / TEXT("config"), TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
	{
		UE_LOG(LogPlanetoPBR, Display, TEXT("Gradio config response. Success=%s, Status=%d"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			Response.IsValid() ? Response->GetResponseCode() : 0);
		if (!bSucceeded || !Response.IsValid())
		{
			Fail(TEXT("Connection error fetching Hugging Face Space config."));
			return;
		}

		if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			Fail(FString::Printf(TEXT("HTTP error fetching Space config: %d"), Response->GetResponseCode()));
			return;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			Fail(TEXT("Invalid JSON received from Space config."));
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
		if (!Root->TryGetArrayField(TEXT("dependencies"), Dependencies))
		{
			Fail(TEXT("Space config missing 'dependencies' field."));
			return;
		}

		for (int32 Index = 0; Index < Dependencies->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* Dependency = nullptr;
			if ((*Dependencies)[Index]->TryGetObject(Dependency) && Dependency && (*Dependency)->GetStringField(TEXT("api_name")) == TEXT("predict"))
			{
				FnIndex = Index;
				UE_LOG(LogPlanetoPBR, Display, TEXT("Resolved Gradio api_name='predict' to fn_index=%d"), FnIndex);
				UploadSourceImage();
				return;
			}
		}

		Fail(TEXT("api_name 'predict' not found in Space config."));
	});
	Request->ProcessRequest();
}

void UPlanetoPBRRequest::UploadSourceImage()
{
	UE_LOG(LogPlanetoPBR, Display, TEXT("Uploading source image to Hugging Face Space."));
	const FString Boundary = TEXT("----Boundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TArray<uint8> Body;
	const FString Header = FString::Printf(
		TEXT("--%s\r\nContent-Disposition: form-data; name=\"files\"; filename=\"%s\"\r\nContent-Type: image/png\r\n\r\n"),
		*Boundary,
		*SourceFileName);
	FTCHARToUTF8 HeaderUtf8(*Header);
	Body.Append(reinterpret_cast<const uint8*>(HeaderUtf8.Get()), HeaderUtf8.Length());
	Body.Append(SourceImageBytes);
	const FString Footer = FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary);
	FTCHARToUTF8 FooterUtf8(*Footer);
	Body.Append(reinterpret_cast<const uint8*>(FooterUtf8.Get()), FooterUtf8.Length());

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(SpaceBaseUrl / TEXT("gradio_api/upload"), TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
	Request->SetContent(Body);
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
	{
		UE_LOG(LogPlanetoPBR, Display, TEXT("Upload response. Success=%s, Status=%d"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			Response.IsValid() ? Response->GetResponseCode() : 0);
		if (!bSucceeded || !Response.IsValid())
		{
			Fail(TEXT("Upload connection error."));
			return;
		}

		if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			Fail(FString::Printf(TEXT("Upload HTTP error: %d"), Response->GetResponseCode()));
			return;
		}

		TArray<TSharedPtr<FJsonValue>> UploadData;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, UploadData) || UploadData.IsEmpty())
		{
			Fail(TEXT("Invalid or empty JSON returned during upload."));
			return;
		}

		UploadedPath = UploadData[0]->AsString();
		UE_LOG(LogPlanetoPBR, Display, TEXT("Upload returned path='%s'"), *UploadedPath);
		JoinQueue();
	});
	Request->ProcessRequest();
}

void UPlanetoPBRRequest::JoinQueue()
{
	UE_LOG(LogPlanetoPBR, Display, TEXT("Joining Gradio queue. SessionHash='%s'"), *SessionHash);
	TSharedRef<FJsonObject> FileObject = MakeShared<FJsonObject>();
	FileObject->SetStringField(TEXT("path"), UploadedPath);
	FileObject->SetStringField(TEXT("orig_name"), SourceFileName);
	FileObject->SetNumberField(TEXT("size"), SourceImageBytes.Num());
	FileObject->SetStringField(TEXT("mime_type"), TEXT("image/png"));

	TArray<TSharedPtr<FJsonValue>> Data;
	Data.Add(MakeShared<FJsonValueObject>(FileObject));
	Data.Add(MakeShared<FJsonValueString>(PromptText));

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetArrayField(TEXT("data"), Data);
	Payload->SetField(TEXT("event_data"), MakeShared<FJsonValueNull>());
	Payload->SetNumberField(TEXT("fn_index"), FnIndex);
	Payload->SetStringField(TEXT("session_hash"), SessionHash);

	FString PayloadString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
	FJsonSerializer::Serialize(Payload, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(SpaceBaseUrl / TEXT("gradio_api/queue/join"), TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(PayloadString);
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
	{
		UE_LOG(LogPlanetoPBR, Display, TEXT("Queue join response. Success=%s, Status=%d"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			Response.IsValid() ? Response->GetResponseCode() : 0);
		if (!bSucceeded || !Response.IsValid())
		{
			Fail(TEXT("Queue join connection error."));
			return;
		}

		if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			Fail(FString::Printf(TEXT("Queue join HTTP error: %d"), Response->GetResponseCode()));
			return;
		}

		TSharedPtr<FJsonObject> JoinData;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, JoinData) || !JoinData.IsValid() || JoinData->GetStringField(TEXT("event_id")).IsEmpty())
		{
			Fail(FString::Printf(TEXT("Invalid queue response: %s"), *Response->GetContentAsString()));
			return;
		}

		UE_LOG(LogPlanetoPBR, Display, TEXT("Queue joined. Polling for output."));
		PollQueue();
	});
	Request->ProcessRequest();
}

void UPlanetoPBRRequest::PollQueue()
{
	UE_LOG(LogPlanetoPBR, Display, TEXT("Polling Gradio queue data."));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(
		SpaceBaseUrl / TEXT("gradio_api/queue/data?session_hash=") + SessionHash,
		TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
	{
		UE_LOG(LogPlanetoPBR, Display, TEXT("Queue poll response. Success=%s, Status=%d, Bytes=%d"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			Response.IsValid() ? Response->GetResponseCode() : 0,
			Response.IsValid() ? Response->GetContent().Num() : 0);
		if (!bSucceeded || !Response.IsValid())
		{
			Fail(TEXT("Queue polling connection error."));
			return;
		}

		const FString Body = Response->GetContentAsString();
		TArray<FString> Lines;
		Body.ParseIntoArrayLines(Lines);

		const TArray<TSharedPtr<FJsonValue>>* OutputData = nullptr;
		for (const FString& Line : Lines)
		{
			if (!Line.StartsWith(TEXT("data:")))
			{
				continue;
			}

			const FString JsonLine = Line.RightChop(5).TrimStartAndEnd();
			if (JsonLine.IsEmpty())
			{
				continue;
			}

			TSharedPtr<FJsonObject> Event;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
			if (!FJsonSerializer::Deserialize(Reader, Event) || !Event.IsValid())
			{
				Fail(TEXT("Invalid JSON received while polling queue."));
				return;
			}

			const FString Message = Event->GetStringField(TEXT("msg"));
			if (Message == TEXT("process_failed"))
			{
				UE_LOG(LogPlanetoPBR, Error, TEXT("Hugging Face process_failed event: %s"), *JsonLine);
				Fail(FString::Printf(TEXT("Space failed: %s"), *JsonLine));
				return;
			}

			if (Message == TEXT("process_completed"))
			{
				UE_LOG(LogPlanetoPBR, Display, TEXT("Hugging Face process completed."));
				const TSharedPtr<FJsonObject>* Output = nullptr;
				if (!Event->TryGetObjectField(TEXT("output"), Output) || !Output || !(*Output)->TryGetArrayField(TEXT("data"), OutputData) || !OutputData || OutputData->Num() < 4)
				{
					Fail(FString::Printf(TEXT("Unexpected output format: %s"), *JsonLine));
					return;
				}

				const FString DiffusePath = BuildOutputPath(TEXT("diffuse"));
				UTexture2D* DiffuseTexture = nullptr;
				if (!FFileHelper::SaveArrayToFile(SourceImageBytes, *DiffusePath) || !DecodeImageFile(DiffusePath, DiffuseTexture, &TextureSet.SourceWidth, &TextureSet.SourceHeight))
				{
					Fail(TEXT("Failed to save or decode diffuse texture."));
					return;
				}
				TextureSet.Diffuse = DiffuseTexture;
				TextureSet.DiffusePath = DiffusePath;

				const TSharedPtr<FJsonObject> DepthObject = (*OutputData)[0]->AsObject();
				const TSharedPtr<FJsonObject> NormalObject = (*OutputData)[1]->AsObject();
				const TSharedPtr<FJsonObject> RoughnessObject = (*OutputData)[2]->AsObject();
				const TSharedPtr<FJsonObject> MaskObject = (*OutputData)[3]->AsObject();
				if (!DepthObject.IsValid() || !NormalObject.IsValid() || !RoughnessObject.IsValid() || !MaskObject.IsValid())
				{
					Fail(TEXT("Missing expected Hugging Face output file objects."));
					return;
				}

				const FString DepthUrl = DepthObject->GetStringField(TEXT("url"));
				const FString NormalUrl = NormalObject->GetStringField(TEXT("url"));
				const FString RoughnessUrl = RoughnessObject->GetStringField(TEXT("url"));
				const FString MaskUrl = MaskObject->GetStringField(TEXT("url"));

				TextureSet.DepthPath = BuildOutputPathForUrl(TEXT("depth"), DepthUrl);
				TextureSet.NormalPath = BuildOutputPathForUrl(TEXT("normal"), NormalUrl);
				TextureSet.RoughnessPath = BuildOutputPathForUrl(TEXT("roughness"), RoughnessUrl);
				TextureSet.MaskPath = BuildOutputPathForUrl(TEXT("mask"), MaskUrl);

				DownloadResult(DepthUrl, TextureSet.DepthPath, true, [this](UTexture2D* Texture) { TextureSet.Depth = Texture; }, [this, NormalUrl, RoughnessUrl, MaskUrl]()
				{
					DownloadResult(NormalUrl, TextureSet.NormalPath, true, [this](UTexture2D* Texture) { TextureSet.Normal = Texture; }, [this, RoughnessUrl, MaskUrl]()
					{
						DownloadResult(RoughnessUrl, TextureSet.RoughnessPath, true, [this](UTexture2D* Texture) { TextureSet.Roughness = Texture; }, [this, MaskUrl]()
						{
							DownloadResult(MaskUrl, TextureSet.MaskPath, true, [this](UTexture2D* Texture) { TextureSet.Mask = Texture; }, [this]()
							{
								Finish();
							});
						});
					});
				});
				return;
			}
		}

		Fail(TEXT("No output received from Hugging Face Space."));
	});
	Request->ProcessRequest();
}

void UPlanetoPBRRequest::DownloadResult(const FString& Url, const FString& FilePath, bool bRequireDecodedTexture, TFunction<void(UTexture2D*)> AssignTexture, TFunction<void()> Continue)
{
	UE_LOG(LogPlanetoPBR, Display, TEXT("Downloading generated texture. Url='%s', FilePath='%s'"), *Url, *FilePath);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(Url, TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([this, FilePath, bRequireDecodedTexture, AssignTexture, Continue](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
	{
		UE_LOG(LogPlanetoPBR, Display, TEXT("Texture download response. Success=%s, Status=%d, FilePath='%s'"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			Response.IsValid() ? Response->GetResponseCode() : 0,
			*FilePath);
		if (!bSucceeded || !Response.IsValid())
		{
			Fail(FString::Printf(TEXT("Download connection error: %s"), *FilePath));
			return;
		}

		if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			Fail(FString::Printf(TEXT("Download HTTP error %d: %s"), Response->GetResponseCode(), *FilePath));
			return;
		}

		if (!FFileHelper::SaveArrayToFile(Response->GetContent(), *FilePath))
		{
			Fail(FString::Printf(TEXT("Failed to write file: %s"), *FilePath));
			return;
		}

		UTexture2D* Texture = nullptr;
		if (!DecodeImageFile(FilePath, Texture))
		{
			const FString Extension = FPaths::GetExtension(FilePath);
			UE_LOG(LogPlanetoPBR, Warning, TEXT("Saved generated texture but could not decode it as a runtime UTexture2D. FilePath='%s', Extension='%s'"), *FilePath, *Extension);
			if (bRequireDecodedTexture)
			{
				Fail(FString::Printf(TEXT("Failed to decode texture: %s"), *FilePath));
				return;
			}
		}

		AssignTexture(Texture);
		Continue();
	});
	Request->ProcessRequest();
}

void UPlanetoPBRRequest::Finish()
{
	UE_LOG(LogPlanetoPBR, Display, TEXT("Finishing generation and spawning plane actor."));
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	APlanetoPBRPlaneActor* PlaneActor = nullptr;
	if (World)
	{
		PlaneActor = World->SpawnActor<APlanetoPBRPlaneActor>(APlanetoPBRPlaneActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
		if (PlaneActor)
		{
			PlaneActor->ApplyGeneratedTextures(TextureSet, MasterMaterial);
		}
	}

	OnCompleted.Broadcast(TextureSet, PlaneActor);
	UE_LOG(LogPlanetoPBR, Display, TEXT("Generation completed. PlaneActor='%s'"), *GetNameSafe(PlaneActor));
	RemoveFromRoot();
}

void UPlanetoPBRRequest::Fail(const FString& ErrorMessage)
{
	UE_LOG(LogPlanetoPBR, Error, TEXT("Generation failed: %s"), *ErrorMessage);
	OnFailed.Broadcast(ErrorMessage);
	RemoveFromRoot();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UPlanetoPBRRequest::CreateRequest(const FString& Url, const FString& Verb)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(Verb);
	Request->SetTimeout(RequestTimeoutSeconds);
	ActiveRequests.Add(Request);
	return Request;
}

bool UPlanetoPBRRequest::DecodeImageFile(const FString& FilePath, UTexture2D*& OutTexture, int32* OutWidth, int32* OutHeight) const
{
	TArray<uint8> CompressedData;
	if (!FFileHelper::LoadFileToArray(CompressedData, *FilePath))
	{
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(CompressedData.GetData(), CompressedData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		return false;
	}

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
	{
		return false;
	}

	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		return false;
	}

	const int32 Width = ImageWrapper->GetWidth();
	const int32 Height = ImageWrapper->GetHeight();
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
	{
		return false;
	}

	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();

	OutTexture = Texture;
	if (OutWidth)
	{
		*OutWidth = Width;
	}
	if (OutHeight)
	{
		*OutHeight = Height;
	}
	return true;
}

FString UPlanetoPBRRequest::BuildOutputPath(const FString& Prefix) const
{
	return OutputDirectory / FString::Printf(TEXT("%s_%s.png"), *Prefix, *Timestamp);
}

FString UPlanetoPBRRequest::BuildOutputPathForUrl(const FString& Prefix, const FString& Url) const
{
	FString UrlWithoutQuery = Url;
	FString Query;
	UrlWithoutQuery.Split(TEXT("?"), &UrlWithoutQuery, &Query);

	FString Extension = FPaths::GetExtension(UrlWithoutQuery).ToLower();
	if (Extension.IsEmpty())
	{
		Extension = TEXT("png");
	}

	return OutputDirectory / FString::Printf(TEXT("%s_%s.%s"), *Prefix, *Timestamp, *Extension);
}
