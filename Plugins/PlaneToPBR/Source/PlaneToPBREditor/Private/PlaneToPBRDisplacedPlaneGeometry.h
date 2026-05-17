#pragma once

#include "CoreMinimal.h"

struct FPlaneToPBRDepthImage
{
	TArray<uint8> Pixels;
	int32 Width = 0;
	int32 Height = 0;

	bool IsValid() const;
	float SampleHeight(float U, float V) const;
};

struct FPlaneToPBRDisplacedPlaneSettings
{
	int32 SubdivisionsX = 96;
	float PlaneWidthCm = 200.0f;
	float DisplacementStrengthCm = 25.0f;
	float HeightCenter = 0.5f;
};

struct FPlaneToPBRDisplacedPlaneGeometry
{
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FIntVector> Faces;
	int32 SubdivisionsY = 0;
};

class FPlaneToPBRDisplacedPlaneGeometryBuilder
{
public:
	static bool Build(
		const FPlaneToPBRDepthImage& DepthImage,
		const FPlaneToPBRDisplacedPlaneSettings& Settings,
		FPlaneToPBRDisplacedPlaneGeometry& OutGeometry);

	static FString WriteObjString(const FPlaneToPBRDisplacedPlaneGeometry& Geometry);
};
