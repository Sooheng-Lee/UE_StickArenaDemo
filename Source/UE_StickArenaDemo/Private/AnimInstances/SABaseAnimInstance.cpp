// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/SABaseAnimInstance.h"
#include "Characters/SABaseCharacter.h"
#include "SADebugHelper.h"
#include "GameFramework/CharacterMovementComponent.h"
void USABaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwningCharacter = Cast<ASABaseCharacter>(TryGetPawnOwner());
	if (OwningCharacter) {
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
	}
	OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &USABaseAnimInstance::OnNotifyBegin);
	MaxAttackCount = AttackNames.Num();
	AttackCount = 0;
}

void USABaseAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
}

void USABaseAnimInstance::PlayAttackMontage()
{
	if (bAttacking) return;
	bAttacking = true;
	if (Montage_IsPlaying(PunchMontage))
	{

		const FString DebugMsg = FString::Printf(TEXT("Attack Idx : %d"), AttackCount);
		SADebugHelper::Print(DebugMsg);
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

void USABaseAnimInstance::OnNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	if(NotifyName == TEXT("AttackEnabled"))
	{
		SetAttackEnabled();
	}
}

void USABaseAnimInstance::SetAttackEnabled()
{
	bAttacking = false;
}
