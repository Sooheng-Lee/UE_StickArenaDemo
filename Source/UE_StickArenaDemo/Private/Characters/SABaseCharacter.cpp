// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/SABaseCharacter.h"
#include "Components/SAStatusComponent.h"
#include "AnimInstances/SABaseAnimInstance.h"
#include "TimerManager.h"
#include "Items/SACRWeapon.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GameModes/SAGameModeBase.h"
// Sets default values
ASABaseCharacter::ASABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	StatusComponent = CreateDefaultSubobject<USAStatusComponent>(TEXT("StatusComponent"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SpawnMaterial(
		TEXT("/Game/PlayerCharacters/Materials/MI_CharacterSpawn.MI_CharacterSpawn"));
	SpawnDissolveMaterial = SpawnMaterial.Object;

}


void ASABaseCharacter::SetWeaponType(EWeaponType Type)
{
	this->WeaponType = Type;
}

void ASABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	AnimInstance = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance());
	SavedCollision = GetCapsuleComponent()->GetCollisionEnabled();
	BeginSpawning();
}

void ASABaseCharacter::BeginSpawning()
{
	RestoreLifecycleMaterials();
	bNetworkLifecycleDriven = false;
	bDeathDissolving = false;
	bRespawnScheduled = false;
	CharacterState = ECharacterState::Spawning;
	GetWorldTimerManager().ClearTimer(DamageRecoveryTimerHandle);
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorldTimerManager().ClearTimer(DeathAnimationTimerHandle);
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->StopAllMontages(0.0f);
	if (USABaseAnimInstance* Anim = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance())) Anim->ResetDamageAccumulator();
	if (StatusComponent && StatusComponent->HP <= 0.0f) StatusComponent->HP = StatusComponent->MaxHP;
	GetMesh()->SetVisibility(true, true);
	GetCapsuleComponent()->SetCollisionEnabled(SavedCollision);
	ActivateLifecycleMaterials();
	SetLifecycleVisibility(0.0f);
}

void ASABaseCharacter::Respawn()
{
	if (CharacterState != ECharacterState::Dead || !CanRespawnLocally()) return;
	if (ASAGameModeBase* Mode = GetWorld()->GetAuthGameMode<ASAGameModeBase>())
	{
		FTransform SpawnTransform;
		if (Mode->FindRandomPlayerSpawnTransform(GetController(), SpawnTransform))
			SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->ClearAccumulatedForces();
	if (StatusComponent) StatusComponent->HP = FMath::Max(0.0f, StatusComponent->MaxHP);
	BeginSpawning();
}

void ASABaseCharacter::SetCharacterState(ECharacterState State, float ReactionDamage)
{
	if (CharacterState == State) return;
	if (State == ECharacterState::Spawning) { BeginSpawning(); return; }
	const ECharacterState Previous = CharacterState;
	CharacterState = State;
	if (State != ECharacterState::Dead)
	{
		GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
		bRespawnScheduled = false;
	}
	if (Previous == ECharacterState::Spawning || (Previous == ECharacterState::Dead && State != ECharacterState::Dead))
	{
		bDeathDissolving = false;
		SetLifecycleVisibility(1.0f);
		RestoreLifecycleMaterials();
		GetMesh()->SetVisibility(true, true);
		GetCapsuleComponent()->SetCollisionEnabled(SavedCollision);
	}
	if (State == ECharacterState::Dead)
	{
		GetWorldTimerManager().ClearTimer(DamageRecoveryTimerHandle);
		if (ASACRWeapon* Weapon = Cast<ASACRWeapon>(GetWeapon())) Weapon->DeactivateHitCollision();
		AnimInstance = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance());
		if (AnimInstance) AnimInstance->PlayDeadAnim(ReactionDamage);
		else BeginDeathDissolve();
	}
}

void ASABaseCharacter::ActivateLifecycleMaterials()
{
	if (bLifecycleMaterialsActive) return;
	bLifecycleMaterialsActive = true;
	OriginalOverlay = GetMesh()->GetOverlayMaterial();
	GetMesh()->SetOverlayMaterial(nullptr);
	for (int32 Index = 0; Index < GetMesh()->GetNumMaterials(); ++Index)
	{
		OriginalMaterials.Add(GetMesh()->GetMaterial(Index));
		UMaterialInstanceDynamic* Dynamic = SpawnDissolveMaterial ? UMaterialInstanceDynamic::Create(SpawnDissolveMaterial, this) : nullptr;
		LifecycleMaterials.Add(Dynamic);
		if (Dynamic) GetMesh()->SetMaterial(Index, Dynamic);
	}
}

