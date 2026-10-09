#include "PlaneToPBRDisplacedMeshBuilder.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlaneToPBRDisplacedPlaneGeometry.h"
#include "PlaneToPBRGeneratedAssetNames.h"
#include "PlaneToPBRTextureRoles.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

bool FPlaneToPBRDisplacedMeshBuilder::CreateGeneratedDisplacedPlaneActor(
	const FString& ContentPath,
	UMaterialInterface* Material,
	const TMap<FString, FString>& TexturePaths,
	FString& OutActorLabel,
	FString& OutErrorMessage)
{
	FString MeshPath;
	FString ActorName;
	return CreateGeneratedDisplacedPlaneActor(ContentPath, Material, TexturePaths,
		OutActorLabel, OutErrorMessage, FPlaneToPBRMeshOptions(), MeshPath, ActorName);
}

bool FPlaneToPBRDisplacedMeshBuilder::CreateGeneratedDisplacedPlaneActor(
	const FString& ContentPath,
	UMaterialInterface* Material,
	const TMap<FString, FString>& TexturePaths,
	FString& OutActorLabel,
	FString& OutErrorMessage,
	const FPlaneToPBRMeshOptions& Options,
	FString& OutMeshPath,
	FString& OutActorName)
{
	if (!IsInGameThread() || !FMath::IsFinite(Options.PlaneWidthCm) || Options.PlaneWidthCm <= 0.0f ||
		!FMath::IsFinite(Options.DisplacementStrengthCm) || Options.DisplacementStrengthCm < 0.0f ||
		Options.Subdivisions < 1 || Options.Subdivisions > 512)
	{
		OutErrorMessage = TEXT("Mesh creation requires the GameThread, positive finite width, subdivisions 1-512, and nonnegative finite displacement.");
		return false;
	}
	// Locate active level editing world context
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		OutErrorMessage = TEXT("Failed to find the current editor world for PlaneToPBR plane creation.");
		return false;
	}

	if (!Material)
	{
		OutErrorMessage = TEXT("Generated material is not available for PlaneToPBR plane creation.");
		return false;
	}

	// Verify availability of the generated depth map PNG
	const FString* DepthTexturePath = TexturePaths.Find(FPlaneToPBRTextureRoles::GetInfo(EPlaneToPBRTextureRole::Depth).Key);
	if (!DepthTexturePath || DepthTexturePath->IsEmpty() || !FPaths::FileExists(*DepthTexturePath))
	{
		OutErrorMessage = TEXT("Generated depth PNG is not available for displaced plane creation.");
		return false;
	}

	const FString MeshOutputDir = FPlaneToPBRGeneratedAssetNames::GetDisplacedMeshOutputDir();
	IFileManager::Get().MakeDirectory(*MeshOutputDir, true);

	// Generate intermediate OBJ file to leverage Unreal's existing mesh import pipeline
	const FString ObjPath = MeshOutputDir / FPlaneToPBRGeneratedAssetNames::GetDisplacedMeshObjName();
	int32 DepthWidth = 0;
	int32 DepthHeight = 0;
	int32 SubdivisionsY = 0;
	if (!CreateDisplacedPlaneObj(*DepthTexturePath, ObjPath, DepthWidth, DepthHeight, SubdivisionsY, OutErrorMessage, Options))
	{
		return false;
	}

	// Setup automated asset import task for the generated OBJ
	UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
	ImportTask->Filename = ObjPath;
	ImportTask->DestinationPath = ContentPath;
	ImportTask->DestinationName = FPlaneToPBRGeneratedAssetNames::GetDisplacedMeshAssetName();
	ImportTask->bAutomated = true;
	ImportTask->bReplaceExisting = true;
	ImportTask->bSave = true;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().ImportAssetTasks({ ImportTask });

	if (!ImportTask || ImportTask->ImportedObjectPaths.Num() == 0)
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to import displaced plane mesh: %s"), *ObjPath);
		return false;
	}

	UStaticMesh* DisplacedPlaneMesh = LoadObject<UStaticMesh>(nullptr, *ImportTask->ImportedObjectPaths[0]);
	if (!DisplacedPlaneMesh)
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to load imported displaced plane mesh: %s"), *ImportTask->ImportedObjectPaths[0]);
		return false;
	}

	// Assign the generated PBR material to material slot 0
	DisplacedPlaneMesh->Modify();
	TArray<FStaticMaterial>& StaticMaterials = DisplacedPlaneMesh->GetStaticMaterials();
	if (StaticMaterials.IsEmpty())
	{
		StaticMaterials.Add(FStaticMaterial(Material));
	}
	else
	{
		StaticMaterials[0].MaterialInterface = Material;
	}
	DisplacedPlaneMesh->PostEditChange();
	DisplacedPlaneMesh->MarkPackageDirty();

	// Explicit save required to persist mesh material assignment to disk
	UPackage* MeshPackage = DisplacedPlaneMesh->GetOutermost();
	const FString MeshPackageFileName = FPackageName::LongPackageNameToFilename(MeshPackage->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs MeshSaveArgs;
	MeshSaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(MeshPackage, DisplacedPlaneMesh, *MeshPackageFileName, MeshSaveArgs))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to save displaced plane mesh package: %s"), *MeshPackageFileName);
		return false;
	}

	// Spawn actor in editor level with undo transaction support
	const FScopedTransaction Transaction(NSLOCTEXT("FPlaneToPBRDisplacedMeshBuilder", "CreateGeneratedPlaneActorTransaction", "Create PlaneToPBR Generated Plane"));
	World->Modify();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(World, AStaticMeshActor::StaticClass(), TEXT("PlaneToPBR_GeneratedPlane"));
	AStaticMeshActor* PlaneActor = World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);

	if (!PlaneActor || !PlaneActor->GetStaticMeshComponent())
	{
		OutErrorMessage = TEXT("Failed to create the PlaneToPBR plane actor.");
		return false;
	}

	PlaneActor->Modify();
	PlaneActor->SetActorLabel(TEXT("PlaneToPBR_DisplacedPlane"));
	PlaneActor->SetActorScale3D(FVector::OneVector);

	UStaticMeshComponent* StaticMeshComponent = PlaneActor->GetStaticMeshComponent();
	StaticMeshComponent->Modify();
	StaticMeshComponent->SetStaticMesh(DisplacedPlaneMesh);
	StaticMeshComponent->OverrideMaterials.Empty();
	StaticMeshComponent->SetMaterial(0, Material);
	StaticMeshComponent->PostEditChange();
	StaticMeshComponent->MarkRenderStateDirty();
	PlaneActor->PostEditChange();

	// Select the newly created actor in the active viewport
	World->MarkPackageDirty();
	GEditor->SelectNone(false, true);
	GEditor->SelectActor(PlaneActor, true, true);
	GEditor->EditorUpdateComponents();
	GEditor->RedrawLevelEditingViewports();

	OutActorLabel = PlaneActor->GetActorLabel();
	OutActorName = PlaneActor->GetName();
	OutMeshPath = DisplacedPlaneMesh->GetPathName();
	return true;
}

