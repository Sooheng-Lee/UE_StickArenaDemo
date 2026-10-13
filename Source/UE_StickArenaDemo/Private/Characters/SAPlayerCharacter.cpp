// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/SAPlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "SADebugHelper.h"
ASAPlayerCharacter::ASAPlayerCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Boom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 600.0f;
	CameraBoom->SetRelativeRotation(FRotator(60.0f, 0.0f, 0.0f));
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Follow Camera"));
	FollowCamera->SetupAttachment(CameraBoom);
	CameraBoom->bUsePawnControlRotation = false;
	FollowCamera->bUsePawnControlRotation = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ASAPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	SADebugHelper::Print(TEXT("ASAPlayerCharacter::BeginPlay()"));
}
