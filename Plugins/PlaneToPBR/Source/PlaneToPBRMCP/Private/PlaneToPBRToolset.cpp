#include "PlaneToPBRToolset.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PlaneToPBRGenerationWorkflow.h"
#include "PlaneToPBRMCPTestHooks.h"
#include "PlaneToPBRMaterialBuilder.h"
#include "PlaneToPBRTextureImporter.h"
#include "PlaneToPBRTextureRoles.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
PlaneToPBRMCPTests::FWorkflowRunner PlaneToPBRMCPTests::WorkflowRunner;
#endif

namespace
{
	using FPendingResult = TSharedRef<TStrongObjectPtr<UToolCallAsyncResultString>>;
	TArray<TWeakObjectPtr<UToolCallAsyncResultString>> PendingResults;

	FPendingResult NewResult()
	{
		check(IsInGameThread());
		PendingResults.RemoveAll([](const auto& Result) { return !Result.IsValid() || Result->bIsComplete; });
		FPendingResult Result = MakeShared<TStrongObjectPtr<UToolCallAsyncResultString>>(NewObject<UToolCallAsyncResultString>());
		PendingResults.Add(Result->Get());
		return Result;
	}

	TSharedRef<FJsonObject> Success()
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("success"), true);
		return Json;
	}

	FString Serialize(const TSharedRef<FJsonObject>& Json)
	{
		FString Text;
		FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
		return Text;
	}

	void Fail(const FPendingResult& Result, const FString& Message)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("success"), false);
		Json->SetStringField(TEXT("error"), Message);
		(*Result)->SetError(Serialize(Json));
	}

	void SetMap(const TSharedRef<FJsonObject>& Json, const TCHAR* Key, const TMap<FString, FString>& Map)
	{
		TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
		for (const auto& Pair : Map)
		{
			Values->SetStringField(Pair.Key, Pair.Value);
		}
		Json->SetObjectField(Key, Values);
	}

	bool ValidateImage(const FPendingResult& Result, const FString& Path)
	{
		if (Path.IsEmpty() || !FPaths::FileExists(Path))
		{
			Fail(Result, FString::Printf(TEXT("Image file does not exist: %s"), *Path));
			return false;
		}
		return true;
	}

	bool ValidateContent(const FPendingResult& Result, const FString& Path)
	{
		if (!Path.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(Path))
		{
			Fail(Result, TEXT("ContentPath must be a valid /Game content folder."));
			return false;
		}
		return true;
	}

	bool ValidateGeometry(const FPendingResult& Result, float Width, int32 Subdivisions, float Strength)
	{
		if (!FMath::IsFinite(Width) || Width <= 0.0f || Subdivisions < 1 || Subdivisions > 512 ||
			!FMath::IsFinite(Strength) || Strength < 0.0f)
		{
			Fail(Result, TEXT("PlaneWidthCm must be positive and finite; Subdivisions must be 1-512; DisplacementStrengthCm must be nonnegative and finite."));
			return false;
		}
		return true;
	}

	void Defer(TFunction<void()> Action)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Action = MoveTemp(Action)](float)
		{
			Action();
			return false;
		}));
	}

	FString NormalizePrompt(const FString& Prompt)
	{
		return Prompt == TEXT("\"\"") ? FString() : Prompt;
	}
}

void UPlaneToPBRToolset::CancelPendingOperations()
{
	for (const auto& Result : PendingResults)
	{
		if (Result.IsValid() && !Result->bIsComplete)
		{
			Result->SetError(TEXT("{\"success\":false,\"error\":\"PlaneToPBR is shutting down.\"}"));
		}
	}
	PendingResults.Reset();
}

