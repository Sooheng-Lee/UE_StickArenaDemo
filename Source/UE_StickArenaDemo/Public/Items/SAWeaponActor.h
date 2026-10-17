// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Attribute", meta = (AllowPrivateAccess="true"))
	float Damage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Attribute", meta = (AllowPrivateAccess="true"))
	float WalkSpeed;
	
};
