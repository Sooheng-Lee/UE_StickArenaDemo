#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "LevelObjects/SACrystalTower.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSACrystalTowerEffectTest, "StickArena.LevelObjects.CrystalTowerEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSACrystalTowerEffectTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ASACrystalTower* Tower = World->SpawnActor<ASACrystalTower>();
	ASACrystalTower* Other = World->SpawnActor<ASACrystalTower>();
	if (TestNotNull(TEXT("Crystal tower spawned"), Tower) && TestNotNull(TEXT("Second tower spawned"), Other))
	{
		TestNotNull(TEXT("Default crystal mesh exists"), Tower->GetObjectMesh()->GetStaticMesh().Get());
		ASALevelObject* LevelObject = Tower;
		LevelObject->PlayHitEffect();
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Tower->GetObjectMesh()->GetMaterial(0));
		if (TestNotNull(TEXT("Attack creates per-tower dynamic material"), Material))
		{
			TestEqual(TEXT("Attack starts crystal emission"), Material->K2_GetScalarParameterValue(TEXT("CrystalGlowStrength")), 15.0f);
			TestTrue(TEXT("Point light turns on"), Tower->FindComponentByClass<UPointLightComponent>()->IsVisible());
			TestTrue(TEXT("Other tower material remains independent"), Other->GetObjectMesh()->GetMaterial(0) != Material);
			Tower->Tick(0.5f);
			TestEqual(TEXT("Glow fades halfway through effect"), Material->K2_GetScalarParameterValue(TEXT("CrystalGlowStrength")), 3.75f);
			Tower->PlayEffect();
			TestEqual(TEXT("Direct PlayEffect restarts glow"), Material->K2_GetScalarParameterValue(TEXT("CrystalGlowStrength")), 15.0f);
			Tower->Tick(1.1f);
			TestEqual(TEXT("Emission resets after effect"), Material->K2_GetScalarParameterValue(TEXT("CrystalGlowStrength")), 0.0f);
			TestFalse(TEXT("Light turns off"), Tower->FindComponentByClass<UPointLightComponent>()->IsVisible());
			TestFalse(TEXT("Idle tower does not tick"), Tower->IsActorTickEnabled());
		}
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
