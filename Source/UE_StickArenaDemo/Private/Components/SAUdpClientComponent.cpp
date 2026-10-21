#include "Components/SAUdpClientComponent.h"
#include "GamePlay/SAUdpClientGameInstance.h"
#include "Characters/SAEnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include <charconv>
#include "LevelObjects/SATreasureBox.h"
#include "LevelObjects/SACrystalTower.h"
#include "EngineUtils.h"
#include "Items/SAWeaponActor.h"
#include "Components/SAStatusComponent.h"
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
		if ((Fields.Num() != 11 && Fields.Num() != 15 && Fields.Num() != 19) || Fields[0] != TEXT("SA3") || Fields[1] != TEXT("type=PLAYER_STATE")) return false;
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
		if (Fields.Num() >= 15)
		{
			const TCHAR* WeaponKeys[] = { TEXT("weapon="), TEXT("weaponid="), TEXT("ammo="), TEXT("maxammo=") };
			for (int32 Index = 0; Index < 4; ++Index)
				if (!Fields[Index + 11].RemoveFromStart(WeaponKeys[Index], ESearchCase::CaseSensitive)) return false;
			if (!DecodeRegistrationField(Fields[11], State.WeaponClassPath)) return false;
			State.WeaponInstanceId = Fields[12];
			if (!State.WeaponClassPath.IsEmpty())
			{
				if (State.WeaponInstanceId.Len() != 32) return false;
				for (TCHAR Digit : State.WeaponInstanceId) if (!FChar::IsHexDigit(Digit)) return false;
			}
			else if (!State.WeaponInstanceId.IsEmpty()) return false;
			const auto ParseAmmo = [](const FString& Text, int32& Value)
			{
				const FTCHARToUTF8 Utf8(*Text);
				const auto Result = std::from_chars(Utf8.Get(), Utf8.Get() + Utf8.Length(), Value);
				return Result.ec == std::errc{} && Result.ptr == Utf8.Get() + Utf8.Length() && Value >= 0 && Value <= 1000000;
			};
			if (!ParseAmmo(Fields[13], State.Ammo) || !ParseAmmo(Fields[14], State.MaxAmmo) || State.Ammo > State.MaxAmmo) return false;
			if (State.WeaponClassPath.IsEmpty() && (State.Ammo != 0 || State.MaxAmmo != 0)) return false;
			State.bHasWeaponState = true;
		}
		if (Fields.Num() == 19)
		{
			const TCHAR* StatusKeys[] = { TEXT("hp="), TEXT("maxhp="), TEXT("damage="), TEXT("defense=") };
			float* Values[] = { &State.HP, &State.MaxHP, &State.DefaultDamage, &State.Defense };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				if (!Fields[Index + 15].RemoveFromStart(StatusKeys[Index], ESearchCase::CaseSensitive)) return false;
				const FTCHARToUTF8 Utf8(*Fields[Index + 15]);
				const auto Parsed = std::from_chars(Utf8.Get(), Utf8.Get() + Utf8.Length(), *Values[Index]);
				if (Parsed.ec != std::errc{} || Parsed.ptr != Utf8.Get() + Utf8.Length()
					|| !FMath::IsFinite(*Values[Index]) || *Values[Index] < 0.0f || *Values[Index] > 1000000.0f) return false;
			}
			if (State.HP > State.MaxHP) return false;
			State.bHasStatusState = true;
		}
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
	// Possession can happen later; TickComponent will attach once locally controlled.
	EnsureConnection();
}

bool USAUdpClientComponent::EnsureConnection()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bEnabled || !Pawn || !Pawn->IsLocallyControlled()) return false;
	if (!NetworkGameInstance) NetworkGameInstance = Cast<USAUdpClientGameInstance>(Pawn->GetGameInstance());
	if (!NetworkGameInstance || !NetworkGameInstance->AttachClient(this)) return false;
	const FString ConnectionSession = NetworkGameInstance->GetSessionId();
	if (SessionId != ConnectionSession)
	{
		ResetNetworkState();
		SessionId = ConnectionSession;
		PlayerRegistration.PlayerId = SessionId;
		SendRegistration();
	}
	return true;
}

