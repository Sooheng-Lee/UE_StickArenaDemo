// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/SABaseAnimInstance.h"
#include "SAPlayerAnimInstance.generated.h"

UCLASS()
class UE_STICKARENADEMO_API USAPlayerAnimInstance : public USABaseAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// Larger values make locomotion react faster; zero applies the target immediately.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Locomotion", meta=(ClampMin="0.0"))
	float LocomotionInterpSpeed = 12.0f;

};
