#pragma once

#include "CoreMinimal.h"

/**
 * Raw 8-bit grayscale depth texture representation in memory.
 */
struct FPlaneToPBRDepthImage
{
	/** Row-major byte array containing 8-bit depth values [0..255]. */
	TArray<uint8> Pixels;

	/** Image width in pixels. */
	int32 Width = 0;

	/** Image height in pixels. */
	int32 Height = 0;

	/** Checks whether pixel array and dimensions represent valid non-empty data. */
	bool IsValid() const;

	/**
	 * Samples normalized height [0.0..1.0] from UV coordinates with coordinate orientation handling.
	 *
	 * @param U Normalized horizontal coordinate [0..1].
	 * @param V Normalized vertical coordinate [0..1].
	 * @return Sampled normalized height value.
	 */
	float SampleHeight(float U, float V) const;
};

/**
 * Parameter settings governing mesh subdivision, dimensions, and displacement scaling.
 */
struct FPlaneToPBRDisplacedPlaneSettings
{
	/** Number of subdivisions along the X (horizontal) axis. */
	int32 SubdivisionsX = 96;

	/** Width of the plane in unreal centimeters. */
	float PlaneWidthCm = 200.0f;

	/** Maximum displacement height range in centimeters. */
	float DisplacementStrengthCm = 25.0f;

	/** Normalized center baseline offset [0..1] where zero displacement occurs. */
	float HeightCenter = 0.5f;
};

/**
 * Generated geometric buffers for displaced plane static meshes.
 */
struct FPlaneToPBRDisplacedPlaneGeometry
{
	/** 3D vertex positions in local space. */
	TArray<FVector> Vertices;

	/** 2D texture UV coordinates. */
	TArray<FVector2D> UVs;

	/** Triangular face vertex index triplets. */
	TArray<FIntVector> Faces;

	/** Calculated number of vertical subdivisions matching aspect ratio. */
	int32 SubdivisionsY = 0;
};

/**
 * Procedural geometry builder that calculates 3D vertex displacement from a depth map
 * and exports Wavefront OBJ text files for native mesh importing.
 */
class FPlaneToPBRDisplacedPlaneGeometryBuilder
{
public:
	/**
	 * Generates displaced plane vertex, UV, and face buffers from depth pixel data and settings.
	 *
	 * @param DepthImage Source grayscale depth image.
	 * @param Settings Sizing, subdivision, and displacement parameters.
	 * @param OutGeometry Output geometric buffers.
	 * @return True if geometry was successfully calculated.
	 */
	static bool Build(
		const FPlaneToPBRDepthImage& DepthImage,
		const FPlaneToPBRDisplacedPlaneSettings& Settings,
		FPlaneToPBRDisplacedPlaneGeometry& OutGeometry);

	/**
	 * Serializes geometry buffers into Wavefront OBJ format.
	 *
	 * @param Geometry Geometric data containing vertices, UVs, and faces.
	 * @return Formatted OBJ file string.
	 */
	static FString WriteObjString(const FPlaneToPBRDisplacedPlaneGeometry& Geometry);
};
