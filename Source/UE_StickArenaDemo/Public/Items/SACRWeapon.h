#pragma once

#include "CoreMinimal.h"
#include "Items/SAWeaponActor.h"
#include "SACRWeapon.generated.h"

class UBoxComponent;

UCLASS()
class UE_STICKARENADEMO_API ASACRWeapon : public ASAWeaponActor
{
	GENERATED_BODY()

public:
	ASACRWeapon();

private:
	UFUNCTION()
	void OnHitBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	TSet<TWeakObjectPtr<AActor>> HitActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon Components", meta=(AllowPrivateAccess="true"))
	UBoxComponent* HitComponent;
	
public:
	void ActivateHitCollision();
	void DeactivateHitCollision();
};
