#include "PlaneToPBREditor.h"

#include "ToolMenus.h"
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

TSharedRef<SDockTab> FPlaneToPBREditorModule::SpawnPlaneToPBRTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBorder)
			.Padding(24.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PlaneToPBRHeading", "PlaneToPBR"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PlaneToPBRPlaceholder", "Texture generation and Unreal asset creation controls will be added here."))
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPlaneToPBREditorModule, PlaneToPBREditor)
