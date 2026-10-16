// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/SAPlayerAnimInstance.h"
#include "Characters/SAPlayerCharacter.h"
#include "SADebugHelper.h"
void USAPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	if (OwningCharacter) {
		OwningPlayerCharacter = Cast<ASAPlayerCharacter>(OwningCharacter);
	}
}

void USAPlayerAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
}
