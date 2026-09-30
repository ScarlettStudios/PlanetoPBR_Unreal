#include "PlaneToPBRToolset.h"

#include "IModelContextProtocolModule.h"
#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlaneToPBRMCPTestHooks.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/StrongObjectPtr.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaneToPBRMCPModuleLoadTest, "PlaneToPBR.MCP.ModuleLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRMCPModuleLoadTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("MCP module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("PlaneToPBRMCP")));
	TestTrue(TEXT("Shared editor module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("PlaneToPBREditor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaneToPBRMCPToolsetRegistrationTest, "PlaneToPBR.MCP.ToolsetRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRMCPToolsetRegistrationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Toolset is registered"), UToolsetRegistry::IsToolsetClassRegistered(UPlaneToPBRToolset::StaticClass()));
	IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
	if (!TestNotNull(TEXT("Engine MCP integration is loaded"), Module))
	{
		return false;
	}
	FString DiscoveryText;
	if (GetDefault<UModelContextProtocolSettings>()->bEnableToolSearch)
	{
		TSharedPtr<IModelContextProtocolTool> ListTool = Module->FindTool(TEXT("list_toolsets"));
		TSharedPtr<IModelContextProtocolTool> DescribeTool = Module->FindTool(TEXT("describe_toolset"));
		TestTrue(TEXT("MCP dispatch tool exists"), Module->FindTool(TEXT("call_tool")).IsValid());
		if (!TestTrue(TEXT("MCP discovery tools exist"), ListTool.IsValid() && DescribeTool.IsValid()))
		{
			return false;
		}
		FModelContextProtocolToolResult Catalog = ListTool->Run(MakeShared<FJsonObject>());
		TestTrue(TEXT("MCP catalog includes PlaneToPBR"), Catalog.JsonObject->GetArrayField(TEXT("content"))[0]->AsObject()->GetStringField(TEXT("text")).Contains(TEXT("PlaneToPBRMCP.PlaneToPBRToolset")));
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("toolset_name"), TEXT("PlaneToPBRMCP.PlaneToPBRToolset"));
		FModelContextProtocolToolResult Description = DescribeTool->Run(Params);
		DiscoveryText = Description.JsonObject->GetArrayField(TEXT("content"))[0]->AsObject()->GetStringField(TEXT("text"));
	}
	for (const TCHAR* Name : { TEXT("GeneratePBRPlaneFromImage"), TEXT("GeneratePBRTextures"), TEXT("ImportPBRTextures"),
		TEXT("CreatePBRMaterial"), TEXT("CreateDisplacedMesh") })
	{
		UFunction* Function = UPlaneToPBRToolset::StaticClass()->FindFunctionByName(Name);
		TestNotNull(Name, Function);
		if (Function)
		{
			TestTrue(TEXT("Function is AICallable"), Function->HasMetaData(TEXT("AICallable")));
		}
		const FString ToolName = FString(TEXT("PlaneToPBRMCP.PlaneToPBRToolset.")) + Name;
		if (GetDefault<UModelContextProtocolSettings>()->bEnableToolSearch)
		{
			TestTrue(TEXT("MCP description exposes tool"), DiscoveryText.Contains(ToolName));
		}
		else
		{
			TestTrue(TEXT("Tool is discoverable through MCP"), Module->FindTool(ToolName).IsValid());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaneToPBRMCPInputValidationTest, "PlaneToPBR.MCP.InputValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRMCPInputValidationTest::RunTest(const FString& Parameters)
{
	auto CheckError = [this](UToolCallAsyncResultString* Result, const TCHAR* Diagnostic)
	{
		TestTrue(TEXT("Invalid input completes immediately"), Result->bIsComplete);
		TestTrue(TEXT("Result has structured failure"), Result->Error.Contains(TEXT("\"success\": false")));
		TestTrue(TEXT("Diagnostic is informative"), Result->Error.Contains(Diagnostic));
	};
	CheckError(UPlaneToPBRToolset::GeneratePBRTextures(TEXT("")), TEXT("Image file"));
	CheckError(UPlaneToPBRToolset::GeneratePBRPlaneFromImage(TEXT("missing.png")), TEXT("Image file"));
	CheckError(UPlaneToPBRToolset::GeneratePBRPlaneFromImage(TEXT(""), TEXT(""), -1.0f), TEXT("PlaneWidthCm"));
	CheckError(UPlaneToPBRToolset::GeneratePBRPlaneFromImage(TEXT(""), TEXT(""), 200.0f, 513), TEXT("Subdivisions"));
	CheckError(UPlaneToPBRToolset::GeneratePBRPlaneFromImage(TEXT(""), TEXT(""), 200.0f, 128, -1.0f), TEXT("DisplacementStrengthCm"));
	CheckError(UPlaneToPBRToolset::ImportPBRTextures(TEXT(""), {}), TEXT("Image file"));
	CheckError(UPlaneToPBRToolset::CreatePBRMaterial(TEXT("/Engine/Invalid"), {}), TEXT("ContentPath"));
	CheckError(UPlaneToPBRToolset::CreateDisplacedMesh(TEXT("bad"), TEXT(""), TEXT("")), TEXT("ContentPath"));
	CheckError(UToolsetRegistry::ExecuteTool(TEXT("PlaneToPBRMCP.PlaneToPBRToolset"), TEXT("GeneratePBRTextures"), TEXT("{\"imagePath\":\"\"}")), TEXT("Image file"));
	CheckError(UToolsetRegistry::ExecuteTool(TEXT("PlaneToPBRMCP.PlaneToPBRToolset"), TEXT("GeneratePBRPlaneFromImage"), TEXT("{\"imagePath\":\"\"}")), TEXT("Image file"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaneToPBRMCPWorkflowTest, "PlaneToPBR.MCP.WorkflowInvocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaneToPBRMCPWorkflowTest::RunTest(const FString& Parameters)
{
	const FString ImagePath = FPaths::ProjectPluginsDir() / TEXT("PlaneToPBR/Resources/Icon128.png");
	TFunction<void(const FPlaneToPBRGenerationResult&)> Complete;
	PlaneToPBRMCPTests::WorkflowRunner = [this, &Complete, ImagePath](const FPlaneToPBRHuggingFaceRequest& Request,
		const FPlaneToPBRMeshOptions& Options, TFunction<void(const FPlaneToPBRGenerationResult&)> Callback)
	{
		TestEqual(TEXT("Image forwarded"), Request.ImagePath, FPaths::ConvertRelativePathToFull(ImagePath));
		TestEqual(TEXT("Prompt forwarded"), Request.HFPrompt, FString(TEXT("stone")));
		TestEqual(TEXT("Width forwarded"), Options.PlaneWidthCm, 300.0f);
		TestEqual(TEXT("Subdivisions forwarded"), Options.Subdivisions, 32);
		TestEqual(TEXT("Displacement forwarded"), Options.DisplacementStrengthCm, 12.0f);
		Complete = MoveTemp(Callback);
	};
	TStrongObjectPtr<UToolCallAsyncResultString> Result(UPlaneToPBRToolset::GeneratePBRPlaneFromImage(ImagePath, TEXT("stone"), 300.0f, 32, 12.0f));
	PlaneToPBRMCPTests::WorkflowRunner = {};
	TestFalse(TEXT("No premature success"), Result->bIsComplete);
	if (!Complete)
	{
		AddError(TEXT("Workflow was not invoked with the valid test image."));
		return false;
	}
	FPlaneToPBRGenerationResult Output;
	Output.bSucceeded = true;
	Output.MeshPath = TEXT("/Game/Test/Plane.Plane");
	Output.ActorName = TEXT("TestPlane");
	Complete(Output);
	TestTrue(TEXT("Completion observed"), Result->bIsComplete);
	TestTrue(TEXT("Created mesh returned"), Result->Value.Contains(Output.MeshPath));
	TestTrue(TEXT("Created actor returned"), Result->Value.Contains(Output.ActorName));
	PlaneToPBRMCPTests::WorkflowRunner = [this, &Complete](const FPlaneToPBRHuggingFaceRequest& Request, const FPlaneToPBRMeshOptions& Options,
		TFunction<void(const FPlaneToPBRGenerationResult&)> Callback)
	{
		TestEqual(TEXT("MCP omitted prompt defaults to empty"), Request.HFPrompt, FString());
		TestEqual(TEXT("MCP default width"), Options.PlaneWidthCm, 200.0f);
		TestEqual(TEXT("MCP default subdivisions"), Options.Subdivisions, 128);
		TestEqual(TEXT("MCP default displacement"), Options.DisplacementStrengthCm, 25.0f);
		Complete = MoveTemp(Callback);
	};
	TSharedRef<FJsonObject> Input = MakeShared<FJsonObject>();
	Input->SetStringField(TEXT("imagePath"), ImagePath);
	FString JsonInput;
	FJsonSerializer::Serialize(Input, TJsonWriterFactory<>::Create(&JsonInput));
	TStrongObjectPtr<UToolCallAsyncResultString> Dispatched(UToolsetRegistry::ExecuteTool(TEXT("PlaneToPBRMCP.PlaneToPBRToolset"), TEXT("GeneratePBRPlaneFromImage"), JsonInput));
	PlaneToPBRMCPTests::WorkflowRunner = {};
	Complete(Output);
	TestTrue(TEXT("MCP dispatch completes"), Dispatched->bIsComplete);
	PlaneToPBRMCPTests::WorkflowRunner = [&Complete](const FPlaneToPBRHuggingFaceRequest&, const FPlaneToPBRMeshOptions&,
		TFunction<void(const FPlaneToPBRGenerationResult&)> Callback) { Complete = MoveTemp(Callback); };
	TStrongObjectPtr<UToolCallAsyncResultString> Canceled(UPlaneToPBRToolset::GeneratePBRPlaneFromImage(ImagePath));
	PlaneToPBRMCPTests::WorkflowRunner = {};
	UPlaneToPBRToolset::CancelPendingOperations();
	TestTrue(TEXT("Shutdown completes pending request"), Canceled->bIsComplete);
	TestTrue(TEXT("Shutdown is an error"), Canceled->Error.Contains(TEXT("shutting down")));
	Complete(Output);
	TestTrue(TEXT("Late completion cannot turn cancellation into success"), Canceled->Value.IsEmpty());
	return true;
}

#endif