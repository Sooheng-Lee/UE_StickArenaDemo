#pragma once

#include "CoreMinimal.h"
#include "SAPlayerNetworkState.generated.h"

UENUM(BlueprintType)
enum class ESAPlayerActivity : uint8
{
	Idle,
	Moving,
	Falling,
	Attacking
};

USTRUCT(BlueprintType)
struct UE_STICKARENADEMO_API FSAPlayerNetworkState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString PlayerId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int64 Sequence = 0;
	// Persisted in heartbeats so losing the ATTACK datagram does not lose the animation event.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int64 AttackSequence = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString Action;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ESAPlayerActivity State = ESAPlayerActivity::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector MovementInput = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Position = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Velocity = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FRotator Rotation = FRotator::ZeroRotator;

	// False for legacy snapshots that did not carry equipment information.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bHasWeaponState = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString WeaponClassPath;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString WeaponInstanceId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Ammo = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 MaxAmmo = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSAOnRemotePlayerState, const FSAPlayerNetworkState&, PlayerState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSAOnRemotePlayerRemoved, const FString&, PlayerId);
