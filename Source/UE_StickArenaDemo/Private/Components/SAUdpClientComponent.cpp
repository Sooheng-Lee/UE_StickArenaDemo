#include "Components/SAUdpClientComponent.h"
#include "Characters/SAEnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include <charconv>
#include "Misc/CoreDelegates.h"
#include "LevelObjects/SATreasureBox.h"
#include "Items/SAWeaponActor.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSAUdp, Log, All);

namespace
{
	// Hex-encoded UTF-8 keeps delimiters and Korean names safe in the text protocol.
	FString EncodeRegistrationField(const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		return BytesToHex(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	bool DecodeRegistrationField(const FString& Text, FString& Value)
	{
		if (Text.Len() % 2 != 0 || Text.Len() > 2048) return false;
		TArray<uint8> Bytes;
		for (int32 Index = 0; Index < Text.Len(); Index += 2)
		{
			if (!FChar::IsHexDigit(Text[Index]) || !FChar::IsHexDigit(Text[Index + 1])) return false;
			const uint8 Byte = (FParse::HexDigit(Text[Index]) << 4) | FParse::HexDigit(Text[Index + 1]);
			if (Byte < 32 || Byte == 127) return false;
			Bytes.Add(Byte);
		}
		Value.Reset();
		if (!Bytes.IsEmpty())
		{
			const FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
			Value = FString(Utf8.Length(), Utf8.Get());
		}
		return true;
	}

	bool ParsePlayerInfo(const FString& Packet, FSAPlayerRegistration& Info)
	{
		TArray<FString> Fields;
		Packet.ParseIntoArray(Fields, TEXT("|"), false);
		if (Fields.Num() != 7 || Fields[0] != TEXT("SA2") || Fields[1] != TEXT("type=PLAYER_INFO")) return false;
		const TCHAR* Keys[] = { TEXT("session="), TEXT("name="), TEXT("character="), TEXT("mesh="), TEXT("anim=") };
		for (int32 Index = 0; Index < 5; ++Index)
			if (!Fields[Index + 2].RemoveFromStart(Keys[Index], ESearchCase::CaseSensitive)) return false;
		if (Fields[2].Len() != 32) return false;
		for (TCHAR Digit : Fields[2]) if (!FChar::IsHexDigit(Digit)) return false;
		Info.PlayerId = Fields[2];
		return DecodeRegistrationField(Fields[3], Info.PlayerName)
			&& DecodeRegistrationField(Fields[4], Info.CharacterClassPath) && !Info.CharacterClassPath.IsEmpty()
			&& DecodeRegistrationField(Fields[5], Info.SkeletalMeshPath)
			&& DecodeRegistrationField(Fields[6], Info.AnimInstanceClassPath);
	}

	const TCHAR* ActivityName(ESAPlayerActivity State)
	{
		switch (State)
		{
		case ESAPlayerActivity::Moving: return TEXT("MOVING");
		case ESAPlayerActivity::Falling: return TEXT("FALLING");
		case ESAPlayerActivity::Attacking: return TEXT("ATTACKING");
		default: return TEXT("IDLE");
		}
	}

	bool ParseVector(const FString& Text, FVector& Value)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), false);
		if (Parts.Num() != 3) return false;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FTCHARToUTF8 Utf8(*Parts[Index]);
			double Number = 0;
			const auto Parsed = std::from_chars(Utf8.Get(), Utf8.Get() + Utf8.Length(), Number);
			if (Parsed.ec != std::errc{} || Parsed.ptr != Utf8.Get() + Utf8.Length() || !FMath::IsFinite(Number)) return false;
			Value[Index] = Number;
		}
		return true;
	}

	bool ParsePlayerState(const FString& Packet, FSAPlayerNetworkState& State)
	{
		TArray<FString> Fields;
		Packet.ParseIntoArray(Fields, TEXT("|"), false);
		if (Fields.Num() != 11 || Fields[0] != TEXT("SA3") || Fields[1] != TEXT("type=PLAYER_STATE")) return false;
		const TCHAR* Keys[] = { TEXT("session="), TEXT("seq="), TEXT("action="), TEXT("state="),
			TEXT("input="), TEXT("pos="), TEXT("vel="), TEXT("rot="), TEXT("attack=") };
		for (int32 Index = 0; Index < 9; ++Index)
		{
			if (!Fields[Index + 2].RemoveFromStart(Keys[Index], ESearchCase::CaseSensitive)) return false;
		}
		if (Fields[2].Len() != 32) return false;
		for (TCHAR Digit : Fields[2]) if (!FChar::IsHexDigit(Digit)) return false;
		const FTCHARToUTF8 SequenceText(*Fields[3]);
		const auto SequenceResult = std::from_chars(SequenceText.Get(), SequenceText.Get() + SequenceText.Length(), State.Sequence);
		if (SequenceResult.ec != std::errc{} || SequenceResult.ptr != SequenceText.Get() + SequenceText.Length() || State.Sequence <= 0) return false;
		const FTCHARToUTF8 AttackText(*Fields[10]);
		const auto AttackResult = std::from_chars(AttackText.Get(), AttackText.Get() + AttackText.Length(), State.AttackSequence);
		if (AttackResult.ec != std::errc{} || AttackResult.ptr != AttackText.Get() + AttackText.Length() || State.AttackSequence < 0) return false;
		State.PlayerId = Fields[2];
		State.Action = Fields[4];
		if (State.Action != TEXT("STATE") && State.Action != TEXT("MOVE")
			&& State.Action != TEXT("STOP") && State.Action != TEXT("ATTACK")) return false;
		if (Fields[5] == TEXT("IDLE")) State.State = ESAPlayerActivity::Idle;
		else if (Fields[5] == TEXT("MOVING")) State.State = ESAPlayerActivity::Moving;
		else if (Fields[5] == TEXT("FALLING")) State.State = ESAPlayerActivity::Falling;
		else if (Fields[5] == TEXT("ATTACKING")) State.State = ESAPlayerActivity::Attacking;
		else return false;
		FVector Rotation;
		if (!ParseVector(Fields[6], State.MovementInput) || !ParseVector(Fields[7], State.Position)
			|| !ParseVector(Fields[8], State.Velocity) || !ParseVector(Fields[9], Rotation)) return false;
		State.Rotation = FRotator(Rotation.X, Rotation.Y, Rotation.Z);
		return true;
	}
}

