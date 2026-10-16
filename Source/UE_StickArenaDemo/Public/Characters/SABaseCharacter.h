// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SABaseCharacter.generated.h"

class USABaseAnimInstance;
UCLASS()
class UE_STICKARENADEMO_API ASABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASABaseCharacter();

protected:
	virtual void BeginPlay() override;
	void Attack();

private:
	USABaseAnimInstance* AnimInstance;
};
