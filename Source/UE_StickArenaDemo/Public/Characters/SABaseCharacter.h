// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../UE_StickArenaDemo.h"
#include "GameFramework/Character.h"
#include "SABaseCharacter.generated.h"

class USABaseAnimInstance;
class ASAWeaponActor;
UCLASS()
class UE_STICKARENADEMO_API ASABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASABaseCharacter();
	virtual FVector2D GetMoveVector() const;
	UFUNCTION(BlueprintCallable, Category="Combat")
	virtual void GetDamage(AActor* Attacker = nullptr);
	UFUNCTION(BlueprintCallable)
	ECharacterState GetCharacterState() const { return CharacterState; }
	void SetCharacterState(ECharacterState State) { CharacterState = State; }
	EWeaponType GetWeaponType() const { return WeaponType; }
	void SetWeaponType(EWeaponType Type);
	ASAWeaponActor* GetWeapon() const { return OwnedWeapon.Get(); }
	void SetWeapon(ASAWeaponActor* Weapon) { OwnedWeapon = Weapon; };
protected:
	virtual void BeginPlay() override;
	void Attack();

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	ECharacterState CharacterState = ECharacterState::Idle;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	EWeaponType WeaponType = EWeaponType::None;
	TObjectPtr<ASAWeaponActor> OwnedWeapon = nullptr;

private:
	FTimerHandle DamageRecoveryTimerHandle;
	void FinishDamageRecovery();

	UPROPERTY(Transient)
	USABaseAnimInstance* AnimInstance;
};
