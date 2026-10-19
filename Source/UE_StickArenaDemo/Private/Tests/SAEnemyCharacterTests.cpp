#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AnimInstances/SAPlayerAnimInstance.h"
#include "Characters/SAEnemyCharacter.h"
#include "Characters/SAPlayerCharacter.h"
#include "Components/SAUdpClientComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "UObject/UnrealType.h"
#include "Components/SAStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Items/SAWeaponActor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformProcess.h"
#include "LevelObjects/SATreasureBox.h"

namespace
{
	struct FSAReplicationTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FSAReplicationTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			// This isolated world has no GameMode to invoke the normal BeginPlay notification.
			World->GetWorldSettings()->NotifyBeginPlay();
		}
		~FSAReplicationTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	FSAPlayerRegistration TestRegistration()
	{
		FSAPlayerRegistration Info;
		Info.PlayerId = TEXT("0123456789abcdef0123456789abcdef");
		Info.PlayerName = TEXT("RemotePlayer");
		Info.CharacterClassPath = TEXT("/Game/PlayerCharacters/TypeA/BP_TypeA.BP_TypeA_C");
		Info.SkeletalMeshPath = TEXT("/Game/PlayerCharacters/TypeA/SKM_TypeA.SKM_TypeA");
		Info.AnimInstanceClassPath = TEXT("/Game/PlayerCharacters/TypeA/Animations/ABP_Player.ABP_Player_C");
		return Info;
	}

	struct FSATestSocket
	{
		ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		FSocket* Socket = nullptr;
		FSATestSocket()
		{
			Socket = Subsystem->CreateSocket(NAME_DGram, TEXT("SA replication test"), false);
			if (!Socket) return;
			Socket->SetNonBlocking(true);
			TSharedRef<FInternetAddr> Address = Subsystem->CreateInternetAddr();
			bool bValid;
			Address->SetIp(TEXT("127.0.0.1"), bValid);
			Address->SetPort(0);
			if (!Socket->Bind(*Address))
			{
				Subsystem->DestroySocket(Socket);
				Socket = nullptr;
			}
		}
		~FSATestSocket() { if (Socket) Subsystem->DestroySocket(Socket); }
		void Send(const FString& Packet, const FInternetAddr& Destination)
		{
			const FTCHARToUTF8 Utf8(*Packet);
			int32 Sent = 0;
			Socket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent, Destination);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAEnemyCharacterTest, "StickArena.Networking.EnemyCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSAEnemyCharacterTest::RunTest(const FString& Parameters)
{
	FSAReplicationTestWorld TestWorld;
	ASAEnemyCharacter* Enemy = TestWorld.World->SpawnActor<ASAEnemyCharacter>();
	if (!TestNotNull(TEXT("Enemy actor spawned"), Enemy)) return false;
	TestFalse(TEXT("UDP proxy is not replicated again by Unreal networking"), Enemy->GetIsReplicated());
	const FSAPlayerRegistration Info = TestRegistration();
	Enemy->InitializeRemotePlayer(Info);
	TestNotNull(TEXT("Remote skeletal mesh loaded"), Enemy->GetMesh()->GetSkeletalMeshAsset());
	USAPlayerAnimInstance* Anim = Cast<USAPlayerAnimInstance>(Enemy->GetMesh()->GetAnimInstance());
	if (!TestNotNull(TEXT("Existing player AnimBlueprint works on enemy"), Anim)) return false;
	Enemy->InitializeRemotePlayer(Info);
	TestTrue(TEXT("Metadata heartbeat preserves AnimInstance"), Enemy->GetMesh()->GetAnimInstance() == Anim);

	FSAPlayerNetworkState State;
	State.PlayerId = Info.PlayerId;
	State.Sequence = 1;
	State.Action = TEXT("STATE");
	State.Position = FVector(100, 200, 90);
	State.Rotation = FRotator(0, 90, 0);
	State.Velocity = FVector(100, 0, 0);
	State.MovementInput = FVector::ForwardVector;
	State.State = ESAPlayerActivity::Moving;
	Enemy->ApplyNetworkState(State);
	TestTrue(TEXT("First snapshot sets position"), Enemy->GetActorLocation().Equals(State.Position));
	TestTrue(TEXT("Remote velocity reaches animation"), Enemy->GetVelocity().Equals(State.Velocity));
	TestFalse(TEXT("No local movement simulation"), Enemy->GetCharacterMovement()->IsComponentTickEnabled());
	Anim->NativeUpdateAnimation(0.1f);
	const FStructProperty* SpeedProperty = FindFProperty<FStructProperty>(USABaseAnimInstance::StaticClass(), TEXT("WalkSpeed"));
	if (TestNotNull(TEXT("Locomotion animation speed exists"), SpeedProperty))
		TestTrue(TEXT("Enemy drives locomotion BlendSpace"), SpeedProperty->ContainerPtrToValuePtr<FVector2D>(Anim)->SizeSquared() > 1.0f);
	State.Sequence = 2;
	State.Position.X += 100;
	State.Rotation.Yaw = 180;
	Enemy->ApplyNetworkState(State);
	Enemy->Tick(0.05f);
	TestTrue(TEXT("Network snapshots move enemy"), Enemy->GetActorLocation().X > 100);
	TestTrue(TEXT("Network snapshots rotate enemy"), Enemy->GetActorRotation().Yaw > 90);
	State.Sequence = 1;
	State.Position.X = -999;
	Enemy->ApplyNetworkState(State);
	TestEqual(TEXT("Old snapshots rejected"), Enemy->NetworkState.Sequence, int64{2});
	State.Sequence = 3;
	State.State = ESAPlayerActivity::Falling;
	Enemy->ApplyNetworkState(State);
	TestTrue(TEXT("Falling state reaches movement/animation"), Enemy->GetCharacterMovement()->IsFalling());
	State.Sequence = 4;
	State.AttackSequence = 1;
	Enemy->ApplyNetworkState(State); // ATTACK packet lost; the next heartbeat carries the counter.
	TestNotNull(TEXT("Attack heartbeat starts montage"), Anim->GetCurrentActiveMontage());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAEnemyUdpReceiveTest, "StickArena.Networking.UdpSpawnsEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSAEnemyUdpReceiveTest::RunTest(const FString& Parameters)
{
	FSAReplicationTestWorld TestWorld;
	FSATestSocket Server;
	if (!TestNotNull(TEXT("Mock server socket"), Server.Socket)) return false;
	ASAPlayerCharacter* Player = TestWorld.World->SpawnActor<ASAPlayerCharacter>();
	APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
	Controller->Possess(Player);
	USAUdpClientComponent* Client = Player->FindComponentByClass<USAUdpClientComponent>();
	if (!TestNotNull(TEXT("Player UDP component"), Client)) return false;
	TestTrue(TEXT("Test player is locally controlled"), Player->IsLocallyControlled());
	TSharedRef<FInternetAddr> Address = Server.Subsystem->CreateInternetAddr();
	Server.Socket->GetAddress(*Address);
	FindFProperty<FIntProperty>(USAUdpClientComponent::StaticClass(), TEXT("ServerPort"))->SetPropertyValue_InContainer(Client, Address->GetPort());
	Client->TickComponent(0.1f, LEVELTICK_All, nullptr);
	uint8 Buffer[8192];
	int32 Received = 0;
	TSharedRef<FInternetAddr> ClientAddress = Server.Subsystem->CreateInternetAddr();
	Server.Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1));
	if (!TestTrue(TEXT("Initial registration received"), Server.Socket->RecvFrom(Buffer, sizeof(Buffer), Received, *ClientAddress))) return false;
	// Registration is reflected as a struct; use its actual generated ID for the ACK.
	const FStructProperty* InfoProperty = FindFProperty<FStructProperty>(USAUdpClientComponent::StaticClass(), TEXT("PlayerRegistration"));
	const FString LocalId = InfoProperty->ContainerPtrToValuePtr<FSAPlayerRegistration>(Client)->PlayerId;
	Server.Send(TEXT("SA2|type=REGISTER_ACK|session=") + LocalId, *ClientAddress);
	const FSAPlayerRegistration Info = TestRegistration();
	const auto Encode = [](const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return BytesToHex(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	};
	const FString Metadata = FString::Printf(TEXT("SA2|type=PLAYER_INFO|session=%s|name=%s|character=%s|mesh=%s|anim=%s"),
		*Info.PlayerId, *Encode(Info.PlayerName), *Encode(Info.CharacterClassPath), *Encode(Info.SkeletalMeshPath), *Encode(Info.AnimInstanceClassPath));
	const FString Packet = FString::Printf(TEXT("SA3|type=PLAYER_STATE|session=%s|seq=1|action=STATE|state=MOVING|input=1,0,0|pos=100,200,90|vel=100,0,0|rot=0,90,0|attack=0"), *Info.PlayerId);
	Server.Send(Packet, *ClientAddress); // State arriving before metadata must be cached.
	Server.Send(Metadata, *ClientAddress);
	Client->TickComponent(0.1f, LEVELTICK_All, nullptr);
	ASAEnemyCharacter* Enemy = Client->GetRemoteCharacter(Info.PlayerId);
	if (!TestNotNull(TEXT("UDP metadata/state spawn enemy automatically"), Enemy)) return false;
	TestTrue(TEXT("Received position applied"), Enemy->GetActorLocation().Equals(FVector(100, 200, 90)));
	TestTrue(TEXT("Received rotation applied"), Enemy->GetActorRotation().Equals(FRotator(0, 90, 0)));
	Server.Send(Metadata, *ClientAddress);
	Server.Send(Packet, *ClientAddress);
	Client->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Repeated packets preserve one actor per player"), Client->GetRemoteCharacter(Info.PlayerId) == Enemy);
	Player->Destroy();
	TestTrue(TEXT("Remote actors destroyed with local session"), Enemy->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAWeaponReplicationTest, "StickArena.Networking.WeaponEquipment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSAWeaponReplicationTest::RunTest(const FString& Parameters)
{
	FSAReplicationTestWorld TestWorld;
	UClass* PlayerClass = LoadClass<ASAPlayerCharacter>(nullptr, TEXT("/Game/PlayerCharacters/TypeA/BP_TypeA.BP_TypeA_C"));
	UClass* SwordClass = LoadClass<ASAWeaponActor>(nullptr, TEXT("/Game/Items/Weapons/Blueprints/BP_WeaponSword.BP_WeaponSword_C"));
	if (!TestNotNull(TEXT("Player blueprint loaded"), PlayerClass) || !TestNotNull(TEXT("Sword blueprint loaded"), SwordClass)) return false;
	int32 Port = 0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SAWeaponTestPort="), Port) || Port <= 0)
	{
		AddError(TEXT("Run this integration test with -SAWeaponTestPort=<isolated updated UDP server port>."));
		return false;
	}
	TArray<USAUdpClientComponent*> Clients;
	const auto SpawnPlayer = [&](const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASAPlayerCharacter* Player = TestWorld.World->SpawnActor<ASAPlayerCharacter>(PlayerClass, Location, FRotator::ZeroRotator, Params);
		APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
		Controller->Possess(Player);
		USAUdpClientComponent* Client = Player->FindComponentByClass<USAUdpClientComponent>();
		FindFProperty<FIntProperty>(USAUdpClientComponent::StaticClass(), TEXT("ServerPort"))->SetPropertyValue_InContainer(Client, Port);
		Clients.Add(Client);
		return Player;
	};
	const auto PlayerId = [](USAUdpClientComponent* Client)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(USAUdpClientComponent::StaticClass(), TEXT("PlayerRegistration"));
		return Property->ContainerPtrToValuePtr<FSAPlayerRegistration>(Client)->PlayerId;
	};
	const auto PumpUntil = [&](TFunctionRef<bool()> Condition)
	{
		for (int32 Attempt = 0; Attempt < 200; ++Attempt)
		{
			for (USAUdpClientComponent* Client : Clients) Client->TickComponent(0.1f, LEVELTICK_All, nullptr);
			if (Condition()) return true;
			FPlatformProcess::Sleep(0.005f);
		}
		return false;
	};
	ASAPlayerCharacter* PlayerA = SpawnPlayer(FVector(0, 0, 100));
	SpawnPlayer(FVector(5000, 0, 100));
	USAUdpClientComponent* ClientA = Clients[0];
	USAUdpClientComponent* ClientB = Clients[1];
	const FString IdA = PlayerId(ClientA);
	if (!TestTrue(TEXT("Two clients see each other through real UDP server"),
		PumpUntil([&] { return ClientB->GetRemoteCharacter(IdA) != nullptr; }))) return false;
	ASAEnemyCharacter* RemoteA = ClientB->GetRemoteCharacter(IdA);
	ASAWeaponActor* Sword = TestWorld.World->SpawnActor<ASAWeaponActor>(SwordClass, FVector(2000, 0, 300), FRotator::ZeroRotator);
	if (!TestTrue(TEXT("Local base-character equipment succeeds"), Sword->EquipToCharacter(PlayerA))) return false;
	PlayerA->GetStatusComponent()->MaxAmmo = 10;
	PlayerA->GetStatusComponent()->Ammo = 3;
	if (!TestTrue(TEXT("Client B equips client A's weapon"), PumpUntil([&] { return IsValid(RemoteA->GetWeapon()); }))) return false;
	ASAWeaponActor* RemoteSword = RemoteA->GetWeapon();
	TestTrue(TEXT("Remote sword attached to remote mesh"), RemoteSword->GetAttachParentActor() == RemoteA);
	TestEqual(TEXT("Remote weapon identity matches local weapon"), RemoteSword->GetWeaponInstanceId(), Sword->GetWeaponInstanceId());
	TestEqual(TEXT("Remote weapon selects sword animations"), RemoteA->GetWeaponType(), EWeaponType::Sword);
	TestEqual(TEXT("Ammo snapshot synchronized"), RemoteA->GetStatusComponent()->Ammo, 3);
	ClientA->SendAction(TEXT("ATTACK"));
	TestTrue(TEXT("Armed attack reaches remote character"), PumpUntil([&] { return RemoteA->NetworkState.AttackSequence > 0; }));
	TestNotNull(TEXT("Equipped remote attack plays montage"), RemoteA->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage());
	TestEqual(TEXT("Remote animation does not spend synchronized ammo"), RemoteA->GetStatusComponent()->Ammo, 3);
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		for (USAUdpClientComponent* Client : Clients) Client->TickComponent(0.1f, LEVELTICK_All, nullptr);
		FPlatformProcess::Sleep(0.01f);
	}
	TestTrue(TEXT("Repeated snapshots do not duplicate weapon"), RemoteA->GetWeapon() == RemoteSword);
	// A third client joins after pickup; registration must restore the stored equipment.
	SpawnPlayer(FVector(10000, 0, 100));
	USAUdpClientComponent* LateClient = Clients[2];
	TestTrue(TEXT("Late join restores equipped sword"), PumpUntil([&]
	{
		ASAEnemyCharacter* Remote = LateClient->GetRemoteCharacter(IdA);
		return Remote && IsValid(Remote->GetWeapon());
	}));
	ASAWeaponActor* Replacement = TestWorld.World->SpawnActor<ASAWeaponActor>(SwordClass, FVector(2000, 0, 500), FRotator::ZeroRotator);
	TestTrue(TEXT("Same-class replacement equips"), Replacement->EquipToCharacter(PlayerA));
	TestTrue(TEXT("New instance of same class replaces remote weapon"), PumpUntil([&]
	{
		return IsValid(RemoteA->GetWeapon()) && RemoteA->GetWeapon()->GetWeaponInstanceId() == Replacement->GetWeaponInstanceId();
	}));
	TestTrue(TEXT("Previous remote sword destroyed"), RemoteSword->IsActorBeingDestroyed());
	// Remote proxies must not collect an unrelated world item through overlap.
	ASAWeaponActor* GroundSword = TestWorld.World->SpawnActor<ASAWeaponActor>(SwordClass, FVector(2000, 0, 500), FRotator::ZeroRotator);
	USphereComponent* Sphere = GroundSword->FindComponentByClass<USphereComponent>();
	Sphere->OnComponentBeginOverlap.Broadcast(Sphere, RemoteA, RemoteA->GetCapsuleComponent(), 0, false, FHitResult());
	TestNull(TEXT("Remote overlap does not claim a local ground item"), GroundSword->GetAttachParentActor());
	ASAWeaponActor* EquippedVisual = RemoteA->GetWeapon();
	PlayerA->SetWeapon(nullptr);
	PlayerA->SetWeaponType(EWeaponType::None);
	Replacement->Destroy();
	TestTrue(TEXT("Weapon removal propagates"), PumpUntil([&] { return RemoteA->GetWeapon() == nullptr; }));
	TestTrue(TEXT("Unequipped remote actor is destroyed"), EquippedVisual->IsActorBeingDestroyed());
	TestEqual(TEXT("Remote unarmed animation type restored"), RemoteA->GetWeaponType(), EWeaponType::None);
	TestEqual(TEXT("Remote ammo cleared"), RemoteA->GetStatusComponent()->Ammo, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSATreasurePickupIntegrationTest, "StickArena.Networking.TreasurePickup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSATreasurePickupIntegrationTest::RunTest(const FString& Parameters)
{
	FSAReplicationTestWorld TestWorld;
	int32 Port = 0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SAWeaponTestPort="), Port) || Port <= 0)
	{
		AddError(TEXT("TreasurePickup requires an isolated updated server via -SAWeaponTestPort=<port>."));
		return false;
	}
	UClass* PlayerClass = LoadClass<ASAPlayerCharacter>(nullptr, TEXT("/Game/PlayerCharacters/TypeA/BP_TypeA.BP_TypeA_C"));
	if (!TestNotNull(TEXT("Pickup test player class"), PlayerClass)) return false;
	TArray<USAUdpClientComponent*> Clients;
	const auto SpawnPlayer = [&](const FVector& Position)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASAPlayerCharacter* Player = TestWorld.World->SpawnActor<ASAPlayerCharacter>(PlayerClass, Position, FRotator::ZeroRotator, Params);
		TestWorld.World->SpawnActor<APlayerController>()->Possess(Player);
		USAUdpClientComponent* Client = Player->FindComponentByClass<USAUdpClientComponent>();
		FindFProperty<FIntProperty>(USAUdpClientComponent::StaticClass(), TEXT("ServerPort"))->SetPropertyValue_InContainer(Client, Port);
		Clients.Add(Client);
		return Player;
	};
	const auto PumpUntil = [&](TFunctionRef<bool()> Condition, double Timeout = 3.0)
	{
		const double End = FPlatformTime::Seconds() + Timeout;
		do
		{
			for (USAUdpClientComponent* Client : Clients) Client->TickComponent(0.1f, LEVELTICK_All, nullptr);
			if (Condition()) return true;
			FPlatformProcess::Sleep(0.005f);
		} while (FPlatformTime::Seconds() < End);
		return false;
	};
	ASAPlayerCharacter* PlayerA = SpawnPlayer(FVector(0,0,100));
	SpawnPlayer(FVector(5000,0,100));
	USAUdpClientComponent* ClientA = Clients[0];
	USAUdpClientComponent* ClientB = Clients[1];
	FString Id;
	if (!TestTrue(TEXT("Real server spawns a shared treasure box"), PumpUntil([&]
	{
		for (const auto& Entry : ClientA->TreasureActors)
			if (Cast<ASATreasureBox>(Entry.Value) && Cast<ASATreasureBox>(ClientB->TreasureActors.FindRef(Entry.Key)))
			{ Id = Entry.Key; return true; }
		return false;
	}, 8.0))) return false;
	const FVector Position = ClientA->TreasureActors.FindRef(Id)->GetActorLocation();
	PlayerA->SetActorLocation(Position + FVector(250,0,88));
	for (int32 Hit = 1; Hit <= 3; ++Hit)
	{
		ClientA->SendAction(TEXT("STATE"));
		ClientA->SendTreasureHit(Id);
		if (!TestTrue(TEXT("Shared box hit reaches both clients"), PumpUntil([&]
		{ return ClientA->TreasureHits.FindRef(Id) == Hit && ClientB->TreasureHits.FindRef(Id) == Hit; }))) return false;
		if (Hit < 3) FPlatformProcess::Sleep(0.22f);
	}
	ASAWeaponActor* GroundA = Cast<ASAWeaponActor>(ClientA->TreasureActors.FindRef(Id));
	ASAWeaponActor* GroundB = Cast<ASAWeaponActor>(ClientB->TreasureActors.FindRef(Id));
	if (!TestNotNull(TEXT("A sees dropped weapon"), GroundA) || !TestNotNull(TEXT("B sees dropped weapon"), GroundB)) return false;
	PlayerA->SetActorLocation(GroundA->GetActorLocation() + FVector(0,0,88));
	PlayerA->GetCapsuleComponent()->UpdateOverlaps();
	GroundA->FindComponentByClass<USphereComponent>()->UpdateOverlaps();
	TestNull(TEXT("Overlap waits for server grant before attachment"), PlayerA->GetWeapon());
	if (!TestTrue(TEXT("A equips after grant and B's ground copy is removed"), PumpUntil([&]
	{
		return PlayerA->GetWeapon() == GroundA && !ClientB->TreasureActors.Contains(Id)
			&& ClientB->TreasureOwners.FindRef(Id) == ClientA->SessionId;
	}))) return false;
	TestTrue(TEXT("B's actual ground actor was destroyed"), GroundB->IsActorBeingDestroyed());
	TestTrue(TEXT("Equipped winner is preserved"), IsValid(GroundA) && GroundA->GetAttachParentActor() == PlayerA);
	const FString StaleDrop = FString::Printf(TEXT("SA4|type=TREASURE_STATE|epoch=%s|id=%s|item=0|hits=3|pos=%.6f,%.6f,%.6f"),
		*ClientA->TreasureEpoch, *Id, Position.X, Position.Y, Position.Z);
	ClientB->ReceiveTreasure(StaleDrop);
	TestFalse(TEXT("Out-of-order drop cannot return on B"), ClientB->TreasureActors.Contains(Id));
	SpawnPlayer(FVector(10000,0,100));
	USAUdpClientComponent* Late = Clients[2];
	TestTrue(TEXT("Late join keeps collected ground empty and restores equipped remote weapon"), PumpUntil([&]
	{
		ASAEnemyCharacter* RemoteA = Late->GetRemoteCharacter(ClientA->SessionId);
		return Late->TreasureOwners.FindRef(Id) == ClientA->SessionId && !Late->TreasureActors.Contains(Id)
			&& RemoteA && IsValid(RemoteA->GetWeapon());
	}));
	return true;
}

#endif
