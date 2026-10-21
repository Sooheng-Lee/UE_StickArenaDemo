#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SASpawnMaterialLibrary.generated.h"

class UMaterial;

UCLASS()
class UE_STICKARENADEMO_API USASpawnMaterialLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Imported materials can retain constant inputs even after Python connects a graph. */
	UFUNCTION(BlueprintCallable, Category="Character|Editor")
	static void EnableDissolveInputs(UMaterial* Material);
};
