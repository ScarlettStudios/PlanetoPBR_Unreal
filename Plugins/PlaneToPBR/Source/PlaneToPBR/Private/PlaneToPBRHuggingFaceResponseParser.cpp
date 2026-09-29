#include "PlaneToPBRHuggingFaceResponseParser.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace PlaneToPBRHuggingFaceResponseParser
{
	/** Name of the Gradio API endpoint in the dependencies array. */
	const FString PredictApiName = TEXT("predict");
}

FString FPlaneToPBRHuggingFaceResponseParser::TruncateResponseBody(const FString& ResponseBody, const int32 MaxLength) const
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

bool FPlaneToPBRHuggingFaceResponseParser::DidQueuePollReachTimeout(const double ElapsedSeconds, const double TimeoutSeconds) const
{
	// 1-second margin accounts for HTTP socket close latency
	return TimeoutSeconds > 0.0 && ElapsedSeconds >= TimeoutSeconds - 1.0;
}

bool FPlaneToPBRHuggingFaceResponseParser::ResolvePredictFunctionIndex(const FString& ConfigJson, int32& OutFunctionIndex, FString& OutErrorMessage) const
{
	OutFunctionIndex = INDEX_NONE;

	// Parse JSON config object
	TSharedPtr<FJsonObject> ConfigObject;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ConfigJson);
	if (!FJsonSerializer::Deserialize(JsonReader, ConfigObject) || !ConfigObject.IsValid())
	{
		OutErrorMessage = TEXT("Hugging Face Space config returned invalid JSON. The API response format may have changed.");
		return false;
	}

	// Locate dependencies array
	const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
	if (!ConfigObject->TryGetArrayField(TEXT("dependencies"), Dependencies) || !Dependencies)
	{
		OutErrorMessage = TEXT("Hugging Face Space config is missing 'dependencies' field. The API response format may have changed.");
		return false;
	}

	// Iterate dependencies to find the function index matching "predict"
	for (int32 DependencyIndex = 0; DependencyIndex < Dependencies->Num(); ++DependencyIndex)
	{
		if (!(*Dependencies)[DependencyIndex].IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject> DependencyObject = (*Dependencies)[DependencyIndex]->AsObject();
		if (!DependencyObject.IsValid())
		{
			continue;
		}

		FString ApiName;
		if (DependencyObject->TryGetStringField(TEXT("api_name"), ApiName) && ApiName == PlaneToPBRHuggingFaceResponseParser::PredictApiName)
		{
			OutFunctionIndex = DependencyIndex;
			return true;
		}
	}

	OutErrorMessage = TEXT("Hugging Face Space config does not include the 'predict' API. The Space may be misconfigured or the API format may have changed.");
	return false;
}

bool FPlaneToPBRHuggingFaceResponseParser::TryParseUploadPath(const FString& UploadJson, FString& OutUploadedPath, FString& OutErrorMessage) const
{
	OutUploadedPath.Empty();

	// Gradio upload response returns an array of uploaded file paths: ["/path/to/uploaded.png"]
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

	if (UploadResponseArray[0]->Type != EJson::String)
	{
		OutErrorMessage = TEXT("Hugging Face upload response did not include a string uploaded path. The API response format may have changed.");
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

bool FPlaneToPBRHuggingFaceResponseParser::TryParseQueueEventId(const FString& QueueJoinJson, FString& OutEventId, FString& OutErrorMessage) const
{
	OutEventId.Empty();

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

bool FPlaneToPBRHuggingFaceResponseParser::TryParseQueuePollResponse(const FString& QueuePollText, FString& OutRawOutputJson, FString& OutErrorMessage) const
{
	OutRawOutputJson.Empty();

	// Gradio SSE stream contains lines formatted as "data: { ... }"
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

		// Handle explicit server execution failure
		if (MessageType == TEXT("process_failed"))
		{
			FString ErrorDetail;
			const TSharedPtr<FJsonObject>* OutputObject = nullptr;
			if (EventObject->TryGetObjectField(TEXT("output"), OutputObject) && OutputObject && OutputObject->IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* ErrorArray = nullptr;
				if ((*OutputObject)->TryGetArrayField(TEXT("error"), ErrorArray) &&
					ErrorArray &&
					ErrorArray->Num() > 0 &&
					(*ErrorArray)[0].IsValid() &&
					(*ErrorArray)[0]->Type == EJson::String)
				{
					ErrorDetail = (*ErrorArray)[0]->AsString();
				}
				else if ((*OutputObject)->HasField(TEXT("error")))
				{
					(*OutputObject)->TryGetStringField(TEXT("error"), ErrorDetail);
				}
			}

			if (ErrorDetail.IsEmpty())
			{
				ErrorDetail = TruncateResponseBody(EventJson, 100);
			}

			OutErrorMessage = FString::Printf(TEXT("Hugging Face generation failed: %s. Check your input image and prompt."), *ErrorDetail);
			return false;
		}

		// Look for completion event
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

		// Ensure all 4 outputs (depth, normal, roughness, mask) are present
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

bool FPlaneToPBRHuggingFaceResponseParser::TryParseOutputUrls(const FString& RawOutputJson, TMap<FString, FString>& OutTextureUrls, FString& OutErrorMessage) const
{
	OutTextureUrls.Empty();

	TArray<TSharedPtr<FJsonValue>> OutputData;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(RawOutputJson);
	if (!FJsonSerializer::Deserialize(JsonReader, OutputData))
	{
		OutErrorMessage = TEXT("Hugging Face output metadata returned invalid JSON. The API response format may have changed.");
		return false;
	}

	// Map index positions to semantic texture roles
	static const TArray<FString> TextureKeys = { TEXT("depth"), TEXT("normal"), TEXT("roughness"), TEXT("mask") };
	if (OutputData.Num() < TextureKeys.Num())
	{
		OutErrorMessage = FString::Printf(TEXT("Hugging Face output metadata included %d files; expected 4. The API may have changed."), OutputData.Num());
		return false;
	}

	TMap<FString, FString> ParsedTextureUrls;
	for (int32 TextureIndex = 0; TextureIndex < TextureKeys.Num(); ++TextureIndex)
	{
		if (!OutputData[TextureIndex].IsValid())
		{
			OutErrorMessage = FString::Printf(TEXT("Hugging Face output metadata for %s texture is invalid. The API response format may have changed."), *TextureKeys[TextureIndex]);
			return false;
		}

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

		ParsedTextureUrls.Add(TextureKeys[TextureIndex], TextureUrl);
	}

	OutTextureUrls = MoveTemp(ParsedTextureUrls);
	return true;
}
