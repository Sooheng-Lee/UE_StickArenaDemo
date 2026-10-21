#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AnimInstances/SAPlayerAnimInstance.h"
#include "Characters/SAEnemyCharacter.h"
#include "Characters/SAPlayerCharacter.h"
#include "Components/SAUdpClientComponent.h"
#include "GamePlay/SAUdpClientGameInstance.h"
#include "UObject/StrongObjectPtr.h"
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
		TArray<TStrongObjectPtr<USAUdpClientGameInstance>> Connections;
		void Connect(USAUdpClientComponent* Client, int32 Port)
		{
			// Network feature tests start after the appearance animation; lifecycle tests opt back in.
			if (ASABaseCharacter* Owner = Cast<ASABaseCharacter>(Client->GetOwner())) Owner->SetCharacterState(ECharacterState::Idle);
			USAUdpClientGameInstance* Connection = NewObject<USAUdpClientGameInstance>(GEngine);
			Connections.Emplace(Connection);
			Connection->ConnectToServer(TEXT("127.0.0.1"), Port);
			FindFProperty<FObjectProperty>(USAUdpClientComponent::StaticClass(), TEXT("NetworkGameInstance"))
				->SetObjectPropertyValue_InContainer(Client, Connection);
			Client->TickComponent(0.0f, LEVELTICK_All, nullptr);
		}
		void Tick(USAUdpClientComponent* Client, float DeltaTime = 0.1f)
		{
			Client->TickComponent(DeltaTime, LEVELTICK_All, nullptr);
			if (Client->GetConnection()) Client->GetConnection()->TickNetwork(DeltaTime);
		}
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
			for (const auto& Connection : Connections) Connection->DisconnectFromServer();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSACharacterCapsuleTest, "StickArena.Characters.CapsuleCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSACharacterCapsuleTest::RunTest(const FString& Parameters)
{
	FSAReplicationTestWorld TestWorld;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASAPlayerCharacter* Player = TestWorld.World->SpawnActor<ASAPlayerCharacter>(
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	ASAEnemyCharacter* Enemy = TestWorld.World->SpawnActor<ASAEnemyCharacter>(
		FVector(1000, 0, 0), FRotator::ZeroRotator, SpawnParameters);
	if (!TestNotNull(TEXT("Player spawned"), Player) || !TestNotNull(TEXT("Enemy spawned"), Enemy)) return false;
	const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
	const auto CheckCapsule = [this, PlayerCapsule](const UCapsuleComponent* Capsule)
	{
		TestEqual(TEXT("Capsule half height"), Capsule->GetUnscaledCapsuleHalfHeight(), 88.0f);
		TestEqual(TEXT("Capsule radius"), Capsule->GetUnscaledCapsuleRadius(), 34.0f);
		TestEqual(TEXT("Same collision profile as player"), Capsule->GetCollisionProfileName(), PlayerCapsule->GetCollisionProfileName());
		TestEqual(TEXT("Same collision mode as player"), Capsule->GetCollisionEnabled(), PlayerCapsule->GetCollisionEnabled());
		TestEqual(TEXT("Same object type as player"), Capsule->GetCollisionObjectType(), PlayerCapsule->GetCollisionObjectType());
		TestTrue(TEXT("Same channel responses as player"), Capsule->GetCollisionResponseToChannels() == PlayerCapsule->GetCollisionResponseToChannels());
		TestEqual(TEXT("Same overlap generation as player"), Capsule->GetGenerateOverlapEvents(), PlayerCapsule->GetGenerateOverlapEvents());
	};
	TestEqual(TEXT("Player uses Pawn profile"), PlayerCapsule->GetCollisionProfileName(), FName(TEXT("Pawn")));
	CheckCapsule(PlayerCapsule);
	CheckCapsule(Enemy->GetCapsuleComponent());
	Enemy->InitializeRemotePlayer(TestRegistration());
	CheckCapsule(Enemy->GetCapsuleComponent());
	Enemy->GetCapsuleComponent()->SetCapsuleSize(50.0f, 120.0f);
	Enemy->GetCapsuleComponent()->SetCollisionProfileName(TEXT("NoCollision"));
	Enemy->OnConstruction(Enemy->GetActorTransform());
	CheckCapsule(Enemy->GetCapsuleComponent());
	return true;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSADamageKnockbackTest, "StickArena.Combat.DamageKnockback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSADamageKnockbackTest::RunTest(const FString& Parameters)
{
	FSAReplicationTestWorld TestWorld;
	int32 SpawnIndex = 0;
	const auto SpawnEnemy = [&]()
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASAEnemyCharacter* Enemy = TestWorld.World->SpawnActor<ASAEnemyCharacter>(
			FVector(0, 1000.0f * SpawnIndex++, 100), FRotator::ZeroRotator, SpawnParameters);
		Enemy->InitializeRemotePlayer(TestRegistration());
		Enemy->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetCharacterState(ECharacterState::Damaged);
		return Enemy;
	};
	ASAEnemyCharacter* Weak = SpawnEnemy();
	ASAEnemyCharacter* Strong = SpawnEnemy();
	USABaseAnimInstance* WeakAnim = Cast<USABaseAnimInstance>(Weak->GetMesh()->GetAnimInstance());
	USABaseAnimInstance* StrongAnim = Cast<USABaseAnimInstance>(Strong->GetMesh()->GetAnimInstance());
	if (!TestNotNull(TEXT("Weak animation instance"), WeakAnim)
		|| !TestNotNull(TEXT("Strong animation instance"), StrongAnim)) return false;
	const float WeakStrength = FindFProperty<FFloatProperty>(USABaseAnimInstance::StaticClass(), TEXT("WeakKnockbackStrength"))->GetPropertyValue_InContainer(WeakAnim);
	const float StrongStrength = FindFProperty<FFloatProperty>(USABaseAnimInstance::StaticClass(), TEXT("StrongKnockbackStrength"))->GetPropertyValue_InContainer(StrongAnim);
	WeakAnim->PlayDamageAnim(29.0f);
	StrongAnim->PlayDamageAnim(30.0f);
	TestTrue(TEXT("Below 30 applies weak backward velocity"), Weak->GetVelocity().Equals(FVector(-WeakStrength, 0, 0)));
	TestTrue(TEXT("Exactly 30 applies strong backward velocity"), Strong->GetVelocity().Equals(FVector(-StrongStrength, 0, 0)));
	const FVector WeakStart = Weak->GetActorLocation();
	const FVector StrongStart = Strong->GetActorLocation();
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Weak->Tick(1.0f / 60.0f);
		Strong->Tick(1.0f / 60.0f);
	}
	const float WeakDistance = WeakStart.X - Weak->GetActorLocation().X;
	const float StrongDistance = StrongStart.X - Strong->GetActorLocation().X;
	TestTrue(TEXT("Placed enemy actually moves backward"), WeakDistance > 20.0f);
	TestTrue(TEXT("Strong hit travels substantially farther"), StrongDistance > WeakDistance * 2.0f);
	TestTrue(TEXT("Friction stops knockback"), Strong->GetVelocity().IsNearlyZero());
	WeakAnim->PlayDamageAnim(1.0f);
	TestTrue(TEXT("Accumulated 29 + 1 selects strong response"), Weak->GetVelocity().Equals(FVector(-StrongStrength, 0, 0)));

	ASAEnemyCharacter* Remote = SpawnEnemy();
	FSAPlayerNetworkState State;
	State.PlayerId = TestRegistration().PlayerId;
	State.Position = FVector(1000, 0, 100);
	State.Sequence = 1;
	Remote->ApplyNetworkState(State);
	CastChecked<USABaseAnimInstance>(Remote->GetMesh()->GetAnimInstance())->PlayDamageAnim(30.0f);
	Remote->Tick(1.0f / 60.0f);
	TestTrue(TEXT("Network proxy does not add a second physical impulse"), Remote->GetActorLocation().Equals(State.Position));
	TestTrue(TEXT("Network velocity excludes local hit prediction"), Remote->GetVelocity().IsNearlyZero());
	State.bHasLifecycleState = true;
	State.CharacterState = ECharacterState::Damaged;
	State.LifecycleVisibility = 1.0f;
	State.Velocity = FVector(-250, 0, 0);
	for (int32 Packet = 0; Packet < 6; ++Packet)
	{
		++State.Sequence;
		State.Position.X -= 25.0f;
		const FVector BeforePacket = Remote->GetActorLocation();
		Remote->ApplyNetworkState(State);
		TestTrue(TEXT("Normal knockback packet does not teleport the proxy"), Remote->GetActorLocation().Equals(BeforePacket));
		TestTrue(TEXT("Proxy uses the authoritative impulse velocity once"), Remote->GetVelocity().Equals(State.Velocity));
		for (int32 Frame = 0; Frame < 6; ++Frame)
		{
			const float PreviousX = Remote->GetActorLocation().X;
			Remote->Tick(1.0f / 60.0f);
			TestTrue(TEXT("Authoritative knockback advances without a forward correction"), Remote->GetActorLocation().X <= PreviousX + 0.01f);
		}
	}
	TestFalse(TEXT("Network movement simulation stays disabled"), Remote->GetCharacterMovement()->IsComponentTickEnabled());
	++State.Sequence;
	State.Velocity = FVector::ZeroVector;
	State.CharacterState = ECharacterState::Idle;
	const FVector BeforeRecovery = Remote->GetActorLocation();
	Remote->ApplyNetworkState(State);
	TestTrue(TEXT("Leaving damage state does not reset the position"), Remote->GetActorLocation().Equals(BeforeRecovery));
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Remote->Tick(1.0f / 60.0f);
		TestTrue(TEXT("Recovery cannot pull the proxy back to its pre-hit position"), Remote->GetActorLocation().X <= BeforeRecovery.X + 0.01f);
	}
	TestTrue(TEXT("Recovered proxy remains at the final knockback position"), Remote->GetActorLocation().Equals(State.Position, 0.2f));
	const FVector SettledPosition = Remote->GetActorLocation();
	FSAPlayerNetworkState StaleState = State;
	--StaleState.Sequence;
	StaleState.Position.X += 150.0f;
	Remote->ApplyNetworkState(StaleState);
	Remote->ApplyDamageKnockback(FVector(-500, 0, 0));
	Remote->Tick(1.0f / 60.0f);
	TestTrue(TEXT("Stale snapshots and later local hit reactions cannot move a settled proxy"), Remote->GetActorLocation().Equals(SettledPosition, 0.2f));
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
	TestWorld.Connect(Client, Address->GetPort());
	TestWorld.Tick(Client);
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
	TestWorld.Tick(Client);
	ASAEnemyCharacter* Enemy = Client->GetRemoteCharacter(Info.PlayerId);
	if (!TestNotNull(TEXT("UDP metadata/state spawn enemy automatically"), Enemy)) return false;
	TestTrue(TEXT("Received position applied"), Enemy->GetActorLocation().Equals(FVector(100, 200, 90)));
	TestTrue(TEXT("Received rotation applied"), Enemy->GetActorRotation().Equals(FRotator(0, 90, 0)));
	Server.Send(Metadata, *ClientAddress);
	Server.Send(Packet, *ClientAddress);
	TestWorld.Tick(Client);
	TestTrue(TEXT("Repeated packets preserve one actor per player"), Client->GetRemoteCharacter(Info.PlayerId) == Enemy);
	const FString HealthPacket = Packet.Replace(TEXT("seq=1"), TEXT("seq=2"))
		+ TEXT("|weapon=|weaponid=|ammo=0|maxammo=0|hp=37.5|maxhp=150|damage=12.5|defense=7");
	Server.Send(HealthPacket, *ClientAddress);
	TestWorld.Tick(Client);
	TestEqual(TEXT("UDP health snapshot applied"), Enemy->GetStatusComponent()->HP, 37.5f);
	TestEqual(TEXT("UDP maximum health applied"), Enemy->GetStatusComponent()->MaxHP, 150.0f);
	for (const FString InvalidHP : {TEXT("nan"), TEXT("inf"), TEXT("-1"), TEXT("151"), TEXT("1000001"), TEXT("37junk")})
	{
		Server.Send(HealthPacket.Replace(TEXT("seq=2"), TEXT("seq=3")).Replace(TEXT("hp=37.5|"), *(TEXT("hp=") + InvalidHP + TEXT("|"))), *ClientAddress);
		TestWorld.Tick(Client);
	}
	TestEqual(TEXT("Invalid health packets do not replace latest state"), Enemy->NetworkState.Sequence, int64{2});
	Server.Send(Packet, *ClientAddress);
	TestWorld.Tick(Client);
	TestEqual(TEXT("Stale legacy packet cannot reset health"), Enemy->GetStatusComponent()->HP, 37.5f);

	// A player hit with no movement input must still move and publish the resulting transform.
	uint32 PendingKnockbackPacket = 0;
	while (Server.Socket->HasPendingData(PendingKnockbackPacket))
		Server.Socket->RecvFrom(Buffer, sizeof(Buffer), Received, *ClientAddress);
	Player->SetCharacterState(ECharacterState::Damaged);
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Flying); // Isolate the impulse from gravity/ground collision.
	const FVector BeforeKnockback = Player->GetActorLocation();
	Player->ApplyDamageKnockback(FVector(-250, 0, 0));
	Player->Tick(0.016f);
	Movement->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Damage input lock preserves AddImpulse movement"), Player->GetActorLocation().X < BeforeKnockback.X);
	TestTrue(TEXT("Knockback retains backward velocity"), Player->GetVelocity().X < 0.0f);
	Player->Tick(0.016f);
	TestTrue(TEXT("Next player tick preserves knockback velocity"), Player->GetVelocity().X < 0.0f);
	Client->TickComponent(0.001f, LEVELTICK_All, nullptr); // Less than the regular send interval.
	Server.Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1));
	bool bReceivedKnockbackState = false;
	const FVector KnockbackPosition = Player->GetActorLocation();
	const FVector KnockbackVelocity = Player->GetVelocity();
	const FString ExpectedPosition = FString::Printf(TEXT("|pos=%.6f,%.6f,%.6f|"),
		KnockbackPosition.X, KnockbackPosition.Y, KnockbackPosition.Z);
	const FString ExpectedVelocity = FString::Printf(TEXT("|vel=%.6f,%.6f,%.6f|"),
		KnockbackVelocity.X, KnockbackVelocity.Y, KnockbackVelocity.Z);
	while (Server.Socket->HasPendingData(PendingKnockbackPacket))
	{
		if (!Server.Socket->RecvFrom(Buffer, sizeof(Buffer) - 1, Received, *ClientAddress)) break;
		Buffer[Received] = 0;
		const FString SentPacket(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer)));
		if (!SentPacket.StartsWith(TEXT("SA3|type=PLAYER_STATE|"))) continue;
		bReceivedKnockbackState = true;
		TestTrue(TEXT("Server receives position after impulse movement"), SentPacket.Contains(ExpectedPosition));
		TestTrue(TEXT("Server receives impulse velocity"), SentPacket.Contains(ExpectedVelocity));
		TestTrue(TEXT("No-input knockback is published as moving"), SentPacket.Contains(TEXT("|state=MOVING|input=0.000000,0.000000,0.000000|")));
		TestTrue(TEXT("Damage state is included"), SentPacket.Contains(TEXT("|character=DAMAGED|")));
	}
	TestTrue(TEXT("Knockback requests a packet before the regular interval"), bReceivedKnockbackState);
	USAUdpClientGameInstance* Connection = Client->GetConnection();
	Player->Destroy();
	TestTrue(TEXT("Remote actors destroyed with local session"), Enemy->IsActorBeingDestroyed());
	TestTrue(TEXT("Pawn teardown does not close the game instance socket"), Connection->IsConnected());
	Connection->DisconnectFromServer();
	bool bReceivedLeave = false;
	Server.Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1));
	uint32 Pending = 0;
	while (Server.Socket->HasPendingData(Pending))
	{
		if (!Server.Socket->RecvFrom(Buffer, sizeof(Buffer) - 1, Received, *ClientAddress)) break;
		Buffer[Received] = 0;
		bReceivedLeave |= FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer))) == TEXT("SA2|type=LEAVE|session=") + LocalId;
	}
	TestTrue(TEXT("Game instance disconnect sends LEAVE before closing its socket"), bReceivedLeave);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAHealthReplicationTest, "StickArena.Networking.HealthSynchronization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSAHealthReplicationTest::RunTest(const FString& Parameters)
{
	int32 Port = 0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SAHealthTestPort="), Port) || Port <= 0)
	{
		AddError(TEXT("Run with -SAHealthTestPort=<isolated updated UDP server port>."));
		return false;
	}
	FSAReplicationTestWorld TestWorld;
	TArray<USAUdpClientComponent*> Clients;
	const auto SpawnPlayer = [&]()
	{
		ASAPlayerCharacter* Player = TestWorld.World->SpawnActor<ASAPlayerCharacter>();
		APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
		Controller->Possess(Player);
		USAUdpClientComponent* Client = Player->FindComponentByClass<USAUdpClientComponent>();
		TestWorld.Connect(Client, Port);
		Clients.Add(Client);
		return Player;
	};
	const auto Id = [](USAUdpClientComponent* Client)
	{
		return FindFProperty<FStructProperty>(USAUdpClientComponent::StaticClass(), TEXT("PlayerRegistration"))
			->ContainerPtrToValuePtr<FSAPlayerRegistration>(Client)->PlayerId;
	};
	const auto Pump = [&](TFunctionRef<bool()> Condition)
	{
		for (int32 Attempt = 0; Attempt < 300; ++Attempt)
		{
			for (USAUdpClientComponent* Client : Clients) TestWorld.Tick(Client);
			if (Condition()) return true;
			FPlatformProcess::Sleep(0.005f);
		}
		return false;
	};
	ASAPlayerCharacter* Owner = SpawnPlayer();
	ASAPlayerCharacter* Observer = SpawnPlayer();
	USAStatusComponent* Status = Owner->GetStatusComponent();
	Status->MaxHP = 200;
	Status->HP = 137.5f;
	Status->DefaultDamage = 12.5f;
	Status->Defense = 7;
	const FString OwnerId = Id(Clients[0]);
	if (!TestTrue(TEXT("Remote proxy receives owner's current health"), Pump([&]
	{
		ASAEnemyCharacter* Remote = Clients[1]->GetRemoteCharacter(OwnerId);
		return Remote && Remote->HasSynchronizedHealth() && Remote->GetStatusComponent()->HP == Status->HP;
	}))) return false;
	ASAEnemyCharacter* Remote = Clients[1]->GetRemoteCharacter(OwnerId);
	TestEqual(TEXT("Maximum health synchronized"), Remote->GetStatusComponent()->MaxHP, 200.0f);
	TestEqual(TEXT("Attack buff synchronized"), Remote->GetStatusComponent()->DefaultDamage, 12.5f);
	TestEqual(TEXT("Defense buff synchronized"), Remote->GetStatusComponent()->Defense, 7.0f);
	Remote->TakeDamage(Observer);
	TestEqual(TEXT("Predicted remote hit cannot independently subtract health"), Remote->GetStatusComponent()->HP, Status->HP);
	TestEqual(TEXT("Owner health unaffected by proxy-only prediction"), Status->HP, 137.5f);
	Owner->TakeDamage(Observer);
	TestEqual(TEXT("Owning client applies damage once"), Status->HP, 136.5f);
	TestTrue(TEXT("Damage reaches remote proxy"), Pump([&] { return Remote->GetStatusComponent()->HP == Status->HP; }));
	Status->HP = 160.25f;
	TestTrue(TEXT("Healing reaches remote proxy"), Pump([&] { return Remote->GetStatusComponent()->HP == Status->HP; }));
	SpawnPlayer();
	TestTrue(TEXT("Late join receives current health and maximum"), Pump([&]
	{
		ASAEnemyCharacter* LateRemote = Clients[2]->GetRemoteCharacter(OwnerId);
		return LateRemote && LateRemote->GetStatusComponent()->HP == 160.25f && LateRemote->GetStatusComponent()->MaxHP == 200.0f;
	}));
	Owner->BeginSpawning();
	Owner->Tick(0.8f);
	TestTrue(TEXT("Spawn state and progress reach other clients"), Pump([&]
	{
		return Remote->GetCharacterState() == ECharacterState::Spawning
			&& FMath::IsNearlyEqual(Remote->GetLifecycleVisibility(), 0.4f, 0.01f);
	}));
	const float ProtectedHP = Remote->GetStatusComponent()->HP;
	Remote->TakeDamage(Observer);
	TestEqual(TEXT("Remote spawning is invulnerable"), Remote->GetStatusComponent()->HP, ProtectedHP);
	const int64 ProtectedAttackSequence = Clients[0]->GetConnection()->GetAttackSequence();
	Clients[0]->SendAction(TEXT("ATTACK"));
	TestEqual(TEXT("Spawn cannot emit a network attack"), Clients[0]->GetConnection()->GetAttackSequence(), ProtectedAttackSequence);
	SpawnPlayer();
	TestTrue(TEXT("Late join restores spawning progress"), Pump([&]
	{
		ASAEnemyCharacter* LateSpawn = Clients[3]->GetRemoteCharacter(OwnerId);
		return LateSpawn && LateSpawn->GetCharacterState() == ECharacterState::Spawning
			&& FMath::IsNearlyEqual(LateSpawn->GetLifecycleVisibility(), 0.4f, 0.01f);
	}));
	Owner->Tick(2.0f);
	TestTrue(TEXT("Spawn completion reaches other clients"), Pump([&] { return Remote->GetCharacterState() == ECharacterState::Idle; }));
	Status->HP = 0;
	Owner->SetCharacterState(ECharacterState::Dead);
	Owner->Tick(1.5f);
	TestTrue(TEXT("Death synchronized across clients"), Pump([&]
	{
		const ASAEnemyCharacter* LateRemote = Clients[2]->GetRemoteCharacter(OwnerId);
		return Remote->GetCharacterState() == ECharacterState::Dead
			&& LateRemote && LateRemote->GetCharacterState() == ECharacterState::Dead;
	}));
	Remote->RestoreFromDamaged();
	TestEqual(TEXT("Damage recovery cannot revive a dead proxy"), Remote->GetCharacterState(), ECharacterState::Dead);
	const FSAPlayerNetworkState DeadState = Remote->NetworkState;
	Owner->Respawn();
	TestTrue(TEXT("Owner respawn appearance reaches remote proxy"), Pump([&]
	{
		return Remote->GetStatusComponent()->HP == 200.0f && Remote->GetCharacterState() == ECharacterState::Spawning;
	}));
	Owner->Tick(2.0f);
	TestTrue(TEXT("Owner respawn restores proxy health and state"), Pump([&]
	{
		return Remote->GetStatusComponent()->HP == 200.0f && Remote->GetCharacterState() == ECharacterState::Idle;
	}));
	Remote->ApplyNetworkState(DeadState);
	TestEqual(TEXT("Delayed death snapshot cannot undo respawn"), Remote->GetStatusComponent()->HP, 200.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAGameInstanceConnectionTest, "StickArena.Networking.GameInstanceLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSAGameInstanceConnectionTest::RunTest(const FString& Parameters)
{
	int32 Port = 0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SAHealthTestPort="), Port) || Port <= 0)
	{
		AddError(TEXT("Run with -SAHealthTestPort=<isolated UDP server port>."));
		return false;
	}
	FSAReplicationTestWorld FirstWorld;
	const auto Spawn = [](UWorld* World)
	{
		ASAPlayerCharacter* Player = World->SpawnActor<ASAPlayerCharacter>();
		World->SpawnActor<APlayerController>()->Possess(Player);
		return Player;
	};
	ASAPlayerCharacter* FirstPawn = Spawn(FirstWorld.World);
	ASAPlayerCharacter* Observer = Spawn(FirstWorld.World);
	USAUdpClientComponent* FirstClient = FirstPawn->FindComponentByClass<USAUdpClientComponent>();
	USAUdpClientComponent* ObserverClient = Observer->FindComponentByClass<USAUdpClientComponent>();
	FirstWorld.Connect(FirstClient, Port);
	FirstWorld.Connect(ObserverClient, Port);
	USAUdpClientGameInstance* Connection = FirstClient->GetConnection();
	const FString Session = Connection->GetSessionId();
	const auto Pump = [&](USAUdpClientComponent* Client, TFunctionRef<bool()> Condition)
	{
		for (int32 Attempt = 0; Attempt < 300; ++Attempt)
		{
			if (Client) FirstWorld.Tick(Client);
			FirstWorld.Tick(ObserverClient);
			if (Condition()) return true;
			FPlatformProcess::Sleep(0.005f);
		}
		return false;
	};
	if (!TestTrue(TEXT("Connection registered through game instance"), Pump(FirstClient, [&]
	{
		return Connection->IsRegistered() && ObserverClient->GetRemoteCharacter(Session);
	}))) return false;
	ASAEnemyCharacter* Remote = ObserverClient->GetRemoteCharacter(Session);
	const int64 FirstSequence = Remote->NetworkState.Sequence;
	FirstPawn->Destroy();
	TestTrue(TEXT("Destroying Pawn keeps connection open"), Connection->IsConnected());
	TestEqual(TEXT("Destroying Pawn keeps session identity"), Connection->GetSessionId(), Session);
	// Registration can be retried without any active Pawn.
	Connection->TickNetwork(3.1f);
	TestTrue(TEXT("Detached connection remains registered"), Connection->IsRegistered());
	FSAReplicationTestWorld NextWorld;
	ASAPlayerCharacter* NextPawn = Spawn(NextWorld.World);
	NextPawn->GetStatusComponent()->HP = 77.0f;
	USAUdpClientComponent* NextClient = NextPawn->FindComponentByClass<USAUdpClientComponent>();
	FindFProperty<FObjectProperty>(USAUdpClientComponent::StaticClass(), TEXT("NetworkGameInstance"))
		->SetObjectPropertyValue_InContainer(NextClient, Connection);
	TestTrue(TEXT("New world/Pawn resumes state sequence on the same connection"), Pump(NextClient, [&]
	{
		return Remote->NetworkState.Sequence > FirstSequence && Remote->GetStatusComponent()->HP == 77.0f;
	}));
	TestEqual(TEXT("Pawn in the new world uses existing session"), Connection->GetSessionId(), Session);
	TestTrue(TEXT("Observer retains the same proxy across Pawn/world replacement"), ObserverClient->GetRemoteCharacter(Session) == Remote);
	Connection->DisconnectFromServer();
	TestFalse(TEXT("Explicit disconnect closes socket"), Connection->IsConnected());
	TestFalse(TEXT("Explicit disconnect clears registration"), Connection->IsRegistered());
	TestFalse(TEXT("Sending after disconnect cannot reopen socket"), Connection->SendPacket(TEXT("SA2|type=LEAVE|session=") + Session));
	TestTrue(TEXT("Explicit reconnect succeeds"), Connection->ConnectToServer(TEXT("127.0.0.1"), Port));
	TestNotEqual(TEXT("Reconnect creates fresh session"), Connection->GetSessionId(), Session);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAGameInstanceConfiguredTest, "StickArena.Networking.GameInstanceConfigured",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSAGameInstanceConfiguredTest::RunTest(const FString& Parameters)
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType != EWorldType::Game || !Context.World()) continue;
		USAUdpClientGameInstance* Connection = Cast<USAUdpClientGameInstance>(Context.World()->GetGameInstance());
		if (!TestNotNull(TEXT("Configured game instance created at startup"), Connection)) return false;
		APlayerController* Controller = Context.World()->GetFirstPlayerController();
		USAUdpClientComponent* Client = Controller && Controller->GetPawn()
			? Controller->GetPawn()->FindComponentByClass<USAUdpClientComponent>() : nullptr;
		if (!TestNotNull(TEXT("Local player's gameplay adapter"), Client)) return false;
		TestTrue(TEXT("Player uses its actual game instance connection"), Client->GetConnection() == Connection);
		TestTrue(TEXT("Game instance core ticker received registration ACK"), Connection->IsRegistered());
		return true;
	}
	AddError(TEXT("Run this test in a game instance with an isolated server."));
	return false;
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
		TestWorld.Connect(Client, Port);
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
			for (USAUdpClientComponent* Client : Clients) TestWorld.Tick(Client);
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
		for (USAUdpClientComponent* Client : Clients) TestWorld.Tick(Client);
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
		TestWorld.Connect(Client, Port);
		Clients.Add(Client);
		return Player;
	};
	const auto PumpUntil = [&](TFunctionRef<bool()> Condition, double Timeout = 3.0)
	{
		const double End = FPlatformTime::Seconds() + Timeout;
		do
		{
			for (USAUdpClientComponent* Client : Clients) TestWorld.Tick(Client);
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
	for (int32 Hit = 1; Hit <= 5; ++Hit)
	{
		ClientA->SendAction(TEXT("STATE"));
		ClientA->SendTreasureHit(Id);
		if (!TestTrue(TEXT("Shared box hit reaches both clients"), PumpUntil([&]
		{ return ClientA->TreasureHits.FindRef(Id) == Hit && ClientB->TreasureHits.FindRef(Id) == Hit; }))) return false;
		if (Hit < 5) FPlatformProcess::Sleep(0.22f);
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
	const FString StaleDrop = FString::Printf(TEXT("SA4|type=TREASURE_STATE|epoch=%s|id=%s|item=0|hits=5|pos=%.6f,%.6f,%.6f"),
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
