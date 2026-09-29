#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PlaneToPBRHuggingFaceRequestBuilder.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * Automation test validating that image extensions correctly map to their standard MIME types.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceMimeTypeTest,
	"PlaneToPBR.HuggingFace.RequestBuilder.MimeTypeMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceMimeTypeTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceRequestBuilder RequestBuilder;

	TestEqual(TEXT("PNG maps to image/png"), RequestBuilder.GetMimeTypeForImagePath(TEXT("Texture.PNG")), FString(TEXT("image/png")));
	TestEqual(TEXT("JPG maps to image/jpeg"), RequestBuilder.GetMimeTypeForImagePath(TEXT("Texture.jpg")), FString(TEXT("image/jpeg")));
	TestEqual(TEXT("JPEG maps to image/jpeg"), RequestBuilder.GetMimeTypeForImagePath(TEXT("Texture.jpeg")), FString(TEXT("image/jpeg")));
	TestEqual(TEXT("EXR maps to image/x-exr"), RequestBuilder.GetMimeTypeForImagePath(TEXT("Texture.exr")), FString(TEXT("image/x-exr")));
	TestEqual(TEXT("Unknown extension maps to octet-stream"), RequestBuilder.GetMimeTypeForImagePath(TEXT("Texture.tif")), FString(TEXT("application/octet-stream")));

	return true;
}

/**
 * Automation test verifying the structure and boundaries of generated multipart/form-data payloads.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceMultipartBodyTest,
	"PlaneToPBR.HuggingFace.RequestBuilder.MultipartUploadBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceMultipartBodyTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceRequestBuilder RequestBuilder;
	const TArray<uint8> ImageBytes = { 'A', 'B', 'C' };
	const TArray<uint8> Body = RequestBuilder.BuildMultipartUploadBody(
		TEXT("Boundary123"),
		TEXT("Input.png"),
		TEXT("image/png"),
		ImageBytes);

	const FUTF8ToTCHAR ConvertedBody(reinterpret_cast<const ANSICHAR*>(Body.GetData()), Body.Num());
	const FString BodyText(ConvertedBody.Length(), ConvertedBody.Get());

	TestTrue(TEXT("Body contains boundary"), BodyText.Contains(TEXT("--Boundary123")));
	TestTrue(TEXT("Body contains file name"), BodyText.Contains(TEXT("filename=\"Input.png\"")));
	TestTrue(TEXT("Body contains MIME type"), BodyText.Contains(TEXT("Content-Type: image/png")));
	TestTrue(TEXT("Body contains image bytes"), BodyText.Contains(TEXT("ABC")));
	TestTrue(TEXT("Body contains closing boundary"), BodyText.Contains(TEXT("--Boundary123--")));

	return true;
}

/**
 * Automation test ensuring the JSON payload to join the Gradio queue adheres to schema requirements.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceQueueJoinPayloadTest,
	"PlaneToPBR.HuggingFace.RequestBuilder.QueueJoinPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceQueueJoinPayloadTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceRequestBuilder RequestBuilder;
	const FString PayloadJson = RequestBuilder.BuildQueueJoinPayload(
		TEXT("/tmp/uploaded.png"),
		TEXT("Input.png"),
		123,
		TEXT("image/png"),
		TEXT("wood grain"),
		3,
		TEXT("Session123"));

	TSharedPtr<FJsonObject> PayloadObject;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(PayloadJson);
	TestTrue(TEXT("Payload is valid JSON"), FJsonSerializer::Deserialize(JsonReader, PayloadObject) && PayloadObject.IsValid());
	if (!PayloadObject.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* DataValues = nullptr;
	TestTrue(TEXT("Payload contains data array"), PayloadObject->TryGetArrayField(TEXT("data"), DataValues) && DataValues && DataValues->Num() == 2);
	if (!DataValues || DataValues->Num() < 2)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> ImageObject = (*DataValues)[0]->AsObject();
	TestTrue(TEXT("First data element is image metadata"), ImageObject.IsValid());
	if (!ImageObject.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("Image path serialized"), ImageObject->GetStringField(TEXT("path")), FString(TEXT("/tmp/uploaded.png")));
	TestEqual(TEXT("Original file name serialized"), ImageObject->GetStringField(TEXT("orig_name")), FString(TEXT("Input.png")));
	TestEqual(TEXT("Image size serialized"), static_cast<int32>(ImageObject->GetNumberField(TEXT("size"))), 123);
	TestEqual(TEXT("MIME type serialized"), ImageObject->GetStringField(TEXT("mime_type")), FString(TEXT("image/png")));
	TestEqual(TEXT("Prompt serialized"), (*DataValues)[1]->AsString(), FString(TEXT("wood grain")));
	TestEqual(TEXT("Function index serialized"), static_cast<int32>(PayloadObject->GetNumberField(TEXT("fn_index"))), 3);
	TestEqual(TEXT("Session hash serialized"), PayloadObject->GetStringField(TEXT("session_hash")), FString(TEXT("Session123")));
	TestTrue(TEXT("Event data is present as null"), PayloadObject->HasField(TEXT("event_data")));

	return true;
}

#endif
