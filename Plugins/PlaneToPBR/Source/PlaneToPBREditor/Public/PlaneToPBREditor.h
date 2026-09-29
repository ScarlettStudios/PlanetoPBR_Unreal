#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;

/**
 * Editor module interface for PlaneToPBR.
 * Registers editor UI extensions, menu entries under LevelEditor.MainMenu.Tools, and tab spawners.
 */
class FPlaneToPBREditorModule : public IModuleInterface
{
public:
	/** Registers the Nomad tab spawner and menu integration during editor startup. */
	virtual void StartupModule() override;

	/** Unregisters tab spawners and menu bindings when the editor module shuts down. */
	virtual void ShutdownModule() override;

private:
	/** Registers the PlaneToPBR menu entry in the Tools menu. */
	void RegisterMenus();

	/** Invokes the PlaneToPBR Nomad tab in the active editor. */
	void OpenPlaneToPBRTab();

	/** Spawns the Slate widget tab hosting the PlaneToPBR control panel. */
	TSharedRef<SDockTab> SpawnPlaneToPBRTab(const class FSpawnTabArgs& SpawnTabArgs);
};