bool FPlaneToPBRDisplacedMeshBuilder::CreateDisplacedPlaneObj(
	const FString& DepthTexturePath,
	const FString& ObjPath,
	int32& OutDepthWidth,
	int32& OutDepthHeight,
	int32& OutSubdivisionsY,
	FString& OutErrorMessage)
{
	return CreateDisplacedPlaneObj(DepthTexturePath, ObjPath, OutDepthWidth, OutDepthHeight,
		OutSubdivisionsY, OutErrorMessage, FPlaneToPBRMeshOptions());
}

bool FPlaneToPBRDisplacedMeshBuilder::CreateDisplacedPlaneObj(
	const FString& DepthTexturePath,
	const FString& ObjPath,
	int32& OutDepthWidth,
	int32& OutDepthHeight,
	int32& OutSubdivisionsY,
	FString& OutErrorMessage,
	const FPlaneToPBRMeshOptions& Options)
{
	// Load compressed image file from disk
	TArray<uint8> CompressedDepthData;
	if (!FFileHelper::LoadFileToArray(CompressedDepthData, *DepthTexturePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to read generated depth PNG: %s"), *DepthTexturePath);
		return false;
	}

	// Detect format and decode image wrapper
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(CompressedDepthData.GetData(), CompressedDepthData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		OutErrorMessage = FString::Printf(TEXT("Unsupported generated depth image format: %s"), *DepthTexturePath);
		return false;
	}

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedDepthData.GetData(), CompressedDepthData.Num()))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to decode generated depth image: %s"), *DepthTexturePath);
		return false;
	}

	// Decompress 8-bit grayscale pixels
	const int64 PixelCount = static_cast<int64>(ImageWrapper->GetWidth()) * ImageWrapper->GetHeight();
	const double VerticalSubdivisions = static_cast<double>(Options.Subdivisions) * ImageWrapper->GetHeight() / FMath::Max(1, ImageWrapper->GetWidth());
	if (PixelCount <= 0 || PixelCount > 67108864 || VerticalSubdivisions > 2048.0 ||
		(static_cast<double>(Options.Subdivisions) + 1.0) * (VerticalSubdivisions + 2.0) > 1048576.0)
	{
		OutErrorMessage = TEXT("Depth image dimensions or requested mesh exceed safe generation limits.");
		return false;
	}
	FPlaneToPBRDepthImage DepthImage;
	if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 8, DepthImage.Pixels))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to extract generated depth pixels: %s"), *DepthTexturePath);
		return false;
	}

	DepthImage.Width = ImageWrapper->GetWidth();
	DepthImage.Height = ImageWrapper->GetHeight();
	OutDepthWidth = DepthImage.Width;
	OutDepthHeight = DepthImage.Height;
	if (OutDepthWidth <= 0 || OutDepthHeight <= 0)
	{
		OutErrorMessage = FString::Printf(TEXT("Generated depth image has invalid dimensions: %s"), *DepthTexturePath);
		return false;
	}

	// Build procedural vertex and face arrays
	FPlaneToPBRDisplacedPlaneGeometry Geometry;
	FPlaneToPBRDisplacedPlaneSettings Settings;
	Settings.PlaneWidthCm = Options.PlaneWidthCm;
	Settings.SubdivisionsX = Options.Subdivisions;
	Settings.DisplacementStrengthCm = Options.DisplacementStrengthCm;
	if (!FPlaneToPBRDisplacedPlaneGeometryBuilder::Build(DepthImage, Settings, Geometry))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to generate displaced plane geometry from depth image: %s"), *DepthTexturePath);
		return false;
	}

	OutSubdivisionsY = Geometry.SubdivisionsY;
	const FString ObjContents = FPlaneToPBRDisplacedPlaneGeometryBuilder::WriteObjString(Geometry);
	if (!FFileHelper::SaveStringToFile(ObjContents, *ObjPath))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to write displaced plane OBJ: %s"), *ObjPath);
		return false;
	}

	return true;
}
