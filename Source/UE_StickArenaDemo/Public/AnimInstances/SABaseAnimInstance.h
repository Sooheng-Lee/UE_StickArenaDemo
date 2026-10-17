// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../UE_StickArenaDemo.h"
#include "Animation/AnimInstance.h"
#include "SABaseAnimInstance.generated.h"

class UCharacterMovementComponent;
class ASABaseCharacter;
UCLASS()
class UE_STICKARENADEMO_API USABaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	void PlayAttackMontage(EWeaponType WeaponType);

protected:
	UFUNCTION()
	void OnNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);
	void SetAttackEnabled();

private:
	void PlayPunch();
	void PlaySword();
protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	ASABaseCharacter* OwningCharacter;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	UCharacterMovementComponent* OwningMovementComponent;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	FVector2D WalkSpeed;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AnimMontage")
	UAnimMontage* PunchMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AnimMontage")
	UAnimMontage* SwordMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AnimMontage")
	TArray<FName> AttackNames;
	UINT AttackCount;
	UINT MaxAttackCount;
	bool bAttacking = false;
};
