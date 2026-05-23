#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;

class FPlaneToPBREditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OpenPlaneToPBRTab();
	TSharedRef<SDockTab> SpawnPlaneToPBRTab(const class FSpawnTabArgs& SpawnTabArgs);
};
