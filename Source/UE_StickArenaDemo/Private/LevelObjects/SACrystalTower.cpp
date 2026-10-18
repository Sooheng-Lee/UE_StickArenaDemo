#include "LevelObjects/SACrystalTower.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASACrystalTower::ASACrystalTower()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TowerMesh(
		TEXT("/Game/Maps/StaticMeshes/SM_CrystalTower.SM_CrystalTower"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TowerMaterial(
		TEXT("/Game/Maps/MaterialInstances/MI_CrystalTowerHit.MI_CrystalTowerHit"));
	if (TowerMesh.Succeeded()) ObjectMesh->SetStaticMesh(TowerMesh.Object);
	if (TowerMaterial.Succeeded()) ObjectMesh->SetMaterial(0, TowerMaterial.Object);

	CrystalLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CrystalLight"));
	CrystalLight->SetupAttachment(ObjectMesh);
	CrystalLight->SetRelativeLocation(FVector(0.0f, 0.0f, 410.0f));
	CrystalLight->SetMobility(EComponentMobility::Movable);
	CrystalLight->SetLightColor(FLinearColor(0.1f, 0.85f, 1.0f));
	CrystalLight->SetAttenuationRadius(450.0f);
	CrystalLight->SetCastShadows(false);
	CrystalLight->SetIntensity(0.0f);
	CrystalLight->SetVisibility(false);
}

void ASACrystalTower::BeginPlay()
{
	Super::BeginPlay();
	GlowMaterial = ObjectMesh->CreateDynamicMaterialInstance(0);
	SetEffectStrength(0.0f);
}

void ASACrystalTower::PlayHitEffect()
{
	Super::PlayHitEffect();
	PlayEffect();
}

void ASACrystalTower::PlayEffect()
{
	// Each tower owns its material instance; hits cannot brighten other towers.
	if (!GlowMaterial) GlowMaterial = ObjectMesh->CreateDynamicMaterialInstance(0);
	Elapsed = 0.0f;
	bPlaying = true;
	SetEffectStrength(1.0f);
	SetActorTickEnabled(true);
}

void ASACrystalTower::SetEffectStrength(float Strength)
{
	if (GlowMaterial) GlowMaterial->SetScalarParameterValue(TEXT("CrystalGlowStrength"), GlowStrength * Strength);
	CrystalLight->SetIntensity(LightIntensity * Strength);
	CrystalLight->SetVisibility(Strength > 0.0f);
}

void ASACrystalTower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaying) return;
	Elapsed += FMath::Max(DeltaSeconds, 0.0f);
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(EffectDuration, 0.1f), 0.0f, 1.0f);
	SetEffectStrength(FMath::Square(1.0f - Alpha));
	if (Alpha >= 1.0f)
	{
		bPlaying = false;
		SetActorTickEnabled(false);
	}
}
