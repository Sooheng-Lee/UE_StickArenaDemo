// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/SABaseAnimInstance.h"
#include "SAPlayerAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class UE_STICKARENADEMO_API USAPlayerAnimInstance : public USABaseAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	
};
