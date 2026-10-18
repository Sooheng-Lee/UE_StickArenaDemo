#pragma once

#include "CoreMinimal.h"
#include "LevelObjects/SALevelObject.h"
#include "SACrystalTower.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;

UCLASS()
class UE_STICKARENADEMO_API ASACrystalTower : public ASALevelObject
{
	GENERATED_BODY()

public:
	ASACrystalTower();
	virtual void PlayHitEffect() override;

	UFUNCTION(BlueprintCallable, Category="Crystal Effect")
	virtual void PlayEffect();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crystal Effect")
	TObjectPtr<UPointLightComponent> CrystalLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crystal Effect", meta=(ClampMin="0.1"))
	float EffectDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crystal Effect", meta=(ClampMin="0.0"))
	float GlowStrength = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crystal Effect", meta=(ClampMin="0.0"))
	float LightIntensity = 500.0f;

private:
	void SetEffectStrength(float Strength);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;

	float Elapsed = 0.0f;
	bool bPlaying = false;
};
