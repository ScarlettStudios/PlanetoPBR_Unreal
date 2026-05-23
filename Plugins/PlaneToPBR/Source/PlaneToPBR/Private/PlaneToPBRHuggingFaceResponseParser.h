#pragma once

#include "CoreMinimal.h"

class FPlaneToPBRHuggingFaceResponseParser
{
public:
	FString TruncateResponseBody(const FString& ResponseBody, int32 MaxLength = 150) const;
	bool DidQueuePollReachTimeout(double ElapsedSeconds, double TimeoutSeconds) const;
	bool ResolvePredictFunctionIndex(const FString& ConfigJson, int32& OutFunctionIndex, FString& OutErrorMessage) const;
	bool TryParseUploadPath(const FString& UploadJson, FString& OutUploadedPath, FString& OutErrorMessage) const;
	bool TryParseQueueEventId(const FString& QueueJoinJson, FString& OutEventId, FString& OutErrorMessage) const;
	bool TryParseQueuePollResponse(const FString& QueuePollText, FString& OutRawOutputJson, FString& OutErrorMessage) const;
	bool TryParseOutputUrls(const FString& RawOutputJson, TMap<FString, FString>& OutTextureUrls, FString& OutErrorMessage) const;
};
