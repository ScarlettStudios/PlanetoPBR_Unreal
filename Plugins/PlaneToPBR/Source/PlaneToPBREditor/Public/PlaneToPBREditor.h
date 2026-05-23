#pragma once

#include "Modules/ModuleManager.h"

class SEditableTextBox;
class SButton;
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
	TSharedRef<SDockTab> SpawnPlaneToPBRTab(const class FSpawnTabArgs& SpawnTabArgs);

	FPlaneToPBRWorkflowState WorkflowState;
	bool bGenerationInProgress = false;
	TSharedPtr<SEditableTextBox> ImagePathTextBox;
	TSharedPtr<SButton> GenerateButton;
	TSharedPtr<STextBlock> StatusTextBlock;
};
