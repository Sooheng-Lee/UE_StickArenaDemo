#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SAPlayerWidget.generated.h"

class USAStatusComponent;
class USAUdpClientComponent;
class UTextBlock;
class UProgressBar;
class UImage;
class UTexture2D;

/** Native HUD; a Widget Blueprint may replace the layout using the optional named widgets. */
UCLASS()
class UE_STICKARENADEMO_API USAPlayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USAPlayerWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="HUD")
	void SetStatusComponent(USAStatusComponent* InStatusComponent);

	UFUNCTION(BlueprintCallable, Category="HUD")
	void RefreshStatus();

	UFUNCTION(BlueprintCallable, Category="HUD")
	void SetMatchClient(USAUdpClientComponent* InMatchClient);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Icons")
	TObjectPtr<UTexture2D> AttackIconTexture;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Icons")
	TObjectPtr<UTexture2D> DefenseIconTexture;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Icons")
	TObjectPtr<UTexture2D> AmmoIconTexture;

	UPROPERTY(BlueprintReadOnly, Transient, Category="HUD")
	TObjectPtr<USAStatusComponent> StatusComponent;
	UPROPERTY(Transient)
	TObjectPtr<USAUdpClientComponent> MatchClient;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD")
	TObjectPtr<UTextBlock> MatchTimeText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD")
	TObjectPtr<UTextBlock> AttackText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD")
	TObjectPtr<UTextBlock> DefenseText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD")
	TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD")
	TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD")
	TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD")
	TObjectPtr<UImage> MinimapImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Minimap")
	bool bShowMinimap = true;

private:
	void BuildDefaultLayout();
};
