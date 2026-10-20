#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SAStatusComponent.generated.h"

UENUM(BlueprintType)
enum class ESACrystalBlessing : uint8
{
	Healing,
	Attack,
	Defense
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UE_STICKARENADEMO_API USAStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USAStatusComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Status|Crystal")
	void ApplyCrystalBlessing(ESACrystalBlessing Blessing, float RemainingDuration = -1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Combat", meta=(ClampMin="0.0"))
	float DefaultDamage = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Combat", meta=(ClampMin="0.0"))
	float Defense = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Health", meta=(ClampMin="0.0"))
	float HP = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Health", meta=(ClampMin="0.0"))
	float MaxHP = 100.0f;

	// Remaining weapon uses.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Ammo", meta=(ClampMin="0"))
	int32 Ammo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Ammo", meta=(ClampMin="0"))
	int32 MaxAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Speed", meta=(ClampMin="0.0"))
	float IdleSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Speed", meta=(ClampMin="0.0"))
	float AttackSpeed = 400.0f;

private:
	struct FCrystalEffect
	{
		ESACrystalBlessing Type;
		float Remaining;
	};
	TArray<FCrystalEffect> CrystalEffects;
};
