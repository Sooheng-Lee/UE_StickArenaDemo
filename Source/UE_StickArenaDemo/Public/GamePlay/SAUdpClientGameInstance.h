#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Components/SAPlayerRegistration.h"
#include "Containers/Ticker.h"
#include "SAUdpClientGameInstance.generated.h"

class FSocket;
class FInternetAddr;
class USAUdpClientComponent;

/** One UDP connection per game instance, surviving Pawn replacement and map travel. */
UCLASS(Config=Game)
class UE_STICKARENADEMO_API USAUdpClientGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintCallable, Category="UDP")
	bool ConnectToServer(const FString& Address, int32 Port);
	UFUNCTION(BlueprintCallable, Category="UDP")
	void DisconnectFromServer();
	UFUNCTION(BlueprintPure, Category="UDP")
	bool IsConnected() const { return Socket != nullptr; }
	UFUNCTION(BlueprintPure, Category="UDP")
	bool IsRegistered() const { return bRegistrationAcknowledged; }
	UFUNCTION(BlueprintPure, Category="UDP")
	FString GetSessionId() const { return SessionId; }

	bool AttachClient(USAUdpClientComponent* Client);
	void DetachClient(USAUdpClientComponent* Client);
	void RegisterPlayer(const FSAPlayerRegistration& Info);
	bool SendPacket(const FString& Packet);
	// The core ticker invokes this on the game thread, including during world transitions.
	bool TickNetwork(float DeltaTime);
	int64 NextStateSequence() { return ++StateSequence; }
	int64 GetAttackSequence() const { return AttackSequence; }
	void RecordAttack() { ++AttackSequence; }
	int64 NextRequestSequence() { return ++RequestSequence; }

protected:
	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category="UDP")
	FString ServerAddress = TEXT("127.0.0.1");
	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category="UDP", meta=(ClampMin="1", ClampMax="65535"))
	int32 ServerPort = 7777;

private:
	bool EnsureSocket();
	void SendRegistration();
	void ReceivePackets();
	void CloseSocket();
	TWeakObjectPtr<USAUdpClientComponent> ActiveClient;
	FSocket* Socket = nullptr;
	TSharedPtr<FInternetAddr> RemoteAddress;
	FString SessionId;
	FString RegistrationPacket;
	bool bRegistrationAcknowledged = false;
	bool bConnectionDesired = true;
	bool bReportedSendFailure = false;
	float RegistrationElapsed = 0.0f;
	int64 StateSequence = 0;
	int64 AttackSequence = 0;
	int64 RequestSequence = 0;
	FTSTicker::FDelegateHandle NetworkTickHandle;
	FDelegateHandle PreExitHandle;
};
