#pragma once

#include "CoreMinimal.h"

class FPlaneToPBRHuggingFaceRequestBuilder
{
public:
	FString GetMimeTypeForImagePath(const FString& ImagePath) const;
	TArray<uint8> BuildMultipartUploadBody(
		const FString& Boundary,
		const FString& FileName,
		const FString& MimeType,
		const TArray<uint8>& ImageBytes) const;
	FString BuildQueueJoinPayload(
		const FString& UploadedPath,
		const FString& OriginalFileName,
		int32 ImageSizeBytes,
		const FString& MimeType,
		const FString& HFPrompt,
		int32 PredictFunctionIndex,
		const FString& SessionHash) const;
};
