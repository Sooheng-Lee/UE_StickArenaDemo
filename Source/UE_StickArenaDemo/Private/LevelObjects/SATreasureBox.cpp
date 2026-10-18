#include "LevelObjects/SATreasureBox.h"
#include "Components/SAUdpClientComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ASATreasureBox::ASATreasureBox()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TreasureMesh(
		TEXT("/Game/Maps/StaticMeshes/SM_TreasureBox.SM_TreasureBox"));
	if (TreasureMesh.Succeeded()) ObjectMesh->SetStaticMesh(TreasureMesh.Object);
}

void ASATreasureBox::PlayHitEffect()
{
	Super::PlayHitEffect();
	if (bPlaying || !IsValid(ObjectMesh) || ObjectMesh->IsSimulatingPhysics()) return;
	RestRotation = ObjectMesh->GetRelativeRotation().Quaternion();
	Elapsed = 0.0f;
	bPlaying = true;
	SetActorTickEnabled(true);
}

void ASATreasureBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaying) return;
	Elapsed += FMath::Max(DeltaSeconds, 0.0f);
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(Duration, 0.1f), 0.0f, 1.0f);
	if (Alpha >= 1.0f)
	{
		ObjectMesh->SetRelativeRotation(RestRotation);
		bPlaying = false;
		SetActorTickEnabled(false);
		return;
	}
	const float Angle = FMath::DegreesToRadians(AngleDegrees)
		* FMath::Sin(Alpha * 2.0f * PI * Oscillations) * FMath::Square(1.0f - Alpha);
	ObjectMesh->SetRelativeRotation(RestRotation * FQuat(FVector::ForwardVector, Angle));
}

void ASATreasureBox::InitializeNetworkBox(const FString& Id, ESATreasureItem Item)
{
    NetworkId = Id;
    SpawnItem = Item;
}

void ASATreasureBox::ApplyNetworkHits(int32 Hits)
{
    if (Hits > HitCount) PlayHitEffect();
    HitCount = Hits;
}

void ASATreasureBox::RequestHit(AActor* Attacker)
{
    const APawn* Pawn = Cast<APawn>(Attacker);
    if (!Pawn || !Pawn->IsLocallyControlled()) return;
    if (USAUdpClientComponent* Client = Attacker->FindComponentByClass<USAUdpClientComponent>())
        Client->SendTreasureHit(NetworkId);
}