USAUdpClientComponent::USAUdpClientComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	EnemyCharacterClass = ASAEnemyCharacter::StaticClass();
    static ConstructorHelpers::FClassFinder<ASAWeaponActor> Sword(TEXT("/Game/Items/Weapons/Blueprints/BP_WeaponSword"));
    if (Sword.Succeeded()) SwordItemClass = Sword.Class;
}

void USAUdpClientComponent::BeginPlay()
{
	Super::BeginPlay();
    PreExitHandle = FCoreDelegates::OnPreExit.AddUObject(this, &USAUdpClientComponent::SendLeave);
	SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	PlayerRegistration.PlayerId = SessionId;
}

bool USAUdpClientComponent::EnsureSocket()
{
    if (bLeaving) return false;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bEnabled || !Pawn || !Pawn->IsLocallyControlled()) return false;

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem) return false;
	if (!Socket)
	{
		RemoteAddress = Subsystem->CreateInternetAddr();
		bool bValidAddress = false;
		RemoteAddress->SetIp(*ServerAddress, bValidAddress);
		if (!bValidAddress || ServerPort < 1 || ServerPort > 65535)
		{
			UE_LOG(LogSAUdp, Error, TEXT("Invalid UDP endpoint: %s:%d"), *ServerAddress, ServerPort);
			bEnabled = false;
			return false;
		}
		RemoteAddress->SetPort(ServerPort);
		Socket = Subsystem->CreateSocket(NAME_DGram, TEXT("StickArena UDP client"), false);
		if (!Socket || !Socket->SetNonBlocking(true))
		{
			if (Socket) Subsystem->DestroySocket(Socket);
			Socket = nullptr;
			bEnabled = false;
			UE_LOG(LogSAUdp, Error, TEXT("Failed to create UDP socket"));
			return false;
		}
		UE_LOG(LogSAUdp, Log, TEXT("Sending player actions to %s:%d"), *ServerAddress, ServerPort);
	}
	return true;
}

