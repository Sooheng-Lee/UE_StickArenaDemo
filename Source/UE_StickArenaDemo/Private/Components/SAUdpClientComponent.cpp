#include "Components/SAUdpClientComponent.h"
#include "GameFramework/Pawn.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSAUdp, Log, All);

USAUdpClientComponent::USAUdpClientComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USAUdpClientComponent::BeginPlay()
{
	Super::BeginPlay();
	SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
}

void USAUdpClientComponent::SendAction(const TCHAR* Action, const FVector& Input)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bEnabled || !Pawn || !Pawn->IsLocallyControlled()) return;

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem) return;
	if (!Socket)
	{
		RemoteAddress = Subsystem->CreateInternetAddr();
		bool bValidAddress = false;
		RemoteAddress->SetIp(*ServerAddress, bValidAddress);
		if (!bValidAddress || ServerPort < 1 || ServerPort > 65535)
		{
			UE_LOG(LogSAUdp, Error, TEXT("Invalid UDP endpoint: %s:%d"), *ServerAddress, ServerPort);
			bEnabled = false;
			return;
		}
		RemoteAddress->SetPort(ServerPort);
		Socket = Subsystem->CreateSocket(NAME_DGram, TEXT("StickArena UDP client"), false);
		if (!Socket || !Socket->SetNonBlocking(true))
		{
			if (Socket) Subsystem->DestroySocket(Socket);
			Socket = nullptr;
			bEnabled = false;
			UE_LOG(LogSAUdp, Error, TEXT("Failed to create UDP socket"));
			return;
		}
		UE_LOG(LogSAUdp, Log, TEXT("Sending player actions to %s:%d"), *ServerAddress, ServerPort);
	}

	const FVector Position = Pawn->GetActorLocation();
	const FVector Velocity = Pawn->GetVelocity();
	const FRotator Rotation = Pawn->GetActorRotation();
	const FString Packet = FString::Printf(
		TEXT("SA1|session=%s|seq=%u|action=%s|input=%.3f,%.3f,%.3f|pos=%.2f,%.2f,%.2f|vel=%.2f,%.2f,%.2f|yaw=%.2f"),
		*SessionId, ++Sequence, Action, Input.X, Input.Y, Input.Z,
		Position.X, Position.Y, Position.Z, Velocity.X, Velocity.Y, Velocity.Z, Rotation.Yaw);
	const FTCHARToUTF8 Utf8(*Packet);
	int32 BytesSent = 0;
	if (!Socket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), BytesSent, *RemoteAddress)
		|| BytesSent != Utf8.Length())
	{
		if (!bReportedSendFailure)
			UE_LOG(LogSAUdp, Warning, TEXT("UDP send failed (socket error %d)"), static_cast<int32>(Subsystem->GetLastErrorCode()));
		bReportedSendFailure = true;
		return;
	}
	bReportedSendFailure = false;
	LastSentLocation = Position;
	LastSentRotation = Rotation;
}

void USAUdpClientComponent::SetMovementInput(const FVector& Input)
{
	if (MovementInput.Equals(Input, 0.01f)) return;
	MovementInput = Input;
	SendAction(Input.IsNearlyZero() ? TEXT("STOP") : TEXT("MOVE"), Input);
}

void USAUdpClientComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bEnabled || !Pawn || !Pawn->IsLocallyControlled()) return;
	Elapsed += DeltaTime;
	if (Elapsed < FMath::Max(StateSendInterval, 0.02f)) return;
	Elapsed = 0.0f;
	if (!Pawn->GetActorLocation().Equals(LastSentLocation, 0.1f)
		|| !Pawn->GetActorRotation().Equals(LastSentRotation, 0.1f)
		|| !MovementInput.IsNearlyZero())
	{
		SendAction(TEXT("STATE"), MovementInput);
	}
}

void USAUdpClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Socket)
	{
		Socket->Close();
		if (ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
			Subsystem->DestroySocket(Socket);
		Socket = nullptr;
	}
	RemoteAddress.Reset();
	Super::EndPlay(EndPlayReason);
}
