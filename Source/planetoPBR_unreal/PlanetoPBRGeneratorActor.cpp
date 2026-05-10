// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlanetoPBRGeneratorActor.h"

#include "PlanetoPBRPlaneActor.h"
#include "PlanetoPBRRequest.h"
#include "PlanetoPBRTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetoPBRGenerator, Log, All);

APlanetoPBRGeneratorActor::APlanetoPBRGeneratorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Game/PlaneToPBR/M_PlaneToPBR.M_PlaneToPBR"));
	if (MaterialFinder.Succeeded())
	{
		MasterMaterial = MaterialFinder.Object;
	}
}

void APlanetoPBRGeneratorActor::BeginPlay()
{
	Super::BeginPlay();

	if (bGenerateOnBeginPlay)
	{
		Generate();
	}
}

void APlanetoPBRGeneratorActor::Generate()
{
	UE_LOG(LogPlanetoPBRGenerator, Display, TEXT("Generate requested. ImagePath='%s', Prompt='%s', MasterMaterial='%s'"),
		*ImagePath,
		*Prompt,
		*GetNameSafe(MasterMaterial));

	ActiveRequest = UPlanetoPBRRequest::GeneratePBRPlaneFromImage(this, ImagePath, Prompt, MasterMaterial);
	if (!ActiveRequest)
	{
		UE_LOG(LogPlanetoPBRGenerator, Error, TEXT("Failed to create PlaneToPBR request."));
		return;
	}

	ActiveRequest->OnCompleted.AddDynamic(this, &APlanetoPBRGeneratorActor::HandleCompleted);
	ActiveRequest->OnFailed.AddDynamic(this, &APlanetoPBRGeneratorActor::HandleFailed);
}

void APlanetoPBRGeneratorActor::HandleCompleted(const FPlanetoPBRTextureSet& Textures, APlanetoPBRPlaneActor* PlaneActor)
{
	UE_LOG(LogPlanetoPBRGenerator, Display, TEXT("Generation completed. PlaneActor='%s', Diffuse='%s'"),
		*GetNameSafe(PlaneActor),
		*Textures.DiffusePath);
	ActiveRequest = nullptr;
}

void APlanetoPBRGeneratorActor::HandleFailed(const FString& ErrorMessage)
{
	UE_LOG(LogPlanetoPBRGenerator, Error, TEXT("Generation failed: %s"), *ErrorMessage);
	ActiveRequest = nullptr;
}