void USAUdpClientComponent::SendAction(const TCHAR* Action, const FVector& Input)
{
	if (!EnsureSocket()) return;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (FCString::Strcmp(Action, TEXT("ATTACK")) == 0) ++AttackSequence;
	if (!bRegistrationAcknowledged && RegistrationElapsed >= 1.0f) SendRegistration();
	// Wait for registration so worker processing cannot accept an unknown player.
	if (!bRegistrationAcknowledged) return;
	LocalPlayerState.PlayerId = SessionId;
	LocalPlayerState.Sequence = ++Sequence;
	LocalPlayerState.AttackSequence = AttackSequence;
	LocalPlayerState.Action = Action;
	LocalPlayerState.MovementInput = FCString::Strcmp(Action, TEXT("ATTACK")) == 0 ? MovementInput : Input;
	LocalPlayerState.Position = Pawn->GetActorLocation();
	LocalPlayerState.Velocity = Pawn->GetVelocity();
	LocalPlayerState.Rotation = Pawn->GetActorRotation();
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	LocalPlayerState.State = FCString::Strcmp(Action, TEXT("ATTACK")) == 0 ? ESAPlayerActivity::Attacking
		: Character && Character->GetCharacterMovement()->IsFalling() ? ESAPlayerActivity::Falling
		: !Input.IsNearlyZero() || !LocalPlayerState.Velocity.IsNearlyZero(1.0f) ? ESAPlayerActivity::Moving
		: ESAPlayerActivity::Idle;
	const FSAPlayerNetworkState& State = LocalPlayerState;
	const FString Packet = FString::Printf(
		TEXT("SA3|type=PLAYER_STATE|session=%s|seq=%lld|action=%s|state=%s|input=%.6f,%.6f,%.6f|pos=%.6f,%.6f,%.6f|vel=%.6f,%.6f,%.6f|rot=%.6f,%.6f,%.6f|attack=%lld"),
		*State.PlayerId, State.Sequence, *State.Action, ActivityName(State.State),
		State.MovementInput.X, State.MovementInput.Y, State.MovementInput.Z,
		State.Position.X, State.Position.Y, State.Position.Z, State.Velocity.X, State.Velocity.Y, State.Velocity.Z,
		State.Rotation.Pitch, State.Rotation.Yaw, State.Rotation.Roll, State.AttackSequence);
	SendPacket(Packet);
}

bool USAUdpClientComponent::SendPacket(const FString& Packet)
{
	const FTCHARToUTF8 Utf8(*Packet);
	if (Utf8.Length() > 8192)
	{
		UE_LOG(LogSAUdp, Warning, TEXT("UDP packet exceeds 8192 bytes"));
		return false;
	}
	int32 BytesSent = 0;
	if (!Socket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), BytesSent, *RemoteAddress)
		|| BytesSent != Utf8.Length())
	{
		if (!bReportedSendFailure)
			UE_LOG(LogSAUdp, Warning, TEXT("UDP send failed"));
		bReportedSendFailure = true;
		return false;
	}
	bReportedSendFailure = false;
	return true;
}

void USAUdpClientComponent::SendRegistration()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !EnsureSocket()) return;
	PlayerRegistration.PlayerName = Pawn->GetName();
	if (const APlayerState* State = Pawn->GetPlayerState())
	{
		if (!State->GetPlayerName().IsEmpty()) PlayerRegistration.PlayerName = State->GetPlayerName();
	}
	PlayerRegistration.CharacterClassPath = Pawn->GetClass()->GetPathName();
	PlayerRegistration.SkeletalMeshPath.Reset();
	PlayerRegistration.AnimInstanceClassPath.Reset();
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			PlayerRegistration.SkeletalMeshPath = GetPathNameSafe(Mesh->GetSkeletalMeshAsset());
			if (const UAnimInstance* Anim = Mesh->GetAnimInstance())
				PlayerRegistration.AnimInstanceClassPath = Anim->GetClass()->GetPathName();
			else if (Mesh->GetAnimClass())
				PlayerRegistration.AnimInstanceClassPath = Mesh->GetAnimClass()->GetPathName();
		}
	}
	const FString Packet = FString::Printf(
		TEXT("SA2|type=REGISTER|session=%s|name=%s|character=%s|mesh=%s|anim=%s"),
		*PlayerRegistration.PlayerId, *EncodeRegistrationField(PlayerRegistration.PlayerName),
		*EncodeRegistrationField(PlayerRegistration.CharacterClassPath),
		*EncodeRegistrationField(PlayerRegistration.SkeletalMeshPath),
		*EncodeRegistrationField(PlayerRegistration.AnimInstanceClassPath));
	SendPacket(Packet);
	RegistrationElapsed = 0.0f;
}

