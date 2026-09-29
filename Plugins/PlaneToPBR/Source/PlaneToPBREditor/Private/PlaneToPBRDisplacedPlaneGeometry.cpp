#include "PlaneToPBRDisplacedPlaneGeometry.h"

bool FPlaneToPBRDepthImage::IsValid() const
{
	return Width > 0 && Height > 0 && Pixels.Num() >= Width * Height;
}

float FPlaneToPBRDepthImage::SampleHeight(const float U, const float V) const
{
	if (!IsValid())
	{
		return 0.0f;
	}

	// Clamp pixel coordinates; V is inverted (1.0 - V) to match image space top-down row order
	const int32 X = FMath::Clamp(FMath::RoundToInt(U * static_cast<float>(Width - 1)), 0, Width - 1);
	const int32 Y = FMath::Clamp(FMath::RoundToInt((1.0f - V) * static_cast<float>(Height - 1)), 0, Height - 1);
	return static_cast<float>(Pixels[Y * Width + X]) / 255.0f;
}

bool FPlaneToPBRDisplacedPlaneGeometryBuilder::Build(
	const FPlaneToPBRDepthImage& DepthImage,
	const FPlaneToPBRDisplacedPlaneSettings& Settings,
	FPlaneToPBRDisplacedPlaneGeometry& OutGeometry)
{
	if (!DepthImage.IsValid() || Settings.SubdivisionsX <= 0 || Settings.PlaneWidthCm <= 0.0f)
	{
		return false;
	}

	OutGeometry = FPlaneToPBRDisplacedPlaneGeometry();

	// Calculate vertical subdivision count and plane height preserving source aspect ratio
	const float AspectRatio = static_cast<float>(DepthImage.Width) / static_cast<float>(DepthImage.Height);
	OutGeometry.SubdivisionsY = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Settings.SubdivisionsX) / AspectRatio));
	const float PlaneHeightCm = Settings.PlaneWidthCm / AspectRatio;

	// Pre-allocate buffer storage
	OutGeometry.Vertices.Reserve((Settings.SubdivisionsX + 1) * (OutGeometry.SubdivisionsY + 1));
	OutGeometry.UVs.Reserve((Settings.SubdivisionsX + 1) * (OutGeometry.SubdivisionsY + 1));
	OutGeometry.Faces.Reserve(Settings.SubdivisionsX * OutGeometry.SubdivisionsY * 2);

	// Generate vertex grid with Z displacement sampled from depth image
	for (int32 YIndex = 0; YIndex <= OutGeometry.SubdivisionsY; ++YIndex)
	{
		const float V = static_cast<float>(YIndex) / static_cast<float>(OutGeometry.SubdivisionsY);
		const float YPosition = (V - 0.5f) * PlaneHeightCm;
		for (int32 XIndex = 0; XIndex <= Settings.SubdivisionsX; ++XIndex)
		{
			const float U = static_cast<float>(XIndex) / static_cast<float>(Settings.SubdivisionsX);
			const float XPosition = (U - 0.5f) * Settings.PlaneWidthCm;
			const float ZPosition = (DepthImage.SampleHeight(U, V) - Settings.HeightCenter) * Settings.DisplacementStrengthCm;
			OutGeometry.Vertices.Add(FVector(XPosition, YPosition, ZPosition));
			OutGeometry.UVs.Add(FVector2D(U, V));
		}
	}

	// Triangulate grid quads into two triangles per cell (1-based indices for OBJ export)
	const int32 RowWidth = Settings.SubdivisionsX + 1;
	for (int32 YIndex = 0; YIndex < OutGeometry.SubdivisionsY; ++YIndex)
	{
		for (int32 XIndex = 0; XIndex < Settings.SubdivisionsX; ++XIndex)
		{
			const int32 A = YIndex * RowWidth + XIndex + 1;
			const int32 B = A + 1;
			const int32 C = A + RowWidth;
			const int32 D = C + 1;
			OutGeometry.Faces.Add(FIntVector(A, B, D));
			OutGeometry.Faces.Add(FIntVector(A, D, C));
		}
	}

	return true;
}

FString FPlaneToPBRDisplacedPlaneGeometryBuilder::WriteObjString(const FPlaneToPBRDisplacedPlaneGeometry& Geometry)
{
	FString ObjContents;
	ObjContents.Reserve(Geometry.Vertices.Num() * 64);
	ObjContents += TEXT("# PlaneToPBR generated displaced plane\n");
	ObjContents += TEXT("o PlaneToPBR_DisplacedPlane\n");

	// Output vertex positions (v X Y Z)
	for (const FVector& Vertex : Geometry.Vertices)
	{
		ObjContents += FString::Printf(TEXT("v %.6f %.6f %.6f\n"), Vertex.X, Vertex.Y, Vertex.Z);
	}

	// Output UV coordinates (vt U V)
	for (const FVector2D& UV : Geometry.UVs)
	{
		ObjContents += FString::Printf(TEXT("vt %.6f %.6f\n"), UV.X, UV.Y);
	}

	// Output faces (f v1/vt1 v2/vt2 v3/vt3)
	for (const FIntVector& Face : Geometry.Faces)
	{
		ObjContents += FString::Printf(TEXT("f %d/%d %d/%d %d/%d\n"), Face.X, Face.X, Face.Y, Face.Y, Face.Z, Face.Z);
	}

	return ObjContents;
}
