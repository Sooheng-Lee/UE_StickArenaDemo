// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/SAPlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/SAInputComponent.h"
#include "Components/SAUdpClientComponent.h"
#include "GamePlay/SAGameplayTags.h"
#include "SADebugHelper.h"
#include "Widget/SAPlayerWidget.h"
#include "GameFramework/PlayerController.h"
#include "Items/SAWeaponActor.h"
ASAPlayerCharacter::ASAPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PlayerWidgetClass = USAPlayerWidget::StaticClass();
	UdpClient = CreateDefaultSubobject<USAUdpClientComponent>(TEXT("UDP Client"));
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
	GetCharacterMovement()->MaxAcceleration = 1000.0f;
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->BrakingFriction = 0.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1400.0f;
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
		SAInputComp->BindWithTag(SAGameplayTags::InputTag_Movement, InputConfig, ETriggerEvent::Completed, this, &ASAPlayerCharacter::StopMovementInput);
		SAInputComp->BindWithTag(SAGameplayTags::InputTag_Movement, InputConfig, ETriggerEvent::Canceled, this, &ASAPlayerCharacter::StopMovementInput);
		SAInputComp->BindWithTag(SAGameplayTags::InputTag_Attack, InputConfig, ETriggerEvent::Triggered, this, &ASAPlayerCharacter::Attack);
	}
}

void ASAPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateMovementState();
	RotateToMouse(DeltaTime);
}

bool ASAPlayerCharacter::IsMovementBlocked() const
{
	return GetCharacterState() == ECharacterState::Dead || GetCharacterState() == ECharacterState::Damaged
		|| (UdpClient && UdpClient->IsMatchExpired());
}

void ASAPlayerCharacter::UpdateMovementState()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const ASAWeaponActor* Weapon = GetWeapon();
	MovementComponent->MaxWalkSpeed = GetCharacterState() == ECharacterState::Attack && IsValid(Weapon)
		? FMath::Max(0.0f, Weapon->GetWalkSpeed()) : DefaultWalkSpeed;
	if (IsMovementBlocked())
	{
		StopMovementInput(FInputActionValue());
		ConsumeMovementInputVector();
		MovementComponent->StopMovementImmediately();
		MovementComponent->ClearAccumulatedForces();
	}
}

FVector2D ASAPlayerCharacter::GetMoveVector() const
{
	const FVector LocalVelocity = GetActorRotation().UnrotateVector(GetVelocity());
	return FVector2D(LocalVelocity.Y, LocalVelocity.X);
}

void ASAPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	PC = Cast<APlayerController>(GetController());
	if (PC) {
		PC->bShowMouseCursor = true;
	}
	RefreshPlayerWidget();
}

void ASAPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	RefreshPlayerWidget();
}

void ASAPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	if (HasActorBegunPlay()) RefreshPlayerWidget();
}

void ASAPlayerCharacter::RefreshPlayerWidget()
{
	PC = Cast<APlayerController>(GetController());
	if (PlayerWidget && (!PC || PlayerWidget->GetOwningPlayer() != PC || !IsLocallyControlled()))
	{
		PlayerWidget->RemoveFromParent();
		PlayerWidget = nullptr;
	}
	if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer() || !IsLocallyControlled() || !PlayerWidgetClass) return;
	PC->bShowMouseCursor = true;
	if (!PlayerWidget)
	{
		PlayerWidget = CreateWidget<USAPlayerWidget>(PC, PlayerWidgetClass);
		if (PlayerWidget)
		{
			PlayerWidget->SetStatusComponent(GetStatusComponent());
			PlayerWidget->SetMatchClient(UdpClient);
			PlayerWidget->AddToPlayerScreen();
		}
	}
}

void ASAPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PlayerWidget)
	{
		PlayerWidget->RemoveFromParent();
		PlayerWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ASAPlayerCharacter::Movement(const FInputActionValue& Value)
{
	UpdateMovementState();
	if (IsMovementBlocked()) return;

	const FVector InputVector = Value.Get<FVector>();
	const FVector MoveDir = FVector(InputVector.X, InputVector.Y, 0).GetSafeNormal();
	if (MoveDir.IsNearlyZero())
	{
		StopMovementInput(Value);
		return;
	}

	UdpClient->SetMovementInput(MoveDir);
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const float Speed = GetVelocity().Size2D();
	const FVector VelocityDirection = GetVelocity().GetSafeNormal2D();
	const bool bNewDirection = !MoveDir.Equals(LastMovementInputDirection, 0.01f);
	const float DirectionDot = FMath::Clamp(FVector::DotProduct(VelocityDirection, MoveDir), -1.0f, 1.0f);
	const float BrakeThreshold = FMath::Cos(FMath::DegreesToRadians(TurnBrakeAngle));
	if (MovementComponent->IsMovingOnGround() && bNewDirection && Speed > 80.0f && DirectionDot <= BrakeThreshold)
	{
		// Brake along the current velocity first. Only then accelerate into the new input.
		const float TurnAmount = (1.0f - DirectionDot) * 0.5f;
		const float ReleaseSpeed = Speed * FMath::Lerp(1.0f, TurnRetainedSpeedRatio, TurnAmount);
		TurnBrakeReleaseSpeed = bBrakingForTurn ? FMath::Min(TurnBrakeReleaseSpeed, ReleaseSpeed) : ReleaseSpeed;
		bBrakingForTurn = true;
	}
	LastMovementInputDirection = MoveDir;
	if (bBrakingForTurn)
	{
		// Returning to the current travel direction cancels the turn immediately.
		if (!MovementComponent->IsMovingOnGround() || Speed <= TurnBrakeReleaseSpeed || DirectionDot > 0.95f)
		{
			bBrakingForTurn = false;
		}
		else
		{
			return;
		}
	}
	AddMovementInput(MoveDir, 1.0f);
}

void ASAPlayerCharacter::StopMovementInput(const FInputActionValue& Value)
{
	UdpClient->SetMovementInput(FVector::ZeroVector);
	LastMovementInputDirection = FVector::ZeroVector;
	bBrakingForTurn = false;
	TurnBrakeReleaseSpeed = 0.0f;
}

void ASAPlayerCharacter::Attack(const FInputActionValue& Value)
{
	UdpClient->SendAction(TEXT("ATTACK"));
	Super::Attack();
}

void ASAPlayerCharacter::RotateToMouse(float DeltaTime)
{
	if (IsMovementBlocked()) return;
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
	const float RotationAlpha = 1.0f - FMath::Exp(-MouseFacingInterpSpeed * FMath::Max(DeltaTime, 0.0f));
	const FRotator SmoothRotation = FQuat::Slerp(
		GetActorRotation().Quaternion(), TargetRotation.Quaternion(), RotationAlpha).Rotator();
	SetActorRotation(FRotator(0.0f, SmoothRotation.Yaw, 0.0f));
}
