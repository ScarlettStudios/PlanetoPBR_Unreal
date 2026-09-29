#pragma once

#include "CoreMinimal.h"

/**
 * Utility class for formulating HTTP requests, multipart payloads, and JSON payloads
 * sent to the Hugging Face Gradio Space endpoints.
 */
class FPlaneToPBRHuggingFaceRequestBuilder
{
public:
	/**
	 * Determines the MIME type string corresponding to an image file's extension.
	 *
	 * @param ImagePath Path or file name of the target image.
	 * @return MIME type string (e.g., "image/png", "image/jpeg", "image/x-exr").
	 */
	FString GetMimeTypeForImagePath(const FString& ImagePath) const;

	/**
	 * Constructs a multipart/form-data byte buffer for uploading an image file to Gradio.
	 *
	 * @param Boundary Unique multipart boundary string.
	 * @param FileName Clean file name of the image being uploaded.
	 * @param MimeType MIME type for the image part.
	 * @param ImageBytes Raw binary bytes of the image file.
	 * @return Byte array containing headers, file payload, and multipart boundary footer.
	 */
	TArray<uint8> BuildMultipartUploadBody(
		const FString& Boundary,
		const FString& FileName,
		const FString& MimeType,
		const TArray<uint8>& ImageBytes) const;

	/**
	 * Constructs the JSON payload required to join the Gradio prediction queue.
	 *
	 * @param UploadedPath Path returned by the Gradio upload endpoint.
	 * @param OriginalFileName Original name of the source image.
	 * @param ImageSizeBytes Size of the image in bytes.
	 * @param MimeType MIME type string of the image.
	 * @param HFPrompt Optional text prompt guiding material mask generation.
	 * @param PredictFunctionIndex Gradio function index for the predict endpoint.
	 * @param SessionHash Unique session identifier hash.
	 * @return Serialized JSON string ready for POST body.
	 */
	FString BuildQueueJoinPayload(
		const FString& UploadedPath,
		const FString& OriginalFileName,
		int32 ImageSizeBytes,
		const FString& MimeType,
		const FString& HFPrompt,
		int32 PredictFunctionIndex,
		const FString& SessionHash) const;
};
