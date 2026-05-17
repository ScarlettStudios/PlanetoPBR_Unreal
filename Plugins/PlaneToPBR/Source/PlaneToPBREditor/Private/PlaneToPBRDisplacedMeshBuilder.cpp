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
	if (!CreateDisplacedPlaneObj(*DepthTexturePath, ObjPath, DepthWidth, DepthHeight, SubdivisionsY, OutErrorMessage))
	{
		return false;
	}

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

	World->MarkPackageDirty();
	GEditor->SelectNone(false, true);
	GEditor->SelectActor(PlaneActor, true, true);
	GEditor->EditorUpdateComponents();
	GEditor->RedrawLevelEditingViewports();

	OutActorLabel = PlaneActor->GetActorLabel();
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
	TArray<uint8> CompressedDepthData;
	if (!FFileHelper::LoadFileToArray(CompressedDepthData, *DepthTexturePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to read generated depth PNG: %s"), *DepthTexturePath);
		return false;
	}

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

	FPlaneToPBRDisplacedPlaneGeometry Geometry;
	if (!FPlaneToPBRDisplacedPlaneGeometryBuilder::Build(DepthImage, FPlaneToPBRDisplacedPlaneSettings(), Geometry))
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
