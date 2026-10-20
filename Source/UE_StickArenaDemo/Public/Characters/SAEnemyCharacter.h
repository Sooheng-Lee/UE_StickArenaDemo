#pragma once

#include "CoreMinimal.h"
#include "Characters/SABaseCharacter.h"
#include "Components/SAPlayerRegistration.h"
#include "Components/SAPlayerNetworkState.h"
#include "SAEnemyCharacter.generated.h"

// A visual proxy driven exclusively by the standalone server's player snapshots.
UCLASS()
class UE_STICKARENADEMO_API ASAEnemyCharacter : public ASABaseCharacter
{
	GENERATED_BODY()

public:
	ASAEnemyCharacter();
	void InitializeRemotePlayer(const FSAPlayerRegistration& Registration);
	void ApplyNetworkState(const FSAPlayerNetworkState& State);
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override;
	virtual void ApplyDamageKnockback(const FVector& VelocityChange) override;
	virtual bool IsNetworkProxy() const override { return true; }

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Remote Player")
	FSAPlayerRegistration PlayerRegistration;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Remote Player")
	FSAPlayerNetworkState NetworkState;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category="Remote Player", meta=(ClampMin="0.0"))
	float NetworkInterpSpeed = 15.0f;
	UPROPERTY(EditDefaultsOnly, Category="Remote Player", meta=(ClampMin="0.0"))
	float TeleportDistance = 600.0f;
	UPROPERTY(EditDefaultsOnly, Category="Remote Player", meta=(ClampMin="0.0", ClampMax="0.25"))
	float MaxExtrapolationTime = 0.1f;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Knockback", meta=(ClampMin="1.0"))
	float KnockbackDeceleration = 1400.0f;

private:
	void ApplyWeaponState(const FSAPlayerNetworkState& State);
	bool bHasNetworkState = false;
	bool bAppearanceInitialized = false;
	double LastStateTime = 0.0;
	FVector KnockbackVelocity = FVector::ZeroVector;
	FVector KnockbackOffset = FVector::ZeroVector;
};
