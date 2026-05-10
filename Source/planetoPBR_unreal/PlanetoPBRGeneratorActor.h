// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlanetoPBRGeneratorActor.generated.h"

class UMaterialInterface;
class UPlanetoPBRRequest;
class APlanetoPBRPlaneActor;
struct FPlanetoPBRTextureSet;

UCLASS(BlueprintType)
class PLANETOPBR_UNREAL_API APlanetoPBRGeneratorActor : public AActor
{
	GENERATED_BODY()

public:
	APlanetoPBRGeneratorActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlaneToPBR")
	FString ImagePath = TEXT("F:\\test_image.png");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlaneToPBR")
	FString Prompt = TEXT("windows");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlaneToPBR")
	TObjectPtr<UMaterialInterface> MasterMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlaneToPBR")
	bool bGenerateOnBeginPlay = true;

	UFUNCTION(BlueprintCallable, Category = "PlaneToPBR")
	void Generate();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UPlanetoPBRRequest> ActiveRequest = nullptr;

	UFUNCTION()
	void HandleCompleted(const FPlanetoPBRTextureSet& Textures, APlanetoPBRPlaneActor* PlaneActor);

	UFUNCTION()
	void HandleFailed(const FString& ErrorMessage);
};
