#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SAUdpClientComponent.generated.h"

class FSocket;
class FInternetAddr;

// Sends diagnostic player actions to the standalone UDP console server.
UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class UE_STICKARENADEMO_API USAUdpClientComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USAUdpClientComponent();
	void SendAction(const TCHAR* Action, const FVector& Input = FVector::ZeroVector);
	void SetMovementInput(const FVector& Input);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

private:
	FSocket* Socket = nullptr;
	TSharedPtr<FInternetAddr> RemoteAddress;
	FString SessionId;
	uint32 Sequence = 0;
	FVector MovementInput = FVector::ZeroVector;
	FVector LastSentLocation = FVector::ZeroVector;
	FRotator LastSentRotation = FRotator::ZeroRotator;
	float Elapsed = 0.0f;
	bool bReportedSendFailure = false;
};
