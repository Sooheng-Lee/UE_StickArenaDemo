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
	if (!AnimInstance) return;
	AnimInstance->PlayAttackMontage();
}