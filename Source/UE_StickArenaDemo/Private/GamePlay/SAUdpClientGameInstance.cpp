#include "GamePlay/SAUdpClientGameInstance.h"
#include "Components/SAUdpClientComponent.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogSAConnection, Log, All);

void USAUdpClientGameInstance::Init()
{
	Super::Init();
	FParse::Value(FCommandLine::Get(), TEXT("SAServerAddress="), ServerAddress);
	FParse::Value(FCommandLine::Get(), TEXT("SAServerPort="), ServerPort);
	NetworkTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &USAUdpClientGameInstance::TickNetwork));
	PreExitHandle = FCoreDelegates::OnPreExit.AddUObject(this, &USAUdpClientGameInstance::DisconnectFromServer);
}

void USAUdpClientGameInstance::Shutdown()
{
	FTSTicker::GetCoreTicker().RemoveTicker(NetworkTickHandle);
	FCoreDelegates::OnPreExit.Remove(PreExitHandle);
	DisconnectFromServer();
	ActiveClient.Reset();
	Super::Shutdown();
}

void USAUdpClientGameInstance::BeginDestroy()
{
	FTSTicker::GetCoreTicker().RemoveTicker(NetworkTickHandle);
	FCoreDelegates::OnPreExit.Remove(PreExitHandle);
	CloseSocket();
	Super::BeginDestroy();
}

bool USAUdpClientGameInstance::ConnectToServer(const FString& Address, int32 Port)
{
	if (Address != ServerAddress || Port != ServerPort) DisconnectFromServer();
	ServerAddress = Address;
	ServerPort = Port;
	bConnectionDesired = true;
	return EnsureSocket();
}

bool USAUdpClientGameInstance::EnsureSocket()
{
	if (!bConnectionDesired) return false;
	if (Socket) return true;
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem) return false;
	RemoteAddress = Subsystem->CreateInternetAddr();
	bool bValidAddress = false;
	RemoteAddress->SetIp(*ServerAddress, bValidAddress);
	if (!bValidAddress || ServerPort < 1 || ServerPort > 65535)
	{
		UE_LOG(LogSAConnection, Error, TEXT("Invalid UDP endpoint: %s:%d"), *ServerAddress, ServerPort);
		RemoteAddress.Reset();
		bConnectionDesired = false;
		return false;
	}
	RemoteAddress->SetPort(ServerPort);
	Socket = Subsystem->CreateSocket(NAME_DGram, TEXT("StickArena game instance UDP"), false);
	if (!Socket || !Socket->SetNonBlocking(true))
	{
		CloseSocket();
		UE_LOG(LogSAConnection, Error, TEXT("Failed to create UDP socket"));
		return false;
	}
	if (SessionId.IsEmpty()) SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UE_LOG(LogSAConnection, Log, TEXT("UDP connection to %s:%d, session %s"), *ServerAddress, ServerPort, *SessionId);
	return true;
}

bool USAUdpClientGameInstance::AttachClient(USAUdpClientComponent* Client)
{
	if (!Client || !EnsureSocket()) return false;
	ActiveClient = Client;
	return true;
}

void USAUdpClientGameInstance::DetachClient(USAUdpClientComponent* Client)
{
	if (ActiveClient.Get() == Client) ActiveClient.Reset();
	// Registration heartbeats keep the session alive without a Pawn during travel/respawn.
}

void USAUdpClientGameInstance::RegisterPlayer(const FSAPlayerRegistration& Info)
{
	if (!EnsureSocket()) return;
	const auto Encode = [](const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		return BytesToHex(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	};
	RegistrationPacket = FString::Printf(TEXT("SA2|type=REGISTER|session=%s|name=%s|character=%s|mesh=%s|anim=%s"),
		*SessionId, *Encode(Info.PlayerName), *Encode(Info.CharacterClassPath),
		*Encode(Info.SkeletalMeshPath), *Encode(Info.AnimInstanceClassPath));
	SendRegistration();
}

void USAUdpClientGameInstance::SendRegistration()
{
	if (RegistrationPacket.IsEmpty()) return;
	SendPacket(RegistrationPacket);
	RegistrationElapsed = 0.0f;
}

bool USAUdpClientGameInstance::SendPacket(const FString& Packet)
{
	// Teardown and disconnected callers must never reopen the socket.
	if (!Socket || !RemoteAddress.IsValid()) return false;
	const FTCHARToUTF8 Utf8(*Packet);
	if (Utf8.Length() <= 0 || Utf8.Length() > 8192) return false;
	int32 BytesSent = 0;
	if (!Socket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), BytesSent, *RemoteAddress)
		|| BytesSent != Utf8.Length())
	{
		if (!bReportedSendFailure) UE_LOG(LogSAConnection, Warning, TEXT("UDP send failed"));
		bReportedSendFailure = true;
		return false;
	}
	bReportedSendFailure = false;
	return true;
}

void USAUdpClientGameInstance::ReceivePackets()
{
	uint32 Pending = 0;
	for (int32 Count = 0; Count < 256 && Socket && Socket->HasPendingData(Pending); ++Count)
	{
		uint8 Buffer[8193];
		int32 Received = 0;
		TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		if (!Socket->RecvFrom(Buffer, 8192, Received, *Sender) || !(*Sender == *RemoteAddress)) continue;
		bool bAscii = Received > 0 && Received <= 8192;
		for (int32 Index = 0; Index < Received; ++Index) bAscii &= Buffer[Index] >= 32 && Buffer[Index] <= 126;
		if (!bAscii) continue;
		Buffer[Received] = 0;
		const FString Packet = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer));
		if (Packet == TEXT("SA2|type=REGISTER_ACK|session=") + SessionId)
		{
			bRegistrationAcknowledged = true;
			continue;
		}
		if (ActiveClient.IsValid()) ActiveClient->HandleServerPacket(Packet);
	}
}

bool USAUdpClientGameInstance::TickNetwork(float DeltaTime)
{
	if (!Socket) return true;
	ReceivePackets();
	RegistrationElapsed += FMath::Max(0.0f, DeltaTime);
	if (RegistrationElapsed >= (bRegistrationAcknowledged ? 3.0f : 1.0f)) SendRegistration();
	return true;
}

void USAUdpClientGameInstance::DisconnectFromServer()
{
	if (Socket && !SessionId.IsEmpty())
	{
		const FString Leave = TEXT("SA2|type=LEAVE|session=") + SessionId;
		for (int32 Attempt = 0; Attempt < 3; ++Attempt) SendPacket(Leave);
	}
	CloseSocket();
	if (ActiveClient.IsValid()) ActiveClient->ResetNetworkState();
	bConnectionDesired = false;
	bRegistrationAcknowledged = false;
	RegistrationPacket.Reset();
	SessionId.Reset();
	StateSequence = AttackSequence = RequestSequence = 0;
	RegistrationElapsed = 0.0f;
}

void USAUdpClientGameInstance::CloseSocket()
{
	if (Socket)
	{
		Socket->Close();
		if (ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)) Subsystem->DestroySocket(Socket);
		Socket = nullptr;
	}
	RemoteAddress.Reset();
}
