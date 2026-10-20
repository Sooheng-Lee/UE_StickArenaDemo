#include "Items/SACRWeapon.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Characters/SABaseCharacter.h"
#include "LevelObjects/SALevelObject.h"
#include "LevelObjects/SATreasureBox.h"

ASACRWeapon::ASACRWeapon()
{
	HitComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("HitComponent"));
	HitComponent->SetupAttachment(ItemMeshComponent);
	HitComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitComponent->SetCollisionObjectType(ECC_Pawn);
	HitComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
	HitComponent->SetGenerateOverlapEvents(true);
	HitComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ASACRWeapon::OnHitBeginOverlap);
}

void ASACRWeapon::ActivateHitCollision()
{
	HitActors.Reset();
	HitComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ASACRWeapon::DeactivateHitCollision()
{
	HitComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASACRWeapon::OnHitBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ASABaseCharacter* Attacker = Cast<ASABaseCharacter>(GetAttachParentActor());
	if (!IsValid(Attacker)) Attacker = Cast<ASABaseCharacter>(GetOwner());
	if (!IsValid(Attacker) || Attacker->GetWeapon() != this
		|| Attacker->GetCharacterState() == ECharacterState::Damaged
		|| Attacker->GetCharacterState() == ECharacterState::Dead
		|| HitComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		|| !IsValid(OtherActor) || OtherActor == this || OtherActor == Attacker
		|| HitActors.Contains(TWeakObjectPtr<AActor>(OtherActor))) return;

	if (ASABaseCharacter* Character = Cast<ASABaseCharacter>(OtherActor))
	{
		HitActors.Add(TWeakObjectPtr<AActor>(OtherActor));
		Character->TakeDamage(Attacker, HitEffect, SweepResult.ImpactPoint);
	}
	else if (ASALevelObject* LevelObject = Cast<ASALevelObject>(OtherActor))
	{
		HitActors.Add(TWeakObjectPtr<AActor>(OtherActor));
		if (ASATreasureBox* Box = Cast<ASATreasureBox>(LevelObject)) Box->RequestHit(Attacker);
		else LevelObject->PlayHitEffect();
	}
}
