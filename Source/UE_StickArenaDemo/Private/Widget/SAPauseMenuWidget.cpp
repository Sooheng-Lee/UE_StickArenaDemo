#include "Widget/SAPauseMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/SAPlayerCharacter.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

USAPauseMenuWidget::USAPauseMenuWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
	static ConstructorHelpers::FObjectFinder<UTexture2D> Panel(TEXT("/Game/UI/Menu/T_UI_MenuPanel.T_UI_MenuPanel"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Button(TEXT("/Game/UI/Menu/T_UI_MenuButton.T_UI_MenuButton"));
	PanelTexture = Panel.Object;
	ButtonTexture = Button.Object;
}

TSharedRef<SWidget> USAPauseMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildLayout();
	return Super::RebuildWidget();
}

void USAPauseMenuWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	const auto Place = [](UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	};
	UBorder* Shade = WidgetTree->ConstructWidget<UBorder>();
	Shade->SetBrushColor(FLinearColor(0.005f, 0.003f, 0.002f, 0.72f));
	UCanvasPanelSlot* ShadeSlot = Root->AddChildToCanvas(Shade);
	ShadeSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ShadeSlot->SetOffsets(FMargin(0));
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	UCanvasPanelSlot* ScaleSlot = Root->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ScaleSlot->SetOffsets(FMargin(24));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(560);
	Size->SetHeightOverride(340);
	Scale->SetContent(Size);
	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>();
	Size->SetContent(Panel);
	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetBrushFromTexture(PanelTexture);
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Panel, Background, FVector2D(20, 0), FVector2D(520, 340));
	const auto Button = [this, Panel, &Place](FName Name, const TCHAR* Label, float Y)
	{
		UButton* Widget = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FSlateBrush Brush;
		Brush.SetResourceObject(ButtonTexture);
		Brush.ImageSize = FVector2D(380, 84);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		FButtonStyle Style;
		Style.SetNormal(Brush);
		Brush.TintColor = FSlateColor(FLinearColor(1.35f, 1.2f, 0.85f));
		Style.SetHovered(Brush);
		Brush.TintColor = FSlateColor(FLinearColor(0.65f, 0.52f, 0.32f));
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		Style.SetNormalPadding(FMargin(10));
		Style.SetPressedPadding(FMargin(10, 12, 10, 8));
		Widget->SetStyle(Style);
		UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>();
		Caption->SetText(FText::FromString(Label));
		Caption->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
		Caption->SetColorAndOpacity(FLinearColor(0.98f, 0.87f, 0.65f));
		Caption->SetShadowColorAndOpacity(FLinearColor::Black);
		Caption->SetShadowOffset(FVector2D(1, 1));
		// Scale against the button's design size, even when a short label still fits.
		USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>();
		LabelSize->SetWidthOverride(360);
		LabelSize->SetHeightOverride(64);
		USizeBoxSlot* LabelSlot = CastChecked<USizeBoxSlot>(LabelSize->AddChild(Caption));
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		UScaleBox* LabelScale = WidgetTree->ConstructWidget<UScaleBox>();
		LabelScale->SetStretch(EStretch::ScaleToFit);
		LabelScale->SetStretchDirection(EStretchDirection::DownOnly);
		LabelScale->SetContent(LabelSize);
		UButtonSlot* ContentSlot = CastChecked<UButtonSlot>(Widget->AddChild(LabelScale));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		Place(Panel, Widget, FVector2D(90, Y), FVector2D(380, 84));
		return Widget;
	};
	ContinueButton = Button(TEXT("ContinueButton"), TEXT("계속하기"), 78);
	QuitButton = Button(TEXT("QuitButton"), TEXT("게임 종료"), 178);
	ContinueButton->OnClicked.AddDynamic(this, &USAPauseMenuWidget::ContinueGame);
	QuitButton->OnClicked.AddDynamic(this, &USAPauseMenuWidget::QuitGame);
}

void USAPauseMenuWidget::FocusContinue()
{
	if (ContinueButton) ContinueButton->SetUserFocus(GetOwningPlayer());
}

FReply USAPauseMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		ContinueGame();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

void USAPauseMenuWidget::ContinueGame()
{
	if (ASAPlayerCharacter* Player = Cast<ASAPlayerCharacter>(GetOwningPlayerPawn())) Player->ResumeGame();
}

void USAPauseMenuWidget::QuitGame()
{
	if (ASAPlayerCharacter* Player = Cast<ASAPlayerCharacter>(GetOwningPlayerPawn()))
	{
		SetIsEnabled(false);
		Player->QuitGame();
	}
}
