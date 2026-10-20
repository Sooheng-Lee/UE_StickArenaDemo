// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/SABaseAnimInstance.h"
#include "Characters/SABaseCharacter.h"
#include "LevelObjects/SALevelObject.h"
#include "LevelObjects/SATreasureBox.h"
#include "SADebugHelper.h"
#include "DrawDebugHelpers.h"
#include "Items/SACRWeapon.h"
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
	if (!IsValid(OwningCharacter) || !Montage || AttackNames.IsEmpty()) return;
	MaxAttackCount = AttackNames.Num();
	if (OwningCharacter->GetCharacterState() != ECharacterState::Idle) return;
	OwningCharacter->SetCharacterState(ECharacterState::Attack);
	switch (WeaponType)
	{
		case EWeaponType::None:
			PlayPunch();
			break;
		case EWeaponType::Sword:
			PlaySword();
			break;
	}	
}

void USABaseAnimInstance::PlayDamageAnim(float Damage)
{
	CumulativeDamage += Damage;
	const bool bStrongDamage = CumulativeDamage >= 15.0f;

	if (DamagedMontage)
	{
		Montage_Play(DamagedMontage);
		Montage_JumpToSection(bStrongDamage ? FName("StrongDamage") : FName("WeakDamage"), DamagedMontage);
	}

	if (IsValid(OwningCharacter))
	{
		const FVector Backward = -OwningCharacter->GetActorForwardVector().GetSafeNormal2D();
		const float KnockbackStrength = bStrongDamage ? StrongKnockbackStrength : WeakKnockbackStrength;
		OwningCharacter->ApplyDamageKnockback(Backward * KnockbackStrength);
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

void USABaseAnimInstance::InitCumulativeDamage()
{
	CumulativeDamage = 0;
	OwningCharacter->RestoreFromDamaged();
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
	else if (NotifyName == TEXT("ActivateHitComponent"))
	{
		if (!IsValid(OwningCharacter) || OwningCharacter->GetCharacterState() != ECharacterState::Attack) return;
		ASACRWeapon* Weapon = Cast<ASACRWeapon>(OwningCharacter->GetWeapon());
		if (!Weapon) return;
		Weapon->ActivateHitCollision();
	}
	else if (NotifyName == TEXT("InitCumulativeDamage"))
	{
		if (!IsValid(OwningCharacter)) return;
		InitCumulativeDamage();
	}
}

void USABaseAnimInstance::SetAttackEnabled()
{
	if (IsValid(OwningCharacter))
	{
		if (ASACRWeapon* Weapon = Cast<ASACRWeapon>(OwningCharacter->GetWeapon()))
		{
			Weapon->DeactivateHitCollision();
		}
	}
	OwningCharacter->SetCharacterState(ECharacterState::Idle);
}

void USABaseAnimInstance::DetectPunchAttack()
{
	if (!IsValid(OwningCharacter) || !GetWorld()) return;
	if (OwningCharacter->GetCharacterState() == ECharacterState::Damaged
		|| OwningCharacter->GetCharacterState() == ECharacterState::Dead) return;
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
	TSet<ASALevelObject*> HitObjects;
	TSet<ASABaseCharacter*> HitCharacters;
	for (const FHitResult& HitResult : HitResults)
	{
		if (ASABaseCharacter* HitCharacter = Cast<ASABaseCharacter>(HitResult.GetActor()))
		{
			if (IsValid(HitCharacter) && HitCharacter != OwningCharacter && !HitCharacters.Contains(HitCharacter))
			{
				HitCharacters.Add(HitCharacter);
				SADebugHelper::Print(TEXT("HelloWorld"));
				HitCharacter->TakeDamage(OwningCharacter, OwningCharacter->GetHitEffect(), HitResult.ImpactPoint);
			}
		}
		else if (ASALevelObject* LevelObject = Cast<ASALevelObject>(HitResult.GetActor()))
		{
			if (!HitObjects.Contains(LevelObject))
			{
				HitObjects.Add(LevelObject);
				if (ASATreasureBox* Box = Cast<ASATreasureBox>(LevelObject)) Box->RequestHit(OwningCharacter);
                else LevelObject->PlayHitEffect();
			}
		}
		const FString DebugMsg = FString::Printf(TEXT("Hit Actor : %s"), *GetNameSafe(HitResult.GetActor()));
		SADebugHelper::Print(DebugMsg, FColor::Blue);
	}
}
