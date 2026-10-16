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

void USAPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	// Read the character on the game thread, before parallel animation evaluation.
	if (!OwningPlayerCharacter)
	{
		OwningPlayerCharacter = Cast<ASAPlayerCharacter>(TryGetPawnOwner());
	}
	const FVector2D TargetSpeed = OwningPlayerCharacter
		? OwningPlayerCharacter->GetMoveVector() : FVector2D::ZeroVector;
	const float Alpha = LocomotionInterpSpeed > 0.0f
		? 1.0f - FMath::Exp(-LocomotionInterpSpeed * FMath::Max(DeltaSeconds, 0.0f))
		: 1.0f;
	WalkSpeed = FMath::Lerp(WalkSpeed, TargetSpeed, Alpha);
	if (TargetSpeed.IsNearlyZero() && WalkSpeed.SizeSquared() < 1.0f)
	{
		WalkSpeed = FVector2D::ZeroVector;
	}
}