void USAUdpClientComponent::ReceivePackets()
{
	if (!Socket) return;
	uint32 Pending = 0;
	// Bound work per frame even if unsolicited packets arrive.
	for (int32 Count = 0; Count < 256 && Socket && Socket->HasPendingData(Pending); ++Count)
	{
		uint8 Buffer[8193];
		int32 Received = 0;
		TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		if (!Socket->RecvFrom(Buffer, 8192, Received, *Sender)) continue;
		if (!(*Sender == *RemoteAddress)) continue;
		const FString Expected = TEXT("SA2|type=REGISTER_ACK|session=") + SessionId;
		const FTCHARToUTF8 Utf8(*Expected);
		if (Received == Utf8.Length() && FMemory::Memcmp(Buffer, Utf8.Get(), Received) == 0)
		{
			if (!bRegistrationAcknowledged) UE_LOG(LogSAUdp, Log, TEXT("Player registered: %s"), *SessionId);
			bRegistrationAcknowledged = true;
			continue;
		}
		bool bAscii = Received > 0 && Received <= 8192;
		for (int32 Index = 0; Index < Received; ++Index) bAscii = bAscii && Buffer[Index] >= 32 && Buffer[Index] <= 126;
		if (!bAscii) continue;
		Buffer[Received] = 0;
		const FString Packet = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer));
        if (Packet.StartsWith(TEXT("SA4|type=TREASURE_STATE|"))) { ReceiveTreasure(Packet); continue; }
        if (Packet.StartsWith(TEXT("SA4|type=HIT_ACK|")))
        {
            TArray<FString> Fields;
            Packet.ParseIntoArray(Fields, TEXT("|"), false);
            if (Fields.Num() == 5 && Fields[2] == TEXT("epoch=") + TreasureEpoch)
            {
                const FString Id = Fields[3].RightChop(3);
                if (const auto* PendingHit = PendingTreasureHits.Find(Id))
                    if (PendingHit->Packet.EndsWith(TEXT("|") + Fields[4])) PendingTreasureHits.Remove(Id);
            }
            continue;
        }
		if (Packet.StartsWith(TEXT("SA2|type=PLAYER_INFO|"), ESearchCase::CaseSensitive))
		{
			FSAPlayerRegistration Info;
			if (ParsePlayerInfo(Packet, Info) && Info.PlayerId != SessionId)
			{
				RemotePlayerRegistrations.Add(Info.PlayerId, Info);
				RemoteLastReceived.Add(Info.PlayerId, FPlatformTime::Seconds());
				UpdateRemoteCharacter(Info.PlayerId);
			}
			continue;
		}
		FSAPlayerNetworkState State;
		if (Received > 2048 || !ParsePlayerState(Packet, State) || State.PlayerId == SessionId) continue;
		const FSAPlayerNetworkState* Previous = RemotePlayerStates.Find(State.PlayerId);
		if (Previous && Previous->Sequence >= State.Sequence) continue;
		RemoteLastReceived.Add(State.PlayerId, FPlatformTime::Seconds());
		RemotePlayerStates.Add(State.PlayerId, State);
		UpdateRemoteCharacter(State.PlayerId);
		OnRemotePlayerStateReceived.Broadcast(State);
	}
}

