#pragma once

#include "Modules/ModuleManager.h"

class SEditableTextBox;
class SDockTab;
class STextBlock;

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
		FString& OutContentPath,
		TMap<FString, FString>& OutTextureAssetPaths,
		FString& OutErrorMessage) const;
	bool CreateGeneratedMaterial(
		const FString& ContentPath,
		const TMap<FString, FString>& TextureAssetPaths,
		FString& OutMaterialPath,
		FString& OutErrorMessage) const;
	TSharedRef<SDockTab> SpawnPlaneToPBRTab(const class FSpawnTabArgs& SpawnTabArgs);

	FPlaneToPBRWorkflowState WorkflowState;
	TSharedPtr<SEditableTextBox> ImagePathTextBox;
	TSharedPtr<STextBlock> StatusTextBlock;
};
