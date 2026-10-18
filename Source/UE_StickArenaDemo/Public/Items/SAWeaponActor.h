// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../UE_StickArenaDemo.h"
#include "Items/SAItemActor.h"
#include "SAWeaponActor.generated.h"

/**
 * 
 */
UCLASS()
class UE_STICKARENADEMO_API ASAWeaponActor : public ASAItemActor
{
	GENERATED_BODY()
protected:
	virtual void OwnItem(AActor* NewOwner);

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon Attribute", meta = (AllowPrivateAccess="true"))
	EWeaponType WeaponType = EWeaponType::None;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Attribute", meta = (AllowPrivateAccess="true"))
	float Damage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Attribute", meta = (AllowPrivateAccess="true"))
	float WalkSpeed;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Attribute", meta = (AllowPrivateAccess="true",
		ClampMin = "1", ClampMax = "100", UIMin="1", UIMax="100"))
	int32 MaxAmmo;
	
};
