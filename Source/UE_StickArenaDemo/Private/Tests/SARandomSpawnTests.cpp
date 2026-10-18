#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameModes/SAGameModeBase.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Script.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSARandomOuterSpawnTest, "StickArena.Spawning.MainMapOutsideBridges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSARandomOuterSpawnTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/Main"));
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!TestNotNull(TEXT("Main map loaded"), World)) return false;
	// Exercise BlueprintNativeEvents against the configured class in this editor test world.
	FEditorScriptExecutionGuard ScriptGuard;
	UClass* ModeClass = LoadClass<ASAGameModeBase>(nullptr, TEXT("/Game/GameModes/BP_SAGameModeBase.BP_SAGameModeBase_C"));
	if (!TestNotNull(TEXT("Configured GameMode class loaded"), ModeClass)) return false;
	ASAGameModeBase* Mode = World->SpawnActor<ASAGameModeBase>(ModeClass);
	if (!TestNotNull(TEXT("GameMode created"), Mode)) return false;

	FVector ArenaCenter = FVector::ZeroVector;
	int32 BridgeCount = 0;
	for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		for (const UStaticMeshComponent* Component : Components)
		{
			if (Component->GetStaticMesh() && Component->GetStaticMesh()->GetFName() == FName(TEXT("SM_WoodBridge")))
			{
				ArenaCenter += Component->Bounds.Origin;
				++BridgeCount;
			}
		}
	}
	TestEqual(TEXT("Current Main map contains six bridges"), BridgeCount, 6);
	if (BridgeCount == 0) { World->DestroyActor(Mode); return false; }
	ArenaCenter /= BridgeCount;
	TSet<int32> SpawnSectors;
	for (int32 Index = 0; Index < 128; ++Index)
	{
		FTransform Transform;
		if (!TestTrue(TEXT("Safe random spawn found"), Mode->FindRandomPlayerSpawnTransform(nullptr, Transform))) break;
		const FVector Position = Transform.GetLocation();
		// The six bridges in Main terminate about 1900cm from the center.
		TestTrue(TEXT("Spawn lies on the outer side of the bridge ring"), FVector::Dist2D(ArenaCenter, Position) > 2000.0);
		const FVector Delta = Position - ArenaCenter;
		const double Angle = FMath::Atan2(Delta.Y, Delta.X) + 2 * UE_DOUBLE_PI + UE_DOUBLE_PI / 6;
		SpawnSectors.Add(FMath::FloorToInt(FMath::Fmod(Angle, 2 * UE_DOUBLE_PI) / (UE_DOUBLE_PI / 3)));
		FHitResult Floor;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SASpawnTest), false);
		const bool bHit = World->LineTraceSingleByChannel(Floor, Position, Position - FVector(0, 0, 500), ECC_Visibility, Query);
		const UStaticMeshComponent* Ground = Cast<UStaticMeshComponent>(Floor.GetComponent());
		TestTrue(TEXT("Spawn is supported by terrain rather than water or a bridge"), bHit && Ground && Ground->GetStaticMesh()
			&& Ground->GetStaticMesh()->GetFName() == FName(TEXT("SM_Ground")));
		if (Ground)
			for (UMaterialInterface* Material : Ground->GetMaterials())
				TestFalse(TEXT("Spawn is not on a boundary rock"), Material && Material->GetFName() == FName(TEXT("MI_Arena_Rock")));
	}
	TestEqual(TEXT("Random samples cover all six outer regions"), SpawnSectors.Num(), 6);

	TArray<APawn*> Players;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		APawn* Pawn = Mode->SpawnDefaultPawnAtTransform(nullptr, FTransform::Identity);
		if (!TestNotNull(TEXT("Player spawned through the normal GameMode path"), Pawn)) break;
		TestTrue(TEXT("Actual pawn spawns outside the bridges"), FVector::Dist2D(Pawn->GetActorLocation(), ArenaCenter) > 2000.0);
		for (const APawn* Other : Players)
			TestTrue(TEXT("Player spawn spacing is preserved"), FVector::Dist2D(Pawn->GetActorLocation(), Other->GetActorLocation()) >= 200.0);
		Players.Add(Pawn);
	}
	TestEqual(TEXT("Sixteen simultaneous player spawns succeeded"), Players.Num(), 16);
	for (APawn* Pawn : Players) World->DestroyActor(Pawn);
	World->DestroyActor(Mode);
	return true;
}

#endif
