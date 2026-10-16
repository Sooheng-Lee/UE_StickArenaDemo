// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/SABaseAnimInstance.h"
#include "Characters/SABaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
void USABaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwningCharacter = Cast<ASABaseCharacter>(TryGetPawnOwner());
	if (OwningCharacter) {
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
	}
	MaxAttackCount = AttackNames.Num();
	AttackCount = 0;
}

void USABaseAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
}

void USABaseAnimInstance::PlayAttackMontage()
{
	if (Montage_IsPlaying(PunchMontage))
	{
		Montage_Play(PunchMontage);
		Montage_JumpToSection(AttackNames[AttackCount], PunchMontage);
		AttackCount = (AttackCount + 1) % MaxAttackCount;
	}
	else
	{
		AttackCount = 0;
		Montage_Play(PunchMontage);
		Montage_JumpToSection(AttackNames[AttackCount], PunchMontage);
	}
}
