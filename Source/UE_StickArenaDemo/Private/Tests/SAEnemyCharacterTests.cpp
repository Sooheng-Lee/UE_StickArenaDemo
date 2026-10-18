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

#endif
