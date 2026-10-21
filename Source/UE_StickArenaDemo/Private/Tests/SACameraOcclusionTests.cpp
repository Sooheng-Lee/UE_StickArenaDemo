#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Characters/SAPlayerCharacter.h"
#include "Components/SACameraOcclusionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSACameraOcclusionTest, "StickArena.Camera.ObstacleVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSACameraOcclusionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	ASAPlayerCharacter* Player = World->SpawnActor<ASAPlayerCharacter>(FVector(0, 0, 200), FRotator::ZeroRotator);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->Possess(Player);
	Controller->SetViewTarget(Player);
	APlayerController* OtherViewer = World->SpawnActor<APlayerController>();
	UCameraComponent* Camera = Player->FindComponentByClass<UCameraComponent>();
	const FVector CameraLocation(-600, 0, 800);
	Camera->SetWorldLocation(CameraLocation);
	USACameraOcclusionComponent* Occlusion = Player->FindComponentByClass<USACameraOcclusionComponent>();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Palm = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Maps/StaticMeshes/SM_Palm.SM_Palm"));
	TestNotNull(TEXT("Actual level palm mesh exists"), Palm);
	const auto MeshAt = [World, Cube](AActor* Actor, FVector Location, UStaticMesh* Asset = nullptr)
	{
		Asset = Asset ? Asset : Cube;
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor);
		Actor->SetRootComponent(Mesh);
		Actor->AddInstanceComponent(Mesh);
		Mesh->SetStaticMesh(Asset);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionObjectType(ECC_WorldStatic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);
		Mesh->RegisterComponent();
		// Normalize real palm geometry around the probe without depending on its art pivot.
		const float Scale = 25.0f / FMath::Max(1.0f, Asset->GetBounds().BoxExtent.GetMax());
		Mesh->SetWorldScale3D(FVector(Scale));
		Mesh->SetWorldLocation(Location - Asset->GetBounds().Origin * Scale);
		return Mesh;
	};
	UStaticMeshComponent* Near = MeshAt(World->SpawnActor<AActor>(), FMath::Lerp(CameraLocation, Player->GetActorLocation(), 0.3f), Palm);
	UStaticMeshComponent* Far = MeshAt(World->SpawnActor<AActor>(), FMath::Lerp(CameraLocation, Player->GetActorLocation(), 0.7f), Palm);
	UStaticMeshComponent* Rock = MeshAt(World->SpawnActor<AActor>(), FMath::Lerp(CameraLocation, Player->GetActorLocation(), 0.4f));
	UStaticMeshComponent* Unrelated = MeshAt(World->SpawnActor<AActor>(), FVector(2000, 2000, 2000));
	UStaticMeshComponent* OtherCharacter = MeshAt(World->SpawnActor<APawn>(), FMath::Lerp(CameraLocation, Player->GetActorLocation(), 0.5f), Palm);
	Controller->HiddenPrimitiveComponents.Add(Unrelated);
	Occlusion->UpdateOcclusion();
	TestTrue(TEXT("Nearest obstacle hidden"), Controller->HiddenPrimitiveComponents.Contains(Near));
	TestTrue(TEXT("Obstacle behind another obstacle also hidden"), Controller->HiddenPrimitiveComponents.Contains(Far));
	TestFalse(TEXT("Non-palm obstacle stays rendered"), Controller->HiddenPrimitiveComponents.Contains(Rock));
	TestFalse(TEXT("Other character remains visible"), Controller->HiddenPrimitiveComponents.Contains(OtherCharacter));
	TestTrue(TEXT("Visibility is local to this viewer"), OtherViewer->HiddenPrimitiveComponents.IsEmpty());
	TestTrue(TEXT("Mesh global visibility unchanged"), Near->IsVisible() && !Near->bHiddenInGame);
	TestEqual(TEXT("Physical collision remains enabled"), Near->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	TSet<FPrimitiveComponentId> HiddenIds;
	Controller->BuildHiddenComponentList(CameraLocation, HiddenIds);
	TestTrue(TEXT("Renderer hidden list contains obstacle"), HiddenIds.Contains(Near->GetPrimitiveSceneId()));
	Near->SetWorldLocation(FVector(3000, 3000, 3000));
	Occlusion->UpdateOcclusion();
	TestFalse(TEXT("Leaving line of sight restores obstacle"), Controller->HiddenPrimitiveComponents.Contains(Near));
	TestTrue(TEXT("Remaining occluder stays hidden"), Controller->HiddenPrimitiveComponents.Contains(Far));
	Near->SetWorldLocation(CameraLocation - Palm->GetBounds().Origin * Near->GetComponentScale().X);
	Occlusion->UpdateOcclusion();
	TestTrue(TEXT("Camera inside obstacle is handled"), Controller->HiddenPrimitiveComponents.Contains(Near));
	Occlusion->bHideObstacles = false;
	Occlusion->UpdateOcclusion();
	TestFalse(TEXT("Disabling restores occlusion-owned entries"), Controller->HiddenPrimitiveComponents.Contains(Far));
	TestTrue(TEXT("Unrelated hidden entry preserved"), Controller->HiddenPrimitiveComponents.Contains(Unrelated));
	Occlusion->bHideObstacles = true;
	Occlusion->UpdateOcclusion();
	Controller->UnPossess();
	TestFalse(TEXT("Unpossess restores obstacles immediately"), Controller->HiddenPrimitiveComponents.Contains(Near));
	TestTrue(TEXT("Unpossess preserves unrelated visibility rules"), Controller->HiddenPrimitiveComponents.Contains(Unrelated));
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
