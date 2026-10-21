// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/SABaseCharacter.h"
#include "Components/SAStatusComponent.h"
#include "AnimInstances/SABaseAnimInstance.h"
#include "TimerManager.h"
#include "Items/SACRWeapon.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
// Sets default values
ASABaseCharacter::ASABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	StatusComponent = CreateDefaultSubobject<USAStatusComponent>(TEXT("StatusComponent"));

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
	if (!AnimInstance || CharacterState != ECharacterState::Idle) return;
	AnimInstance->PlayAttackMontage(WeaponType);
	// Only consume a use when a weapon attack actually starts.
	if (IsNetworkProxy() || CharacterState != ECharacterState::Attack || WeaponType == EWeaponType::None || !StatusComponent) return;

	StatusComponent->Ammo = FMath::Max(0, StatusComponent->Ammo - 1);
	if (StatusComponent->Ammo == 0)
	{
		ASAWeaponActor* Weapon = GetWeapon();
		SetWeapon(nullptr);
		SetWeaponType(EWeaponType::None);
		if (IsValid(Weapon))
		{
			Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Weapon->Destroy();
		}
	}
}

void ASABaseCharacter::ApplyDamageKnockback(const FVector& VelocityChange)
{
	GetCharacterMovement()->AddImpulse(VelocityChange, true);
}

void ASABaseCharacter::RestoreFromDamaged()
{
	if (CharacterState == ECharacterState::Damaged) SetCharacterState(ECharacterState::Idle);
}

void ASABaseCharacter::TakeDamage(AActor* Attacker, UNiagaraSystem* DamagedEffect, FVector ImpactPoint)
{
	if (CharacterState == ECharacterState::Dead) return;
	if (!IsValid(Attacker) || Attacker == this) return;

	// Face the attacker before PlayDamageAnim applies its backward impulse.
	const FVector Direction = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		FRotator FacingRotation = GetActorRotation();
		FacingRotation.Yaw = Direction.Rotation().Yaw;
		SetActorRotation(FacingRotation);
	}

	if (const ASABaseCharacter* AttackingCharacter = Cast<ASABaseCharacter>(Attacker))
	{
		float TotalDamage = 0.0f;
		if (const USAStatusComponent* AttackerStatus = AttackingCharacter->GetStatusComponent())
		{
			TotalDamage += AttackerStatus->DefaultDamage;
		}
		if (const ASAWeaponActor* AttackerWeapon = AttackingCharacter->GetWeapon(); IsValid(AttackerWeapon))
		{
			TotalDamage += AttackerWeapon->GetDamage();
		}
		if (StatusComponent)
		{
			const float AppliedDamage = FMath::Max(1.0f, TotalDamage - FMath::Max(0.0f, StatusComponent->Defense));
			if (DamagedEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DamagedEffect, ImpactPoint, FRotator::ZeroRotator, FVector(0.1f), true, true);
			}
			// A remote proxy may predict a hit reaction, but only its owning client changes HP.
			if (!HasSynchronizedHealth())
				StatusComponent->HP = FMath::Max(0.0f, StatusComponent->HP - AppliedDamage);
			if (StatusComponent->HP > 0) {
				SetCharacterState(ECharacterState::Damaged);
				AnimInstance = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance());
				if (!IsValid(AnimInstance)) return;
				AnimInstance->PlayDamageAnim(AppliedDamage);
			}
			else
			{
				SetCharacterState(ECharacterState::Dead);
				AnimInstance = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance());
				if (!IsValid(AnimInstance)) return;
				AnimInstance->PlayDeadAnim(AppliedDamage);
			}
		
		}
	}

	if (ASACRWeapon* Weapon = Cast<ASACRWeapon>(GetWeapon()))
	{
		Weapon->DeactivateHitCollision();
	}
	GetWorldTimerManager().SetTimer(DamageRecoveryTimerHandle, this,
		&ASABaseCharacter::FinishDamageRecovery, 0.85f, false);
}

FVector2D ASABaseCharacter::GetMoveVector() const
{
	const FVector LocalVelocity = GetActorRotation().UnrotateVector(GetVelocity());
	return FVector2D(LocalVelocity.Y, LocalVelocity.X);
}

void ASABaseCharacter::FinishDamageRecovery()
{
	if (CharacterState == ECharacterState::Damaged)
	{
		SetCharacterState(ECharacterState::Idle);
	}
}
