#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

class FPlaneToPBRMaterialBuilder
{
public:
	static bool CreateGeneratedMaterial(
		const FString& ContentPath,
		const TMap<FString, FString>& TextureAssetPaths,
		FString& OutMaterialPath,
		UMaterialInterface*& OutMaterial,
		FString& OutErrorMessage);
};
