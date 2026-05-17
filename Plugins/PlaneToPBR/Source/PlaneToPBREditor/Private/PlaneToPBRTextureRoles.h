#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "Materials/MaterialExpression.h"

enum class EPlaneToPBRTextureRole : uint8
{
	BaseColor,
	Depth,
	Normal,
	Roughness,
	Mask,
	Unknown
};

struct FPlaneToPBRTextureRoleInfo
{
	EPlaneToPBRTextureRole Role = EPlaneToPBRTextureRole::Unknown;
	const TCHAR* Key = TEXT("");
	const TCHAR* AssetName = TEXT("");
	bool bSRGB = false;
	TextureCompressionSettings CompressionSettings = TC_Default;
	EMaterialSamplerType SamplerType = SAMPLERTYPE_Color;
};

class FPlaneToPBRTextureRoles
{
public:
	static const TArray<EPlaneToPBRTextureRole>& GetRequiredDownloadedRoles();
	static const FPlaneToPBRTextureRoleInfo& GetInfo(EPlaneToPBRTextureRole Role);
	static EPlaneToPBRTextureRole FromKey(const FString& Key);
	static EPlaneToPBRTextureRole FromImportedAssetName(const FString& AssetName);
};
