#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SAStatusComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UE_STICKARENADEMO_API USAStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USAStatusComponent();

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
};
