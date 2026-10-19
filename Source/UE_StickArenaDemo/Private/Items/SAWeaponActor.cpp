// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/SAWeaponActor.h"
#include "Characters/SAPlayerCharacter.h"
#include "Components/SAStatusComponent.h"
void ASAWeaponActor::OwnItem(AActor* NewOwner)
{
	ASAPlayerCharacter* Player = Cast<ASAPlayerCharacter>(NewOwner);
	if (!Player || GetAttachParentActor() || Player->GetWeapon() == this) return;

	USkeletalMeshComponent* TargetMesh = Player->GetMesh();
	if (!TargetMesh || !TargetMesh->DoesSocketExist(FName("WeaponSocket"))) return;
	if (!AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponSocket"))) return;

	ASAWeaponActor* PreviousWeapon = Player->GetWeapon();
	Super::OwnItem(Player);
	Player->SetWeaponType(WeaponType);
	Player->SetWeapon(this);
	if (USAStatusComponent* Status = Player->GetStatusComponent())
	{
		Status->MaxAmmo = MaxAmmo;
		Status->Ammo = MaxAmmo;
	}

	if (IsValid(PreviousWeapon))
	{
		PreviousWeapon->Destroy();
	}
}
