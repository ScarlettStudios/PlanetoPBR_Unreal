#include "PlaneToPBRGeneratedAssetNames.h"

#include "Misc/Paths.h"

FString FPlaneToPBRGeneratedAssetNames::MakeRunFolderName(const FDateTime& Timestamp)
{
	return TEXT("Run_") + Timestamp.ToString(TEXT("%Y%m%d_%H%M%S"));
}

FString FPlaneToPBRGeneratedAssetNames::MakeGeneratedContentPath(const FString& RunFolderName)
{
	return TEXT("/Game/PlaneToPBR/Generated/") + RunFolderName;
}

FString FPlaneToPBRGeneratedAssetNames::GetMaterialAssetName()
{
	return TEXT("M_PlaneToPBR");
}

FString FPlaneToPBRGeneratedAssetNames::GetDisplacedMeshAssetName()
{
	return TEXT("SM_PlaneToPBR_DisplacedPlane");
}

FString FPlaneToPBRGeneratedAssetNames::GetDisplacedMeshObjName()
{
	return TEXT("SM_PlaneToPBR_DisplacedPlane.obj");
}

FString FPlaneToPBRGeneratedAssetNames::GetDisplacedMeshOutputDir()
{
	return FPaths::ProjectSavedDir() / TEXT("PlaneToPBR/Meshes");
}