void ASABaseCharacter::RestoreLifecycleMaterials()
{
	if (!bLifecycleMaterialsActive) return;
	for (int32 Index = 0; Index < OriginalMaterials.Num(); ++Index) GetMesh()->SetMaterial(Index, OriginalMaterials[Index]);
	GetMesh()->SetOverlayMaterial(OriginalOverlay);
	OriginalMaterials.Reset();
	LifecycleMaterials.Reset();
	OriginalOverlay = nullptr;
	bLifecycleMaterialsActive = false;
}

void ASABaseCharacter::RefreshLifecycleMaterials()
{
	if (!bLifecycleMaterialsActive && CharacterState != ECharacterState::Spawning && !bDeathDissolving) return;
	// Rebuild the dynamic instances after changing a proxy's appearance.
	RestoreLifecycleMaterials();
	ActivateLifecycleMaterials();
	SetLifecycleVisibility(LifecycleVisibility);
}

void ASABaseCharacter::SetLifecycleVisibility(float Visibility)
{
	LifecycleVisibility = FMath::Clamp(Visibility, 0.0f, 1.0f);
	for (UMaterialInstanceDynamic* Material : LifecycleMaterials)
		if (Material) Material->SetScalarParameterValue(TEXT("DissolveVisibility"), LifecycleVisibility);
}

void ASABaseCharacter::BeginDeathDissolve()
{
	if (CharacterState != ECharacterState::Dead || bDeathDissolving || bNetworkLifecycleDriven) return;
	GetWorldTimerManager().ClearTimer(DeathAnimationTimerHandle);
	bDeathDissolving = true;
	ActivateLifecycleMaterials();
	SetLifecycleVisibility(1.0f);
}

void ASABaseCharacter::WaitForDeathAnimation(float Duration)
{
	if (CharacterState != ECharacterState::Dead || bNetworkLifecycleDriven) return;
	// Death montages may hold their final pose with auto-blend-out disabled.
	// Their selected section still ends; use its duration without releasing that pose.
	GetWorldTimerManager().SetTimer(DeathAnimationTimerHandle, this, &ASABaseCharacter::BeginDeathDissolve,
		FMath::Max(0.01f, Duration), false);
}

void ASABaseCharacter::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!IsActorBeingDestroyed() && CharacterState == ECharacterState::Dead) BeginDeathDissolve();
}

void ASABaseCharacter::ApplyNetworkLifecycle(ECharacterState State, float Visibility)
{
	if (State == ECharacterState::Spawning && CharacterState != ECharacterState::Spawning) BeginSpawning();
	else if (State == ECharacterState::Dead && CharacterState != ECharacterState::Dead) SetCharacterState(ECharacterState::Dead);
	else if (State != ECharacterState::Spawning && State != ECharacterState::Dead
		&& (CharacterState == ECharacterState::Spawning || CharacterState == ECharacterState::Dead)) SetCharacterState(ECharacterState::Idle);
	bNetworkLifecycleDriven = true;
	if (State == ECharacterState::Dead && Visibility >= 1.0f)
	{
		bDeathDissolving = false;
		SetLifecycleVisibility(1.0f);
		RestoreLifecycleMaterials();
	}
	if (State == ECharacterState::Spawning || (State == ECharacterState::Dead && Visibility < 1.0f))
	{
		bDeathDissolving = State == ECharacterState::Dead;
		ActivateLifecycleMaterials();
		SetLifecycleVisibility(Visibility);
	}
}

void ASABaseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Step = FMath::Max(0.0f, DeltaSeconds);
	if (CharacterState == ECharacterState::Spawning)
	{
		SetLifecycleVisibility(LifecycleVisibility + Step / FMath::Max(SpawnDuration, 0.01f));
		if (LifecycleVisibility >= 1.0f && !bNetworkLifecycleDriven) SetCharacterState(ECharacterState::Idle);
	}
	else if (CharacterState == ECharacterState::Dead && bDeathDissolving)
	{
		SetLifecycleVisibility(LifecycleVisibility - Step / FMath::Max(DeathDissolveDuration, 0.01f));
		if (LifecycleVisibility <= 0.0f)
		{
			GetMesh()->SetVisibility(false, true);
			GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (!bRespawnScheduled && CanRespawnLocally())
			{
				bRespawnScheduled = true;
				GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &ASABaseCharacter::Respawn,
					FMath::Max(0.01f, RespawnDelay), false);
			}
		}
	}
}

