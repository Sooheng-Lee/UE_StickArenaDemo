#include "Characters/SAEnemyCharacter.h"
#include "Animation/AnimInstance.h"
#include "AnimInstances/SABaseAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SAStatusComponent.h"
#include "Items/SAWeaponActor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WidgetComponent.h"
#include "Widget/SAHealthBarWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogSAEnemy, Log, All);

ASAEnemyCharacter::ASAEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarComponent"));
	HealthBarComponent->SetupAttachment(GetRootComponent());
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComponent->SetWidgetClass(USAHealthBarWidget::StaticClass());
	HealthBarComponent->SetDrawSize(FVector2D(110.0f, 12.0f));
	HealthBarComponent->SetPivot(FVector2D(0.5f, 1.0f));
	HealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarComponent->SetGenerateOverlapEvents(false);
	HealthBarComponent->SetVisibility(false);
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
	// Capsule size and collision use the same shared Pawn settings as the player.
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
	HealthBarComponent->InitWidget();
	if (const USAStatusComponent* Status = GetStatusComponent())
	{
		LastObservedHP = Status->HP;
		if (USAHealthBarWidget* Bar = Cast<USAHealthBarWidget>(HealthBarComponent->GetUserWidgetObject()))
			Bar->UpdateHealth(Status->HP, Status->MaxHP, 0.0f, true);
	}
	// Component registration/activation can restore the movement tick after construction.
	GetCharacterMovement()->SetComponentTickEnabled(false);
}

void ASAEnemyCharacter::OnDamageReceived()
{
	if (const USAStatusComponent* Status = GetStatusComponent()) LastObservedHP = Status->HP;
	HealthBarTimeRemaining = HealthBarDisplayDuration;
	HealthBarComponent->SetVisibility(HealthBarTimeRemaining > 0.0f && !IsHidden());
}

void ASAEnemyCharacter::UpdateHealthBar(float DeltaSeconds)
{
	const USAStatusComponent* Status = GetStatusComponent();
	if (!Status) return;
	HealthBarTimeRemaining = FMath::Max(0.0f, HealthBarTimeRemaining - FMath::Max(DeltaSeconds, 0.0f));
	if (Status->HP < LastObservedHP) OnDamageReceived();
	LastObservedHP = Status->HP;
	HealthBarComponent->SetRelativeLocation(FVector(0, 0,
		GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + HealthBarHeightOffset));
	HealthBarComponent->SetVisibility(HealthBarTimeRemaining > 0.0f && !IsHidden()
		&& GetCharacterState() != ECharacterState::Spawning);
	if (USAHealthBarWidget* Bar = Cast<USAHealthBarWidget>(HealthBarComponent->GetUserWidgetObject()))
		Bar->UpdateHealth(Status->HP, Status->MaxHP, DeltaSeconds);
}