void USAUdpClientComponent::UpdateRemoteCharacter(const FString& PlayerId)
{
	const FSAPlayerRegistration* Info = RemotePlayerRegistrations.Find(PlayerId);
	const FSAPlayerNetworkState* State = RemotePlayerStates.Find(PlayerId);
	if (!Info || !State || !EnemyCharacterClass || !GetWorld()) return;
	ASAEnemyCharacter* Enemy = RemoteCharacters.FindRef(PlayerId);
	if (!IsValid(Enemy))
	{
		const FTransform Transform(State->Rotation, State->Position);
		Enemy = GetWorld()->SpawnActorDeferred<ASAEnemyCharacter>(EnemyCharacterClass, Transform,
			GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Enemy) return;
		Enemy->InitializeRemotePlayer(*Info);
		Enemy->FinishSpawning(Transform);
		RemoteCharacters.Add(PlayerId, Enemy);
		UE_LOG(LogSAUdp, Log, TEXT("Spawned remote enemy %s for player %s"), *Enemy->GetName(), *PlayerId);
	}
	else
	{
		Enemy->InitializeRemotePlayer(*Info);
	}
	Enemy->ApplyNetworkState(*State);
}

ASAEnemyCharacter* USAUdpClientComponent::GetRemoteCharacter(const FString& PlayerId) const
{
	return RemoteCharacters.FindRef(PlayerId);
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
	if (!EnsureSocket()) return;
	ReceivePackets();
    const double TreasureNow = FPlatformTime::Seconds();
    for (auto It = PendingTreasureHits.CreateIterator(); It; ++It)
    {
        if (TreasureNow > It.Value().Expires) { It.RemoveCurrent(); continue; }
        if (TreasureNow >= It.Value().NextSend)
        {
            SendPacket(It.Value().Packet);
            It.Value().NextSend = TreasureNow + 0.2;
        }
    }
	RegistrationElapsed += DeltaTime;
	// Periodic registration also recovers if the server restarts or expires a session.
	if (RegistrationElapsed >= (bRegistrationAcknowledged ? 3.0f : 1.0f)) SendRegistration();
	TArray<FString> Expired;
	for (const auto& Entry : RemoteLastReceived)
		if (FPlatformTime::Seconds() - Entry.Value > 10.0) Expired.Add(Entry.Key);
	for (const FString& PlayerId : Expired)
	{
		RemotePlayerStates.Remove(PlayerId);
		RemotePlayerRegistrations.Remove(PlayerId);
		RemoteLastReceived.Remove(PlayerId);
		if (ASAEnemyCharacter* Enemy = RemoteCharacters.FindRef(PlayerId)) Enemy->Destroy();
		RemoteCharacters.Remove(PlayerId);
		OnRemotePlayerRemoved.Broadcast(PlayerId);
	}
	Elapsed += DeltaTime;
	if (Elapsed < FMath::Max(StateSendInterval, 0.02f)) return;
	Elapsed = 0.0f;
	// Idle heartbeats let late joiners receive all current players and expire disconnected peers.
	SendAction(TEXT("STATE"), MovementInput);
}

void USAUdpClientComponent::SendLeave()
{
    if (bLeaving) return;
    bLeaving = true;
    // Use the existing socket before closing it; never create a connection during teardown.
    if (Socket && RemoteAddress.IsValid() && !SessionId.IsEmpty())
    {
        const FString LeavePacket = TEXT("SA2|type=LEAVE|session=") + SessionId;
        for (int32 Attempt = 0; Attempt < 3; ++Attempt) SendPacket(LeavePacket);
    }
}

void USAUdpClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SendLeave();
    FCoreDelegates::OnPreExit.Remove(PreExitHandle);
	for (const auto& Entry : RemoteCharacters)
		if (IsValid(Entry.Value)) Entry.Value->Destroy();
	ClearTreasure();
    RemoteCharacters.Reset();
	RemotePlayerRegistrations.Reset();
	RemotePlayerStates.Reset();
	RemoteLastReceived.Reset();
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

void USAUdpClientComponent::SendTreasureHit(const FString& Id)
{
    if (Id.IsEmpty() || TreasureEpoch.IsEmpty() || !bRegistrationAcknowledged || !EnsureSocket()) return;
    // Only one in-flight hit per box, with the same sequence on every retry.
    if (PendingTreasureHits.Contains(Id)) return;
    FPendingTreasureHit Hit;
    Hit.Packet = FString::Printf(TEXT("SA4|type=BOX_HIT|epoch=%s|session=%s|id=%s|seq=%lld"),
        *TreasureEpoch, *SessionId, *Id, ++TreasureHitSequence);
    Hit.Expires = FPlatformTime::Seconds() + 2.0;
    Hit.NextSend = FPlatformTime::Seconds() + 0.2;
    SendPacket(Hit.Packet);
    PendingTreasureHits.Add(Id, Hit);
}

