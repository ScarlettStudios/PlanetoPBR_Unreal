#pragma once

#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

class PLANETOPBRMCP_API FPlaneToPBRMCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override { return false; }

private:
	FTSTicker::FDelegateHandle RegistrationTicker;
};