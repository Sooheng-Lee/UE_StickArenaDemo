#include "Components/SACameraOcclusionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

USACameraOcclusionComponent::USACameraOcclusionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Spring-arm movement and the camera update must finish before tracing this frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Palm(TEXT("/Game/Maps/StaticMeshes/SM_Palm.SM_Palm"));
	PalmTreeMesh = Palm.Object;
}

void USACameraOcclusionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	UpdateOcclusion();
}

void USACameraOcclusionComponent::UpdateOcclusion()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!bHideObstacles || !PlayerController || !PlayerController->IsLocalController()
		|| PlayerController->GetViewTarget() != Pawn || !Camera || !Camera->IsActive())
	{
		ClearOcclusion();
		return;
	}
	if (ViewController.Get() != PlayerController)
	{
		ClearOcclusion();
		ViewController = PlayerController;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SACameraOcclusion), true, Pawn);
	Params.bFindInitialOverlaps = true;
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors, true, true);
	Params.AddIgnoredActors(AttachedActors);
	TArray<FHitResult> Hits;
	// Object queries collect every intervening obstacle instead of stopping at the first wall.
	GetWorld()->SweepMultiByObjectType(Hits, Camera->GetComponentLocation(), Pawn->GetActorLocation(),
		FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(FMath::Max(1.0f, ProbeRadius)), Params);
	// A complex mesh sweep can miss a camera wholly inside a closed mesh. Test its solid
	// simple collision at the camera as well, where the original spring arm would retract.
	TArray<FOverlapResult> CameraOverlaps;
	Params.bTraceComplex = false;
	GetWorld()->OverlapMultiByObjectType(CameraOverlaps, Camera->GetComponentLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(FMath::Max(1.0f, ProbeRadius)), Params);

	TSet<TWeakObjectPtr<UPrimitiveComponent>> Occluders;
	const auto ConsiderOccluder = [this, &Occluders](UPrimitiveComponent* Component)
	{
		UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
		AActor* Actor = Mesh ? Mesh->GetOwner() : nullptr;
		if (!Mesh || !Actor || !PalmTreeMesh || Mesh->GetStaticMesh() != PalmTreeMesh
			|| !Mesh->IsVisible() || Mesh->bHiddenInGame
			|| Mesh->GetCollisionResponseToChannel(ECC_Camera) != ECR_Block) return;
		// Other players and their attached equipment must remain visible.
		bool bBelongsToPawn = false;
		for (AActor* Parent = Actor; Parent; Parent = Parent->GetAttachParentActor())
			if (Parent->IsA<APawn>()) { bBelongsToPawn = true; break; }
		if (!bBelongsToPawn) Occluders.Add(Mesh);
	};
	for (const FHitResult& Hit : Hits) ConsiderOccluder(Hit.GetComponent());
	for (const FOverlapResult& Overlap : CameraOverlaps) ConsiderOccluder(Overlap.GetComponent());

	for (auto It = HiddenByOcclusion.CreateIterator(); It; ++It)
	{
		if (!Occluders.Contains(*It))
		{
			PlayerController->HiddenPrimitiveComponents.Remove(*It);
			It.RemoveCurrent();
		}
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& Component : Occluders)
	{
		if (!PlayerController->HiddenPrimitiveComponents.Contains(Component))
		{
			PlayerController->HiddenPrimitiveComponents.Add(Component);
			HiddenByOcclusion.Add(Component);
		}
	}
}

void USACameraOcclusionComponent::ClearOcclusion()
{
	if (APlayerController* PlayerController = ViewController.Get())
		for (const TWeakObjectPtr<UPrimitiveComponent>& Component : HiddenByOcclusion)
			PlayerController->HiddenPrimitiveComponents.Remove(Component);
	HiddenByOcclusion.Reset();
	ViewController.Reset();
}

void USACameraOcclusionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearOcclusion();
	Super::EndPlay(EndPlayReason);
}
