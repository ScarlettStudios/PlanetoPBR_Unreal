#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlaneToPBRDisplacedPlaneGeometry.h"
#include "PlaneToPBRTextureRoles.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRModuleLoadTest,
	"PlaneToPBR.Modules.Load",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRModuleLoadTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("PlaneToPBR runtime module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("PlaneToPBR")));
	TestTrue(TEXT("PlaneToPBR editor module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("PlaneToPBREditor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRTextureRoleMappingTest,
	"PlaneToPBR.TextureRoles.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRTextureRoleMappingTest::RunTest(const FString& Parameters)
{
	const FPlaneToPBRTextureRoleInfo& BaseColorInfo = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::BaseColor);
	TestEqual(TEXT("Base color key"), FString(BaseColorInfo.Key), FString(TEXT("basecolor")));
	TestTrue(TEXT("Base color uses sRGB"), BaseColorInfo.bSRGB);
	TestEqual(TEXT("Base color compression"), BaseColorInfo.CompressionSettings, TC_Default);
	TestEqual(TEXT("Base color sampler"), BaseColorInfo.SamplerType, SAMPLERTYPE_Color);

	const FPlaneToPBRTextureRoleInfo& NormalInfo = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::Normal);
	TestEqual(TEXT("Normal key"), FString(NormalInfo.Key), FString(TEXT("normal")));
	TestFalse(TEXT("Normal disables sRGB"), NormalInfo.bSRGB);
	TestEqual(TEXT("Normal compression"), NormalInfo.CompressionSettings, TC_Normalmap);
	TestEqual(TEXT("Normal sampler"), NormalInfo.SamplerType, SAMPLERTYPE_Normal);

	const FPlaneToPBRTextureRoleInfo& DepthInfo = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::Depth);
	TestEqual(TEXT("Depth key"), FString(DepthInfo.Key), FString(TEXT("depth")));
	TestFalse(TEXT("Depth disables sRGB"), DepthInfo.bSRGB);
	TestEqual(TEXT("Depth compression"), DepthInfo.CompressionSettings, TC_Grayscale);
	TestEqual(TEXT("Depth sampler"), DepthInfo.SamplerType, SAMPLERTYPE_LinearGrayscale);

	const FPlaneToPBRTextureRoleInfo& RoughnessInfo = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::Roughness);
	TestEqual(TEXT("Roughness key"), FString(RoughnessInfo.Key), FString(TEXT("roughness")));
	TestFalse(TEXT("Roughness disables sRGB"), RoughnessInfo.bSRGB);
	TestEqual(TEXT("Roughness compression"), RoughnessInfo.CompressionSettings, TC_Grayscale);
	TestEqual(TEXT("Roughness sampler is LinearGrayscale"), RoughnessInfo.SamplerType, SAMPLERTYPE_LinearGrayscale);

	const FPlaneToPBRTextureRoleInfo& MaskInfo = FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::Mask);
	TestEqual(TEXT("Mask key"), FString(MaskInfo.Key), FString(TEXT("mask")));
	TestFalse(TEXT("Mask disables sRGB"), MaskInfo.bSRGB);
	TestEqual(TEXT("Mask compression"), MaskInfo.CompressionSettings, TC_Grayscale);
	TestEqual(TEXT("Mask sampler is LinearGrayscale"), MaskInfo.SamplerType, SAMPLERTYPE_LinearGrayscale);

	TestEqual(TEXT("Role resolves from base color asset name"), FPlaneToPBRTextureRoles::FromImportedAssetName(TEXT("T_BaseColor")), EPlaneToPBRTextureRole::BaseColor);
	TestEqual(TEXT("Role resolves from roughness key"), FPlaneToPBRTextureRoles::FromKey(TEXT("roughness")), EPlaneToPBRTextureRole::Roughness);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRDisplacedPlaneGeometryTest,
	"PlaneToPBR.Geometry.DisplacedPlaneCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRDisplacedPlaneGeometryTest::RunTest(const FString& Parameters)
{
	FPlaneToPBRDepthImage DepthImage;
	DepthImage.Width = 2;
	DepthImage.Height = 2;
	DepthImage.Pixels = { 0, 64, 128, 255 };

	TestTrue(TEXT("Depth image is valid"), DepthImage.IsValid());
	TestEqual(TEXT("Depth sample uses inverted V coordinate"), DepthImage.SampleHeight(0.0f, 0.0f), 128.0f / 255.0f);

	FPlaneToPBRDisplacedPlaneSettings Settings;
	Settings.SubdivisionsX = 2;
	Settings.PlaneWidthCm = 200.0f;
	Settings.DisplacementStrengthCm = 25.0f;
	Settings.HeightCenter = 0.5f;

	FPlaneToPBRDisplacedPlaneGeometry Geometry;
	TestTrue(TEXT("Geometry builds from known depth image"), FPlaneToPBRDisplacedPlaneGeometryBuilder::Build(DepthImage, Settings, Geometry));
	TestEqual(TEXT("Square image keeps matching Y subdivisions"), Geometry.SubdivisionsY, 2);
	TestEqual(TEXT("Vertex count"), Geometry.Vertices.Num(), 9);
	TestEqual(TEXT("UV count"), Geometry.UVs.Num(), 9);
	TestEqual(TEXT("Face count"), Geometry.Faces.Num(), 8);

	const FString ObjContents = FPlaneToPBRDisplacedPlaneGeometryBuilder::WriteObjString(Geometry);
	TestTrue(TEXT("OBJ contains generated object name"), ObjContents.Contains(TEXT("o PlaneToPBR_DisplacedPlane")));
	TestTrue(TEXT("OBJ contains vertex records"), ObjContents.Contains(TEXT("\nv ")));
	TestTrue(TEXT("OBJ contains face records"), ObjContents.Contains(TEXT("\nf ")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaneToPBRPackagingScriptTest,
	"PlaneToPBR.Packaging.Script",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRPackagingScriptTest::RunTest(const FString& Parameters)
{
	const FString ScriptPath = FPaths::ProjectDir() / TEXT("Build/PackagePlaneToPBRPlugin.ps1");
	TestTrue(TEXT("Package script exists"), FPaths::FileExists(ScriptPath));

	FString ScriptContents;
	if (!FFileHelper::LoadFileToString(ScriptContents, *ScriptPath))
	{
		AddError(FString::Printf(TEXT("Failed to read package script: %s"), *ScriptPath));
		return false;
	}

	TestTrue(TEXT("Package script invokes BuildPlugin"), ScriptContents.Contains(TEXT("BuildPlugin")));
	TestTrue(TEXT("Package script targets PlaneToPBR plugin descriptor"), ScriptContents.Contains(TEXT("Plugins/PlaneToPBR/PlaneToPBR.uplugin")));
	return true;
}

#endif
