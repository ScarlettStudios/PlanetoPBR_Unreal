#pragma once

#include "Modules/ModuleManager.h"

/**
 * Runtime module interface for the PlaneToPBR plugin.
 * Handles module lifecycle initialization and shutdown for core PlaneToPBR services.
 */
class FPlaneToPBRModule : public IModuleInterface
{
public:
	/** Called immediately after the module DLL has been loaded and the module object has been created. */
	virtual void StartupModule() override;

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override;
};
