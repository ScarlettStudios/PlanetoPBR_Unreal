#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "Materials/MaterialExpression.h"

/**
 * Enumerates the semantic roles of texture maps involved in the PlaneToPBR workflow.
 */
enum class EPlaneToPBRTextureRole : uint8
{
	BaseColor,
	Depth,
	Normal,
	Roughness,
	Mask,
	Unknown
};

/**
 * Configuration and asset metadata associated with a particular texture role.
 */
struct FPlaneToPBRTextureRoleInfo
{
	/** Texture role identifier. */
	EPlaneToPBRTextureRole Role = EPlaneToPBRTextureRole::Unknown;

	/** Key string used in the Hugging Face API payload and texture mapping dictionary. */
	const TCHAR* Key = TEXT("");

	/** Standard Unreal Engine asset name prefix/identifier (e.g. "T_BaseColor", "T_Normal"). */
	const TCHAR* AssetName = TEXT("");

	/** True if the texture should be imported as sRGB (color data), false for linear data (masks, normals, roughness). */
	bool bSRGB = false;

	/** Unreal texture compression settings (e.g. TC_Normalmap, TC_Grayscale, TC_Default). */
	TextureCompressionSettings CompressionSettings = TC_Default;

	/** Material graph expression sampler type matching the texture format. */
	EMaterialSamplerType SamplerType = SAMPLERTYPE_Color;
};

/**
 * Registry and lookup helper for texture roles, settings, and names.
 */
class FPlaneToPBRTextureRoles
{
public:
	/** Returns the list of texture roles required to be downloaded from the Hugging Face Space. */
	static const TArray<EPlaneToPBRTextureRole>& GetRequiredDownloadedRoles();

	/** Retrieves the metadata descriptor for a given texture role. */
	static const FPlaneToPBRTextureRoleInfo& GetInfo(EPlaneToPBRTextureRole Role);

	/** Looks up the texture role corresponding to a role key string (e.g., "roughness"). */
	static EPlaneToPBRTextureRole FromKey(const FString& Key);

	/** Looks up the texture role corresponding to an imported asset name (e.g., "T_BaseColor"). */
	static EPlaneToPBRTextureRole FromImportedAssetName(const FString& AssetName);
};