UToolCallAsyncResultString* UPlaneToPBRToolset::GeneratePBRPlaneFromImage(
	const FString& ImagePath, const FString& Prompt, float PlaneWidthCm, int32 Subdivisions, float DisplacementStrengthCm)
{
	FPendingResult Result = NewResult();
	if (ValidateGeometry(Result, PlaneWidthCm, Subdivisions, DisplacementStrengthCm) && ValidateImage(Result, ImagePath))
	{
		FPlaneToPBRHuggingFaceRequest Request;
		Request.ImagePath = FPaths::ConvertRelativePathToFull(ImagePath);
		Request.HFPrompt = NormalizePrompt(Prompt);
		FPlaneToPBRMeshOptions Options;
		Options.PlaneWidthCm = PlaneWidthCm;
		Options.Subdivisions = Subdivisions;
		Options.DisplacementStrengthCm = DisplacementStrengthCm;
		Options.ShouldCancel = [Result]() { return (*Result)->bIsComplete; };
		TFunction<void(const FPlaneToPBRGenerationResult&)> OnComplete = [Result](const FPlaneToPBRGenerationResult& Output)
		{
			if ((*Result)->bIsComplete)
			{
				return;
			}
			if (!Output.bSucceeded)
			{
				Fail(Result, Output.Message);
				return;
			}
			TSharedRef<FJsonObject> Json = Success();
			Json->SetStringField(TEXT("contentPath"), Output.ContentPath);
			SetMap(Json, TEXT("textureAssetPaths"), Output.TextureAssetPaths);
			Json->SetStringField(TEXT("materialAssetPath"), Output.MaterialPath);
			Json->SetStringField(TEXT("meshAssetPath"), Output.MeshPath);
			Json->SetStringField(TEXT("actorName"), Output.ActorName);
			(*Result)->SetValue(::Serialize(Json));
		};
#if WITH_DEV_AUTOMATION_TESTS
		if (PlaneToPBRMCPTests::WorkflowRunner)
		{
			PlaneToPBRMCPTests::WorkflowRunner(Request, Options, MoveTemp(OnComplete));
		}
		else
#endif
		{
			FPlaneToPBRGenerationWorkflow Workflow;
			Workflow.GeneratePBRPlane(Request, Options, {}, MoveTemp(OnComplete));
		}
	}
	return Result->Get();
}

UToolCallAsyncResultString* UPlaneToPBRToolset::GeneratePBRTextures(const FString& ImagePath, const FString& Prompt)
{
	FPendingResult Result = NewResult();
	if (ValidateImage(Result, ImagePath))
	{
		FPlaneToPBRHuggingFaceRequest Request;
		Request.ImagePath = FPaths::ConvertRelativePathToFull(ImagePath);
		Request.HFPrompt = NormalizePrompt(Prompt);
		FPlaneToPBRHuggingFaceClient Client;
		Client.GeneratePBRTexturesAsync(Request, [Result](const FPlaneToPBRHuggingFaceResult& Output)
		{
			if ((*Result)->bIsComplete)
			{
				return;
			}
			if (!Output.bSucceeded)
			{
				Fail(Result, Output.Message);
				return;
			}
			TSharedRef<FJsonObject> Json = Success();
			SetMap(Json, TEXT("texturePaths"), Output.TexturePaths);
			(*Result)->SetValue(::Serialize(Json));
		});
	}
	return Result->Get();
}

UToolCallAsyncResultString* UPlaneToPBRToolset::ImportPBRTextures(
	const FString& SourceImagePath, const TMap<FString, FString>& DownloadedTexturePaths)
{
	FPendingResult Result = NewResult();
	if (!ValidateImage(Result, SourceImagePath))
	{
		return Result->Get();
	}
	for (EPlaneToPBRTextureRole Role : FPlaneToPBRTextureRoles::GetRequiredDownloadedRoles())
	{
		const TCHAR* Key = FPlaneToPBRTextureRoles::GetInfo(Role).Key;
		const FString* Path = DownloadedTexturePaths.Find(Key);
		if (!Path || !FPaths::FileExists(*Path))
		{
			Fail(Result, FString::Printf(TEXT("Missing downloaded texture file for role: %s"), Key));
			return Result->Get();
		}
	}
	Defer([Result, SourceImagePath, DownloadedTexturePaths]()
	{
		if ((*Result)->bIsComplete)
		{
			return;
		}
		FString ContentPath;
		TMap<FString, FString> TextureAssetPaths;
		FString Error;
		if (!FPlaneToPBRTextureImporter::ImportDownloadedTextures(DownloadedTexturePaths, SourceImagePath, ContentPath, TextureAssetPaths, Error))
		{
			Fail(Result, Error);
			return;
		}
		TSharedRef<FJsonObject> Json = Success();
		Json->SetStringField(TEXT("contentPath"), ContentPath);
		SetMap(Json, TEXT("textureAssetPaths"), TextureAssetPaths);
		(*Result)->SetValue(::Serialize(Json));
	});
	return Result->Get();
}

