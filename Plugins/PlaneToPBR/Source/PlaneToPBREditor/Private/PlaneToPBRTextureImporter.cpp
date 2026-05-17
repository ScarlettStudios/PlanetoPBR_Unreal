#include "PlaneToPBRTextureImporter.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlaneToPBRGeneratedAssetNames.h"
#include "PlaneToPBRTextureRoles.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

bool FPlaneToPBRTextureImporter::ImportDownloadedTextures(
	const TMap<FString, FString>& TexturePaths,
	const FString& SourceImagePath,
	FString& OutContentPath,
	TMap<FString, FString>& OutTextureAssetPaths,
	FString& OutErrorMessage)
{
	for (const EPlaneToPBRTextureRole Role : FPlaneToPBRTextureRoles::GetRequiredDownloadedRoles())
	{
		const FPlaneToPBRTextureRoleInfo& RoleInfo = FPlaneToPBRTextureRoles::GetInfo(Role);
		const FString* TexturePath = TexturePaths.Find(RoleInfo.Key);
		if (!TexturePath || TexturePath->IsEmpty())
		{
			OutErrorMessage = FString::Printf(TEXT("Missing downloaded %s texture path."), RoleInfo.Key);
			return false;
		}

		if (!FPaths::FileExists(*TexturePath))
		{
			OutErrorMessage = FString::Printf(TEXT("Downloaded %s texture does not exist: %s"), RoleInfo.Key, **TexturePath);
			return false;
		}
	}

	if (!FPaths::FileExists(SourceImagePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Selected source image does not exist: %s"), *SourceImagePath);
		return false;
	}

	const FString RunFolderName = FPlaneToPBRGeneratedAssetNames::MakeRunFolderName(FDateTime::Now());
	OutContentPath = FPlaneToPBRGeneratedAssetNames::MakeGeneratedContentPath(RunFolderName);

	TArray<UAssetImportTask*> ImportTasks;
	for (const EPlaneToPBRTextureRole Role : FPlaneToPBRTextureRoles::GetRequiredDownloadedRoles())
	{
		const FPlaneToPBRTextureRoleInfo& RoleInfo = FPlaneToPBRTextureRoles::GetInfo(Role);
		const FString& TexturePath = *TexturePaths.Find(RoleInfo.Key);

		UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
		ImportTask->Filename = TexturePath;
		ImportTask->DestinationPath = OutContentPath;
		ImportTask->DestinationName = RoleInfo.AssetName;
		ImportTask->bAutomated = true;
		ImportTask->bReplaceExisting = true;
		ImportTask->bSave = true;
		ImportTasks.Add(ImportTask);
	}

	UAssetImportTask* BaseColorImportTask = NewObject<UAssetImportTask>();
	BaseColorImportTask->Filename = SourceImagePath;
	BaseColorImportTask->DestinationPath = OutContentPath;
	BaseColorImportTask->DestinationName = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::BaseColor).AssetName;
	BaseColorImportTask->bAutomated = true;
	BaseColorImportTask->bReplaceExisting = true;
	BaseColorImportTask->bSave = true;
	ImportTasks.Add(BaseColorImportTask);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().ImportAssetTasks(ImportTasks);

	for (UAssetImportTask* ImportTask : ImportTasks)
	{
		if (!ImportTask || ImportTask->ImportedObjectPaths.Num() == 0)
		{
			OutErrorMessage = FString::Printf(TEXT("Failed to import texture: %s"), ImportTask ? *ImportTask->Filename : TEXT("unknown"));
			return false;
		}

		const FString AssetName = FPaths::GetBaseFilename(ImportTask->ImportedObjectPaths[0]);
		const EPlaneToPBRTextureRole Role = FPlaneToPBRTextureRoles::FromImportedAssetName(AssetName);
		const FPlaneToPBRTextureRoleInfo& RoleInfo = FPlaneToPBRTextureRoles::GetInfo(Role);
		if (Role == EPlaneToPBRTextureRole::Unknown)
		{
			OutErrorMessage = FString::Printf(TEXT("Imported texture has an unknown PlaneToPBR role: %s"), *AssetName);
			return false;
		}

		OutTextureAssetPaths.Add(RoleInfo.Key, ImportTask->ImportedObjectPaths[0]);

		if (UTexture2D* ImportedTexture = LoadObject<UTexture2D>(nullptr, *ImportTask->ImportedObjectPaths[0]))
		{
			// Non-sRGB is required for roughness/mask/depth to preserve linear data values
			ImportedTexture->SRGB = RoleInfo.bSRGB;
			ImportedTexture->CompressionSettings = RoleInfo.CompressionSettings;

			ImportedTexture->PostEditChange();
			ImportedTexture->MarkPackageDirty();

			// Explicit save required to persist texture settings (sRGB, compression) to disk
			UPackage* TexturePackage = ImportedTexture->GetOutermost();
			const FString TexturePackageFileName = FPackageName::LongPackageNameToFilename(TexturePackage->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs TextureSaveArgs;
			TextureSaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			if (!UPackage::SavePackage(TexturePackage, ImportedTexture, *TexturePackageFileName, TextureSaveArgs))
			{
				OutErrorMessage = FString::Printf(TEXT("Failed to save imported texture package: %s"), *TexturePackageFileName);
				return false;
			}
		}
	}

	return true;
}
