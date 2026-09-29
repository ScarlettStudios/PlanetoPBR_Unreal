#include "PlaneToPBRHuggingFaceRequestBuilder.h"

#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

FString FPlaneToPBRHuggingFaceRequestBuilder::GetMimeTypeForImagePath(const FString& ImagePath) const
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

	// Fallback generic binary octet stream
	return TEXT("application/octet-stream");
}

TArray<uint8> FPlaneToPBRHuggingFaceRequestBuilder::BuildMultipartUploadBody(
	const FString& Boundary,
	const FString& FileName,
	const FString& MimeType,
	const TArray<uint8>& ImageBytes) const
{
	// Construct the multipart header specifying the form field name "files" expected by Gradio
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

FString FPlaneToPBRHuggingFaceRequestBuilder::BuildQueueJoinPayload(
	const FString& UploadedPath,
	const FString& OriginalFileName,
	const int32 ImageSizeBytes,
	const FString& MimeType,
	const FString& HFPrompt,
	const int32 PredictFunctionIndex,
	const FString& SessionHash) const
{
	// Build the JSON file descriptor matching Gradio's internal FileData object representation
	const TSharedRef<FJsonObject> ImageObject = MakeShared<FJsonObject>();
	ImageObject->SetStringField(TEXT("path"), UploadedPath);
	ImageObject->SetStringField(TEXT("orig_name"), OriginalFileName);
	ImageObject->SetNumberField(TEXT("size"), ImageSizeBytes);
	ImageObject->SetStringField(TEXT("mime_type"), MimeType);

	// Pack the arguments list expected by the predict function: [ImageData, MaterialPromptString]
	TArray<TSharedPtr<FJsonValue>> DataValues;
	DataValues.Add(MakeShared<FJsonValueObject>(ImageObject));
	DataValues.Add(MakeShared<FJsonValueString>(HFPrompt));

	// Wrap in the Gradio queue/join outer request structure
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
