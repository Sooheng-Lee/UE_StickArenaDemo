#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/SAUdpClientComponent.h"
#include "LevelObjects/SATreasureBox.h"
#include "Items/SAWeaponActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

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
