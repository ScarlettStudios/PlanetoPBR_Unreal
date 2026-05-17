#pragma once

#include "CoreMinimal.h"

class FPlaneToPBRGeneratedAssetNames
{
public:
	static FString MakeRunFolderName(const FDateTime& Timestamp);
	static FString MakeGeneratedContentPath(const FString& RunFolderName);
	static FString GetMaterialAssetName();
	static FString GetDisplacedMeshAssetName();
	static FString GetDisplacedMeshObjName();
	static FString GetDisplacedMeshOutputDir();
};
