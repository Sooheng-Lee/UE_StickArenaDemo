#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SAPlayerRegistration.h"
#include "Components/SAPlayerNetworkState.h"
#include "SAUdpClientComponent.generated.h"

class FSocket;
class FInternetAddr;
class ASAEnemyCharacter;

// Registers local players and sends actions to the standalone UDP console server.
UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class UE_STICKARENADEMO_API USAUdpClientComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USAUdpClientComponent();
	void SendAction(const TCHAR* Action, const FVector& Input = FVector::ZeroVector);
	void SetMovementInput(const FVector& Input);
	UFUNCTION(BlueprintPure, Category="UDP")
	ASAEnemyCharacter* GetRemoteCharacter(const FString& PlayerId) const;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UPROPERTY(BlueprintAssignable, Category="UDP")
	FSAOnRemotePlayerState OnRemotePlayerStateReceived;
	UPROPERTY(BlueprintAssignable, Category="UDP")
	FSAOnRemotePlayerRemoved OnRemotePlayerRemoved;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category="UDP")
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, Category="UDP")
	FString ServerAddress = TEXT("127.0.0.1");
	UPROPERTY(EditAnywhere, Category="UDP", meta=(ClampMin="1", ClampMax="65535"))
	int32 ServerPort = 7777;
	UPROPERTY(EditAnywhere, Category="UDP", meta=(ClampMin="0.02"))
	float StateSendInterval = 0.1f;
	UPROPERTY(EditDefaultsOnly, Category="UDP")
	TSubclassOf<ASAEnemyCharacter> EnemyCharacterClass;

private:
	bool EnsureSocket();
	void SendRegistration();
	void ReceivePackets();
	bool SendPacket(const FString& Packet);
	void UpdateRemoteCharacter(const FString& PlayerId);
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="UDP", meta=(AllowPrivateAccess="true"))
	FSAPlayerRegistration PlayerRegistration;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="UDP", meta=(AllowPrivateAccess="true"))
	FSAPlayerNetworkState LocalPlayerState;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="UDP", meta=(AllowPrivateAccess="true"))
	TMap<FString, FSAPlayerNetworkState> RemotePlayerStates;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="UDP", meta=(AllowPrivateAccess="true"))
	TMap<FString, FSAPlayerRegistration> RemotePlayerRegistrations;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="UDP", meta=(AllowPrivateAccess="true"))
	TMap<FString, TObjectPtr<ASAEnemyCharacter>> RemoteCharacters;
	TMap<FString, double> RemoteLastReceived;
	bool bRegistrationAcknowledged = false;
	float RegistrationElapsed = 1.0f;
	FSocket* Socket = nullptr;
	TSharedPtr<FInternetAddr> RemoteAddress;
	FString SessionId;
	int64 Sequence = 0;
	int64 AttackSequence = 0;
	FVector MovementInput = FVector::ZeroVector;
	float Elapsed = 0.0f;
	bool bReportedSendFailure = false;
};
