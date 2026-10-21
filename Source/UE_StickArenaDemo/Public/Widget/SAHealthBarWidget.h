#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SAHealthBarWidget.generated.h"

class UProgressBar;

/** Screen-space overhead bar, advanced by its owning character even while hidden. */
UCLASS()
class UE_STICKARENADEMO_API USAHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void UpdateHealth(float HP, float MaxHP, float DeltaSeconds, bool bSnap = false);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;
	float DisplayedPercent = 1.0f;
};
