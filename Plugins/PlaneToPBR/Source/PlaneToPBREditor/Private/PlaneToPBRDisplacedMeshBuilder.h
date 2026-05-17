#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

class FPlaneToPBRDisplacedMeshBuilder
{
public:
	static bool CreateGeneratedDisplacedPlaneActor(
		const FString& ContentPath,
		UMaterialInterface* Material,
		const TMap<FString, FString>& TexturePaths,
		FString& OutActorLabel,
		FString& OutErrorMessage);

private:
	static bool CreateDisplacedPlaneObj(
		const FString& DepthTexturePath,
		const FString& ObjPath,
		int32& OutDepthWidth,
		int32& OutDepthHeight,
		int32& OutSubdivisionsY,
		FString& OutErrorMessage);
};
