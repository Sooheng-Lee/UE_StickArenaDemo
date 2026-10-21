// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../UE_StickArenaDemo.h"
#include "GameFramework/Character.h"
#include "SABaseCharacter.generated.h"

class USABaseAnimInstance;
class ASAWeaponActor;
class USAStatusComponent;
class UNiagaraSystem;
UCLASS()
class UE_STICKARENADEMO_API ASABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASABaseCharacter();
	UFUNCTION(BlueprintPure, Category="Status")
	USAStatusComponent* GetStatusComponent() const { return StatusComponent; }
	virtual FVector2D GetMoveVector() const;
	virtual bool IsNetworkProxy() const { return false; }
	virtual bool HasSynchronizedHealth() const { return false; }
	virtual void ApplyDamageKnockback(const FVector& VelocityChange);
	void RestoreFromDamaged();
	UFUNCTION(BlueprintCallable, Category="Combat")
	virtual void TakeDamage(AActor* Attacker = nullptr, UNiagaraSystem* DamagedEffect = nullptr, FVector ImpactPoint = FVector::ZeroVector);
	UFUNCTION(BlueprintCallable)
	ECharacterState GetCharacterState() const { return CharacterState; }
	void SetCharacterState(ECharacterState State) { CharacterState = State; }
	EWeaponType GetWeaponType() const { return WeaponType; }
	void SetWeaponType(EWeaponType Type);
	ASAWeaponActor* GetWeapon() const { return OwnedWeapon.Get(); }
	void SetWeapon(ASAWeaponActor* Weapon) { OwnedWeapon = Weapon; };
	UNiagaraSystem* GetHitEffect() const { return HitEffect.Get(); };
protected:
	virtual void BeginPlay() override;
	void Attack();

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	ECharacterState CharacterState = ECharacterState::Idle;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	EWeaponType WeaponType = EWeaponType::None;
	TObjectPtr<ASAWeaponActor> OwnedWeapon = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status")
	TObjectPtr<USAStatusComponent> StatusComponent;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	TObjectPtr<UNiagaraSystem> HitEffect;


private:
	FTimerHandle DamageRecoveryTimerHandle;
	void FinishDamageRecovery();

	UPROPERTY(Transient)
	USABaseAnimInstance* AnimInstance;
};
