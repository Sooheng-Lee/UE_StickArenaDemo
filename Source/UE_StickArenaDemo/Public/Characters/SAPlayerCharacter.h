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
UCLASS()
class UE_STICKARENADEMO_API ASAPlayerCharacter : public ASABaseCharacter
{
	GENERATED_BODY()
public:
	ASAPlayerCharacter();
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;
protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* CameraBoom;
	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	UCameraComponent* FollowCamera;
	UPROPERTY(EditDefaultsOnly, Category="Input Config", meta=(AllowPrivateAccess="true"))
	UDataAsset_InputConfigs* InputConfig;
	
public:
	void Movement(const FInputActionValue& Value);

private:
	UPROPERTY(VisibleInstanceOnly)
	APlayerController* PC;
	UPROPERTY(VisibleInstanceOnly)
	FVector LookVector;
	UPROPERTY(VisibleInstanceOnly)
	FVector2D LookDirection;
private:
	void RotateToMouse(float DeltaTime);
};
