#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "LevelObjects/SACrystalTower.h"
#include "Characters/SABaseCharacter.h"
#include "Components/SAStatusComponent.h"
#include "Components/SAUdpClientComponent.h"
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
	ASABaseCharacter* Character = World->SpawnActor<ASABaseCharacter>();
	USAUdpClientComponent* Client = NewObject<USAUdpClientComponent>(Character);
	Client->RegisterComponent();
	Client->SessionId = TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	const FString Id = Tower->GetFName().ToString();
	const auto State = [&](int32 Revision, int32 Cycle, int32 Hits, bool Lit, FString Epoch = TEXT("100"))
	{
		return FString::Printf(TEXT("SA5|type=CRYSTAL_STATE|epoch=%s|id=%s|rev=%d|cycle=%d|hits=%d|lit=%d|winner=%s|effect=1|remaining=20.000000"),
			*Epoch, *Id, Revision, Cycle, Hits, Lit ? 1 : 0, Lit ? TEXT("") : *Client->SessionId);
	};
	if (TestNotNull(TEXT("Tower spawned"), Tower) && TestNotNull(TEXT("Character spawned"), Character))
	{
		Client->ReceiveCrystal(State(1,0,0,true));
		TestTrue(TEXT("Server initial state lights tower"), Tower->bLit);
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Tower->GetObjectMesh()->GetMaterial(0));
		const FQuat Rest = Tower->GetObjectMesh()->GetRelativeRotation().Quaternion();
		Client->ReceiveCrystal(State(2,0,1,true));
		Tower->Tick(0.05f);
		TestFalse(TEXT("Hit shakes the mesh"), Tower->GetObjectMesh()->GetRelativeRotation().Quaternion().Equals(Rest));
		TestTrue(TEXT("Hit flickers glow"), Material->K2_GetScalarParameterValue(TEXT("CrystalGlowStrength")) < 15.0f);
		Tower->Tick(2.0f);
		TestTrue(TEXT("Mesh returns to rest"), Tower->GetObjectMesh()->GetRelativeRotation().Quaternion().Equals(Rest));
		TestEqual(TEXT("Lit tower remains glowing after effect"), Material->K2_GetScalarParameterValue(TEXT("CrystalGlowStrength")),15.0f);
		TestTrue(TEXT("Other tower material is independent"), Other->GetObjectMesh()->GetMaterial(0) != Material);
		Client->ReceiveCrystal(State(5,0,4,true));
		TestTrue(TEXT("Fourth hit keeps tower lit"), Tower->bLit);
		const float BaseAttack = Character->GetStatusComponent()->DefaultDamage;
		Client->ReceiveCrystal(State(6,0,5,false));
		TestFalse(TEXT("Fifth hit extinguishes light"), Tower->FindComponentByClass<UPointLightComponent>()->IsVisible());
		TestEqual(TEXT("Final hitter receives server-selected attack bonus"),Character->GetStatusComponent()->DefaultDamage,BaseAttack+5.0f);
		Client->ReceiveCrystal(State(6,0,5,false));
		Client->ReceiveCrystal(State(5,0,4,true));
		TestFalse(TEXT("Duplicate and stale states cannot relight"), Tower->bLit);
		TestEqual(TEXT("Duplicate state does not stack bonus"), Character->GetStatusComponent()->DefaultDamage,BaseAttack+5.0f);
		Tower->Tick(40.0f);
		TestFalse(TEXT("Local elapsed time cannot relight tower"), Tower->bLit);
		Client->ReceiveCrystal(State(7,1,0,true));
		TestTrue(TEXT("Only server reset relights"),Tower->bLit);
		TestEqual(TEXT("Relight resets count"),Tower->HitCount,0);
		Client->ReceiveCrystal(State(1,0,0,true,TEXT("101")));
		Client->ReceiveCrystal(State(6,0,5,false));
		TestTrue(TEXT("Old server epoch cannot restore extinguished state"),Tower->bLit);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSACrystalBlessingTest, "StickArena.Status.CrystalBlessings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSACrystalBlessingTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ASABaseCharacter* Character = World->SpawnActor<ASABaseCharacter>();
	USAStatusComponent* Status = Character->GetStatusComponent();
	Status->HP = 50.0f;
	Status->ApplyCrystalBlessing(ESACrystalBlessing::Healing);
	Status->TickComponent(2.5f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Heal grants ten HP halfway through five seconds"), Status->HP,60.0f);
	Status->TickComponent(3.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Heal is capped at twenty total"), Status->HP,70.0f);
	Status->TickComponent(5.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Finished heal stops"), Status->HP,70.0f);
	Status->HP = 99.0f;
	Status->ApplyCrystalBlessing(ESACrystalBlessing::Healing);
	Status->TickComponent(5.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Healing never exceeds MaxHP"),Status->HP,100.0f);
	Status->ApplyCrystalBlessing(ESACrystalBlessing::Attack);
	Status->ApplyCrystalBlessing(ESACrystalBlessing::Defense);
	TestEqual(TEXT("Attack bonus"),Status->DefaultDamage,10.0f);
	TestEqual(TEXT("Defense bonus"),Status->Defense,7.0f);
	Status->TickComponent(10.0f, LEVELTICK_All, nullptr);
	Status->ApplyCrystalBlessing(ESACrystalBlessing::Attack);
	Status->DefaultDamage += 2.0f;
	Status->TickComponent(10.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Expired bonus preserves newer buff and base stat changes"),Status->DefaultDamage,12.0f);
	TestEqual(TEXT("Defense expires after twenty seconds"),Status->Defense,2.0f);
	Status->TickComponent(10.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Second attack buff expires independently"),Status->DefaultDamage,7.0f);
	Status->ApplyCrystalBlessing(ESACrystalBlessing::Healing);
	Status->HP = 0.0f;
	Status->TickComponent(5.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Healing cannot revive dead character"),Status->HP,0.0f);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
