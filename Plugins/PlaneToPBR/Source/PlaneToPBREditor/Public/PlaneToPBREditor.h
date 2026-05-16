#pragma once

#include "Modules/ModuleManager.h"

class SEditableTextBox;
class SDockTab;
class STextBlock;
class UMaterialInterface;

class FPlaneToPBREditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	struct FPlaneToPBRWorkflowState
	{
		FString HFPrompt;
		FString ImagePath;
	};

	void RegisterMenus();
	void OpenPlaneToPBRTab();
	FReply BrowseForImage();
	FReply GeneratePBRPlane();
	void OnHFPromptChanged(const FText& NewText);
	void OnImagePathChanged(const FText& NewText);
	bool ImportDownloadedTextures(
		const TMap<FString, FString>& TexturePaths,
		const FString& SourceImagePath,
		FString& OutContentPath,
		TMap<FString, FString>& OutTextureAssetPaths,
		FString& OutErrorMessage) const;
	bool CreateGeneratedMaterial(
		const FString& ContentPath,
		const TMap<FString, FString>& TextureAssetPaths,
		FString& OutMaterialPath,
		UMaterialInterface*& OutMaterial,
		FString& OutErrorMessage) const;
	bool CreateGeneratedDisplacedPlaneActor(
		const FString& ContentPath,
		UMaterialInterface* Material,
		const TMap<FString, FString>& TexturePaths,
		FString& OutActorLabel,
		FString& OutErrorMessage) const;
	bool CreateDisplacedPlaneObj(
		const FString& DepthTexturePath,
		const FString& ObjPath,
		int32& OutDepthWidth,
		int32& OutDepthHeight,
		int32& OutSubdivisionsY,
		FString& OutErrorMessage) const;
	TSharedRef<SDockTab> SpawnPlaneToPBRTab(const class FSpawnTabArgs& SpawnTabArgs);

	FPlaneToPBRWorkflowState WorkflowState;
	TSharedPtr<SEditableTextBox> ImagePathTextBox;
	TSharedPtr<STextBlock> StatusTextBlock;
};
