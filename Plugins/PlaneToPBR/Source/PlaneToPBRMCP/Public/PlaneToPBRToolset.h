#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "PlaneToPBRToolset.generated.h"

/** Generate PBR textures, materials, and displaced planes in the active editor level. */
UCLASS()
class PLANETOPBRMCP_API UPlaneToPBRToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Complete pending requests before editor shutdown. */
	static void CancelPendingOperations();
	/** Convert an image into imported textures, a material, and a displaced plane actor. Completes after generation finishes. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "PlaneToPBR|MCP")
	static UToolCallAsyncResultString* GeneratePBRPlaneFromImage(
		const FString& ImagePath, const FString& Prompt = TEXT(""),
		float PlaneWidthCm = 200.0f, int32 Subdivisions = 128, float DisplacementStrengthCm = 25.0f);

	/** Generate and download texture maps without importing assets. Returns texturePaths on completion. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "PlaneToPBR|MCP")
	static UToolCallAsyncResultString* GeneratePBRTextures(const FString& ImagePath, const FString& Prompt = TEXT(""));

	/** Import depth, normal, roughness, mask, and the source color image. Returns contentPath and textureAssetPaths. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "PlaneToPBR|MCP")
	static UToolCallAsyncResultString* ImportPBRTextures(const FString& SourceImagePath, const TMap<FString, FString>& DownloadedTexturePaths);

	/** Build a material in a /Game content folder from basecolor, normal, roughness, and mask texture assets. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "PlaneToPBR|MCP")
	static UToolCallAsyncResultString* CreatePBRMaterial(const FString& ContentPath, const TMap<FString, FString>& TextureAssetPaths);

	/** Build and spawn a displaced plane with the specified material and depth image. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "PlaneToPBR|MCP")
	static UToolCallAsyncResultString* CreateDisplacedMesh(
		const FString& ContentPath, const FString& MaterialAssetPath, const FString& DepthImagePath,
		float PlaneWidthCm = 200.0f, int32 Subdivisions = 128, float DisplacementStrengthCm = 25.0f);
};