// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlanetoPBRPlaneActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

APlanetoPBRPlaneActor::APlanetoPBRPlaneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	SetRootComponent(PlaneMesh);

	if (UStaticMesh* PlaneAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		PlaneMesh->SetStaticMesh(PlaneAsset);
	}
}

void APlanetoPBRPlaneActor::ApplyGeneratedTextures(const FPlanetoPBRTextureSet& InTextures, UMaterialInterface* MasterMaterial)
{
	Textures = InTextures;

	if (Textures.SourceWidth > 0 && Textures.SourceHeight > 0)
	{
		const float AspectRatio = static_cast<float>(Textures.SourceWidth) / static_cast<float>(Textures.SourceHeight);
		const float WidthCm = 200.0f;
		const float HeightCm = WidthCm / AspectRatio;
		SetActorScale3D(FVector(WidthCm / 100.0f, HeightCm / 100.0f, 1.0f));
	}

	if (!MasterMaterial)
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(MasterMaterial, this);
	if (!DynamicMaterial)
	{
		return;
	}

	DynamicMaterial->SetTextureParameterValue(TEXT("Diffuse"), Textures.Diffuse);
	DynamicMaterial->SetTextureParameterValue(TEXT("BaseColor"), Textures.Diffuse);
	DynamicMaterial->SetTextureParameterValue(TEXT("BaseColorTexture"), Textures.Diffuse);
	DynamicMaterial->SetTextureParameterValue(TEXT("Depth"), Textures.Depth);
	DynamicMaterial->SetTextureParameterValue(TEXT("DepthTexture"), Textures.Depth);
	DynamicMaterial->SetTextureParameterValue(TEXT("Normal"), Textures.Normal);
	DynamicMaterial->SetTextureParameterValue(TEXT("NormalTexture"), Textures.Normal);
	DynamicMaterial->SetTextureParameterValue(TEXT("Roughness"), Textures.Roughness);
	DynamicMaterial->SetTextureParameterValue(TEXT("RoughnessTexture"), Textures.Roughness);
	DynamicMaterial->SetTextureParameterValue(TEXT("Mask"), Textures.Mask);
	DynamicMaterial->SetTextureParameterValue(TEXT("MaskTexture"), Textures.Mask);

	PlaneMesh->SetMaterial(0, DynamicMaterial);
}
