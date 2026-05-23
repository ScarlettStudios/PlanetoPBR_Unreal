#pragma once

#include "CoreMinimal.h"
#include "PlaneToPBRGenerationWorkflow.h"
#include "PlaneToPBRPanelState.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
class SEditableTextBox;
class STextBlock;

class SPlaneToPBRPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlaneToPBRPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply BrowseForImage();
	FReply GeneratePBRPlane();
	bool CanGeneratePBRPlane() const;
	void OnHFPromptChanged(const FText& NewText);
	void OnImagePathChanged(const FText& NewText);
	void SetStatusText(const FText& StatusText);
	void FinishGeneration();

	FPlaneToPBRPanelState WorkflowState;
	FPlaneToPBRGenerationWorkflow GenerationWorkflow;
	bool bGenerationInProgress = false;
	TSharedPtr<SEditableTextBox> ImagePathTextBox;
	TSharedPtr<SButton> GenerateButton;
	TSharedPtr<STextBlock> StatusTextBlock;
};
