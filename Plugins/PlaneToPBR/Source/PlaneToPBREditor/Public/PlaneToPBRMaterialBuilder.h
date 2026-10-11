#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

/**
 * Builds and persists Unreal Material graph assets incorporating imported PBR texture maps
 * (BaseColor, Normal, Roughness, and Material Mask blend expressions).
 */
class PLANETOPBREDITOR_API FPlaneToPBRMaterialBuilder
{
public:
	/**
	 * Creates a new UMaterial asset, builds its expression graph wiring up BaseColor, Normal,
	 * Roughness and Alpha Lerp mask, and saves the material package to disk.
	 *
	 * @param ContentPath Destination /Game content directory for the material asset.
	 * @param TextureAssetPaths Map of texture role keys to imported UTexture asset package paths.
	 * @param OutMaterialPath Resulting full package path to the created material.
	 * @param OutMaterial Pointer to the created UMaterialInterface object.
	 * @param OutErrorMessage Diagnostic message populated on failure.
	 * @return True if the material asset was successfully created and saved.
	 */
	static bool CreateGeneratedMaterial(
		const FString& ContentPath,
		const TMap<FString, FString>& TextureAssetPaths,
		FString& OutMaterialPath,
		UMaterialInterface*& OutMaterial,
		FString& OutErrorMessage);
};