UToolCallAsyncResultString* UPlaneToPBRToolset::CreatePBRMaterial(
	const FString& ContentPath, const TMap<FString, FString>& TextureAssetPaths)
{
	FPendingResult Result = NewResult();
	if (ValidateContent(Result, ContentPath))
	{
		Defer([Result, ContentPath, TextureAssetPaths]()
		{
			if ((*Result)->bIsComplete)
			{
				return;
			}
			for (EPlaneToPBRTextureRole Role : { EPlaneToPBRTextureRole::BaseColor, EPlaneToPBRTextureRole::Normal,
				EPlaneToPBRTextureRole::Roughness, EPlaneToPBRTextureRole::Mask })
			{
				const TCHAR* Key = FPlaneToPBRTextureRoles::GetInfo(Role).Key;
				const FString* Path = TextureAssetPaths.Find(Key);
				if (!Path || !LoadObject<UTexture2D>(nullptr, **Path))
				{
					Fail(Result, FString::Printf(TEXT("Missing or invalid texture asset for role: %s"), Key));
					return;
				}
			}
			FString MaterialPath;
			UMaterialInterface* Material = nullptr;
			FString Error;
			if (!FPlaneToPBRMaterialBuilder::CreateGeneratedMaterial(ContentPath, TextureAssetPaths, MaterialPath, Material, Error))
			{
				Fail(Result, Error);
				return;
			}
			TSharedRef<FJsonObject> Json = Success();
			Json->SetStringField(TEXT("materialAssetPath"), MaterialPath);
			(*Result)->SetValue(::Serialize(Json));
		});
	}
	return Result->Get();
}

UToolCallAsyncResultString* UPlaneToPBRToolset::CreateDisplacedMesh(
	const FString& ContentPath, const FString& MaterialAssetPath, const FString& DepthImagePath,
	float PlaneWidthCm, int32 Subdivisions, float DisplacementStrengthCm)
{
	FPendingResult Result = NewResult();
	if (ValidateContent(Result, ContentPath) && ValidateGeometry(Result, PlaneWidthCm, Subdivisions, DisplacementStrengthCm) && ValidateImage(Result, DepthImagePath))
	{
		Defer([Result, ContentPath, MaterialAssetPath, DepthImagePath, PlaneWidthCm, Subdivisions, DisplacementStrengthCm]()
		{
			if ((*Result)->bIsComplete)
			{
				return;
			}
			UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialAssetPath);
			if (!Material)
			{
				Fail(Result, TEXT("MaterialAssetPath does not identify a material asset."));
				return;
			}
			FPlaneToPBRMeshOptions Options;
			Options.PlaneWidthCm = PlaneWidthCm;
			Options.Subdivisions = Subdivisions;
			Options.DisplacementStrengthCm = DisplacementStrengthCm;
			FString ActorLabel;
			FString Error;
			FString MeshPath;
			FString ActorName;
			if (!FPlaneToPBRDisplacedMeshBuilder::CreateGeneratedDisplacedPlaneActor(ContentPath, Material,
				{ { TEXT("depth"), DepthImagePath } }, ActorLabel, Error, Options, MeshPath, ActorName))
			{
				Fail(Result, Error);
				return;
			}
			TSharedRef<FJsonObject> Json = Success();
			Json->SetStringField(TEXT("meshAssetPath"), MeshPath);
			Json->SetStringField(TEXT("actorName"), ActorName);
			(*Result)->SetValue(::Serialize(Json));
		});
	}
	return Result->Get();
}