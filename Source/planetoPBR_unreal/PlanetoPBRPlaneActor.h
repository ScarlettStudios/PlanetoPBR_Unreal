// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlanetoPBRTypes.h"
#include "PlanetoPBRPlaneActor.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

UCLASS(BlueprintType)
class PLANETOPBR_UNREAL_API APlanetoPBRPlaneActor : public AActor
{
	GENERATED_BODY()

public:
	APlanetoPBRPlaneActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlaneToPBR")
	TObjectPtr<UStaticMeshComponent> PlaneMesh;

	UPROPERTY(BlueprintReadOnly, Category = "PlaneToPBR")
	FPlanetoPBRTextureSet Textures;

	UFUNCTION(BlueprintCallable, Category = "PlaneToPBR")
	void ApplyGeneratedTextures(const FPlanetoPBRTextureSet& InTextures, UMaterialInterface* MasterMaterial);
};
