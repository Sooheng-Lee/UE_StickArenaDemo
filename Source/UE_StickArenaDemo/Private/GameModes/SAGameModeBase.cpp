// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/SAGameModeBase.h"
#include "Characters/SABaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSASpawn, Log, All);

void ASAGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			if (!Component->GetStaticMesh() || Component->GetStaticMesh()->GetFName() != FName(TEXT("SM_Water"))) continue;
			// The visual water mesh has no usable simple overlap collision.
			// Its rectangular local bounds provide a volume that follows the Water actor.
			FVector Minimum, Maximum;
			Component->GetLocalBounds(Minimum, Maximum);
			UBoxComponent* Trigger = NewObject<UBoxComponent>(*Actor, TEXT("WaterDeathTrigger"));
			Actor->AddInstanceComponent(Trigger);
			Trigger->SetupAttachment(Component);
			Trigger->SetRelativeLocation((Minimum + Maximum) * 0.5f);
			Trigger->SetBoxExtent(((Maximum - Minimum) * 0.5f).ComponentMax(FVector(1.0f)));
			Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Trigger->SetCollisionObjectType(ECC_WorldDynamic);
			Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
			Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			Trigger->SetGenerateOverlapEvents(true);
			Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ASAGameModeBase::OnWaterBeginOverlap);
			Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			Trigger->RegisterComponent();
			// Include characters already touching water when play begins.
			TArray<AActor*> Overlapping;
			Trigger->GetOverlappingActors(Overlapping, ASABaseCharacter::StaticClass());
			for (AActor* Overlap : Overlapping) KillCharacterInWater(Overlap);
		}
	}
}

void ASAGameModeBase::OnWaterBeginOverlap(UPrimitiveComponent* Water, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const ASABaseCharacter* Character = Cast<ASABaseCharacter>(OtherActor);
	if (Character && OtherComponent == Character->GetCapsuleComponent()) KillCharacterInWater(OtherActor);
}

void ASAGameModeBase::KillCharacterInWater(AActor* Actor)
{
	ASABaseCharacter* Character = Cast<ASABaseCharacter>(Actor);
	// Network proxies receive death/respawn from their owning client.
	if (Character) Character->DieInWater();
}

namespace
{
	bool IsSpawnGround(const UStaticMeshComponent& Component)
	{
		if (Component.ComponentHasTag(TEXT("SAPlayerSpawnGround")) || Component.GetOwner()->ActorHasTag(TEXT("SAPlayerSpawnGround"))) return true;
		if (!Component.GetStaticMesh() || Component.GetStaticMesh()->GetFName() != FName(TEXT("SM_Ground"))) return false;
		// Boundary rocks reuse SM_Ground, but are obstacles rather than spawn terrain.
		for (UMaterialInterface* Material : Component.GetMaterials())
			if (Material && Material->GetFName() == FName(TEXT("MI_Arena_Rock"))) return false;
		return true;
	}

	bool IsBridge(const UStaticMeshComponent& Component)
	{
		return Component.ComponentHasTag(TEXT("SAPlayerSpawnBridge")) || Component.GetOwner()->ActorHasTag(TEXT("SAPlayerSpawnBridge"))
			|| (Component.GetStaticMesh() && Component.GetStaticMesh()->GetFName() == FName(TEXT("SM_WoodBridge")));
	}
}

APawn* ASAGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FTransform RandomTransform;
	if (FindRandomPlayerSpawnTransform(NewPlayer, RandomTransform))
	{
		APawn* Pawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, RandomTransform);
		if (Pawn) UE_LOG(LogSASpawn, Log, TEXT("Player spawned outside bridges at %s"), *Pawn->GetActorLocation().ToString());
		return Pawn;
	}
	UE_LOG(LogSASpawn, Warning, TEXT("No safe random ground spawn found; using the supplied player start."));
	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
}

