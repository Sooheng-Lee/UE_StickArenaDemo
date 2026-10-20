#include "LevelObjects/SACrystalTower.h"
#include "Characters/SABaseCharacter.h"
#include "Components/SAStatusComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/SAUdpClientComponent.h"
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
	RestRotation = ObjectMesh->GetRelativeRotation().Quaternion();
	GlowMaterial = ObjectMesh->CreateDynamicMaterialInstance(0);
	SetEffectStrength(bLit ? 1.0f : 0.0f);
}

void ASACrystalTower::RequestHit(AActor* Attacker)
{
	ASABaseCharacter* Character = Cast<ASABaseCharacter>(Attacker);
	if (!bLit || !IsValid(Character) || !Character->IsLocallyControlled()
		|| Character->GetCharacterState() == ECharacterState::Dead) return;
	if (USAUdpClientComponent* Client = Character->FindComponentByClass<USAUdpClientComponent>())
		Client->SendCrystalHit(GetFName().ToString());
}

void ASACrystalTower::ApplyNetworkState(int32 Hits, bool bIsLit)
{
	const bool bWasLit = bLit;
	const int32 PreviousHits = HitCount;
	bLit = bIsLit;
	HitCount = Hits;
	if (!GlowMaterial) GlowMaterial = ObjectMesh->CreateDynamicMaterialInstance(0);
	if (bWasLit && Hits > PreviousHits)
	{
		bLit = true;
		PlayHitEffect();
		bLit = bIsLit;
	}
	SetEffectStrength(bLit ? 1.0f : 0.0f);
}

void ASACrystalTower::PlayHitEffect()
{
	Super::PlayHitEffect();
	PlayEffect();
}

void ASACrystalTower::PlayEffect()
{
	if (!bLit) return;
	if (!GlowMaterial) GlowMaterial = ObjectMesh->CreateDynamicMaterialInstance(0);
	if (!bPlaying) RestRotation = ObjectMesh->GetRelativeRotation().Quaternion();
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
	const float Wave = FMath::Sin(Alpha * 6.0f * PI);
	ObjectMesh->SetRelativeRotation(RestRotation * FQuat(FVector::ForwardVector,
		FMath::DegreesToRadians(ShakeAngleDegrees) * Wave * FMath::Square(1.0f - Alpha)));
	SetEffectStrength(bLit ? (0.15f + 0.85f * FMath::Square(FMath::Cos(Alpha * 6.0f * PI))) : 0.0f);
	if (Alpha >= 1.0f)
	{
		ObjectMesh->SetRelativeRotation(RestRotation);
		SetEffectStrength(bLit ? 1.0f : 0.0f);
		bPlaying = false;
		SetActorTickEnabled(false);
	}
}
