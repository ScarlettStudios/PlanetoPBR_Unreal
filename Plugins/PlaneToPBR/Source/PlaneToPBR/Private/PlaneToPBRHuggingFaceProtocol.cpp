#include "PlaneToPBRHuggingFaceProtocol.h"

#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace PlaneToPBRHuggingFaceProtocol
{
	const FString PredictApiName = TEXT("predict");
}

FString FPlaneToPBRHuggingFaceProtocol::TruncateResponseBody(const FString& ResponseBody, const int32 MaxLength)
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

bool FPlaneToPBRHuggingFaceProtocol::DidQueuePollReachTimeout(const double ElapsedSeconds, const double TimeoutSeconds)
{
	return TimeoutSeconds > 0.0 && ElapsedSeconds >= TimeoutSeconds - 1.0;
}

bool FPlaneToPBRHuggingFaceProtocol::ResolvePredictFunctionIndex(const FString& ConfigJson, int32& OutFunctionIndex, FString& OutErrorMessage)
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
		if (DependencyObject->TryGetStringField(TEXT("api_name"), ApiName) && ApiName == PlaneToPBRHuggingFaceProtocol::PredictApiName)
		{
			OutFunctionIndex = DependencyIndex;
			return true;
		}
	}

	OutErrorMessage = TEXT("Hugging Face Space config does not include the 'predict' API. The Space may be misconfigured or the API format may have changed.");
	return false;
}

FString FPlaneToPBRHuggingFaceProtocol::GetMimeTypeForImagePath(const FString& ImagePath)
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

TArray<uint8> FPlaneToPBRHuggingFaceProtocol::BuildMultipartUploadBody(
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

bool FPlaneToPBRHuggingFaceProtocol::TryParseUploadPath(const FString& UploadJson, FString& OutUploadedPath, FString& OutErrorMessage)
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

FString FPlaneToPBRHuggingFaceProtocol::BuildQueueJoinPayload(
	const FString& UploadedPath,
	const FString& OriginalFileName,
	const int32 ImageSizeBytes,
	const FString& MimeType,
	const FString& HFPrompt,
	const int32 PredictFunctionIndex,
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

bool FPlaneToPBRHuggingFaceProtocol::TryParseQueueEventId(const FString& QueueJoinJson, FString& OutEventId, FString& OutErrorMessage)
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

bool FPlaneToPBRHuggingFaceProtocol::TryParseQueuePollResponse(const FString& QueuePollText, FString& OutRawOutputJson, FString& OutErrorMessage)
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
				ErrorDetail = TruncateResponseBody(EventJson, 100);
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

bool FPlaneToPBRHuggingFaceProtocol::TryParseOutputUrls(const FString& RawOutputJson, TMap<FString, FString>& OutTextureUrls, FString& OutErrorMessage)
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
