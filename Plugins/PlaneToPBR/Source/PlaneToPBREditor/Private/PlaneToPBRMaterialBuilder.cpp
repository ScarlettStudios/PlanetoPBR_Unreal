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
#include "PlaneToPBRGeneratedAssetNames.h"
#include "PlaneToPBRTextureRoles.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace PlaneToPBRMaterialBuilder
{
	/** Helper to resolve and load a UTexture asset by its role identifier. */
	static UTexture* LoadTextureForRole(
		const TMap<FString, FString>& TextureAssetPaths,
		const EPlaneToPBRTextureRole Role)
	{
		const FPlaneToPBRTextureRoleInfo& RoleInfo = FPlaneToPBRTextureRoles::GetInfo(Role);
		const FString* AssetPath = TextureAssetPaths.Find(RoleInfo.Key);
		return AssetPath ? LoadObject<UTexture>(nullptr, **AssetPath) : nullptr;
	}
}

bool FPlaneToPBRMaterialBuilder::CreateGeneratedMaterial(
	const FString& ContentPath,
	const TMap<FString, FString>& TextureAssetPaths,
	FString& OutMaterialPath,
	UMaterialInterface*& OutMaterial,
	FString& OutErrorMessage)
{
	OutMaterial = nullptr;

	// Load imported textures for each required role
	UTexture* BaseColorTexture = PlaneToPBRMaterialBuilder::LoadTextureForRole(TextureAssetPaths, EPlaneToPBRTextureRole::BaseColor);
	UTexture* NormalTexture = PlaneToPBRMaterialBuilder::LoadTextureForRole(TextureAssetPaths, EPlaneToPBRTextureRole::Normal);
	UTexture* RoughnessTexture = PlaneToPBRMaterialBuilder::LoadTextureForRole(TextureAssetPaths, EPlaneToPBRTextureRole::Roughness);
	UTexture* MaskTexture = PlaneToPBRMaterialBuilder::LoadTextureForRole(TextureAssetPaths, EPlaneToPBRTextureRole::Mask);

	if (!BaseColorTexture || !NormalTexture || !RoughnessTexture || !MaskTexture)
	{
		OutErrorMessage = TEXT("Missing or failed to load one or more imported texture assets for material creation.");
		return false;
	}

	// Create new Material asset package
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(FPlaneToPBRGeneratedAssetNames::GetMaterialAssetName(), ContentPath, UMaterial::StaticClass(), MaterialFactory);
	UMaterial* Material = Cast<UMaterial>(CreatedAsset);
	if (!Material)
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to create material in: %s"), *ContentPath);
		return false;
	}

	// 1. BaseColor Texture Sample Expression
	UMaterialExpressionTextureSample* BaseColorExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	BaseColorExpression->Texture = BaseColorTexture;
	// LinearGrayscale sampler type matches non-sRGB texture settings for correct shader sampling
	BaseColorExpression->SamplerType = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::BaseColor).SamplerType;
	BaseColorExpression->MaterialExpressionEditorX = -600;
	BaseColorExpression->MaterialExpressionEditorY = -240;
	Material->GetExpressionCollection().AddExpression(BaseColorExpression);
	Material->GetEditorOnlyData()->BaseColor.Expression = BaseColorExpression;

	// 2. Normal Map Texture Sample Expression
	UMaterialExpressionTextureSample* NormalExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	NormalExpression->Texture = NormalTexture;
	NormalExpression->SamplerType = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::Normal).SamplerType;
	NormalExpression->MaterialExpressionEditorX = -600;
	NormalExpression->MaterialExpressionEditorY = 120;
	Material->GetExpressionCollection().AddExpression(NormalExpression);
	Material->GetEditorOnlyData()->Normal.Expression = NormalExpression;

	// 3. Roughness Map Texture Sample Expression
	UMaterialExpressionTextureSample* RoughnessExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	RoughnessExpression->Texture = RoughnessTexture;
	RoughnessExpression->SamplerType = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::Roughness).SamplerType;
	RoughnessExpression->MaterialExpressionEditorX = -600;
	RoughnessExpression->MaterialExpressionEditorY = -20;
	Material->GetExpressionCollection().AddExpression(RoughnessExpression);

	// 4. Material Mask Texture Sample Expression
	UMaterialExpressionTextureSample* MaskExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	MaskExpression->Texture = MaskTexture;
	MaskExpression->SamplerType = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::Mask).SamplerType;
	MaskExpression->MaterialExpressionEditorX = -600;
	MaskExpression->MaterialExpressionEditorY = 300;
	Material->GetExpressionCollection().AddExpression(MaskExpression);

	// 5. Linear Interpolate Expression blending Roughness with shiny specular highlights (0.082f) where mask is active
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

	// Explicit save required to persist material graph and connections to disk
	UPackage* MaterialPackage = Material->GetOutermost();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(MaterialPackage->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(MaterialPackage, Material, *PackageFileName, SaveArgs))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to save material package: %s"), *PackageFileName);
		return false;
	}

	OutMaterialPath = ContentPath / FPlaneToPBRGeneratedAssetNames::GetMaterialAssetName();
	OutMaterial = Material;
	return true;
}
