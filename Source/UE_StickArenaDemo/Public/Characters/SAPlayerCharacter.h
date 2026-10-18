// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/SABaseCharacter.h"
#include "InputActionValue.h"
#include "SAPlayerCharacter.generated.h"

/**
 * 
 */
class USpringArmComponent;
class UCameraComponent;
class UDataAsset_InputConfigs;
class USAUdpClientComponent;
UCLASS()
class UE_STICKARENADEMO_API ASAPlayerCharacter : public ASABaseCharacter
{
	GENERATED_BODY()
public:
	ASAPlayerCharacter();
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;
	virtual FVector2D GetMoveVector() const override;
protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Networking")
	USAUdpClientComponent* UdpClient;

	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* CameraBoom;
	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	UCameraComponent* FollowCamera;
	// Exponential response rate, separate from CharacterMovement's degrees-per-second rotation rate.
	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="0.1"))
	float MouseFacingInterpSpeed = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category="Input Config", meta=(AllowPrivateAccess="true"))
	UDataAsset_InputConfigs* InputConfig;

	// Direction changes below this angle keep their momentum.
	UPROPERTY(EditDefaultsOnly, Category="Movement|Turning", meta=(ClampMin="1.0", ClampMax="180.0"))
	float TurnBrakeAngle = 60.0f;

	// Speed retained on a complete reversal; smaller turns retain more speed.
	UPROPERTY(EditDefaultsOnly, Category="Movement|Turning", meta=(ClampMin="0.0", ClampMax="0.9"))
	float TurnRetainedSpeedRatio = 0.25f;
	
public:
	void Movement(const FInputActionValue& Value);
	void StopMovementInput(const FInputActionValue& Value);
	void Attack(const FInputActionValue& Value);

private:
	UPROPERTY(VisibleInstanceOnly)
	APlayerController* PC;
	FVector LastMovementInputDirection = FVector::ZeroVector;
	float TurnBrakeReleaseSpeed = 0.0f;
	bool bBrakingForTurn = false;
private:
	void RotateToMouse(float DeltaTime);
};
