#include "PlaneToPBRMCP.h"
#include "PlaneToPBRToolset.h"

#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UObjectGlobals.h"

void FPlaneToPBRMCPModule::StartupModule()
{
	FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("ModelContextProtocolEditor"));
	RegistrationTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
	{
		if (!UToolsetRegistry::IsAvailable())
		{
			return true;
		}
		UClass* ToolsetClass = FindObject<UClass>(nullptr, TEXT("/Script/PlaneToPBRMCP.PlaneToPBRToolset"));
		if (ToolsetClass && ToolsetClass->IsChildOf(UToolsetDefinition::StaticClass()))
		{
			// The schema generator needs an import-text representation for an empty string default.
			for (const TCHAR* Name : { TEXT("GeneratePBRPlaneFromImage"), TEXT("GeneratePBRTextures") })
			{
				ToolsetClass->FindFunctionByName(Name)->SetMetaData(TEXT("CPP_Default_Prompt"), TEXT("\"\""));
			}
			UToolsetRegistry::RegisterToolsetClass(ToolsetClass);
		}
		return false;
	}));
}

void FPlaneToPBRMCPModule::ShutdownModule()
{
	FTSTicker::GetCoreTicker().RemoveTicker(RegistrationTicker);
	UPlaneToPBRToolset::CancelPendingOperations();
	if (UToolsetRegistry::IsAvailable())
	{
		if (UClass* ToolsetClass = FindObject<UClass>(nullptr, TEXT("/Script/PlaneToPBRMCP.PlaneToPBRToolset")))
		{
			UToolsetRegistry::UnregisterToolsetClass(ToolsetClass);
		}
	}
}

IMPLEMENT_MODULE(FPlaneToPBRMCPModule, PlaneToPBRMCP)