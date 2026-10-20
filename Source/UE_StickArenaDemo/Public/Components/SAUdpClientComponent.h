#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SAPlayerRegistration.h"
#include "Components/SAPlayerNetworkState.h"
#include "SAUdpClientComponent.generated.h"

class FSocket;
class FInternetAddr;
class ASAEnemyCharacter;
class ASAWeaponActor;

// Registers local players and sends actions to the standalone UDP console server.
UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class UE_STICKARENADEMO_API USAUdpClientComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USAUdpClientComponent();
	void SendAction(const TCHAR* Action, const FVector& Input = FVector::ZeroVector);
	void SetMovementInput(const FVector& Input);
	UFUNCTION(BlueprintPure, Category="Match")
	float GetMatchRemainingSeconds() const;
	UFUNCTION(BlueprintPure, Category="Match")
	bool HasMatchTime() const { return !MatchEpoch.IsEmpty(); }
	UFUNCTION(BlueprintPure, Category="Match")
	bool IsMatchExpired() const { return HasMatchTime() && GetMatchRemainingSeconds() <= 0.0f; }
    void SendTreasureHit(const FString& Id);
	void SendCrystalHit(const FString& Id);
	void RequestTreasurePickup(ASAWeaponActor* Weapon);
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
	friend class FSAMatchTimerTest;
	void ReceiveMatchState(const FString& Packet);
	FString MatchEpoch;
	TSet<FString> RetiredMatchEpochs;
	int64 MatchSequence = 0;
	double MatchDeadline = 0.0;
	bool EnsureSocket();
	friend class FSACrystalTowerEffectTest;
	void ReceiveCrystal(const FString& Packet);
	FString CrystalEpoch;
	TSet<FString> RetiredCrystalEpochs;
	TMap<FString, int64> CrystalRevisions;
	TMap<FString, int64> CrystalCycles;
	TSet<FString> AppliedCrystalRewards;
	struct FPendingCrystalHit { FString Packet; double Expires = 0; double NextSend = 0; };
	TMap<FString, FPendingCrystalHit> PendingCrystalHits;
	int64 CrystalHitSequence = 0;
    void SendLeave();
    FDelegateHandle PreExitHandle;
    bool bLeaving = false;
    friend class FSATreasureNetworkTest;
	friend class FSATreasurePickupIntegrationTest;
    void ReceiveTreasure(const FString& Packet);
    void ClearTreasure();
    UPROPERTY()
    TMap<FString, TObjectPtr<AActor>> TreasureActors;
    TMap<FString, int32> TreasureHits;
	TMap<FString, FString> TreasureOwners;
	TSet<FString> CompletedLocalPickups;
    FString TreasureEpoch;
    TSet<FString> RetiredTreasureEpochs;
    struct FPendingTreasureHit { FString Packet; double Expires = 0; double NextSend = 0; };
    TMap<FString, FPendingTreasureHit> PendingTreasureHits;
    int64 TreasureHitSequence = 0;
	struct FPendingTreasurePickup { FString Packet; TWeakObjectPtr<ASAWeaponActor> Weapon; double NextSend = 0; };
	TMap<FString, FPendingTreasurePickup> PendingTreasurePickups;
	int64 TreasurePickupSequence = 0;
	TMap<FString, int64> RequestedPickupSequences;
	int64 LastAppliedPickupSequence = 0;
    UPROPERTY(EditDefaultsOnly, Category="Treasure")
    TSubclassOf<AActor> SwordItemClass;
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
