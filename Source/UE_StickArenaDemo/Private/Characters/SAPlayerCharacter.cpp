// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/SAPlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/SAInputComponent.h"
#include "GamePlay/SAGameplayTags.h"
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

void ASAPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController == nullptr || InputConfig==nullptr) return;
	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (LocalPlayer == nullptr) return;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (InputSubsystem == nullptr) return;
	USAInputComponent* SAInputComp = Cast<USAInputComponent>(PlayerInputComponent);
	if (SAInputComp)
	{
		
		InputSubsystem->AddMappingContext(InputConfig->GetInputMappng(), 0);
		SAInputComp->BindWithTag(SAGameplayTags::InputTag_Movement, InputConfig, ETriggerEvent::Triggered, this, &ASAPlayerCharacter::Movement);
	}
}

void ASAPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ASAPlayerCharacter::Movement(const FInputActionValue& Value)
{
	FVector2D InputVector = Value.Get<FVector2D>();
	const FString InputVectorStr = FString::Printf(TEXT("Input Vector : %s"), *InputVector.ToString());
	SADebugHelper::Print(InputVectorStr, FColor::Green);
}
