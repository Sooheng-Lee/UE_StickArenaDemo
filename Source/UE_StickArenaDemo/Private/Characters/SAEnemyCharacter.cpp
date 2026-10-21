#include "Characters/SAEnemyCharacter.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SAStatusComponent.h"
#include "Items/SAWeaponActor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSAEnemy, Log, All);

ASAEnemyCharacter::ASAEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	bUseControllerRotationYaw = false;
	// Every UDP client owns its own visual proxy; Unreal net replication would duplicate it.
	bReplicates = false;
	SetReplicateMovement(false);
	// Network positions already include movement/physics. Simulating them again causes drift.
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCharacterMovement()->PrimaryComponentTick.bStartWithTickEnabled = false;
	GetCharacterMovement()->SetAutoActivate(false);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	// Receive attack sweeps without adding local physics to the network proxy.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->AddTickPrerequisiteActor(this);
	// Overlay preserves the remote player's original materials and highlights the silhouette.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EnemyOutline(
		TEXT("/Game/PlayerCharacters/Materials/MI_EnemyRedOutline.MI_EnemyRedOutline"));
	if (EnemyOutline.Succeeded())
		GetMesh()->SetOverlayMaterial(EnemyOutline.Object);
}

void ASAEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Component registration/activation can restore the movement tick after construction.
	GetCharacterMovement()->SetComponentTickEnabled(false);
}

