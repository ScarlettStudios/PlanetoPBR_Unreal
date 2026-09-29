#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

/**
 * Handles creation, importing, material assignment, and level actor spawning
 * for procedural displaced plane static meshes.
 */
class FPlaneToPBRDisplacedMeshBuilder
{
public:
	/**
	 * Creates a displaced plane static mesh asset from depth data, assigns the generated material,
	 * saves the asset package, and spawns a StaticMeshActor in the editor world oriented towards the viewport.
	 *
	 * @param ContentPath Destination /Game package content folder.
	 * @param Material Generated Unreal material to assign to the mesh.
	 * @param TexturePaths Map of downloaded texture file paths.
	 * @param OutActorLabel Human-readable label of the spawned world actor.
	 * @param OutErrorMessage Diagnostic message populated on failure.
	 * @return True if mesh was built, imported, and actor spawned in the world.
	 */
	static bool CreateGeneratedDisplacedPlaneActor(
		const FString& ContentPath,
		UMaterialInterface* Material,
		const TMap<FString, FString>& TexturePaths,
		FString& OutActorLabel,
		FString& OutErrorMessage);

private:
	/**
	 * Reads the depth image file, evaluates displaced plane geometry, and writes out an OBJ file on disk.
	 */
	static bool CreateDisplacedPlaneObj(
		const FString& DepthTexturePath,
		const FString& ObjPath,
		int32& OutDepthWidth,
		int32& OutDepthHeight,
		int32& OutSubdivisionsY,
		FString& OutErrorMessage);
};
