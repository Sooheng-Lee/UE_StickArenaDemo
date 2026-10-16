// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SABaseAnimInstance.generated.h"

class UCharacterMovementComponent;
class ASABaseCharacter;
UCLASS()
class UE_STICKARENADEMO_API USABaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	ASABaseCharacter* OwningCharacter;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	UCharacterMovementComponent* OwningMovementComponent;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	FVector2D WalkSpeed;
};