bool ASAGameModeBase::FindRandomPlayerSpawnTransform(AController* Player, FTransform& OutTransform)
{
	UWorld* World = GetWorld();
	if (!World) return false;

	TSet<const UPrimitiveComponent*> GroundComponents;
	TArray<const UStaticMeshComponent*> Bridges;
	FBox GroundBounds(ForceInit);
	FVector ArenaCenter = FVector::ZeroVector;
	for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		for (const UStaticMeshComponent* Component : Components)
		{
			if (!Component->GetStaticMesh() || !Component->IsQueryCollisionEnabled()) continue;
			if (IsBridge(*Component))
			{
				Bridges.Add(Component);
				ArenaCenter += Component->Bounds.Origin;
			}
			else if (IsSpawnGround(*Component))
			{
				GroundComponents.Add(Component);
				GroundBounds += Component->Bounds.GetBox();
			}
		}
	}
	if (!GroundBounds.IsValid || GroundComponents.IsEmpty()) return false;
	ArenaCenter = Bridges.IsEmpty() ? GroundBounds.GetCenter() : ArenaCenter / Bridges.Num();

	const UClass* PawnClass = GetDefaultPawnClassForController(Player);
	const ACharacter* CharacterDefaults = PawnClass ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr;
	const UCapsuleComponent* Capsule = CharacterDefaults ? CharacterDefaults->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* Movement = CharacterDefaults ? CharacterDefaults->GetCharacterMovement() : nullptr;
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.0f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.0f;
	const float WalkableFloorZ = Movement ? Movement->GetWalkableFloorZ() : 0.71f;
	const float MaxStepHeight = Movement ? Movement->MaxStepHeight : 45.0f;
	const FName CollisionProfile = Capsule ? Capsule->GetCollisionProfileName() : FName(TEXT("Pawn"));

	// Use transformed local corners so rotated bridges don't inflate the exclusion radius.
	double BridgeOuterRadius = 0.0;
	for (const UStaticMeshComponent* Bridge : Bridges)
	{
		const FBox LocalBounds = Bridge->GetStaticMesh()->GetBoundingBox();
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector LocalCorner(Corner & 1 ? LocalBounds.Max.X : LocalBounds.Min.X,
				Corner & 2 ? LocalBounds.Max.Y : LocalBounds.Min.Y, Corner & 4 ? LocalBounds.Max.Z : LocalBounds.Min.Z);
			BridgeOuterRadius = FMath::Max(BridgeOuterRadius,
				FVector::Dist2D(ArenaCenter, Bridge->GetComponentTransform().TransformPosition(LocalCorner)));
		}
	}
	const double MinimumRadius = Bridges.IsEmpty() ? 0.0 : BridgeOuterRadius + BridgeSpawnClearance + Radius;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SAPlayerSpawn), false);
	if (Player && Player->GetPawn()) Query.AddIgnoredActor(Player->GetPawn());
	const auto HasGround = [&](const FVector& From, const FVector& To, FHitResult& Hit)
	{
		return World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query)
			&& GroundComponents.Contains(Hit.GetComponent()) && Hit.ImpactNormal.Z >= WalkableFloorZ;
	};

	for (int32 Attempt = 0; Attempt < FMath::Clamp(MaxRandomSpawnAttempts, 1, 4096); ++Attempt)
	{
		FVector Candidate(FMath::FRandRange(GroundBounds.Min.X, GroundBounds.Max.X),
			FMath::FRandRange(GroundBounds.Min.Y, GroundBounds.Max.Y), 0.0);
		if (FVector::Dist2D(Candidate, ArenaCenter) < MinimumRadius) continue;
		FHitResult Floor;
		if (!HasGround(FVector(Candidate.X, Candidate.Y, GroundBounds.Max.Z + HalfHeight + 100.0),
			FVector(Candidate.X, Candidate.Y, GroundBounds.Min.Z - 100.0), Floor)) continue;
		bool bSupported = true;
		for (int32 Direction = 0; Direction < 8; ++Direction)
		{
			const double Angle = Direction * UE_DOUBLE_PI / 4.0;
			const FVector Offset(FMath::Cos(Angle) * (Radius + GroundEdgeClearance), FMath::Sin(Angle) * (Radius + GroundEdgeClearance), 0);
			FHitResult Support;
			if (!HasGround(Floor.ImpactPoint + Offset + FVector(0, 0, MaxStepHeight + 2),
				Floor.ImpactPoint + Offset - FVector(0, 0, MaxStepHeight + 2), Support))
			{
				bSupported = false;
				break;
			}
		}
		if (!bSupported) continue;
		Candidate = Floor.ImpactPoint + FVector(0, 0, HalfHeight + 2.5f);
		if (World->OverlapBlockingTestByProfile(Candidate, FQuat::Identity, CollisionProfile,
			FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) continue;
		bool bNearPlayer = false;
		for (TActorIterator<APawn> Pawn(World); Pawn; ++Pawn)
		{
			if (Player && *Pawn == Player->GetPawn()) continue;
			if (FVector::Dist2D(Pawn->GetActorLocation(), Candidate) < MinimumPlayerSpawnDistance)
			{
				bNearPlayer = true;
				break;
			}
		}
		if (bNearPlayer) continue;
		OutTransform = FTransform(FRotator(0, (ArenaCenter - Candidate).Rotation().Yaw, 0), Candidate);
		return true;
	}
	return false;
}

