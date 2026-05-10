// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlanetoPBRBlueprintLibrary.h"

#include "PlanetoPBRRequest.h"

UPlanetoPBRRequest* UPlanetoPBRBlueprintLibrary::GeneratePBRPlaneFromImage(UObject* WorldContextObject, const FString& ImagePath, const FString& Prompt, UMaterialInterface* MasterMaterial)
{
	return UPlanetoPBRRequest::GeneratePBRPlaneFromImage(WorldContextObject, ImagePath, Prompt, MasterMaterial);
}
