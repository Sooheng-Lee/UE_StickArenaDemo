#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SACameraOcclusionComponent.generated.h"

class APlayerController;
class UPrimitiveComponent;
class UStaticMesh;

/** Hides camera-blocking palm trees only from the owning player's view. */
UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class UE_STICKARENADEMO_API USACameraOcclusionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USACameraOcclusionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	void UpdateOcclusion();
	void ClearOcclusion();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Occlusion")
	bool bHideObstacles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Occlusion", meta=(ClampMin="1.0"))
	float ProbeRadius = 32.0f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY() TObjectPtr<UStaticMesh> PalmTreeMesh;
	TWeakObjectPtr<APlayerController> ViewController;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> HiddenByOcclusion;
};
