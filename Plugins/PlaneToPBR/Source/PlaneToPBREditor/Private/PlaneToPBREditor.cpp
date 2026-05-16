#include "PlaneToPBREditor.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Containers/Ticker.h"
#include "DesktopPlatformModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "PlaneToPBRHuggingFaceClient.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
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
				[this, WeakStatusTextBlock, TexturePaths = Result.TexturePaths, SourceImagePath = WorkflowState.ImagePath.TrimStartAndEnd()](float DeltaTime)
				{
					if (const TSharedPtr<STextBlock> DeferredStatusTextBlock = WeakStatusTextBlock.Pin())
					{
						FString ContentPath;
						TMap<FString, FString> TextureAssetPaths;
						FString ImportErrorMessage;
						if (!ImportDownloadedTextures(TexturePaths, SourceImagePath, ContentPath, TextureAssetPaths, ImportErrorMessage))
						{
							DeferredStatusTextBlock->SetText(FText::FromString(ImportErrorMessage));
							return false;
						}

						FString MaterialPath;
						FString MaterialErrorMessage;
						if (!CreateGeneratedMaterial(ContentPath, TextureAssetPaths, MaterialPath, MaterialErrorMessage))
						{
							DeferredStatusTextBlock->SetText(FText::FromString(MaterialErrorMessage));
							return false;
						}

						DeferredStatusTextBlock->SetText(FText::FromString(FString::Printf(TEXT("Created PlaneToPBR material: %s"), *MaterialPath)));
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
	const FString& SourceImagePath,
	FString& OutContentPath,
	TMap<FString, FString>& OutTextureAssetPaths,
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

	if (!FPaths::FileExists(SourceImagePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Selected source image does not exist: %s"), *SourceImagePath);
		return false;
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

	UAssetImportTask* BaseColorImportTask = NewObject<UAssetImportTask>();
	BaseColorImportTask->Filename = SourceImagePath;
	BaseColorImportTask->DestinationPath = OutContentPath;
	BaseColorImportTask->DestinationName = TEXT("T_BaseColor");
	BaseColorImportTask->bAutomated = true;
	BaseColorImportTask->bReplaceExisting = true;
	BaseColorImportTask->bSave = true;
	ImportTasks.Add(BaseColorImportTask);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().ImportAssetTasks(ImportTasks);

	for (UAssetImportTask* ImportTask : ImportTasks)
	{
		if (!ImportTask || ImportTask->ImportedObjectPaths.Num() == 0)
		{
			OutErrorMessage = FString::Printf(TEXT("Failed to import texture: %s"), ImportTask ? *ImportTask->Filename : TEXT("unknown"));
			return false;
		}

		const FString AssetName = FPaths::GetBaseFilename(ImportTask->ImportedObjectPaths[0]);
		const FString TextureKey = AssetName == TEXT("T_BaseColor")
			? TEXT("basecolor")
			: AssetName.RightChop(2).ToLower();
		OutTextureAssetPaths.Add(TextureKey, ImportTask->ImportedObjectPaths[0]);
	}

	return true;
}

bool FPlaneToPBREditorModule::CreateGeneratedMaterial(
	const FString& ContentPath,
	const TMap<FString, FString>& TextureAssetPaths,
	FString& OutMaterialPath,
	FString& OutErrorMessage) const
{
	const FString* NormalAssetPath = TextureAssetPaths.Find(TEXT("normal"));
	const FString* RoughnessAssetPath = TextureAssetPaths.Find(TEXT("roughness"));
	const FString* MaskAssetPath = TextureAssetPaths.Find(TEXT("mask"));
	const FString* BaseColorAssetPath = TextureAssetPaths.Find(TEXT("basecolor"));

	if (!NormalAssetPath || !RoughnessAssetPath || !MaskAssetPath || !BaseColorAssetPath)
	{
		OutErrorMessage = TEXT("Missing imported texture assets required for material creation.");
		return false;
	}

	UTexture* BaseColorTexture = LoadObject<UTexture>(nullptr, **BaseColorAssetPath);
	UTexture* NormalTexture = LoadObject<UTexture>(nullptr, **NormalAssetPath);
	UTexture* RoughnessTexture = LoadObject<UTexture>(nullptr, **RoughnessAssetPath);
	UTexture* MaskTexture = LoadObject<UTexture>(nullptr, **MaskAssetPath);

	if (!BaseColorTexture || !NormalTexture || !RoughnessTexture || !MaskTexture)
	{
		OutErrorMessage = TEXT("Failed to load one or more imported texture assets for material creation.");
		return false;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(TEXT("M_PlaneToPBR"), ContentPath, UMaterial::StaticClass(), MaterialFactory);
	UMaterial* Material = Cast<UMaterial>(CreatedAsset);
	if (!Material)
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to create material in: %s"), *ContentPath);
		return false;
	}

	UMaterialExpressionTextureSample* BaseColorExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	BaseColorExpression->Texture = BaseColorTexture;
	BaseColorExpression->SamplerType = SAMPLERTYPE_Color;
	BaseColorExpression->MaterialExpressionEditorX = -600;
	BaseColorExpression->MaterialExpressionEditorY = -240;
	Material->GetExpressionCollection().AddExpression(BaseColorExpression);
	Material->GetEditorOnlyData()->BaseColor.Expression = BaseColorExpression;

	UMaterialExpressionTextureSample* NormalExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	NormalExpression->Texture = NormalTexture;
	NormalExpression->SamplerType = SAMPLERTYPE_Normal;
	NormalExpression->MaterialExpressionEditorX = -600;
	NormalExpression->MaterialExpressionEditorY = 120;
	Material->GetExpressionCollection().AddExpression(NormalExpression);
	Material->GetEditorOnlyData()->Normal.Expression = NormalExpression;

	UMaterialExpressionTextureSample* RoughnessExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	RoughnessExpression->Texture = RoughnessTexture;
	RoughnessExpression->SamplerType = SAMPLERTYPE_LinearColor;
	RoughnessExpression->MaterialExpressionEditorX = -600;
	RoughnessExpression->MaterialExpressionEditorY = -20;
	Material->GetExpressionCollection().AddExpression(RoughnessExpression);

	UMaterialExpressionTextureSample* MaskExpression = NewObject<UMaterialExpressionTextureSample>(Material);
	MaskExpression->Texture = MaskTexture;
	MaskExpression->SamplerType = SAMPLERTYPE_LinearColor;
	MaskExpression->MaterialExpressionEditorX = -600;
	MaskExpression->MaterialExpressionEditorY = 300;
	Material->GetExpressionCollection().AddExpression(MaskExpression);

	UMaterialExpressionLinearInterpolate* RoughnessMaskBlend = NewObject<UMaterialExpressionLinearInterpolate>(Material);
	RoughnessMaskBlend->ConstB = 0.082f;
	RoughnessMaskBlend->MaterialExpressionEditorX = -250;
	RoughnessMaskBlend->MaterialExpressionEditorY = 120;
	RoughnessMaskBlend->A.Connect(0, RoughnessExpression);
	RoughnessMaskBlend->Alpha.Connect(0, MaskExpression);
	Material->GetExpressionCollection().AddExpression(RoughnessMaskBlend);
	Material->GetEditorOnlyData()->Roughness.Expression = RoughnessMaskBlend;

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	UPackage* MaterialPackage = Material->GetOutermost();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(MaterialPackage->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(MaterialPackage, Material, *PackageFileName, SaveArgs))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to save material package: %s"), *PackageFileName);
		return false;
	}

	OutMaterialPath = ContentPath / TEXT("M_PlaneToPBR");
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
