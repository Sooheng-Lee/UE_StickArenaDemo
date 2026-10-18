// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/SABaseAnimInstance.h"
#include "Characters/SABaseCharacter.h"
#include "SADebugHelper.h"
#include "DrawDebugHelpers.h"
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

void USABaseAnimInstance::PlayAttackMontage(EWeaponType WeaponType)
{
	const UAnimMontage* Montage = WeaponType == EWeaponType::None ? PunchMontage : SwordMontage;
	if (!Montage || AttackNames.IsEmpty()) return;
	MaxAttackCount = AttackNames.Num();
	if (bAttacking) return;
	bAttacking = true;
	
	switch (WeaponType)
	{
		case EWeaponType::None:
			PlayPunch();
			break;
		case EWeaponType::Weapon:
			PlaySword();
			break;
	}	
}

void USABaseAnimInstance::PlayPunch()
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

void USABaseAnimInstance::PlaySword()
{
	if (Montage_IsPlaying(SwordMontage))
	{
		Montage_Play(SwordMontage);
		Montage_JumpToSection(AttackNames[AttackCount], SwordMontage);
		AttackCount = (AttackCount + 1) % MaxAttackCount;
	}
	else
	{
		AttackCount = 0;
		Montage_Play(SwordMontage);
		Montage_JumpToSection(AttackNames[AttackCount], SwordMontage);
	}
}

void USABaseAnimInstance::OnNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	if(NotifyName == TEXT("AttackEnabled"))
	{
		SetAttackEnabled();
	}
	else if (NotifyName == TEXT("DetectPunch"))
	{
		DetectPunchAttack();
	}
}

void USABaseAnimInstance::SetAttackEnabled()
{
	bAttacking = false;
}

void USABaseAnimInstance::DetectPunchAttack()
{
	const FVector Forward = OwningCharacter->GetActorForwardVector();
	const FVector Start = OwningCharacter->GetActorLocation() + Forward * 5.0f;
	const FVector End = Start + Forward * 50.0f;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwningCharacter);
	Params.bTraceComplex = false;
	TArray<FHitResult> HitResults;
	GetWorld()->SweepMultiByChannel(
		HitResults,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(20.0f),
		Params
	);
	SADebugHelper::Print(TEXT("Sweep Test"), FColor::Blue);

	if (HitResults.Num() == 0)
	{
		DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f, 50.f, 20.0f, FRotationMatrix::MakeFromZ(Forward).ToQuat(), FColor::Red, false, 2.0f);
		return;
	}
	DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f, 50.0f, 20.0f, FRotationMatrix::MakeFromZ(Forward).ToQuat(), FColor::Green, false, 2.0f);
	for (FHitResult HitResult : HitResults)
	{
		const FString DebugMsg = FString::Printf(TEXT("Hit Actor : %s"), *HitResult.GetActor()->GetName());
		SADebugHelper::Print(DebugMsg, FColor::Blue);
	}
}