bool USAUdpClientComponent::IsRegistered() const
{
	return NetworkGameInstance && NetworkGameInstance->IsRegistered();
}

void USAUdpClientComponent::SendAction(const TCHAR* Action, const FVector& Input)
{
	if (!EnsureConnection()) return;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (FCString::Strcmp(Action, TEXT("ATTACK")) == 0) NetworkGameInstance->RecordAttack();
	// Wait for registration so worker processing cannot accept an unknown player.
	if (!IsRegistered()) return;
	LocalPlayerState.PlayerId = SessionId;
	LocalPlayerState.Sequence = NetworkGameInstance->NextStateSequence();
	LocalPlayerState.AttackSequence = NetworkGameInstance->GetAttackSequence();
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
	LocalPlayerState.bHasWeaponState = true;
	LocalPlayerState.WeaponClassPath.Reset();
	LocalPlayerState.WeaponInstanceId.Reset();
	LocalPlayerState.Ammo = 0;
	LocalPlayerState.MaxAmmo = 0;
	if (const ASABaseCharacter* SACharacter = Cast<ASABaseCharacter>(Pawn))
	{
		if (const ASAWeaponActor* Weapon = SACharacter->GetWeapon(); IsValid(Weapon))
		{
			LocalPlayerState.WeaponClassPath = Weapon->GetClass()->GetPathName();
			LocalPlayerState.WeaponInstanceId = Weapon->GetWeaponInstanceId();
			if (const USAStatusComponent* Status = SACharacter->GetStatusComponent())
			{
				LocalPlayerState.MaxAmmo = FMath::Clamp(Status->MaxAmmo, 0, 1000000);
				LocalPlayerState.Ammo = FMath::Clamp(Status->Ammo, 0, LocalPlayerState.MaxAmmo);
			}
		}
		if (const USAStatusComponent* Status = SACharacter->GetStatusComponent())
		{
			LocalPlayerState.bHasStatusState = true;
			LocalPlayerState.MaxHP = FMath::Clamp(Status->MaxHP, 0.0f, 1000000.0f);
			LocalPlayerState.HP = FMath::Clamp(Status->HP, 0.0f, LocalPlayerState.MaxHP);
			LocalPlayerState.DefaultDamage = FMath::Clamp(Status->DefaultDamage, 0.0f, 1000000.0f);
			LocalPlayerState.Defense = FMath::Clamp(Status->Defense, 0.0f, 1000000.0f);
		}
	}
	const FString Packet = FString::Printf(
		TEXT("SA3|type=PLAYER_STATE|session=%s|seq=%lld|action=%s|state=%s|input=%.6f,%.6f,%.6f|pos=%.6f,%.6f,%.6f|vel=%.6f,%.6f,%.6f|rot=%.6f,%.6f,%.6f|attack=%lld|weapon=%s|weaponid=%s|ammo=%d|maxammo=%d|hp=%.9g|maxhp=%.9g|damage=%.9g|defense=%.9g"),
		*State.PlayerId, State.Sequence, *State.Action, ActivityName(State.State),
		State.MovementInput.X, State.MovementInput.Y, State.MovementInput.Z,
		State.Position.X, State.Position.Y, State.Position.Z, State.Velocity.X, State.Velocity.Y, State.Velocity.Z,
		State.Rotation.Pitch, State.Rotation.Yaw, State.Rotation.Roll, State.AttackSequence,
		*EncodeRegistrationField(State.WeaponClassPath), *State.WeaponInstanceId, State.Ammo, State.MaxAmmo,
		State.HP, State.MaxHP, State.DefaultDamage, State.Defense);
	SendPacket(Packet);
}

bool USAUdpClientComponent::SendPacket(const FString& Packet)
{
	return NetworkGameInstance && NetworkGameInstance->SendPacket(Packet);
}

void USAUdpClientComponent::SendRegistration()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !EnsureConnection()) return;
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
	NetworkGameInstance->RegisterPlayer(PlayerRegistration);
}

