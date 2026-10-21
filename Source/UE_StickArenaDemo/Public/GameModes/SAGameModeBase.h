// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SAGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class UE_STICKARENADEMO_API ASAGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	// Uses the map's current ground and bridge geometry.
	UFUNCTION(BlueprintCallable, Category="Player Spawn")
	bool FindRandomPlayerSpawnTransform(AController* Player, FTransform& OutTransform);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnWaterBeginOverlap(UPrimitiveComponent* Water, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	void KillCharacterInWater(AActor* Actor);

	UPROPERTY(EditDefaultsOnly, Category="Player Spawn", meta=(ClampMin="0.0"))
	float BridgeSpawnClearance = 150.0f;
	UPROPERTY(EditDefaultsOnly, Category="Player Spawn", meta=(ClampMin="0.0"))
	float GroundEdgeClearance = 40.0f;
	UPROPERTY(EditDefaultsOnly, Category="Player Spawn", meta=(ClampMin="0.0"))
	float MinimumPlayerSpawnDistance = 200.0f;
	UPROPERTY(EditDefaultsOnly, Category="Player Spawn", meta=(ClampMin="1", ClampMax="4096"))
	int32 MaxRandomSpawnAttempts = 512;
};
