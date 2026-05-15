#pragma once

#include "Modules/ModuleManager.h"

class SEditableTextBox;
class SDockTab;

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
	void OnHFPromptChanged(const FText& NewText);
	void OnImagePathChanged(const FText& NewText);
	TSharedRef<SDockTab> SpawnPlaneToPBRTab(const class FSpawnTabArgs& SpawnTabArgs);

	FPlaneToPBRWorkflowState WorkflowState;
	TSharedPtr<SEditableTextBox> ImagePathTextBox;
};
