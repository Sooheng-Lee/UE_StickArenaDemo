// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SAItemActor.generated.h"

class USphereComponent;
UENUM(BlueprintType)
enum class EItemState : uint8
{
	Unowned,
	Owned,
};

UCLASS()
class UE_STICKARENADEMO_API ASAItemActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASAItemActor();
	void SetItemState(const EItemState& State);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Components")
	UStaticMeshComponent* ItemMeshComponent;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Components")
	USphereComponent* DetectionSphere;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Configs")
	float RotationSpeed = 0.0f;
	UPROPERTY(VisibleInstanceOnly)
	EItemState ItemState = EItemState::Unowned;

private:
	UFUNCTION()
	void OnDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

};
