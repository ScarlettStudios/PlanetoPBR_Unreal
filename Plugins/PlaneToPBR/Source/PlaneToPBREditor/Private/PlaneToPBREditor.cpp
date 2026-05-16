#include "PlaneToPBREditor.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Containers/Ticker.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "PlaneToPBRHuggingFaceClient.h"
#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FPlaneToPBREditorModule"

namespace PlaneToPBREditor
{
	static const FName TabName(TEXT("PlaneToPBR"));
}

void FPlaneToPBREditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PlaneToPBREditor::TabName,
		FOnSpawnTab::CreateRaw(this, &FPlaneToPBREditorModule::SpawnPlaneToPBRTab))
		.SetDisplayName(LOCTEXT("PlaneToPBRTabTitle", "PlaneToPBR"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPlaneToPBREditorModule::RegisterMenus));
}

void FPlaneToPBREditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlaneToPBREditor::TabName);
}

void FPlaneToPBREditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("PlaneToPBR"));

	Section.AddMenuEntry(
		TEXT("OpenPlaneToPBR"),
		LOCTEXT("OpenPlaneToPBRLabel", "PlaneToPBR"),
		LOCTEXT("OpenPlaneToPBRTooltip", "Open the PlaneToPBR editor window."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FPlaneToPBREditorModule::OpenPlaneToPBRTab)));
}

void FPlaneToPBREditorModule::OpenPlaneToPBRTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PlaneToPBREditor::TabName);
}

FReply FPlaneToPBREditorModule::BrowseForImage()
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

FReply FPlaneToPBREditorModule::GeneratePBRPlane()
{
	FPlaneToPBRHuggingFaceRequest Request;
	Request.ImagePath = WorkflowState.ImagePath.TrimStartAndEnd();
	Request.HFPrompt = WorkflowState.HFPrompt.TrimStartAndEnd();

	FPlaneToPBRHuggingFaceClient Client;
	Client.GeneratePBRTexturesAsync(Request, [this, WeakStatusTextBlock = TWeakPtr<STextBlock>(StatusTextBlock)](const FPlaneToPBRHuggingFaceResult& Result)
	{
		if (const TSharedPtr<STextBlock> PinnedStatusTextBlock = WeakStatusTextBlock.Pin())
		{
			if (!Result.bSucceeded)
			{
				PinnedStatusTextBlock->SetText(FText::FromString(Result.Message));
				return;
			}

			PinnedStatusTextBlock->SetText(LOCTEXT("ImportingTexturesStatus", "Importing PlaneToPBR textures..."));

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[this, WeakStatusTextBlock, TexturePaths = Result.TexturePaths](float DeltaTime)
				{
					if (const TSharedPtr<STextBlock> DeferredStatusTextBlock = WeakStatusTextBlock.Pin())
					{
						FString ContentPath;
						FString ImportErrorMessage;
						if (!ImportDownloadedTextures(TexturePaths, ContentPath, ImportErrorMessage))
						{
							DeferredStatusTextBlock->SetText(FText::FromString(ImportErrorMessage));
							return false;
						}

						DeferredStatusTextBlock->SetText(FText::FromString(FString::Printf(TEXT("Imported PlaneToPBR textures to: %s"), *ContentPath)));
					}

					return false;
				}));
		}
	});

	return FReply::Handled();
}

void FPlaneToPBREditorModule::OnHFPromptChanged(const FText& NewText)
{
	WorkflowState.HFPrompt = NewText.ToString();
}

void FPlaneToPBREditorModule::OnImagePathChanged(const FText& NewText)
{
	WorkflowState.ImagePath = NewText.ToString();
}

bool FPlaneToPBREditorModule::ImportDownloadedTextures(
	const TMap<FString, FString>& TexturePaths,
	FString& OutContentPath,
	FString& OutErrorMessage) const
{
	static const TArray<FString> RequiredTextureKeys = { TEXT("depth"), TEXT("normal"), TEXT("roughness"), TEXT("mask") };

	for (const FString& TextureKey : RequiredTextureKeys)
	{
		const FString* TexturePath = TexturePaths.Find(TextureKey);
		if (!TexturePath || TexturePath->IsEmpty())
		{
			OutErrorMessage = FString::Printf(TEXT("Missing downloaded %s texture path."), *TextureKey);
			return false;
		}

		if (!FPaths::FileExists(*TexturePath))
		{
			OutErrorMessage = FString::Printf(TEXT("Downloaded %s texture does not exist: %s"), *TextureKey, **TexturePath);
			return false;
		}
	}

	const FString RunFolderName = TEXT("Run_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	OutContentPath = TEXT("/Game/PlaneToPBR/Generated/") + RunFolderName;

	TArray<UAssetImportTask*> ImportTasks;
	for (const FString& TextureKey : RequiredTextureKeys)
	{
		const FString& TexturePath = *TexturePaths.Find(TextureKey);

		UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
		ImportTask->Filename = TexturePath;
		ImportTask->DestinationPath = OutContentPath;
		ImportTask->DestinationName = TEXT("T_") + TextureKey.Left(1).ToUpper() + TextureKey.RightChop(1);
		ImportTask->bAutomated = true;
		ImportTask->bReplaceExisting = true;
		ImportTask->bSave = true;
		ImportTasks.Add(ImportTask);
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().ImportAssetTasks(ImportTasks);

	for (UAssetImportTask* ImportTask : ImportTasks)
	{
		if (!ImportTask || ImportTask->ImportedObjectPaths.Num() == 0)
		{
			OutErrorMessage = FString::Printf(TEXT("Failed to import texture: %s"), ImportTask ? *ImportTask->Filename : TEXT("unknown"));
			return false;
		}
	}

	return true;
}

TSharedRef<SDockTab> FPlaneToPBREditorModule::SpawnPlaneToPBRTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
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
							.Text(LOCTEXT("HFPromptLabel", "HF Prompt"))
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(WorkflowState.HFPrompt))
						.OnTextChanged_Raw(this, &FPlaneToPBREditorModule::OnHFPromptChanged)
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
							.Text(LOCTEXT("ImageLabel", "Image:"))
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(ImagePathTextBox, SEditableTextBox)
						.Text(FText::FromString(WorkflowState.ImagePath))
						.OnTextChanged_Raw(this, &FPlaneToPBREditorModule::OnImagePathChanged)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("BrowseImageButton", "..."))
						.OnClicked_Raw(this, &FPlaneToPBREditorModule::BrowseForImage)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(440.0f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("GeneratePBRPlaneButton", "Generate PBR Plane"))
						.OnClicked_Raw(this, &FPlaneToPBREditorModule::GeneratePBRPlane)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SAssignNew(StatusTextBlock, STextBlock)
					.Text(LOCTEXT("InitialStatusText", ""))
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPlaneToPBREditorModule, PlaneToPBREditor)
