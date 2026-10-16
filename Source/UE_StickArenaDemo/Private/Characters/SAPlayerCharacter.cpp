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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Boom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 600.0f;
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Follow Camera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->bUsePawnControlRotation = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;
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

void ASAPlayerCharacter::Tick(float DeltaTime)
{
	RotateToMouse(DeltaTime);
}

void ASAPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	PC = Cast<APlayerController>(GetController());
	if (PC) {
		PC->bShowMouseCursor = true;
	}
}

void ASAPlayerCharacter::Movement(const FInputActionValue& Value)
{
	FVector InputVector = Value.Get<FVector>();
	FRotator ActorRotation = GetActorRotation();
	ActorRotation.Yaw = FMath::RoundToFloat(ActorRotation.Yaw / 45.f) * 45.f;
	const FVector ForwardVector = ActorRotation.RotateVector(FVector::ForwardVector);
	const FVector RightVector = ActorRotation.RotateVector(FVector::RightVector);
	float ForwardDotProductVal = FVector::DotProduct(InputVector, ForwardVector);
	float RightDotProductVal = FVector::DotProduct(InputVector, RightVector);
	const FString DebugMsg = FString::Printf(TEXT("Look Direction : (%f, %f)"), ForwardDotProductVal, RightDotProductVal);
	SADebugHelper::Print(DebugMsg, FColor::Blue);
	FVector MoveDir = FVector(InputVector.X, InputVector.Y, 0).GetSafeNormal();
	AddMovementInput(MoveDir, 1.0f);
}

void ASAPlayerCharacter::RotateToMouse(float DeltaTime)
{
	if (!PC) return;
	FHitResult HitResult;
	const bool bHit = PC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
	if (!bHit) return;
	const FVector ActorLocation = GetActorLocation();
	const FVector MouseLocation = HitResult.ImpactPoint;

	FVector Direction = MouseLocation - ActorLocation;
	Direction.Z = 0.0f;

	if (Direction.IsNearlyZero()) return;

	FRotator TargetRotation = Direction.Rotation();
	const FRotator SmoothRotation = FMath::RInterpTo(
		GetActorRotation(),
		TargetRotation,
		DeltaTime,
		GetCharacterMovement()->RotationRate.Yaw
	);
	SetActorRotation(FRotator(0.0f, SmoothRotation.Yaw, 0.0f));
	TargetRotation.Yaw = FMath::RoundToFloat(TargetRotation.Yaw /45.0) * 45.0f;
	LookVector = TargetRotation.Vector().GetSafeNormal();
}
