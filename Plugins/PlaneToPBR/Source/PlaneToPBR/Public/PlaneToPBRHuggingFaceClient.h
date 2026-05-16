#pragma once

#include "CoreMinimal.h"

struct FPlaneToPBRHuggingFaceRequest
{
	FString ImagePath;
	FString HFPrompt;
};

struct FPlaneToPBRHuggingFaceResult
{
	bool bSucceeded = false;
	FString Message;
};

using FPlaneToPBRHuggingFaceCallback = TFunction<void(const FPlaneToPBRHuggingFaceResult& Result)>;

class PLANETOPBR_API FPlaneToPBRHuggingFaceClient
{
public:
	void GeneratePBRTexturesAsync(
		const FPlaneToPBRHuggingFaceRequest& Request,
		FPlaneToPBRHuggingFaceCallback CompletionCallback);

private:
	static bool ValidateRequest(const FPlaneToPBRHuggingFaceRequest& Request, FString& OutErrorMessage);
	static bool ResolvePredictFunctionIndex(const FString& ConfigJson, int32& OutFunctionIndex, FString& OutErrorMessage);
	static bool TryReadImageFile(const FString& ImagePath, TArray<uint8>& OutImageBytes, FString& OutErrorMessage);
	static FString GetMimeTypeForImagePath(const FString& ImagePath);
	static TArray<uint8> BuildMultipartUploadBody(
		const FString& Boundary,
		const FString& FileName,
		const FString& MimeType,
		const TArray<uint8>& ImageBytes);
	static bool TryParseUploadPath(const FString& UploadJson, FString& OutUploadedPath, FString& OutErrorMessage);
	static FString BuildQueueJoinPayload(
		const FString& UploadedPath,
		const FString& OriginalFileName,
		int32 ImageSizeBytes,
		const FString& MimeType,
		const FString& HFPrompt,
		int32 PredictFunctionIndex,
		const FString& SessionHash);
	static bool TryParseQueueEventId(const FString& QueueJoinJson, FString& OutEventId, FString& OutErrorMessage);
};
