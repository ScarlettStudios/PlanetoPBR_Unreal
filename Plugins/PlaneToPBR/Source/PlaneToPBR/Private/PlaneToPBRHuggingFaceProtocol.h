#pragma once

#include "CoreMinimal.h"

class FPlaneToPBRHuggingFaceProtocol
{
public:
	static FString TruncateResponseBody(const FString& ResponseBody, int32 MaxLength = 150);
	static bool DidQueuePollReachTimeout(double ElapsedSeconds, double TimeoutSeconds);
	static bool ResolvePredictFunctionIndex(const FString& ConfigJson, int32& OutFunctionIndex, FString& OutErrorMessage);
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
	static bool TryParseQueuePollResponse(const FString& QueuePollText, FString& OutRawOutputJson, FString& OutErrorMessage);
	static bool TryParseOutputUrls(const FString& RawOutputJson, TMap<FString, FString>& OutTextureUrls, FString& OutErrorMessage);
};
