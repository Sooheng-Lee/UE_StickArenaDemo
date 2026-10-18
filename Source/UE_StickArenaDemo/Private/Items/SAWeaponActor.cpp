// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/SAWeaponActor.h"
#include "Characters/SAPlayerCharacter.h"
void ASAWeaponActor::OwnItem(AActor* NewOwner)
{
	ASAPlayerCharacter* Player = Cast<ASAPlayerCharacter>(NewOwner);
	if (Player) {
		Super::OwnItem(Player);
		USkeletalMeshComponent* TargetMesh = Player->GetMesh();
		if (!TargetMesh || !TargetMesh->DoesSocketExist(FName("WeaponSocket"))) return;
		AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponSocket"));
		Player->SetWeaponType(WeaponType);
		Player->SetWeapon(this);
	}
}
