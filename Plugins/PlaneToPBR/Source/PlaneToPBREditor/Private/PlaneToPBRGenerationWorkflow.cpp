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
	const FString SourceImagePath = Request.ImagePath;

	if (StatusCallback)
	{
		StatusCallback(LOCTEXT("GeneratingTexturesStatus", "Uploading image, generating maps, and downloading textures..."));
	}

	// 1. Kick off asynchronous texture generation with Hugging Face Space client
	FPlaneToPBRHuggingFaceClient Client;
	Client.GeneratePBRTexturesAsync(
		Request,
		[StatusCallback = MoveTemp(StatusCallback), CompletionCallback = MoveTemp(CompletionCallback), SourceImagePath](const FPlaneToPBRHuggingFaceResult& Result) mutable
		{
			if (!Result.bSucceeded)
			{
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
				[StatusCallback = MoveTemp(StatusCallback), CompletionCallback = MoveTemp(CompletionCallback), TexturePaths = Result.TexturePaths, SourceImagePath](float DeltaTime) mutable
				{
					// 2. Import downloaded PNGs into Unreal Texture2D assets
					FString ContentPath;
					TMap<FString, FString> TextureAssetPaths;
					FString ImportErrorMessage;
					if (!FPlaneToPBRTextureImporter::ImportDownloadedTextures(TexturePaths, SourceImagePath, ContentPath, TextureAssetPaths, ImportErrorMessage))
					{
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

					if (StatusCallback)
					{
						StatusCallback(LOCTEXT("CreatingPlaneStatus", "Creating PlaneToPBR displaced plane..."));
					}

					// 4. Create displaced plane mesh and spawn it into the level
					FString ActorLabel;
					FString ActorErrorMessage;
					if (!FPlaneToPBRDisplacedMeshBuilder::CreateGeneratedDisplacedPlaneActor(ContentPath, GeneratedMaterial, TexturePaths, ActorLabel, ActorErrorMessage))
					{
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
