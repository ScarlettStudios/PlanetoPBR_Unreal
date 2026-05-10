// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PlanetoPBRBlueprintLibrary.generated.h"

class UMaterialInterface;
class UPlanetoPBRRequest;

UCLASS()
class PLANETOPBR_UNREAL_API UPlanetoPBRBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PlaneToPBR", meta = (WorldContext = "WorldContextObject"))
	static UPlanetoPBRRequest* GeneratePBRPlaneFromImage(UObject* WorldContextObject, const FString& ImagePath, const FString& Prompt, UMaterialInterface* MasterMaterial);
};
