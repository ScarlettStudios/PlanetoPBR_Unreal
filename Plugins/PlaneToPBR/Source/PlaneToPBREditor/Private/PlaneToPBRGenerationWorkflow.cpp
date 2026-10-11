#include "PlaneToPBRGenerationWorkflow.h"

#include "Containers/Ticker.h"
#include "Materials/MaterialInterface.h"
#include "PlaneToPBRDisplacedMeshBuilder.h"
#include "PlaneToPBRMaterialBuilder.h"
#include "PlaneToPBRTextureImporter.h"

#define LOCTEXT_NAMESPACE "FPlaneToPBRGenerationWorkflow"

void FPlaneToPBRGenerationWorkflow::GeneratePBRPlane(
	const FPlaneToPBRHuggingFaceRequest& Request,
	FStatusCallback StatusCallback,
	FCompletionCallback CompletionCallback)
{
	GeneratePBRPlane(Request, FPlaneToPBRMeshOptions(), MoveTemp(StatusCallback),
		[CompletionCallback = MoveTemp(CompletionCallback)](const FPlaneToPBRGenerationResult&)
		{
			if (CompletionCallback)
			{
				CompletionCallback();
			}
		});
}

void FPlaneToPBRGenerationWorkflow::GeneratePBRPlane(
	const FPlaneToPBRHuggingFaceRequest& Request,
	const FPlaneToPBRMeshOptions& Options,
	FStatusCallback StatusCallback,
	TFunction<void(const FPlaneToPBRGenerationResult&)> ResultCallback)
{
	FCompletionCallback CompletionCallback;
	TSharedRef<FPlaneToPBRGenerationResult> Output = MakeShared<FPlaneToPBRGenerationResult>();
	CompletionCallback = [Output, ResultCallback = MoveTemp(ResultCallback)]()
	{
		if (ResultCallback)
		{
			ResultCallback(*Output);
		}
	};
	const FString SourceImagePath = Request.ImagePath;

	if (StatusCallback)
	{
		StatusCallback(LOCTEXT("GeneratingTexturesStatus", "Uploading image, generating maps, and downloading textures..."));
	}

	// 1. Kick off asynchronous texture generation with Hugging Face Space client
	FPlaneToPBRHuggingFaceClient Client;
	Client.GeneratePBRTexturesAsync(
		Request,
		[StatusCallback = MoveTemp(StatusCallback), CompletionCallback = MoveTemp(CompletionCallback), SourceImagePath, Options, Output](const FPlaneToPBRHuggingFaceResult& Result) mutable
		{
			if (Options.ShouldCancel && Options.ShouldCancel())
			{
				return;
			}
			if (!Result.bSucceeded)
			{
				Output->Message = Result.Message;
				if (StatusCallback)
				{
					StatusCallback(FText::FromString(Result.Message));
				}

				if (CompletionCallback)
				{
					CompletionCallback();
				}
				return;
			}

			if (StatusCallback)
			{
				StatusCallback(LOCTEXT("ImportingTexturesStatus", "Importing PlaneToPBR textures..."));
			}

			// Defer asset importing and editor actor creation to next engine tick to ensure clean thread context
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[StatusCallback = MoveTemp(StatusCallback), CompletionCallback = MoveTemp(CompletionCallback), TexturePaths = Result.TexturePaths, SourceImagePath, Options, Output](float DeltaTime) mutable
				{
					if (Options.ShouldCancel && Options.ShouldCancel())
					{
						return false;
					}
					// 2. Import downloaded PNGs into Unreal Texture2D assets
					FString ContentPath;
					TMap<FString, FString> TextureAssetPaths;
					FString ImportErrorMessage;
					if (!FPlaneToPBRTextureImporter::ImportDownloadedTextures(TexturePaths, SourceImagePath, ContentPath, TextureAssetPaths, ImportErrorMessage))
					{
						Output->Message = ImportErrorMessage;
						if (StatusCallback)
						{
							StatusCallback(FText::FromString(ImportErrorMessage));
						}

						if (CompletionCallback)
						{
							CompletionCallback();
						}
						return false;
					}

					Output->ContentPath = ContentPath;
					Output->TextureAssetPaths = TextureAssetPaths;
					if (StatusCallback)
					{
						StatusCallback(LOCTEXT("CreatingMaterialStatus", "Creating PlaneToPBR material..."));
					}

					// 3. Create and wire up the PBR material asset
					FString MaterialPath;
					UMaterialInterface* GeneratedMaterial = nullptr;
					FString MaterialErrorMessage;
					if (!FPlaneToPBRMaterialBuilder::CreateGeneratedMaterial(ContentPath, TextureAssetPaths, MaterialPath, GeneratedMaterial, MaterialErrorMessage))
					{
						Output->Message = MaterialErrorMessage;
						if (StatusCallback)
						{
							StatusCallback(FText::FromString(MaterialErrorMessage));
						}

						if (CompletionCallback)
						{
							CompletionCallback();
						}
						return false;
					}

					Output->MaterialPath = MaterialPath;
					if (StatusCallback)
					{
						StatusCallback(LOCTEXT("CreatingPlaneStatus", "Creating PlaneToPBR displaced plane..."));
					}

					// 4. Create displaced plane mesh and spawn it into the level
					FString ActorLabel;
					FString ActorErrorMessage;
					if (!FPlaneToPBRDisplacedMeshBuilder::CreateGeneratedDisplacedPlaneActor(ContentPath, GeneratedMaterial, TexturePaths, ActorLabel, ActorErrorMessage, Options, Output->MeshPath, Output->ActorName))
					{
						Output->Message = ActorErrorMessage;
						if (StatusCallback)
						{
							StatusCallback(FText::FromString(ActorErrorMessage));
						}

						if (CompletionCallback)
						{
							CompletionCallback();
						}
						return false;
					}

					Output->bSucceeded = true;
					Output->Message = FString::Printf(TEXT("Created PlaneToPBR displaced plane: %s"), *ActorLabel);
					if (StatusCallback)
					{
						StatusCallback(FText::FromString(FString::Printf(TEXT("Created PlaneToPBR displaced plane: %s"), *ActorLabel)));
					}

					if (CompletionCallback)
					{
						CompletionCallback();
					}
					return false;
				}));
		});
}

#undef LOCTEXT_NAMESPACE
