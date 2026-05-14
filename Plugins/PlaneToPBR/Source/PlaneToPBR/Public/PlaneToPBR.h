#pragma once

#include "Modules/ModuleManager.h"

class FPlaneToPBRModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
