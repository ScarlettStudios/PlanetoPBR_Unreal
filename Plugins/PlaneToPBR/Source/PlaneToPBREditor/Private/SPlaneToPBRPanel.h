#pragma once

#include "CoreMinimal.h"
#include "PlaneToPBRGenerationWorkflow.h"
#include "PlaneToPBRPanelState.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
class SEditableTextBox;
class STextBlock;

/**
 * Slate UI compound widget presenting the PlaneToPBR control panel.
 * Provides controls for selecting source images, inputting prompt guidance,
 * initiating asynchronous PBR plane generation, and tracking real-time status.
 */
class SPlaneToPBRPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlaneToPBRPanel) {}
	SLATE_END_ARGS()

	/** Constructs the Slate UI layout tree and binds delegates. */
	void Construct(const FArguments& InArgs);

private:
	/** Opens the native OS file picker to browse for an input image. */
	FReply BrowseForImage();

	/** Initiates the PlaneToPBR generation process for the current inputs. */
	FReply GeneratePBRPlane();

	/** Determines whether the Generate button is interactable. */
	bool CanGeneratePBRPlane() const;

	/** Handles user modification to the material mask prompt field. */
	void OnHFPromptChanged(const FText& NewText);

	/** Handles direct edits to the image file path text box. */
	void OnImagePathChanged(const FText& NewText);

	/** Updates the status label displayed at the bottom of the panel. */
	void SetStatusText(const FText& StatusText);

	/** Resets UI progress state upon completion of generation. */
	void FinishGeneration();

	/** Current input state and readiness validation. */
	FPlaneToPBRPanelState WorkflowState;

	/** Workflow executor instance. */
	FPlaneToPBRGenerationWorkflow GenerationWorkflow;

	/** Flag indicating active background generation. */
	bool bGenerationInProgress = false;

	/** Slate UI widget references. */
	TSharedPtr<SEditableTextBox> ImagePathTextBox;
	TSharedPtr<SButton> GenerateButton;
	TSharedPtr<STextBlock> StatusTextBlock;
};
