#include "PlaneToPBRMaterialBuilder.h"

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

bool FPlaneToPBRMaterialBuilder::CreateGeneratedMaterial(
	const FString& ContentPath,
	const TMap<FString, FString>& TextureAssetPaths,
	FString& OutMaterialPath,
	UMaterialInterface*& OutMaterial,
	FString& OutErrorMessage)
{
	OutMaterial = nullptr;

	const FString* NormalAssetPath = TextureAssetPaths.Find(TEXT("normal"));
	const FString* RoughnessAssetPath = TextureAssetPaths.Find(TEXT("roughness"));
	const FString* MaskAssetPath = TextureAssetPaths.Find(TEXT("mask"));
	const FString* BaseColorAssetPath = TextureAssetPaths.Find(TEXT("basecolor"));

	if (!NormalAssetPath || !RoughnessAssetPath || !MaskAssetPath || !BaseColorAssetPath)
	{
		OutErrorMessage = TEXT("Missing imported texture assets required for material creation.");
		return false;
	}

	UTexture* BaseColorTexture = LoadObject<UTexture>(nullptr, **BaseColorAssetPath);
	UTexture* NormalTexture = LoadObject<UTexture>(nullptr, **NormalAssetPath);
	UTexture* RoughnessTexture = LoadObject<UTexture>(nullptr, **RoughnessAssetPath);
	UTexture* MaskTexture = LoadObject<UTexture>(nullptr, **MaskAssetPath);

	if (!BaseColorTexture || !NormalTexture || !RoughnessTexture || !MaskTexture)
	{
		OutErrorMessage = TEXT("Failed to load one or more imported texture assets for material creation.");
		return false;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(TEXT("M_PlaneToPBR"), ContentPath, UMaterial::StaticClass(), MaterialFactory);
	UMaterial* Material = Cast<UMaterial>(CreatedAsset);
	if (!Material)
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to create material in: %s"), *ContentPath);
		return false;
	}

	UMaterialExpressionTextureSample* BaseColorExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	BaseColorExpression->Texture = BaseColorTexture;
	BaseColorExpression->SamplerType = SAMPLERTYPE_Color;
	BaseColorExpression->MaterialExpressionEditorX = -600;
	BaseColorExpression->MaterialExpressionEditorY = -240;
	Material->GetExpressionCollection().AddExpression(BaseColorExpression);
	Material->GetEditorOnlyData()->BaseColor.Expression = BaseColorExpression;

	UMaterialExpressionTextureSample* NormalExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	NormalExpression->Texture = NormalTexture;
	NormalExpression->SamplerType = SAMPLERTYPE_Normal;
	NormalExpression->MaterialExpressionEditorX = -600;
	NormalExpression->MaterialExpressionEditorY = 120;
	Material->GetExpressionCollection().AddExpression(NormalExpression);
	Material->GetEditorOnlyData()->Normal.Expression = NormalExpression;

	UMaterialExpressionTextureSample* RoughnessExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	RoughnessExpression->Texture = RoughnessTexture;
	RoughnessExpression->SamplerType = SAMPLERTYPE_LinearGrayscale;
	RoughnessExpression->MaterialExpressionEditorX = -600;
	RoughnessExpression->MaterialExpressionEditorY = -20;
	Material->GetExpressionCollection().AddExpression(RoughnessExpression);

	UMaterialExpressionTextureSample* MaskExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	MaskExpression->Texture = MaskTexture;
	MaskExpression->SamplerType = SAMPLERTYPE_LinearGrayscale;
	MaskExpression->MaterialExpressionEditorX = -600;
	MaskExpression->MaterialExpressionEditorY = 300;
	Material->GetExpressionCollection().AddExpression(MaskExpression);

	UMaterialExpressionLinearInterpolate* RoughnessMaskBlend = NewObject<UMaterialExpressionLinearInterpolate>(Material);
	RoughnessMaskBlend->ConstB = 0.082f;
	RoughnessMaskBlend->MaterialExpressionEditorX = -250;
	RoughnessMaskBlend->MaterialExpressionEditorY = 120;
	RoughnessMaskBlend->A.Connect(0, RoughnessExpression);
	RoughnessMaskBlend->Alpha.Connect(0, MaskExpression);
	Material->GetExpressionCollection().AddExpression(RoughnessMaskBlend);
	Material->GetEditorOnlyData()->Roughness.Expression = RoughnessMaskBlend;

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	UPackage* MaterialPackage = Material->GetOutermost();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(MaterialPackage->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(MaterialPackage, Material, *PackageFileName, SaveArgs))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to save material package: %s"), *PackageFileName);
		return false;
	}

	OutMaterialPath = ContentPath / TEXT("M_PlaneToPBR");
	OutMaterial = Material;
	return true;
}
