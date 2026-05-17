#pragma once

#include "CoreMinimal.h"

class FPlaneToPBRTextureImporter
{
public:
	static bool ImportDownloadedTextures(
		const TMap<FString, FString>& TexturePaths,
		const FString& SourceImagePath,
		FString& OutContentPath,
		TMap<FString, FString>& OutTextureAssetPaths,
		FString& OutErrorMessage);
};
