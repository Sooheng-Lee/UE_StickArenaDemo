// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/SABaseCharacter.h"
#include "AnimInstances/SABaseAnimInstance.h"
#include "TimerManager.h"
#include "Items/SACRWeapon.h"
// Sets default values
ASABaseCharacter::ASABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

}


void ASABaseCharacter::SetWeaponType(EWeaponType Type)
{
	this->WeaponType = Type;
}

void ASABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	AnimInstance = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance());
}

void ASABaseCharacter::Attack()
{
	// Remote characters assign their mesh/AnimInstance after construction.
	AnimInstance = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance());
	if (!AnimInstance) return;
	AnimInstance->PlayAttackMontage(WeaponType);
}

FVector2D ASABaseCharacter::GetMoveVector() const
{
	const FVector LocalVelocity = GetActorRotation().UnrotateVector(GetVelocity());
	return FVector2D(LocalVelocity.Y, LocalVelocity.X);
}

void ASABaseCharacter::GetDamage(AActor* Attacker)
{
	if (CharacterState == ECharacterState::Dead) return;
	if (IsValid(Attacker) && Attacker != this)
	{
		const FVector Direction = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!Direction.IsNearlyZero())
		{
			FRotator FacingRotation = GetActorRotation();
			FacingRotation.Yaw = Direction.Rotation().Yaw;
			SetActorRotation(FacingRotation);
		}
	}
	SetCharacterState(ECharacterState::Damaged);
	if (ASACRWeapon* Weapon = Cast<ASACRWeapon>(GetWeapon()))
	{
		Weapon->DeactivateHitCollision();
	}
	GetWorldTimerManager().SetTimer(DamageRecoveryTimerHandle, this,
		&ASABaseCharacter::FinishDamageRecovery, 0.85f, false);
}

void ASABaseCharacter::FinishDamageRecovery()
{
	if (CharacterState == ECharacterState::Damaged)
	{
		SetCharacterState(ECharacterState::Idle);
	}
}
