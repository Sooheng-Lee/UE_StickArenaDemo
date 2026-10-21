#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SAPauseMenuWidget.generated.h"

class UButton;
class UTexture2D;

/** Local menu; the multiplayer world and connection keep ticking. */
UCLASS()
class UE_STICKARENADEMO_API USAPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	USAPauseMenuWidget(const FObjectInitializer& ObjectInitializer);
	void FocusContinue();
	UFUNCTION() void ContinueGame();
	UFUNCTION() void QuitGame();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	UPROPERTY() TObjectPtr<UTexture2D> PanelTexture;
	UPROPERTY() TObjectPtr<UTexture2D> ButtonTexture;
	UPROPERTY(Transient) TObjectPtr<UButton> ContinueButton;
	UPROPERTY(Transient) TObjectPtr<UButton> QuitButton;
	void BuildLayout();
};
