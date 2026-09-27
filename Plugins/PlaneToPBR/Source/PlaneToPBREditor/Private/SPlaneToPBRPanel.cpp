#include "SPlaneToPBRPanel.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPlaneToPBRPanel"

void SPlaneToPBRPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.Padding(12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(72.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ImageLabel", "Image:"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(ImagePathTextBox, SEditableTextBox)
					.Text(FText::FromString(WorkflowState.ImagePath))
					.OnTextChanged(this, &SPlaneToPBRPanel::OnImagePathChanged)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseImageButton", "..."))
					.OnClicked(this, &SPlaneToPBRPanel::BrowseForImage)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(72.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MaterialMaskPromptLabel", "Material Mask Prompt"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SEditableTextBox)
					.Text(FText::FromString(WorkflowState.HFPrompt))
					.OnTextChanged(this, &SPlaneToPBRPanel::OnHFPromptChanged)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(440.0f)
				[
					SAssignNew(GenerateButton, SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("GeneratePBRPlaneButton", "Generate PBR Plane"))
					.IsEnabled_Lambda([this]()
					{
						return CanGeneratePBRPlane();
					})
					.OnClicked(this, &SPlaneToPBRPanel::GeneratePBRPlane)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SAssignNew(StatusTextBlock, STextBlock)
				.Text(LOCTEXT("InitialStatusText", "Select a source image to enable generation."))
			]
		]
	];
}

FReply SPlaneToPBRPanel::BrowseForImage()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	TArray<FString> SelectedFilePaths;
	const bool bFileSelected = DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Select PlaneToPBR Source Image"),
		FString(),
		FString(),
		TEXT("Image Files (*.png;*.jpg;*.jpeg;*.exr)|*.png;*.jpg;*.jpeg;*.exr|All Files (*.*)|*.*"),
		EFileDialogFlags::None,
		SelectedFilePaths);

	if (bFileSelected && SelectedFilePaths.Num() > 0 && ImagePathTextBox.IsValid())
	{
		WorkflowState.ImagePath = SelectedFilePaths[0];
		ImagePathTextBox->SetText(FText::FromString(WorkflowState.ImagePath));
	}

	return FReply::Handled();
}

FReply SPlaneToPBRPanel::GeneratePBRPlane()
{
	if (bGenerationInProgress)
	{
		SetStatusText(LOCTEXT("GenerationAlreadyInProgressStatus", "PlaneToPBR generation is already running..."));
		return FReply::Handled();
	}

	FPlaneToPBRHuggingFaceRequest Request;
	Request.ImagePath = WorkflowState.ImagePath.TrimStartAndEnd();
	Request.HFPrompt = WorkflowState.HFPrompt.TrimStartAndEnd();
	if (Request.ImagePath.IsEmpty())
	{
		SetStatusText(LOCTEXT("MissingImagePathStatus", "Select a source image before generating a PBR plane."));
		return FReply::Handled();
	}

	bGenerationInProgress = true;
	const TWeakPtr<SPlaneToPBRPanel> WeakPanel = SharedThis(this);
	GenerationWorkflow.GeneratePBRPlane(
		Request,
		[WeakPanel](const FText& StatusText)
		{
			if (const TSharedPtr<SPlaneToPBRPanel> PinnedPanel = WeakPanel.Pin())
			{
				PinnedPanel->SetStatusText(StatusText);
			}
		},
		[WeakPanel]()
		{
			if (const TSharedPtr<SPlaneToPBRPanel> PinnedPanel = WeakPanel.Pin())
			{
				PinnedPanel->FinishGeneration();
			}
		});

	return FReply::Handled();
}

bool SPlaneToPBRPanel::CanGeneratePBRPlane() const
{
	return WorkflowState.CanGeneratePBRPlane(bGenerationInProgress);
}

void SPlaneToPBRPanel::OnHFPromptChanged(const FText& NewText)
{
	WorkflowState.HFPrompt = NewText.ToString();
}

void SPlaneToPBRPanel::OnImagePathChanged(const FText& NewText)
{
	WorkflowState.ImagePath = NewText.ToString();
}

void SPlaneToPBRPanel::SetStatusText(const FText& StatusText)
{
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(StatusText);
	}
}

void SPlaneToPBRPanel::FinishGeneration()
{
	bGenerationInProgress = false;
}

#undef LOCTEXT_NAMESPACE
