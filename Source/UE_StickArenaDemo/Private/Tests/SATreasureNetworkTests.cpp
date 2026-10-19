#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/SAUdpClientComponent.h"
#include "LevelObjects/SATreasureBox.h"
#include "Items/SAWeaponActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Characters/SABaseCharacter.h"
#include "Components/SAStatusComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSATreasureNetworkTest, "StickArena.Networking.TreasureBox",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSATreasureNetworkTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    AActor* Owner = World->SpawnActor<AActor>();
    USAUdpClientComponent* Client = NewObject<USAUdpClientComponent>(Owner);
    Client->RegisterComponent();
    const auto State = [](int32 Hits) { return FString::Printf(
        TEXT("SA4|type=TREASURE_STATE|epoch=100|id=1|item=0|hits=%d|pos=100,200,30"), Hits); };
    Client->ReceiveTreasure(State(0));
    ASATreasureBox* Box = Cast<ASATreasureBox>(Client->TreasureActors.FindRef(TEXT("1")));
    if (TestNotNull(TEXT("Network packet spawns treasure box"), Box))
    {
        TestEqual(TEXT("Sword enum"), Box->SpawnItem, ESATreasureItem::Sword);
        TestTrue(TEXT("Packet coordinates"), Box->GetActorLocation().Equals(FVector(100,200,30)));
        Client->ReceiveTreasure(State(0));
        TestTrue(TEXT("Duplicate spawn is idempotent"), Client->TreasureActors.FindRef(TEXT("1")) == Box);
        Client->ReceiveTreasure(State(2));
        TestEqual(TEXT("Two hits preserve the box"), Box->HitCount, 2);
        Client->ReceiveTreasure(State(1));
        TestEqual(TEXT("Out-of-order hit state ignored"), Box->HitCount, 2);
        Client->ReceiveTreasure(State(3));
        AActor* Item = Client->TreasureActors.FindRef(TEXT("1"));
        if (TestNotNull(TEXT("Third hit spawns sword blueprint"), Cast<ASAWeaponActor>(Item)))
        {
            TestTrue(TEXT("Item at box position"), Item->GetActorLocation().Equals(FVector(100,200,30)));
            Client->ReceiveTreasure(State(0));
            Client->ReceiveTreasure(State(3));
            TestTrue(TEXT("Repeated/reordered packets do not recreate box or item"), Client->TreasureActors.FindRef(TEXT("1")) == Item);
        }
        Client->ClearTreasure();
        Client->ReceiveTreasure(State(3));
        TestNotNull(TEXT("Late join restores dropped sword without box"), Cast<ASAWeaponActor>(Client->TreasureActors.FindRef(TEXT("1"))));
		AActor* GroundItem = Client->TreasureActors.FindRef(TEXT("1"));
		const FString Collected = State(3) + TEXT("|owner=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		Client->ReceiveTreasure(Collected);
		TestTrue(TEXT("Another player's pickup destroys ground item"), GroundItem->IsActorBeingDestroyed());
		TestNull(TEXT("Collected item removed from ground registry"), Client->TreasureActors.FindRef(TEXT("1")));
		Client->ReceiveTreasure(State(3));
		TestNull(TEXT("Delayed drop snapshot cannot revive collected item"), Client->TreasureActors.FindRef(TEXT("1")));
		Client->ClearTreasure();
		Client->ReceiveTreasure(Collected);
		Client->ReceiveTreasure(State(3));
		TestNull(TEXT("Late join never creates collected ground item"), Client->TreasureActors.FindRef(TEXT("1")));

		UClass* PlayerClass = LoadClass<ASABaseCharacter>(nullptr, TEXT("/Game/PlayerCharacters/TypeA/BP_TypeA.BP_TypeA_C"));
		ASABaseCharacter* Player = World->SpawnActor<ASABaseCharacter>(PlayerClass, FVector(5000,0,100), FRotator::ZeroRotator);
		USAUdpClientComponent* LocalClient = Player->FindComponentByClass<USAUdpClientComponent>();
		LocalClient->SessionId = TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		LocalClient->ReceiveTreasure(State(3));
		TestNull(TEXT("Network drop does not equip without grant"), Player->GetWeapon());
		LocalClient->ReceiveTreasure(Collected);
		ASAWeaponActor* Equipped = Player->GetWeapon();
		if (TestNotNull(TEXT("Server winner equips dropped weapon"), Equipped))
		{
			TestTrue(TEXT("Granted weapon is attached to winner"), Equipped->GetAttachParentActor() == Player);
			Player->GetStatusComponent()->Ammo = 1;
			LocalClient->ReceiveTreasure(Collected);
			TestEqual(TEXT("Repeated grant does not reset ammo"), Player->GetStatusComponent()->Ammo, 1);
			TestTrue(TEXT("Repeated grant preserves equipped instance"), Player->GetWeapon() == Equipped);
			Player->SetWeapon(nullptr);
			Equipped->Destroy();
			LocalClient->ReceiveTreasure(Collected);
			TestNull(TEXT("Collected item cannot return after weapon expires"), Player->GetWeapon());
		}
		LocalClient->ClearTreasure();
		LocalClient->ReceiveTreasure(Collected);
		TestNotNull(TEXT("Grant arriving before drop snapshot still equips winner"), Player->GetWeapon());
		ASAWeaponActor* LatestWeapon = Player->GetWeapon();
		LocalClient->LastAppliedPickupSequence = 2;
		LocalClient->RequestedPickupSequences.Add(TEXT("2"), 1);
		LocalClient->ReceiveTreasure(TEXT("SA4|type=TREASURE_STATE|epoch=100|id=2|item=0|hits=3|pos=0,0,0|owner=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
		TestTrue(TEXT("Delayed older grant cannot replace a newer weapon"), Player->GetWeapon() == LatestWeapon);
        Client->ReceiveTreasure(TEXT("SA4|type=TREASURE_STATE|epoch=101|id=1|item=0|hits=0|pos=0,0,0"));
        Client->ReceiveTreasure(State(3));
        TestNotNull(TEXT("Server restart retires previous epoch"), Cast<ASATreasureBox>(Client->TreasureActors.FindRef(TEXT("1"))));
    }
    Client->ClearTreasure();
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
