#pragma once

#include "CoreMinimal.h"

/**
 * Standardized path and naming conventions for generated PlaneToPBR assets and packages.
 */
class FPlaneToPBRGeneratedAssetNames
{
public:
	/** Formats a unique folder name based on generation timestamp (e.g., "Run_20260929_154300"). */
	static FString MakeRunFolderName(const FDateTime& Timestamp);

	/** Generates the full /Game package content path for a specific generation run. */
	static FString MakeGeneratedContentPath(const FString& RunFolderName);

	/** Returns the asset name for the generated Unreal Material (e.g. "M_PlaneToPBR"). */
	static FString GetMaterialAssetName();

	/** Returns the asset name for the generated Static Mesh (e.g. "SM_PlaneToPBR_DisplacedPlane"). */
	static FString GetDisplacedMeshAssetName();

	/** Returns the file name for intermediate OBJ mesh exports. */
	static FString GetDisplacedMeshObjName();

	/** Returns the directory path on disk where intermediate OBJ meshes are saved. */
	static FString GetDisplacedMeshOutputDir();
};
