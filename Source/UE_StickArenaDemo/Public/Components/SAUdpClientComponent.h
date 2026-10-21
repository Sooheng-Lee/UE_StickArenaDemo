#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SAPlayerRegistration.h"
#include "Components/SAPlayerNetworkState.h"
#include "SAUdpClientComponent.generated.h"

class USAUdpClientGameInstance;
class ASAEnemyCharacter;
class ASAWeaponActor;

// Collects Pawn state and applies server packets; the game instance owns the connection.
UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class UE_STICKARENADEMO_API USAUdpClientComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USAUdpClientComponent();
	USAUdpClientGameInstance* GetConnection() const { return NetworkGameInstance; }
	void SendAction(const TCHAR* Action, const FVector& Input = FVector::ZeroVector);
	void SetMovementInput(const FVector& Input);
	// Sample the new transform after CharacterMovement has applied a pending impulse.
	void RequestStateUpdate() { bStateUpdateRequested = true; }
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
	UPROPERTY(EditAnywhere, Category="UDP", meta=(ClampMin="0.02"))
	float StateSendInterval = 0.1f;
	UPROPERTY(EditDefaultsOnly, Category="UDP")
	TSubclassOf<ASAEnemyCharacter> EnemyCharacterClass;

private:
	friend class USAUdpClientGameInstance;
	void HandleServerPacket(const FString& Packet);
	bool EnsureConnection();
	bool IsRegistered() const;
	void ResetNetworkState();
	UPROPERTY(Transient)
	TObjectPtr<USAUdpClientGameInstance> NetworkGameInstance;
	friend class FSAMatchTimerTest;
	void ReceiveMatchState(const FString& Packet);
	FString MatchEpoch;
	TSet<FString> RetiredMatchEpochs;
	int64 MatchSequence = 0;
	double MatchDeadline = 0.0;
	friend class FSACrystalTowerEffectTest;
	void ReceiveCrystal(const FString& Packet);
	FString CrystalEpoch;
	TSet<FString> RetiredCrystalEpochs;
	TMap<FString, int64> CrystalRevisions;
	TMap<FString, int64> CrystalCycles;
	TSet<FString> AppliedCrystalRewards;
	struct FPendingCrystalHit { FString Packet; double Expires = 0; double NextSend = 0; };
	TMap<FString, FPendingCrystalHit> PendingCrystalHits;
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
	struct FPendingTreasurePickup { FString Packet; TWeakObjectPtr<ASAWeaponActor> Weapon; double NextSend = 0; };
	TMap<FString, FPendingTreasurePickup> PendingTreasurePickups;
	TMap<FString, int64> RequestedPickupSequences;
	int64 LastAppliedPickupSequence = 0;
    UPROPERTY(EditDefaultsOnly, Category="Treasure")
    TSubclassOf<AActor> SwordItemClass;
	void SendRegistration();
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
	FString SessionId;
	FVector MovementInput = FVector::ZeroVector;
	float Elapsed = 0.0f;
	bool bStateUpdateRequested = false;
};
