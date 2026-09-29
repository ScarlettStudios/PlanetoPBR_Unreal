#pragma once

#include "CoreMinimal.h"

/**
 * Parser utility for processing responses from Hugging Face Gradio Space endpoints,
 * including config endpoints, upload responses, and SSE queue poll streams.
 */
class FPlaneToPBRHuggingFaceResponseParser
{
public:
	/**
	 * Truncates an HTTP response string to a maximum character length for safe inclusion in log or UI error messages.
	 *
	 * @param ResponseBody Original response string.
	 * @param MaxLength Maximum character length before adding an ellipsis.
	 * @return Truncated string snippet.
	 */
	FString TruncateResponseBody(const FString& ResponseBody, int32 MaxLength = 150) const;

	/**
	 * Determines whether the elapsed polling duration indicates that the generation process reached its timeout limit.
	 *
	 * @param ElapsedSeconds Total elapsed duration in seconds.
	 * @param TimeoutSeconds Maximum allowed timeout duration in seconds.
	 * @return True if elapsed seconds reached or exceeded the timeout threshold.
	 */
	bool DidQueuePollReachTimeout(double ElapsedSeconds, double TimeoutSeconds) const;

	/**
	 * Parses the Space config JSON to resolve the zero-based function index corresponding to the "predict" API.
	 *
	 * @param ConfigJson Serialized JSON string returned from the /config endpoint.
	 * @param OutFunctionIndex Resolved zero-based function index.
	 * @param OutErrorMessage Diagnostic message populated on failure.
	 * @return True if the predict function index was successfully resolved.
	 */
	bool ResolvePredictFunctionIndex(const FString& ConfigJson, int32& OutFunctionIndex, FString& OutErrorMessage) const;

	/**
	 * Parses the JSON array returned by the Gradio file upload endpoint to extract the uploaded server file path.
	 *
	 * @param UploadJson Serialized JSON response from /gradio_api/upload.
	 * @param OutUploadedPath Path string where the file was stored on the server.
	 * @param OutErrorMessage Diagnostic message populated on failure.
	 * @return True if the upload path was extracted successfully.
	 */
	bool TryParseUploadPath(const FString& UploadJson, FString& OutUploadedPath, FString& OutErrorMessage) const;

	/**
	 * Parses the JSON response returned from joining the Gradio queue to extract the event ID.
	 *
	 * @param QueueJoinJson Serialized JSON response from /gradio_api/queue/join.
	 * @param OutEventId Event ID assigned to this queued request.
	 * @param OutErrorMessage Diagnostic message populated on failure.
	 * @return True if the event ID was successfully extracted.
	 */
	bool TryParseQueueEventId(const FString& QueueJoinJson, FString& OutEventId, FString& OutErrorMessage) const;

	/**
	 * Parses the Server-Sent Events (SSE) text stream from the Gradio queue poll endpoint
	 * to extract the raw JSON output produced upon process completion.
	 *
	 * @param QueuePollText Full SSE text stream received from /gradio_api/queue/data.
	 * @param OutRawOutputJson Extracted output data JSON string.
	 * @param OutErrorMessage Diagnostic message populated on failure or server-side error event.
	 * @return True if process completion data was found and extracted.
	 */
	bool TryParseQueuePollResponse(const FString& QueuePollText, FString& OutRawOutputJson, FString& OutErrorMessage) const;

	/**
	 * Parses the raw output JSON from the predict endpoint to extract download URLs for all generated PBR textures.
	 *
	 * @param RawOutputJson Serialized output array JSON from the predict endpoint.
	 * @param OutTextureUrls Map of texture role keys (e.g., "depth", "normal", "roughness", "mask") to download URLs.
	 * @param OutErrorMessage Diagnostic message populated on failure.
	 * @return True if all required texture URLs were extracted.
	 */
	bool TryParseOutputUrls(const FString& RawOutputJson, TMap<FString, FString>& OutTextureUrls, FString& OutErrorMessage) const;
};
