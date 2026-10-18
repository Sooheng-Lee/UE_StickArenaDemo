#pragma once

#include "CoreMinimal.h"
#include "LevelObjects/SALevelObject.h"
#include "SATreasureBox.generated.h"

UCLASS()
class UE_STICKARENADEMO_API ASATreasureBox : public ASALevelObject
{
	GENERATED_BODY()

public:
	ASATreasureBox();
	virtual void PlayHitEffect() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Treasure Shake", meta=(ClampMin="0.1"))
	float Duration = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Treasure Shake", meta=(ClampMin="0.0", ClampMax="30.0"))
	float AngleDegrees = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Treasure Shake", meta=(ClampMin="1.0"))
	float Oscillations = 3.0f;

private:
	FQuat RestRotation = FQuat::Identity;
	float Elapsed = 0.0f;
	bool bPlaying = false;
};