void ASAEnemyCharacter::InitializeRemotePlayer(const FSAPlayerRegistration& Registration)
{
	const bool bSameAppearance = bAppearanceInitialized
		&& PlayerRegistration.CharacterClassPath == Registration.CharacterClassPath
		&& PlayerRegistration.SkeletalMeshPath == Registration.SkeletalMeshPath
		&& PlayerRegistration.AnimInstanceClassPath == Registration.AnimInstanceClassPath;
	PlayerRegistration = Registration;
	if (bSameAppearance) return;
	RestoreLifecycleMaterials();

	// Copy visual defaults, never spawn the remote player's locally controlled pawn class.
	if (UClass* CharacterClass = LoadClass<ACharacter>(nullptr, *Registration.CharacterClassPath))
	{
		const ACharacter* Defaults = CharacterClass->GetDefaultObject<ACharacter>();
		GetMesh()->SetRelativeTransform(Defaults->GetMesh()->GetRelativeTransform());
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
	RefreshLifecycleMaterials();
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
	ApplyStatusState(State);
	if (State.bHasLifecycleState) ApplyNetworkLifecycle(State.CharacterState, State.LifecycleVisibility);
	else if (GetCharacterState() == ECharacterState::Spawning) SetCharacterState(ECharacterState::Idle);
	NetworkState = State;
	LastStateTime = FPlatformTime::Seconds();
	bHasNetworkState = true;
	// The owning player's snapshots already contain AddImpulse movement. Never add it a second time.
	KnockbackVelocity = FVector::ZeroVector;
	if (bFirstState || FVector::DistSquared(GetActorLocation(), State.Position) > FMath::Square(TeleportDistance))
		SetActorLocationAndRotation(State.Position,
			GetCharacterState() == ECharacterState::Damaged ? GetActorRotation() : State.Rotation,
			false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->SetMovementMode(State.State == ESAPlayerActivity::Falling ? MOVE_Falling : MOVE_Walking);
	GetCharacterMovement()->Velocity = State.Velocity;
	if (bPlayAttack) Attack();
}

void ASAEnemyCharacter::ApplyStatusState(const FSAPlayerNetworkState& State)
{
	if (!State.bHasStatusState || !GetStatusComponent()) return;
	const bool bFirstHealthState = !bHasSynchronizedHealth;
	bHasSynchronizedHealth = true;
	USAStatusComponent* Status = GetStatusComponent();
	const float PreviousHP = Status->HP;
	Status->MaxHP = FMath::Max(0.0f, State.MaxHP);
	Status->HP = FMath::Clamp(State.HP, 0.0f, Status->MaxHP);
	if (!bFirstHealthState && Status->HP < PreviousHP) OnDamageReceived();
	if (bFirstHealthState)
	{
		if (USAHealthBarWidget* Bar = Cast<USAHealthBarWidget>(HealthBarComponent->GetUserWidgetObject()))
			Bar->UpdateHealth(Status->HP, Status->MaxHP, 0.0f, true);
	}
	LastObservedHP = Status->HP;
	Status->DefaultDamage = FMath::Max(0.0f, State.DefaultDamage);
	Status->Defense = FMath::Max(0.0f, State.Defense);
	if (Status->HP <= 0.0f && GetCharacterState() != ECharacterState::Dead)
	{
		SetCharacterState(ECharacterState::Dead, FMath::Max(0.0f, PreviousHP - Status->HP));
	}
	else if (Status->HP > 0.0f && GetCharacterState() == ECharacterState::Dead)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->StopAllMontages(0.1f);
		KnockbackVelocity = FVector::ZeroVector;
		SetCharacterState(ECharacterState::Idle);
	}
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
	// Network proxies receive the owner's physical knockback through position/velocity snapshots.
	// Only placed enemies without a network owner need a locally integrated impulse.
	if (bHasNetworkState) return;
	KnockbackVelocity += FVector(VelocityChange.X, VelocityChange.Y, 0.0f);
}

void ASAEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHealthBar(DeltaSeconds);
	const float Step = FMath::Max(DeltaSeconds, 0.0f);
	if (!bHasNetworkState)
	{
		// Placed test enemies have no server position to drive their movement.
		const float Speed = KnockbackVelocity.Size();
		if (Speed > UE_SMALL_NUMBER)
		{
			const float Deceleration = FMath::Max(KnockbackDeceleration, 1.0f);
			const float MovingTime = FMath::Min(Step, Speed / Deceleration);
			const FVector Direction = KnockbackVelocity / Speed;
			AddActorWorldOffset(Direction * (Speed * MovingTime - 0.5f * Deceleration * MovingTime * MovingTime), true);
			KnockbackVelocity = Direction * FMath::Max(0.0f, Speed - Deceleration * Step);
		}
		return;
	}
	const double Age = FMath::Clamp(FPlatformTime::Seconds() - LastStateTime, 0.0, static_cast<double>(MaxExtrapolationTime));
	const FVector Target = NetworkState.Position + NetworkState.Velocity * Age;
	const float Alpha = NetworkInterpSpeed > 0.0f ? 1.0f - FMath::Exp(-NetworkInterpSpeed * FMath::Max(DeltaSeconds, 0.0f)) : 1.0f;
	const FQuat Rotation = GetCharacterState() == ECharacterState::Damaged
		? GetActorQuat() : FQuat::Slerp(GetActorQuat(), NetworkState.Rotation.Quaternion(), Alpha);
	SetActorLocationAndRotation(FMath::Lerp(GetActorLocation(), Target, Alpha),
		Rotation, false, nullptr, ETeleportType::TeleportPhysics);
}

FVector ASAEnemyCharacter::GetVelocity() const
{
	return bHasNetworkState ? NetworkState.Velocity : KnockbackVelocity;
}