void ASABaseCharacter::Attack()
{
	// Remote characters assign their mesh/AnimInstance after construction.
	AnimInstance = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance());
	if (!AnimInstance || CharacterState != ECharacterState::Idle) return;
	AnimInstance->PlayAttackMontage(WeaponType);
	// Only consume a use when a weapon attack actually starts.
	if (IsNetworkProxy() || CharacterState != ECharacterState::Attack || WeaponType == EWeaponType::None || !StatusComponent) return;

	StatusComponent->Ammo = FMath::Max(0, StatusComponent->Ammo - 1);
	if (StatusComponent->Ammo == 0)
	{
		ASAWeaponActor* Weapon = GetWeapon();
		SetWeapon(nullptr);
		SetWeaponType(EWeaponType::None);
		if (IsValid(Weapon))
		{
			Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Weapon->Destroy();
		}
	}
}

void ASABaseCharacter::ApplyDamageKnockback(const FVector& VelocityChange)
{
	GetCharacterMovement()->AddImpulse(VelocityChange, true);
}

void ASABaseCharacter::RestoreFromDamaged()
{
	if (CharacterState == ECharacterState::Damaged) SetCharacterState(ECharacterState::Idle);
}

void ASABaseCharacter::TakeDamage(AActor* Attacker, UNiagaraSystem* DamagedEffect, FVector ImpactPoint)
{
	if (CharacterState == ECharacterState::Dead || CharacterState == ECharacterState::Spawning) return;
	if (!IsValid(Attacker) || Attacker == this) return;

	// Face the attacker before PlayDamageAnim applies its backward impulse.
	const FVector Direction = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		FRotator FacingRotation = GetActorRotation();
		FacingRotation.Yaw = Direction.Rotation().Yaw;
		SetActorRotation(FacingRotation);
	}

	if (const ASABaseCharacter* AttackingCharacter = Cast<ASABaseCharacter>(Attacker))
	{
		if (AttackingCharacter->GetCharacterState() == ECharacterState::Spawning
			|| AttackingCharacter->GetCharacterState() == ECharacterState::Dead) return;
		float TotalDamage = 0.0f;
		if (const USAStatusComponent* AttackerStatus = AttackingCharacter->GetStatusComponent())
		{
			TotalDamage += AttackerStatus->DefaultDamage;
		}
		if (const ASAWeaponActor* AttackerWeapon = AttackingCharacter->GetWeapon(); IsValid(AttackerWeapon))
		{
			TotalDamage += AttackerWeapon->GetDamage();
		}
		if (StatusComponent)
		{
			const float AppliedDamage = FMath::Max(1.0f, TotalDamage - FMath::Max(0.0f, StatusComponent->Defense));
			if (DamagedEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DamagedEffect, ImpactPoint, FRotator::ZeroRotator, FVector(0.1f), true, true);
			}
			// A remote proxy may predict a hit reaction, but only its owning client changes HP.
			if (!HasSynchronizedHealth())
				StatusComponent->HP = FMath::Max(0.0f, StatusComponent->HP - AppliedDamage);
			if (StatusComponent->HP > 0) {
				SetCharacterState(ECharacterState::Damaged);
				AnimInstance = Cast<USABaseAnimInstance>(GetMesh()->GetAnimInstance());
				if (!IsValid(AnimInstance)) return;
				AnimInstance->PlayDamageAnim(AppliedDamage);
			}
			else
			{
				SetCharacterState(ECharacterState::Dead, AppliedDamage);
			}
		
		}
	}

	if (ASACRWeapon* Weapon = Cast<ASACRWeapon>(GetWeapon()))
	{
		Weapon->DeactivateHitCollision();
	}
	GetWorldTimerManager().SetTimer(DamageRecoveryTimerHandle, this,
		&ASABaseCharacter::FinishDamageRecovery, 0.85f, false);
}

FVector2D ASABaseCharacter::GetMoveVector() const
{
	const FVector LocalVelocity = GetActorRotation().UnrotateVector(GetVelocity());
	return FVector2D(LocalVelocity.Y, LocalVelocity.X);
}

void ASABaseCharacter::FinishDamageRecovery()
{
	if (CharacterState == ECharacterState::Damaged)
	{
		SetCharacterState(ECharacterState::Idle);
	}
}
