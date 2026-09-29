#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

class FPlaneToPBRHuggingFaceRequestBuilder;
class FPlaneToPBRHuggingFaceResponseParser;

/**
 * Encapsulates the parameters required for a PBR texture generation request to Hugging Face.
 */
struct FPlaneToPBRHuggingFaceRequest
{
	/** Absolute or project-relative local file path to the source image to be converted to PBR maps. */
	FString ImagePath;

	/** Optional text prompt specifying semantic material guidance for mask/map generation (e.g., "windows", "wood grain"). */
	FString HFPrompt;
};

/**
 * Result structure returned upon completion of the Hugging Face generation and download process.
 */
struct FPlaneToPBRHuggingFaceResult
{
	/** True if the generation request and all subsequent texture downloads completed successfully. */
	bool bSucceeded = false;

	/** Human-readable status or error message describing the outcome. */
	FString Message;

	/** Raw JSON response string received from the Hugging Face predict endpoint. */
	FString RawOutputJson;

	/** Map of texture role keys (e.g., "depth", "normal", "roughness", "mask") to downloaded local file paths on disk. */
	TMap<FString, FString> TexturePaths;
};

/**
 * Asynchronous completion callback invoked when texture generation finishes or fails.
 */
using FPlaneToPBRHuggingFaceCallback = TFunction<void(const FPlaneToPBRHuggingFaceResult& Result)>;

/**
 * HTTP client responsible for communicating with the Hugging Face Gradio Space API
 * to orchestrate image upload, queue joining, polling for completion, and downloading output PBR texture maps.
 */
class PLANETOPBR_API FPlaneToPBRHuggingFaceClient
{
public:
	FPlaneToPBRHuggingFaceClient();
	~FPlaneToPBRHuggingFaceClient();

	/**
	 * Initiates asynchronous PBR texture generation for the specified request.
	 *
	 * @param Request Information regarding source image path and optional prompt.
	 * @param CompletionCallback Delegate invoked on the GameThread upon completion or failure.
	 */
	void GeneratePBRTexturesAsync(
		const FPlaneToPBRHuggingFaceRequest& Request,
		FPlaneToPBRHuggingFaceCallback CompletionCallback);

private:
	/** Helper for constructing multipart HTTP requests and JSON payloads. */
	TUniquePtr<FPlaneToPBRHuggingFaceRequestBuilder> RequestBuilder;

	/** Helper for parsing JSON responses from Gradio Space endpoints and SSE streams. */
	TUniquePtr<FPlaneToPBRHuggingFaceResponseParser> ResponseParser;
};
