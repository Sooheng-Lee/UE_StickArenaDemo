#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SALevelObject.generated.h"

class UStaticMeshComponent;

/** Base class for level props that react to attack hits. */
UCLASS(Abstract)
class UE_STICKARENADEMO_API ASALevelObject : public AActor
{
	GENERATED_BODY()

public:
	ASALevelObject();

	UFUNCTION(BlueprintCallable, Category="Level Object")
	virtual void PlayHitEffect();
	virtual void RequestHit(AActor* Attacker);

	UStaticMeshComponent* GetObjectMesh() const { return ObjectMesh; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level Object")
	TObjectPtr<UStaticMeshComponent> ObjectMesh;
};
