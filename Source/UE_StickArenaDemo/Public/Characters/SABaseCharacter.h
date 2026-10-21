// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../UE_StickArenaDemo.h"
#include "GameFramework/Character.h"
#include "SABaseCharacter.generated.h"

class USABaseAnimInstance;
class ASAWeaponActor;
class USAStatusComponent;
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAnimMontage;
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
	virtual bool CanRespawnLocally() const { return true; }
	virtual void ApplyDamageKnockback(const FVector& VelocityChange);
	void RestoreFromDamaged();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	UFUNCTION(BlueprintCallable, Category="Character|Lifecycle")
	void BeginSpawning();
	UFUNCTION(BlueprintCallable, Category="Character|Lifecycle")
	void BeginDeathDissolve();
	UFUNCTION(BlueprintPure, Category="Character|Lifecycle")
	float GetLifecycleVisibility() const { return LifecycleVisibility; }
	void ApplyNetworkLifecycle(ECharacterState State, float Visibility);
	void RefreshLifecycleMaterials();
	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void WaitForDeathAnimation(float Duration);
	UFUNCTION(BlueprintCallable, Category="Character|Lifecycle")
	void Respawn();
	void DieInWater();
	UFUNCTION(BlueprintCallable, Category="Combat")
	virtual void TakeDamage(AActor* Attacker = nullptr, UNiagaraSystem* DamagedEffect = nullptr, FVector ImpactPoint = FVector::ZeroVector);
	UFUNCTION(BlueprintCallable)
	ECharacterState GetCharacterState() const { return CharacterState; }
	void SetCharacterState(ECharacterState State, float ReactionDamage = 0.0f);
	EWeaponType GetWeaponType() const { return WeaponType; }
	void SetWeaponType(EWeaponType Type);
	ASAWeaponActor* GetWeapon() const { return OwnedWeapon.Get(); }
	void SetWeapon(ASAWeaponActor* Weapon) { OwnedWeapon = Weapon; };
	UNiagaraSystem* GetHitEffect() const { return HitEffect.Get(); };
protected:
	virtual void BeginPlay() override;
	void ConfigureCharacterCapsule();
	virtual void OnDamageReceived() {}
	void Attack();

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	ECharacterState CharacterState = ECharacterState::Spawning;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character|Lifecycle", meta=(ClampMin="0.01"))
	float SpawnDuration = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character|Lifecycle", meta=(ClampMin="0.01"))
	float DeathDissolveDuration = 1.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character|Lifecycle", meta=(ClampMin="0.01"))
	float RespawnDelay = 5.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character|Lifecycle")
	TObjectPtr<UMaterialInterface> SpawnDissolveMaterial;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	EWeaponType WeaponType = EWeaponType::None;
	TObjectPtr<ASAWeaponActor> OwnedWeapon = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status")
	TObjectPtr<USAStatusComponent> StatusComponent;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	TObjectPtr<UNiagaraSystem> HitEffect;


private:
	friend class ASAEnemyCharacter;
	void ActivateLifecycleMaterials();
	void RestoreLifecycleMaterials();
	void SetLifecycleVisibility(float Visibility);
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LifecycleMaterials;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalOverlay;
	float LifecycleVisibility = 0.0f;
	bool bLifecycleMaterialsActive = false;
	bool bDeathDissolving = false;
	bool bNetworkLifecycleDriven = false;
	bool bRespawnScheduled = false;
	bool bWaterMovementLocked = false;
	FTimerHandle RespawnTimerHandle;
	FTimerHandle DeathAnimationTimerHandle;
	ECollisionEnabled::Type SavedCollision = ECollisionEnabled::QueryAndPhysics;
	FTimerHandle DamageRecoveryTimerHandle;
	void FinishDamageRecovery();

	UPROPERTY(Transient)
	USABaseAnimInstance* AnimInstance;
};
