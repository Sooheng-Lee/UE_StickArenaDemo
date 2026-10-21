#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SAMinimapData.generated.h"

class UTexture2D;

/** The baked image and its orthographic projection must always be updated together. */
UCLASS(BlueprintType)
class UE_STICKARENADEMO_API USAMinimapData : public UDataAsset
{
	GENERATED_BODY()
public:
	// Editor bake helper: Python commandlets must wait for mesh, texture and shader jobs.
	UFUNCTION(BlueprintCallable, Category="Minimap|Editor")
	static void PrepareForCapture(UWorld* World);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minimap")
	TObjectPtr<UTexture2D> Texture;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minimap")
	TSoftObjectPtr<UWorld> SourceMap;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minimap")
	FVector2D WorldCenter = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minimap", meta=(ClampMin="1.0"))
	float WorldSize = 12000.0f;
};
