#pragma once

#include "CoreMinimal.h"

/**
 * Handles importing downloaded image files and the source color image as UTexture2D assets in the Content Browser,
 * ensuring appropriate compression settings and sRGB color space flags per texture role.
 */
class PLANETOPBREDITOR_API FPlaneToPBRTextureImporter
{
public:
	/**
	 * Imports downloaded textures and source image into a timestamped /Game/PlaneToPBR/Generated/Run_... content folder.
	 *
	 * @param TexturePaths Map of texture role keys to local image file paths on disk.
	 * @param SourceImagePath Local file path to the original source image.
	 * @param OutContentPath Destination content folder path where textures were imported.
	 * @param OutTextureAssetPaths Map of texture role keys to imported UTexture asset package paths.
	 * @param OutErrorMessage Diagnostic message populated on failure.
	 * @return True if all texture assets were imported and saved successfully.
	 */
	static bool ImportDownloadedTextures(
		const TMap<FString, FString>& TexturePaths,
		const FString& SourceImagePath,
		FString& OutContentPath,
		TMap<FString, FString>& OutTextureAssetPaths,
		FString& OutErrorMessage);
};
