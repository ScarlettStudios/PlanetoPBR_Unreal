#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PlaneToPBRHuggingFaceResponseParser.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceTimeoutTest,
	"PlaneToPBR.HuggingFace.TimeoutClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceTimeoutTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceResponseParser ResponseParser;

	TestFalse(
		TEXT("Elapsed time below the timeout window does not count as a timeout"),
		ResponseParser.DidQueuePollReachTimeout(298.0, 300.0));

	TestTrue(
		TEXT("Elapsed time inside the final one-second timeout window counts as a timeout"),
		ResponseParser.DidQueuePollReachTimeout(299.0, 300.0));

	TestTrue(
		TEXT("Elapsed time beyond the configured timeout counts as a timeout"),
		ResponseParser.DidQueuePollReachTimeout(301.0, 300.0));

	TestFalse(
		TEXT("Non-positive timeout values are treated as disabled"),
		ResponseParser.DidQueuePollReachTimeout(301.0, 0.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceConfigParseTest,
	"PlaneToPBR.HuggingFace.ResponseParser.ConfigParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceConfigParseTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceResponseParser ResponseParser;

	int32 PredictFunctionIndex = INDEX_NONE;
	FString ErrorMessage;
	TestTrue(
		TEXT("Predict function index is resolved from dependencies"),
		ResponseParser.ResolvePredictFunctionIndex(
			TEXT("{\"dependencies\":[{\"api_name\":\"upload\"},{\"api_name\":\"predict\"}]}"),
			PredictFunctionIndex,
			ErrorMessage));
	TestEqual(TEXT("Predict function index matches dependency position"), PredictFunctionIndex, 1);

	TestFalse(
		TEXT("Missing predict dependency fails"),
		ResponseParser.ResolvePredictFunctionIndex(
			TEXT("{\"dependencies\":[{\"api_name\":\"upload\"}]}"),
			PredictFunctionIndex,
			ErrorMessage));
	TestEqual(TEXT("Failed parse resets function index"), PredictFunctionIndex, INDEX_NONE);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceUploadParseTest,
	"PlaneToPBR.HuggingFace.ResponseParser.UploadParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceUploadParseTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceResponseParser ResponseParser;

	FString UploadedPath;
	FString ErrorMessage;
	TestTrue(
		TEXT("Valid upload response returns uploaded path"),
		ResponseParser.TryParseUploadPath(TEXT("[\"/tmp/input.png\"]"), UploadedPath, ErrorMessage));
	TestEqual(TEXT("Uploaded path parsed"), UploadedPath, FString(TEXT("/tmp/input.png")));

	TestFalse(
		TEXT("Empty upload response fails"),
		ResponseParser.TryParseUploadPath(TEXT("[]"), UploadedPath, ErrorMessage));
	TestTrue(TEXT("Failed upload parse clears path"), UploadedPath.IsEmpty());

	UploadedPath = TEXT("stale");
	TestFalse(
		TEXT("Invalid upload JSON fails"),
		ResponseParser.TryParseUploadPath(TEXT("{not-json"), UploadedPath, ErrorMessage));
	TestTrue(TEXT("Invalid upload parse clears stale path"), UploadedPath.IsEmpty());

	UploadedPath = TEXT("stale");
	TestFalse(
		TEXT("Non-string upload path fails"),
		ResponseParser.TryParseUploadPath(TEXT("[123]"), UploadedPath, ErrorMessage));
	TestTrue(TEXT("Non-string upload parse clears stale path"), UploadedPath.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceQueueJoinParseTest,
	"PlaneToPBR.HuggingFace.ResponseParser.QueueJoinParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceQueueJoinParseTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceResponseParser ResponseParser;

	FString EventId;
	FString ErrorMessage;
	TestTrue(
		TEXT("Valid queue join response returns event id"),
		ResponseParser.TryParseQueueEventId(TEXT("{\"event_id\":\"abc123\"}"), EventId, ErrorMessage));
	TestEqual(TEXT("Event id parsed"), EventId, FString(TEXT("abc123")));

	EventId = TEXT("stale");
	TestFalse(
		TEXT("Missing event id fails"),
		ResponseParser.TryParseQueueEventId(TEXT("{}"), EventId, ErrorMessage));
	TestTrue(TEXT("Missing event id clears stale value"), EventId.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceQueuePollParseTest,
	"PlaneToPBR.HuggingFace.ResponseParser.QueuePollParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceQueuePollParseTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceResponseParser ResponseParser;
	const FString CompletedEvent = TEXT("event: complete\n")
		TEXT("data: {\"msg\":\"process_completed\",\"output\":{\"data\":[{\"url\":\"depth.png\"},{\"url\":\"normal.png\"},{\"url\":\"roughness.png\"},{\"url\":\"mask.png\"}]}}\n");

	FString RawOutputJson;
	FString ErrorMessage;
	TestTrue(
		TEXT("Completed queue poll response returns raw output data"),
		ResponseParser.TryParseQueuePollResponse(CompletedEvent, RawOutputJson, ErrorMessage));
	TestFalse(TEXT("Completed queue poll response produces output JSON"), RawOutputJson.IsEmpty());

	TArray<TSharedPtr<FJsonValue>> OutputData;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(RawOutputJson);
	TestTrue(
		TEXT("Raw output JSON is a serialized array"),
		FJsonSerializer::Deserialize(JsonReader, OutputData) && OutputData.Num() == 4);

	RawOutputJson = TEXT("stale");
	TestFalse(
		TEXT("Failed queue poll response returns error"),
		ResponseParser.TryParseQueuePollResponse(
			TEXT("data: {\"msg\":\"process_failed\",\"output\":{\"error\":[\"bad input\"]}}\n"),
			RawOutputJson,
			ErrorMessage));
	TestTrue(TEXT("Failed queue poll response clears stale output"), RawOutputJson.IsEmpty());
	TestTrue(TEXT("Failed queue poll response includes service error"), ErrorMessage.Contains(TEXT("bad input")));

	RawOutputJson = TEXT("stale");
	TestFalse(
		TEXT("Malformed SSE JSON fails"),
		ResponseParser.TryParseQueuePollResponse(TEXT("data: {not-json}\n"), RawOutputJson, ErrorMessage));
	TestTrue(TEXT("Malformed SSE clears stale output"), RawOutputJson.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRHuggingFaceOutputUrlParseTest,
	"PlaneToPBR.HuggingFace.ResponseParser.OutputUrlParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRHuggingFaceOutputUrlParseTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRHuggingFaceResponseParser ResponseParser;

	TMap<FString, FString> TextureUrls;
	FString ErrorMessage;
	TestTrue(
		TEXT("Valid output metadata returns texture URLs"),
		ResponseParser.TryParseOutputUrls(
			TEXT("[{\"url\":\"depth.png\"},{\"url\":\"normal.png\"},{\"url\":\"roughness.png\"},{\"url\":\"mask.png\"}]"),
			TextureUrls,
			ErrorMessage));
	TestEqual(TEXT("Depth URL parsed"), TextureUrls.FindRef(TEXT("depth")), FString(TEXT("depth.png")));
	TestEqual(TEXT("Normal URL parsed"), TextureUrls.FindRef(TEXT("normal")), FString(TEXT("normal.png")));
	TestEqual(TEXT("Roughness URL parsed"), TextureUrls.FindRef(TEXT("roughness")), FString(TEXT("roughness.png")));
	TestEqual(TEXT("Mask URL parsed"), TextureUrls.FindRef(TEXT("mask")), FString(TEXT("mask.png")));

	TextureUrls.Add(TEXT("stale"), TEXT("value"));
	TestFalse(
		TEXT("Invalid output metadata fails"),
		ResponseParser.TryParseOutputUrls(
			TEXT("[{\"url\":\"depth.png\"},{\"url\":\"normal.png\"}]"),
			TextureUrls,
			ErrorMessage));
	TestEqual(TEXT("Failed output parse clears stale URLs"), TextureUrls.Num(), 0);

	return true;
}

#endif
