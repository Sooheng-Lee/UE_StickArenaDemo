// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/SAItemActor.h"
#include "Components/SphereComponent.h"
#include "SADebugHelper.h"
// Sets default values
ASAItemActor::ASAItemActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	ItemMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Item Mesh"));
	SetRootComponent(ItemMeshComponent);
	DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Detection Sphere"));
	DetectionSphere->SetupAttachment(RootComponent);
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionSphere->OnComponentBeginOverlap.AddDynamic(this, &ASAItemActor::OnDetectionBeginOverlap);
}

void ASAItemActor::SetItemState(const EItemState& State)
{
	ItemState = State;
}

// Called when the game starts or when spawned
void ASAItemActor::BeginPlay()
{
	Super::BeginPlay();
	ItemMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASAItemActor::OwnItem(AActor* NewOwner)
{
	SetItemState(EItemState::Owned);
}

// Called every frame
void ASAItemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	switch (ItemState)
	{
		case EItemState::Unowned:
		{
			AddActorLocalRotation(FRotator(0.0f, RotationSpeed * DeltaTime, 0.0f));
			break;
		}
		case EItemState::Owned:
		{

			break;
		}
	}

}

void ASAItemActor::OnDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	OwnItem(OtherActor);
}