void USAUdpClientComponent::HandleServerPacket(const FString& Packet)
{
	if (!bEnabled) return;
	if (Packet.StartsWith(TEXT("SA6|type=MATCH_STATE|"))) { ReceiveMatchState(Packet); return; }
        if (Packet.StartsWith(TEXT("SA5|type=CRYSTAL_STATE|"))) { ReceiveCrystal(Packet); return; }
        if (Packet.StartsWith(TEXT("SA5|type=CRYSTAL_ACK|")))
        {
            TArray<FString> Fields;
            Packet.ParseIntoArray(Fields, TEXT("|"), false);
            if (Fields.Num() == 5 && Fields[2] == TEXT("epoch=") + CrystalEpoch && Fields[3].StartsWith(TEXT("id=")))
            {
                const FString Id = Fields[3].RightChop(3);
                if (const auto* Hit = PendingCrystalHits.Find(Id))
                    if (Hit->Packet.Contains(TEXT("|") + Fields[4] + TEXT("|"))) PendingCrystalHits.Remove(Id);
            }
            return;
        }
        if (Packet.StartsWith(TEXT("SA4|type=TREASURE_STATE|"))) { ReceiveTreasure(Packet); return; }
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
            return;
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
			return;
		}
		FSAPlayerNetworkState State;
		if (Packet.Len() > 2048 || !ParsePlayerState(Packet, State) || State.PlayerId == SessionId) return;
		const FSAPlayerNetworkState* Previous = RemotePlayerStates.Find(State.PlayerId);
		if (Previous && Previous->Sequence >= State.Sequence) return;
		RemoteLastReceived.Add(State.PlayerId, FPlatformTime::Seconds());
		RemotePlayerStates.Add(State.PlayerId, State);
		UpdateRemoteCharacter(State.PlayerId);
		OnRemotePlayerStateReceived.Broadcast(State);
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

float USAUdpClientComponent::GetMatchRemainingSeconds() const
{
	return HasMatchTime() ? static_cast<float>(FMath::Max(0.0, MatchDeadline - FPlatformTime::Seconds())) : 0.0f;
}

void USAUdpClientComponent::ReceiveMatchState(const FString& Packet)
{
	TArray<FString> Fields;
	Packet.ParseIntoArray(Fields, TEXT("|"), false);
	if (Fields.Num() != 5 || Fields[0] != TEXT("SA6") || Fields[1] != TEXT("type=MATCH_STATE")
		|| !Fields[2].RemoveFromStart(TEXT("epoch="), ESearchCase::CaseSensitive)
		|| !Fields[3].RemoveFromStart(TEXT("seq="), ESearchCase::CaseSensitive)
		|| !Fields[4].RemoveFromStart(TEXT("remaining_ms="), ESearchCase::CaseSensitive)) return;
	if (Fields[2].IsEmpty() || Fields[2].Len() > 64 || RetiredMatchEpochs.Contains(Fields[2])) return;
	for (TCHAR Digit : Fields[2]) if (!FChar::IsHexDigit(Digit)) return;
	const auto ParseInteger = [](const FString& Text, int64& Value)
	{
		const FTCHARToUTF8 Utf8(*Text);
		const auto Parsed = std::from_chars(Utf8.Get(), Utf8.Get() + Utf8.Length(), Value);
		return Parsed.ec == std::errc{} && Parsed.ptr == Utf8.Get() + Utf8.Length();
	};
	int64 NewSequence = 0;
	int64 RemainingMs = 0;
	if (!ParseInteger(Fields[3], NewSequence) || NewSequence <= 0
		|| !ParseInteger(Fields[4], RemainingMs) || RemainingMs < 0 || RemainingMs > 180000) return;
	const bool bNewMatch = MatchEpoch != Fields[2];
	if (!bNewMatch && NewSequence <= MatchSequence) return;
	const double NewDeadline = FPlatformTime::Seconds() + RemainingMs / 1000.0;
	if (bNewMatch)
	{
		if (!MatchEpoch.IsEmpty()) RetiredMatchEpochs.Add(MatchEpoch);
		MatchEpoch = Fields[2];
		MatchDeadline = NewDeadline;
	}
	else
	{
		// Packet delay/reordering must never add time or reopen an expired match.
		MatchDeadline = FMath::Min(MatchDeadline, NewDeadline);
	}
	MatchSequence = NewSequence;
	if (IsMatchExpired())
	{
		MovementInput = FVector::ZeroVector;
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			Character->ConsumeMovementInputVector();
			Character->GetCharacterMovement()->StopMovementImmediately();
			Character->GetCharacterMovement()->ClearAccumulatedForces();
		}
	}
}

void USAUdpClientComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bEnabled || !Pawn || !Pawn->IsLocallyControlled()) return;
	if (!EnsureConnection()) return;
    const double TreasureNow = FPlatformTime::Seconds();
    for (auto It = PendingCrystalHits.CreateIterator(); It; ++It)
    {
        if (TreasureNow > It.Value().Expires) { It.RemoveCurrent(); continue; }
        if (TreasureNow >= It.Value().NextSend)
        {
            SendPacket(It.Value().Packet);
            It.Value().NextSend = TreasureNow + 0.2;
        }
    }
    for (auto It = PendingTreasureHits.CreateIterator(); It; ++It)
    {
        if (TreasureNow > It.Value().Expires) { It.RemoveCurrent(); continue; }
        if (TreasureNow >= It.Value().NextSend)
        {
            SendPacket(It.Value().Packet);
            It.Value().NextSend = TreasureNow + 0.2;
        }
    }
	for (auto It = PendingTreasurePickups.CreateIterator(); It; ++It)
	{
		ASAWeaponActor* Item = It.Value().Weapon.Get();
		if (!IsValid(Item) || FVector::DistSquared(Pawn->GetActorLocation(), Item->GetActorLocation()) > FMath::Square(350.0f))
		{
			It.RemoveCurrent();
			continue;
		}
		if (IsRegistered() && TreasureNow >= It.Value().NextSend)
		{
			// Refresh position before retrying so a pickup immediately after movement is valid.
			SendAction(TEXT("STATE"), MovementInput);
			SendPacket(It.Value().Packet);
			It.Value().NextSend = TreasureNow + 0.2;
		}
	}
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

void USAUdpClientComponent::ResetNetworkState()
{
	for (const auto& Entry : RemoteCharacters)
		if (IsValid(Entry.Value)) Entry.Value->Destroy();
	ClearTreasure();
	RemoteCharacters.Reset();
	RemotePlayerRegistrations.Reset();
	RemotePlayerStates.Reset();
	RemoteLastReceived.Reset();
	MatchEpoch.Reset();
	RetiredMatchEpochs.Reset();
	MatchSequence = 0;
	MatchDeadline = 0.0;
	CrystalEpoch.Reset();
	RetiredCrystalEpochs.Reset();
	CrystalRevisions.Reset();
	CrystalCycles.Reset();
	AppliedCrystalRewards.Reset();
	PendingCrystalHits.Reset();
	TreasureEpoch.Reset();
	RetiredTreasureEpochs.Reset();
}

void USAUdpClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (NetworkGameInstance) NetworkGameInstance->DetachClient(this);
	ResetNetworkState();
	NetworkGameInstance = nullptr;
	Super::EndPlay(EndPlayReason);
}

void USAUdpClientComponent::SendTreasureHit(const FString& Id)
{
    if (Id.IsEmpty() || TreasureEpoch.IsEmpty() || !IsRegistered() || !EnsureConnection()) return;
    // Only one in-flight hit per box, with the same sequence on every retry.
    if (PendingTreasureHits.Contains(Id)) return;
    FPendingTreasureHit Hit;
    Hit.Packet = FString::Printf(TEXT("SA4|type=BOX_HIT|epoch=%s|session=%s|id=%s|seq=%lld"),
        *TreasureEpoch, *SessionId, *Id, NetworkGameInstance->NextRequestSequence());
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
	TreasureOwners.Reset();
	CompletedLocalPickups.Reset();
	PendingTreasurePickups.Reset();
	RequestedPickupSequences.Reset();
	LastAppliedPickupSequence = 0;
}

