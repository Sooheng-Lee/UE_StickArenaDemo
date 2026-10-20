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
public:
	// Network proxies equip an explicit visual copy rather than collecting world items.
	bool EquipToCharacter(class ASABaseCharacter* Character);
	const FString& GetWeaponInstanceId() const { return WeaponInstanceId; }
	void SetWeaponInstanceId(const FString& Id) { WeaponInstanceId = Id; }
	void DisablePickup();
	void SetNetworkTreasure(const FString& Epoch, const FString& Id) { TreasureEpoch = Epoch; TreasureId = Id; }
	const FString& GetTreasureEpoch() const { return TreasureEpoch; }
	const FString& GetTreasureId() const { return TreasureId; }

	UFUNCTION(BlueprintPure, Category="Weapon Attribute")
	float GetDamage() const { return Damage; }

	UFUNCTION(BlueprintPure, Category="Weapon Attribute")
	float GetWalkSpeed() const { return WalkSpeed; }

protected:
	virtual void OwnItem(AActor* NewOwner) override;

private:
	UPROPERTY(Transient)
	FString WeaponInstanceId;
	UPROPERTY(Transient)
	FString TreasureEpoch;
	UPROPERTY(Transient)
	FString TreasureId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon Attribute", meta = (AllowPrivateAccess="true"))
	EWeaponType WeaponType = EWeaponType::None;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Attribute", meta = (AllowPrivateAccess="true"))
	float Damage = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Attribute", meta = (AllowPrivateAccess="true"))
	float WalkSpeed;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Attribute", meta = (AllowPrivateAccess="true",
		ClampMin = "1", ClampMax = "100", UIMin="1", UIMax="100"))
	int32 MaxAmmo;
	
};
