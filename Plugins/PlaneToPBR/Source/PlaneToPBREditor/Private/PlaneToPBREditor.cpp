#include "PlaneToPBREditor.h"

#include "SPlaneToPBRPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FPlaneToPBREditorModule"

namespace PlaneToPBREditor
{
	/** Unique tab identifier for the PlaneToPBR editor panel. */
	static const FName TabName(TEXT("PlaneToPBR"));
}

void FPlaneToPBREditorModule::StartupModule()
{
	// Register the Nomad tab spawner so Unreal can dock/open the PlaneToPBR panel anywhere in the Editor layout
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PlaneToPBREditor::TabName,
		FOnSpawnTab::CreateRaw(this, &FPlaneToPBREditorModule::SpawnPlaneToPBRTab))
		.SetDisplayName(LOCTEXT("PlaneToPBRTabTitle", "PlaneToPBR"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Register startup callback to extend the editor Tools menu
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPlaneToPBREditorModule::RegisterMenus));
}

void FPlaneToPBREditorModule::ShutdownModule()
{
	// Clean up menu bindings and owners
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	// Unregister tab spawner from the global tab manager
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlaneToPBREditor::TabName);
}

void FPlaneToPBREditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Extend the main Level Editor "Tools" menu
	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("PlaneToPBR"));

	// Add menu item to launch PlaneToPBR editor tab
	Section.AddMenuEntry(
		TEXT("OpenPlaneToPBR"),
		LOCTEXT("OpenPlaneToPBRLabel", "PlaneToPBR"),
		LOCTEXT("OpenPlaneToPBRTooltip", "Open the PlaneToPBR editor window.\n\n1. Select the flat cropped image you want to convert into a textured plane. PNG or JPG recommended.\n2. Add an optional short material mask prompt only if you want an AI-assisted material mask, i.e. \"windows\"."),
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
			SNew(SPlaneToPBRPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPlaneToPBREditorModule, PlaneToPBREditor)
