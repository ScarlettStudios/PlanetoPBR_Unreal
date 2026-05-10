// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/IHttpRequest.h"
#include "PlanetoPBRTypes.h"
#include "PlanetoPBRRequest.generated.h"

class APlanetoPBRPlaneActor;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlanetoPBRGenerationCompleted, const FPlanetoPBRTextureSet&, Textures, APlanetoPBRPlaneActor*, PlaneActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlanetoPBRGenerationFailed, const FString&, ErrorMessage);

UCLASS(BlueprintType)
class PLANETOPBR_UNREAL_API UPlanetoPBRRequest : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "PlaneToPBR")
	FPlanetoPBRGenerationCompleted OnCompleted;

	UPROPERTY(BlueprintAssignable, Category = "PlaneToPBR")
	FPlanetoPBRGenerationFailed OnFailed;

	UFUNCTION(BlueprintCallable, Category = "PlaneToPBR", meta = (WorldContext = "WorldContextObject"))
	static UPlanetoPBRRequest* GeneratePBRPlaneFromImage(UObject* WorldContextObject, const FString& ImagePath, const FString& Prompt, UMaterialInterface* MasterMaterial);

private:
	static constexpr int32 RequestTimeoutSeconds = 120;

	static const FString SpaceBaseUrl;

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MasterMaterial;

	FString SourceImagePath;
	FString SourceFileName;
	FString PromptText;
	FString SessionHash;
	FString UploadedPath;
	FString OutputDirectory;
	FString Timestamp;
	TArray<uint8> SourceImageBytes;
	int32 FnIndex = INDEX_NONE;
	FPlanetoPBRTextureSet TextureSet;
	TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> ActiveRequests;

	void Start();
	void ResolveFunctionIndex();
	void UploadSourceImage();
	void JoinQueue();
	void PollQueue();
	void DownloadResult(const FString& Url, const FString& FilePath, bool bRequireDecodedTexture, TFunction<void(UTexture2D*)> AssignTexture, TFunction<void()> Continue);
	void Finish();
	void Fail(const FString& ErrorMessage);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateRequest(const FString& Url, const FString& Verb);
	bool DecodeImageFile(const FString& FilePath, UTexture2D*& OutTexture, int32* OutWidth = nullptr, int32* OutHeight = nullptr) const;
	FString BuildOutputPath(const FString& Prefix) const;
	FString BuildOutputPathForUrl(const FString& Prefix, const FString& Url) const;
};
