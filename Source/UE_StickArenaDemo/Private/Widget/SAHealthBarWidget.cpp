#include "Widget/SAHealthBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Brushes/SlateRoundedBoxBrush.h"

TSharedRef<SWidget> USAHealthBarWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
		FProgressBarStyle Style;
		Style.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.015f, 0.015f, 0.015f, 0.9f), 4.0f));
		Style.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 4.0f));
		HealthBar->SetWidgetStyle(Style);
		HealthBar->SetBarFillStyle(EProgressBarFillStyle::Scale);
		HealthBar->SetFillColorAndOpacity(FLinearColor(0.85f, 0.025f, 0.035f));
		HealthBar->SetPercent(DisplayedPercent);
		WidgetTree->RootWidget = HealthBar;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void USAHealthBarWidget::UpdateHealth(float HP, float MaxHP, float DeltaSeconds, bool bSnap)
{
	const float Target = MaxHP > 0.0f ? FMath::Clamp(HP / MaxHP, 0.0f, 1.0f) : 0.0f;
	const float Alpha = 1.0f - FMath::Exp(-10.0f * FMath::Max(DeltaSeconds, 0.0f));
	DisplayedPercent = bSnap ? Target : FMath::Lerp(DisplayedPercent, Target, Alpha);
	if (FMath::IsNearlyEqual(DisplayedPercent, Target, 0.0001f)) DisplayedPercent = Target;
	if (HealthBar) HealthBar->SetPercent(DisplayedPercent);
}
