#include "PlaneToPBRTextureImporter.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

bool FPlaneToPBRTextureImporter::ImportDownloadedTextures(
	const TMap<FString, FString>& TexturePaths,
	const FString& SourceImagePath,
	FString& OutContentPath,
	TMap<FString, FString>& OutTextureAssetPaths,
	FString& OutErrorMessage)
{
	static const TArray<FString> RequiredTextureKeys = { TEXT("depth"), TEXT("normal"), TEXT("roughness"), TEXT("mask") };

	for (const FString& TextureKey : RequiredTextureKeys)
	{
		const FString* TexturePath = TexturePaths.Find(TextureKey);
		if (!TexturePath || TexturePath->IsEmpty())
		{
			OutErrorMessage = FString::Printf(TEXT("Missing downloaded %s texture path."), *TextureKey);
			return false;
		}

		if (!FPaths::FileExists(*TexturePath))
		{
			OutErrorMessage = FString::Printf(TEXT("Downloaded %s texture does not exist: %s"), *TextureKey, **TexturePath);
			return false;
		}
	}

	if (!FPaths::FileExists(SourceImagePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Selected source image does not exist: %s"), *SourceImagePath);
		return false;
	}

	const FString RunFolderName = TEXT("Run_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	OutContentPath = TEXT("/Game/PlaneToPBR/Generated/") + RunFolderName;

	TArray<UAssetImportTask*> ImportTasks;
	for (const FString& TextureKey : RequiredTextureKeys)
	{
		const FString& TexturePath = *TexturePaths.Find(TextureKey);

		UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
		ImportTask->Filename = TexturePath;
		ImportTask->DestinationPath = OutContentPath;
		ImportTask->DestinationName = TEXT("T_") + TextureKey.Left(1).ToUpper() + TextureKey.RightChop(1);
		ImportTask->bAutomated = true;
		ImportTask->bReplaceExisting = true;
		ImportTask->bSave = true;
		ImportTasks.Add(ImportTask);
	}

	UAssetImportTask* BaseColorImportTask = NewObject<UAssetImportTask>();
	BaseColorImportTask->Filename = SourceImagePath;
	BaseColorImportTask->DestinationPath = OutContentPath;
	BaseColorImportTask->DestinationName = TEXT("T_BaseColor");
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
		const FString TextureKey = AssetName == TEXT("T_BaseColor")
			? TEXT("basecolor")
			: AssetName.RightChop(2).ToLower();
		OutTextureAssetPaths.Add(TextureKey, ImportTask->ImportedObjectPaths[0]);

		if (UTexture2D* ImportedTexture = LoadObject<UTexture2D>(nullptr, *ImportTask->ImportedObjectPaths[0]))
		{
			if (TextureKey == TEXT("basecolor"))
			{
				ImportedTexture->SRGB = true;
				ImportedTexture->CompressionSettings = TC_Default;
			}
			else if (TextureKey == TEXT("normal"))
			{
				ImportedTexture->SRGB = false;
				ImportedTexture->CompressionSettings = TC_Normalmap;
			}
			else
			{
				ImportedTexture->SRGB = false;
				ImportedTexture->CompressionSettings = TC_Grayscale;
			}

			ImportedTexture->PostEditChange();
			ImportedTexture->MarkPackageDirty();

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