void USAUdpClientComponent::RequestTreasurePickup(ASAWeaponActor* Weapon)
{
	if (!IsValid(Weapon) || Weapon->GetTreasureEpoch() != TreasureEpoch || Weapon->GetTreasureId().IsEmpty()
		|| Weapon->GetAttachParentActor() || SessionId.IsEmpty()) return;
	const FString& Id = Weapon->GetTreasureId();
	if (TreasureActors.FindRef(Id) != Weapon || TreasureOwners.Contains(Id) || PendingTreasurePickups.Contains(Id)) return;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !EnsureConnection()) return;
	FPendingTreasurePickup Request;
	const int64 PickupSequence = NetworkGameInstance->NextRequestSequence();
	Request.Packet = FString::Printf(TEXT("SA4|type=PICKUP|epoch=%s|session=%s|id=%s|seq=%lld"),
		*TreasureEpoch, *SessionId, *Id, PickupSequence);
	Request.Weapon = Weapon;
	RequestedPickupSequences.Add(Id, PickupSequence);
	PendingTreasurePickups.Add(Id, MoveTemp(Request));
}

void USAUdpClientComponent::ReceiveTreasure(const FString& Packet)
{
    TArray<FString> Fields;
    Packet.ParseIntoArray(Fields, TEXT("|"), false);
    if ((Fields.Num() != 7 && Fields.Num() != 8) || Fields[0] != TEXT("SA4") || Fields[1] != TEXT("type=TREASURE_STATE")) return;
    const TCHAR* Keys[] = {TEXT("epoch="), TEXT("id="), TEXT("item="), TEXT("hits="), TEXT("pos=")};
    for (int32 I = 0; I < 5; ++I) if (!Fields[I+2].RemoveFromStart(Keys[I], ESearchCase::CaseSensitive)) return;
    const auto IsDigits = [](const FString& S) {
        if (S.IsEmpty() || S.Len() > 20) return false;
        for (TCHAR C : S) if (C < '0' || C > '9') return false;
        return true;
    };
    if (!IsDigits(Fields[2]) || !IsDigits(Fields[3]) || Fields[4] != TEXT("0")
        || Fields[5].Len() != 1 || Fields[5][0] < TCHAR('0') || Fields[5][0] > TCHAR('0' + ASATreasureBox::HitsToBreak)) return;
	FString ItemOwner;
	if (Fields.Num() == 8)
	{
		if (!Fields[7].RemoveFromStart(TEXT("owner="), ESearchCase::CaseSensitive) || Fields[7].Len() != 32 || Fields[5] != FString::FromInt(ASATreasureBox::HitsToBreak)) return;
		for (TCHAR Digit : Fields[7]) if (!FChar::IsHexDigit(Digit)) return;
		ItemOwner = Fields[7];
	}
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
	// Collected is terminal for this epoch: a delayed drop snapshot cannot revive it.
	if (const FString* PreviousOwner = TreasureOwners.Find(Id))
		if (ItemOwner.IsEmpty() || *PreviousOwner != ItemOwner) return;
    AActor* Existing = TreasureActors.FindRef(Id);
	if (!ItemOwner.IsEmpty())
	{
		TreasureOwners.Add(Id, ItemOwner);
		TreasureHits.Add(Id, Hits);
		PendingTreasureHits.Remove(Id);
		PendingTreasurePickups.Remove(Id);
		if (ItemOwner == SessionId && !CompletedLocalPickups.Contains(Id))
		{
			const int64 PickupSequence = RequestedPickupSequences.FindRef(Id);
			// An older lost grant may arrive after a newer weapon has already been equipped.
			if (PickupSequence < LastAppliedPickupSequence)
			{
				CompletedLocalPickups.Add(Id);
				if (IsValid(Existing)) Existing->Destroy();
				TreasureActors.Remove(Id);
				return;
			}
			ASABaseCharacter* Character = Cast<ASABaseCharacter>(GetOwner());
			ASAWeaponActor* Weapon = Cast<ASAWeaponActor>(Existing);
			if (!IsValid(Weapon) && SwordItemClass && Character)
			{
				if (IsValid(Existing)) Existing->Destroy();
				TreasureActors.Remove(Id);
				Weapon = GetWorld()->SpawnActorDeferred<ASAWeaponActor>(SwordItemClass, FTransform(Position),
					Character, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (Weapon)
				{
					Weapon->SetNetworkTreasure(TreasureEpoch, Id);
					Weapon->DisablePickup();
					Weapon->FinishSpawning(FTransform(Position));
					TreasureActors.Add(Id, Weapon);
				}
			}
			if (IsValid(Weapon) && Character && (Character->GetWeapon() == Weapon || Weapon->EquipToCharacter(Character)))
			{
				CompletedLocalPickups.Add(Id);
				LastAppliedPickupSequence = FMath::Max(LastAppliedPickupSequence, PickupSequence);
				TreasureActors.Remove(Id); // Inventory is no longer a ground-drop actor.
				SendAction(TEXT("STATE"), MovementInput);
			}
			return;
		}
		if (IsValid(Existing)) Existing->Destroy();
		TreasureActors.Remove(Id);
		return;
	}
    const int32* Previous = TreasureHits.Find(Id);
    if (Previous && *Previous > Hits) return;
    if (Previous && *Previous == Hits) return; // Includes locally collected/destroyed items.
    if (ASATreasureBox* Box = Cast<ASATreasureBox>(Existing))
    {
        Box->ApplyNetworkHits(Hits);
        if (Hits < ASATreasureBox::HitsToBreak) { TreasureHits.Add(Id, Hits); return; }
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
    if (Hits < ASATreasureBox::HitsToBreak)
    {
        ASATreasureBox* Box = GetWorld()->SpawnActor<ASATreasureBox>(ASATreasureBox::StaticClass(), Position, FRotator::ZeroRotator, Params);
        if (Box) { Box->InitializeNetworkBox(Id, ESATreasureItem::Sword); Box->ApplyNetworkHits(Hits); }
        Spawned = Box;
    }
    else if (SwordItemClass)
    {
		ASAWeaponActor* Weapon = GetWorld()->SpawnActorDeferred<ASAWeaponActor>(SwordItemClass, FTransform(Position),
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Weapon)
		{
			Weapon->SetNetworkTreasure(TreasureEpoch, Id);
			// Store identity before FinishSpawning can raise the initial overlap.
			TreasureActors.Add(Id, Weapon);
			TreasureHits.Add(Id, Hits);
			Weapon->FinishSpawning(FTransform(Position));
		}
		Spawned = Weapon;
    }
    if (Spawned) { TreasureActors.Add(Id, Spawned); TreasureHits.Add(Id, Hits); }
}
void USAUdpClientComponent::SendCrystalHit(const FString& Id)
{
	const int64* Cycle = CrystalCycles.Find(Id);
	if (!Cycle || CrystalEpoch.IsEmpty() || !IsRegistered() || PendingCrystalHits.Contains(Id) || !EnsureConnection()) return;
	FPendingCrystalHit Hit;
	Hit.Packet = FString::Printf(TEXT("SA5|type=CRYSTAL_HIT|epoch=%s|session=%s|id=%s|seq=%lld|cycle=%lld"),
		*CrystalEpoch, *SessionId, *Id, NetworkGameInstance->NextRequestSequence(), *Cycle);
	Hit.Expires = FPlatformTime::Seconds() + 2.0;
	Hit.NextSend = FPlatformTime::Seconds() + 0.2;
	SendAction(TEXT("STATE"), MovementInput);
	SendPacket(Hit.Packet);
	PendingCrystalHits.Add(Id, Hit);
}

void USAUdpClientComponent::ReceiveCrystal(const FString& Packet)
{
	TArray<FString> Fields;
	Packet.ParseIntoArray(Fields, TEXT("|"), false);
	if (Fields.Num() != 11 || Fields[0] != TEXT("SA5") || Fields[1] != TEXT("type=CRYSTAL_STATE")) return;
	const TCHAR* Keys[] = {TEXT("epoch="), TEXT("id="), TEXT("rev="), TEXT("cycle="), TEXT("hits="),
		TEXT("lit="), TEXT("winner="), TEXT("effect="), TEXT("remaining=")};
	for (int32 Index = 0; Index < 9; ++Index)
		if (!Fields[Index + 2].RemoveFromStart(Keys[Index], ESearchCase::CaseSensitive)) return;
	const auto ParseNumber = [](const FString& Text, int64& Number)
	{
		const FTCHARToUTF8 Utf8(*Text);
		const auto Result = std::from_chars(Utf8.Get(), Utf8.Get() + Utf8.Length(), Number);
		return Result.ec == std::errc{} && Result.ptr == Utf8.Get() + Utf8.Length() && Number >= 0;
	};
	int64 EpochNumber = 0, Revision = 0, Cycle = 0, Hits = 0, Lit = 0, Effect = 0;
	if (!ParseNumber(Fields[2], EpochNumber) || Fields[3].IsEmpty() || !ParseNumber(Fields[4], Revision)
		|| !ParseNumber(Fields[5], Cycle) || !ParseNumber(Fields[6], Hits) || Hits > 5
		|| !ParseNumber(Fields[7], Lit) || Lit > 1 || !ParseNumber(Fields[9], Effect) || Effect > 2
		|| (Lit == 1 && Hits >= 5) || (Lit == 0 && Hits != 5)) return;
	float Remaining = 0.0f;
	const FTCHARToUTF8 RemainingText(*Fields[10]);
	const auto Parsed = std::from_chars(RemainingText.Get(), RemainingText.Get() + RemainingText.Length(), Remaining);
	if (Parsed.ec != std::errc{} || Parsed.ptr != RemainingText.Get() + RemainingText.Length()
		|| !FMath::IsFinite(Remaining) || Remaining < 0.0f || Remaining > (Effect == 0 ? 5.0f : 20.0f)) return;
	if (!Fields[8].IsEmpty())
	{
		if (Fields[8].Len() != 32) return;
		for (TCHAR Digit : Fields[8]) if (!FChar::IsHexDigit(Digit)) return;
	}
	if (CrystalEpoch != Fields[2])
	{
		if (RetiredCrystalEpochs.Contains(Fields[2])) return;
		if (!CrystalEpoch.IsEmpty()) RetiredCrystalEpochs.Add(CrystalEpoch);
		CrystalEpoch = Fields[2];
		CrystalRevisions.Reset();
		CrystalCycles.Reset();
		PendingCrystalHits.Reset();
		AppliedCrystalRewards.Reset();
		for (TActorIterator<ASACrystalTower> Tower(GetWorld()); Tower; ++Tower) Tower->ApplyNetworkState(0, false);
	}
	const FString& Id = Fields[3];
	const int64* Previous = CrystalRevisions.Find(Id);
	if (Previous && Revision < *Previous) return;
	if (!Previous || Revision > *Previous)
	{
		bool bFound = false;
		for (TActorIterator<ASACrystalTower> Tower(GetWorld()); Tower; ++Tower)
		{
			if (Tower->GetFName().ToString() != Id) continue;
			Tower->ApplyNetworkState(static_cast<int32>(Hits), Lit == 1);
			bFound = true;
			break;
		}
		if (!bFound) return; // Retry on a later snapshot if the map is still loading.
		CrystalRevisions.Add(Id, Revision);
		CrystalCycles.Add(Id, Cycle);
	}
	const FString RewardId = FString::Printf(TEXT("%s:%lld"), *Id, Cycle);
	if (Lit == 0 && Remaining > 0.0f && !Fields[8].IsEmpty() && !AppliedCrystalRewards.Contains(RewardId))
	{
		ASABaseCharacter* Winner = Fields[8] == SessionId ? Cast<ASABaseCharacter>(GetOwner()) : GetRemoteCharacter(Fields[8]);
		if (IsValid(Winner) && Winner->GetStatusComponent())
		{
			Winner->GetStatusComponent()->ApplyCrystalBlessing(static_cast<ESACrystalBlessing>(Effect), Remaining);
			AppliedCrystalRewards.Add(RewardId);
		}
	}
}
