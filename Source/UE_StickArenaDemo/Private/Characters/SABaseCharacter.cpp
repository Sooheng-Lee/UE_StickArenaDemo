// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/SABaseCharacter.h"
#include "AnimInstances/SABaseAnimInstance.h"
// Sets default values
ASABaseCharacter::ASABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

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
	AnimInstance->PlayAttackMontage(EWeaponType::Weapon);
}

FVector2D ASABaseCharacter::GetMoveVector() const
{
	const FVector LocalVelocity = GetActorRotation().UnrotateVector(GetVelocity());
	return FVector2D(LocalVelocity.Y, LocalVelocity.X);
}
