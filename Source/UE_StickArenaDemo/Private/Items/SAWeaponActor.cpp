// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/SAWeaponActor.h"
#include "Characters/SABaseCharacter.h"
#include "Components/SAStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SAUdpClientComponent.h"
void ASAWeaponActor::OwnItem(AActor* NewOwner)
{
	ASABaseCharacter* Character = Cast<ASABaseCharacter>(NewOwner);
	if (!Character || Character->IsNetworkProxy()) return;
	if (!TreasureId.IsEmpty())
	{
		// Network drops are equipped only after the server confirms the winner.
		if (USAUdpClientComponent* Client = Character->FindComponentByClass<USAUdpClientComponent>())
			Client->RequestTreasurePickup(this);
		return;
	}
	EquipToCharacter(Character);
}

void ASAWeaponActor::DisablePickup()
{
	DetectionSphere->SetGenerateOverlapEvents(false);
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ASAWeaponActor::EquipToCharacter(ASABaseCharacter* Character)
{
	if (!IsValid(Character) || GetAttachParentActor() || Character->GetWeapon() == this) return false;

	USkeletalMeshComponent* TargetMesh = Character->GetMesh();
	if (!TargetMesh || !TargetMesh->DoesSocketExist(FName("WeaponSocket"))) return false;
	if (!AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponSocket"))) return false;
	if (WeaponInstanceId.IsEmpty()) WeaponInstanceId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	SetOwner(Character);
	DisablePickup();

	ASAWeaponActor* PreviousWeapon = Character->GetWeapon();
	Super::OwnItem(Character);
	Character->SetWeaponType(WeaponType);
	Character->SetWeapon(this);
	if (USAStatusComponent* Status = Character->GetStatusComponent())
	{
		Status->MaxAmmo = MaxAmmo;
		Status->Ammo = MaxAmmo;
	}

	if (IsValid(PreviousWeapon))
	{
		PreviousWeapon->Destroy();
	}
	return true;
}