void USAUdpClientComponent::ClearTreasure()
{
    for (const auto& Entry : TreasureActors) if (IsValid(Entry.Value)) Entry.Value->Destroy();
    TreasureActors.Reset();
    TreasureHits.Reset();
    PendingTreasureHits.Reset();
}

void USAUdpClientComponent::ReceiveTreasure(const FString& Packet)
{
    TArray<FString> Fields;
    Packet.ParseIntoArray(Fields, TEXT("|"), false);
    if (Fields.Num() != 7 || Fields[0] != TEXT("SA4") || Fields[1] != TEXT("type=TREASURE_STATE")) return;
    const TCHAR* Keys[] = {TEXT("epoch="), TEXT("id="), TEXT("item="), TEXT("hits="), TEXT("pos=")};
    for (int32 I = 0; I < 5; ++I) if (!Fields[I+2].RemoveFromStart(Keys[I], ESearchCase::CaseSensitive)) return;
    const auto IsDigits = [](const FString& S) {
        if (S.IsEmpty() || S.Len() > 20) return false;
        for (TCHAR C : S) if (C < '0' || C > '9') return false;
        return true;
    };
    if (!IsDigits(Fields[2]) || !IsDigits(Fields[3]) || Fields[4] != TEXT("0")
        || (Fields[5] != TEXT("0") && Fields[5] != TEXT("1") && Fields[5] != TEXT("2") && Fields[5] != TEXT("3"))) return;
    FVector Position;
    if (!ParseVector(Fields[6], Position) || Position.GetAbsMax() > 1.e8 || !GetWorld()) return;
    if (TreasureEpoch != Fields[2])
    {
        if (RetiredTreasureEpochs.Contains(Fields[2])) return;
        if (!TreasureEpoch.IsEmpty()) RetiredTreasureEpochs.Add(TreasureEpoch);
        ClearTreasure();
        TreasureEpoch = Fields[2];
    }
    const FString& Id = Fields[3];
    const int32 Hits = FCString::Atoi(*Fields[5]);
    const int32* Previous = TreasureHits.Find(Id);
    if (Previous && *Previous > Hits) return;
    AActor* Existing = TreasureActors.FindRef(Id);
    if (Previous && *Previous == Hits) return; // Includes locally collected/destroyed items.
    if (ASATreasureBox* Box = Cast<ASATreasureBox>(Existing))
    {
        Box->ApplyNetworkHits(Hits);
        if (Hits < 3) { TreasureHits.Add(Id, Hits); return; }
        Position = Box->GetActorLocation();
        Box->Destroy();
        TreasureActors.Remove(Id);
    }
    // The console server has no terrain collision. Resolve its candidate XY onto the local map.
    FHitResult Floor;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TreasureGround), false);
    Query.AddIgnoredActor(GetOwner());
    for (const auto& Entry : TreasureActors) if (IsValid(Entry.Value)) Query.AddIgnoredActor(Entry.Value);
    if (!Existing && GetWorld()->LineTraceSingleByChannel(Floor, Position + FVector(0,0,500), Position - FVector(0,0,1500), ECC_Visibility, Query))
        Position = Floor.ImpactPoint;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Spawned = nullptr;
    if (Hits < 3)
    {
        ASATreasureBox* Box = GetWorld()->SpawnActor<ASATreasureBox>(ASATreasureBox::StaticClass(), Position, FRotator::ZeroRotator, Params);
        if (Box) { Box->InitializeNetworkBox(Id, ESATreasureItem::Sword); Box->ApplyNetworkHits(Hits); }
        Spawned = Box;
    }
    else if (SwordItemClass)
    {
        Spawned = GetWorld()->SpawnActor<AActor>(SwordItemClass, Position, FRotator::ZeroRotator, Params);
    }
    if (Spawned) { TreasureActors.Add(Id, Spawned); TreasureHits.Add(Id, Hits); }
}
