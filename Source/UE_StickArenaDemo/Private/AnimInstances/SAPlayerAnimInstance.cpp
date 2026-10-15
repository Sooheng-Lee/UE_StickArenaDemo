// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/SAPlayerAnimInstance.h"
#include "SADebugHelper.h"
void USAPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void USAPlayerAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	const FString& WalkSpeedStr = FString::Printf(TEXT("Walk Speed : %f"), WalkSpeed);
	SADebugHelper::Print(WalkSpeedStr, FColor::Green);
}
