#include "LevelObjects/SALevelObject.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ASALevelObject::ASALevelObject()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	ObjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObjectMesh"));
	ObjectMesh->SetupAttachment(GetRootComponent());
	ObjectMesh->SetMobility(EComponentMobility::Movable);
	ObjectMesh->SetCollisionProfileName(TEXT("BlockAll"));
	ObjectMesh->SetGenerateOverlapEvents(true);
}

void ASALevelObject::PlayHitEffect()
{
}

void ASALevelObject::RequestHit(AActor* Attacker)
{
	PlayHitEffect();
}
