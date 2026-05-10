// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlanetoPBRTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FPlanetoPBRTextureSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	TObjectPtr<UTexture2D> Diffuse = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	TObjectPtr<UTexture2D> Depth = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	TObjectPtr<UTexture2D> Normal = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	TObjectPtr<UTexture2D> Roughness = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	TObjectPtr<UTexture2D> Mask = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	FString DiffusePath;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	FString DepthPath;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	FString NormalPath;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	FString RoughnessPath;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	FString MaskPath;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	int32 SourceWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	int32 SourceHeight = 0;
};