void ASAEnemyCharacter::InitializeRemotePlayer(const FSAPlayerRegistration& Registration)
{
	const bool bSameAppearance = bAppearanceInitialized
		&& PlayerRegistration.CharacterClassPath == Registration.CharacterClassPath
		&& PlayerRegistration.SkeletalMeshPath == Registration.SkeletalMeshPath
		&& PlayerRegistration.AnimInstanceClassPath == Registration.AnimInstanceClassPath;
	PlayerRegistration = Registration;
	if (bSameAppearance) return;

	// Copy visual defaults, never spawn the remote player's locally controlled pawn class.
	if (UClass* CharacterClass = LoadClass<ACharacter>(nullptr, *Registration.CharacterClassPath))
	{
		const ACharacter* Defaults = CharacterClass->GetDefaultObject<ACharacter>();
		GetMesh()->SetRelativeTransform(Defaults->GetMesh()->GetRelativeTransform());
		GetCapsuleComponent()->SetCapsuleSize(Defaults->GetCapsuleComponent()->GetUnscaledCapsuleRadius(),
			Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		GetMesh()->EmptyOverrideMaterials();
		for (int32 Index = 0; Index < Defaults->GetMesh()->GetNumMaterials(); ++Index)
			GetMesh()->SetMaterial(Index, Defaults->GetMesh()->GetMaterial(Index));
	}

	USkeletalMesh* RemoteMesh = Registration.SkeletalMeshPath.IsEmpty() || Registration.SkeletalMeshPath == TEXT("None")
		? nullptr : LoadObject<USkeletalMesh>(nullptr, *Registration.SkeletalMeshPath);
	UClass* AnimClass = Registration.AnimInstanceClassPath.IsEmpty() || Registration.AnimInstanceClassPath == TEXT("None")
		? nullptr : LoadClass<UAnimInstance>(nullptr, *Registration.AnimInstanceClassPath);
	GetMesh()->SetSkeletalMeshAsset(RemoteMesh);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(AnimClass);
	bAppearanceInitialized = RemoteMesh != nullptr && AnimClass != nullptr;
	if (!bAppearanceInitialized)
		UE_LOG(LogSAEnemy, Warning, TEXT("Remote player %s has missing mesh/animation assets: %s, %s"),
			*Registration.PlayerId, *Registration.SkeletalMeshPath, *Registration.AnimInstanceClassPath);
}

void ASAEnemyCharacter::ApplyNetworkState(const FSAPlayerNetworkState& State)
{
	if (State.PlayerId != PlayerRegistration.PlayerId || (bHasNetworkState && State.Sequence <= NetworkState.Sequence)) return;
	const bool bFirstState = !bHasNetworkState;
	const bool bPlayAttack = bFirstState ? State.Action == TEXT("ATTACK") : State.AttackSequence > NetworkState.AttackSequence;
	// Equipment must be installed before choosing the received attack's montage.
	ApplyWeaponState(State);
	NetworkState = State;
	LastStateTime = FPlatformTime::Seconds();
	bHasNetworkState = true;
	if (bFirstState || FVector::DistSquared(GetActorLocation() - KnockbackOffset, State.Position) > FMath::Square(TeleportDistance))
		SetActorLocationAndRotation(State.Position + KnockbackOffset,
			GetCharacterState() == ECharacterState::Damaged ? GetActorRotation() : State.Rotation,
			false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->SetMovementMode(State.State == ESAPlayerActivity::Falling ? MOVE_Falling : MOVE_Walking);
	GetCharacterMovement()->Velocity = State.Velocity;
	if (bPlayAttack) Attack();
}

void ASAEnemyCharacter::ApplyWeaponState(const FSAPlayerNetworkState& State)
{
	if (!State.bHasWeaponState) return;
	ASAWeaponActor* Weapon = GetWeapon();
	if (State.WeaponClassPath.IsEmpty())
	{
		SetWeapon(nullptr);
		SetWeaponType(EWeaponType::None);
		if (IsValid(Weapon)) Weapon->Destroy();
	}
	else if (!IsValid(Weapon) || Weapon->GetClass()->GetPathName() != State.WeaponClassPath
		|| Weapon->GetWeaponInstanceId() != State.WeaponInstanceId)
	{
		if (!State.WeaponClassPath.StartsWith(TEXT("/Game/Items/Weapons/"))
			&& !State.WeaponClassPath.StartsWith(TEXT("/Script/UE_StickArenaDemo."))) return;
		UClass* WeaponClass = LoadClass<ASAWeaponActor>(nullptr, *State.WeaponClassPath);
		if (!WeaponClass || !GetMesh()->DoesSocketExist(TEXT("WeaponSocket"))) return;
		ASAWeaponActor* NewWeapon = GetWorld()->SpawnActorDeferred<ASAWeaponActor>(WeaponClass,
			GetActorTransform(), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!NewWeapon) return;
		// Disable collection before component registration can trigger overlap callbacks.
		NewWeapon->DisablePickup();
		NewWeapon->SetWeaponInstanceId(State.WeaponInstanceId);
		NewWeapon->FinishSpawning(GetActorTransform());
		if (!NewWeapon->EquipToCharacter(this))
		{
			NewWeapon->Destroy();
			return;
		}
	}
	if (USAStatusComponent* Status = GetStatusComponent())
	{
		Status->MaxAmmo = FMath::Max(0, State.MaxAmmo);
		Status->Ammo = FMath::Clamp(State.Ammo, 0, Status->MaxAmmo);
	}
}

void ASAEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ASAWeaponActor* Weapon = GetWeapon();
	SetWeapon(nullptr);
	if (IsValid(Weapon)) Weapon->Destroy();
	Super::EndPlay(EndPlayReason);
}

void ASAEnemyCharacter::ApplyDamageKnockback(const FVector& VelocityChange)
{
	// Proxies never tick CharacterMovement; integrate the local hit response separately.
	KnockbackVelocity += FVector(VelocityChange.X, VelocityChange.Y, 0.0f);
}

void ASAEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Step = FMath::Max(DeltaSeconds, 0.0f);
	const FVector PreviousOffset = KnockbackOffset;
	const float Speed = KnockbackVelocity.Size();
	if (Speed > UE_SMALL_NUMBER)
	{
		const float Deceleration = FMath::Max(KnockbackDeceleration, 1.0f);
		const float MovingTime = FMath::Min(Step, Speed / Deceleration);
		const FVector Direction = KnockbackVelocity / Speed;
		KnockbackOffset += Direction * (Speed * MovingTime - 0.5f * Deceleration * MovingTime * MovingTime);
		KnockbackVelocity = Direction * FMath::Max(0.0f, Speed - Deceleration * Step);
	}
	else if (GetCharacterState() != ECharacterState::Damaged && bHasNetworkState)
	{
		// Once recovered, blend back to the latest authoritative position.
		KnockbackOffset *= FMath::Exp(-8.0f * Step);
		if (KnockbackOffset.IsNearlyZero(0.1f)) KnockbackOffset = FVector::ZeroVector;
	}
	if (!bHasNetworkState)
	{
		// Placed test enemies have no server position to drive their movement.
		AddActorWorldOffset(KnockbackOffset - PreviousOffset, true);
		return;
	}
	const double Age = FMath::Clamp(FPlatformTime::Seconds() - LastStateTime, 0.0, static_cast<double>(MaxExtrapolationTime));
	const FVector Target = NetworkState.Position + NetworkState.Velocity * Age;
	const float Alpha = NetworkInterpSpeed > 0.0f ? 1.0f - FMath::Exp(-NetworkInterpSpeed * FMath::Max(DeltaSeconds, 0.0f)) : 1.0f;
	const FQuat Rotation = GetCharacterState() == ECharacterState::Damaged
		? GetActorQuat() : FQuat::Slerp(GetActorQuat(), NetworkState.Rotation.Quaternion(), Alpha);
	const FVector BasePosition = GetActorLocation() - PreviousOffset;
	SetActorLocationAndRotation(FMath::Lerp(BasePosition, Target, Alpha) + KnockbackOffset,
		Rotation, false, nullptr, ETeleportType::TeleportPhysics);
}

FVector ASAEnemyCharacter::GetVelocity() const
{
	return (bHasNetworkState ? NetworkState.Velocity : FVector::ZeroVector) + KnockbackVelocity;
}
