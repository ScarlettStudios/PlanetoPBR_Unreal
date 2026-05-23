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

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[StatusCallback = MoveTemp(StatusCallback), CompletionCallback = MoveTemp(CompletionCallback), TexturePaths = Result.TexturePaths, SourceImagePath](float DeltaTime) mutable
				{
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
