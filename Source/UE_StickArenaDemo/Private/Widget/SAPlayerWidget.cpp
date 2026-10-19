#include "Widget/SAPlayerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Characters/SABaseCharacter.h"
#include "Components/SAStatusComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Items/SAWeaponActor.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor Gold(0.72f, 0.49f, 0.19f);
	const FLinearColor PanelColor(0.035f, 0.022f, 0.014f, 0.90f);

	void SetTextIfChanged(UTextBlock* Widget, const FText& Text)
	{
		if (Widget && !Widget->GetText().EqualTo(Text)) Widget->SetText(Text);
	}
}

USAPlayerWidget::USAPlayerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> Attack(TEXT("/Game/UI/Icons/T_UI_Attack.T_UI_Attack"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Defense(TEXT("/Game/UI/Icons/T_UI_Defense.T_UI_Defense"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Ammo(TEXT("/Game/UI/Icons/T_UI_Ammo.T_UI_Ammo"));
	AttackIconTexture = Attack.Object;
	DefenseIconTexture = Defense.Object;
	AmmoIconTexture = Ammo.Object;
}

TSharedRef<SWidget> USAPlayerWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();
	return Super::RebuildWidget();
}

void USAPlayerWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDCanvas"));
	WidgetTree->RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	const auto Text = [this](const FName Name, const FString& Label, int32 FontSize, FLinearColor Color)
	{
		UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(FText::FromString(Label));
		Result->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
		Result->SetColorAndOpacity(Color);
		Result->SetShadowColorAndOpacity(FLinearColor::Black);
		Result->SetShadowOffset(FVector2D(1.0f, 1.0f));
		return Result;
	};
	const auto Panel = [this, Root](const FName Name, FAnchors Anchors, FMargin Offsets, FVector2D Alignment)
	{
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrushColor(Gold);
		if (Name == TEXT("HealthPanel"))
		{
			Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 12.0f));
		}
		Frame->SetPadding(FMargin(2.0f));
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Frame);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrushColor(PanelColor);
		if (Name == TEXT("HealthPanel"))
		{
			Background->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 10.0f));
		}
		Background->SetPadding(FMargin(10.0f, 6.0f));
		Frame->SetContent(Background);
		UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
		Background->SetContent(Content);
		return Content;
	};
	const auto Icon = [this](const FName Name, UTexture2D* Texture)
	{
		UImage* Result = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Result->SetBrushFromTexture(Texture, true);
		return Result;
	};
	const auto Place = [](UCanvasPanel* Parent, UWidget* Child, FVector2D Position, FVector2D Size)
	{
		UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	};

	UCanvasPanel* Stats = Panel(TEXT("CombatPanel"), FAnchors(0.0f, 0.0f), FMargin(28, 28, 280, 100), FVector2D::ZeroVector);
	Place(Stats, Icon(TEXT("AttackIcon"), AttackIconTexture), FVector2D(6, 2), FVector2D(34, 34));
	Place(Stats, Text(TEXT("AttackLabel"), TEXT("ATK"), 22, FLinearColor::White), FVector2D(50, 0), FVector2D(90, 38));
	AttackText = Text(TEXT("AttackText"), TEXT("0"), 22, FLinearColor::White);
	Place(Stats, AttackText, FVector2D(160, 0), FVector2D(90, 38));
	UImage* Divider = WidgetTree->ConstructWidget<UImage>();
	Divider->SetColorAndOpacity(Gold);
	Place(Stats, Divider, FVector2D(0, 42), FVector2D(256, 1));
	Place(Stats, Icon(TEXT("DefenseIcon"), DefenseIconTexture), FVector2D(6, 48), FVector2D(34, 34));
	Place(Stats, Text(TEXT("DefenseLabel"), TEXT("DEF"), 22, FLinearColor::White), FVector2D(50, 46), FVector2D(90, 38));
	DefenseText = Text(TEXT("DefenseText"), TEXT("2"), 22, FLinearColor::White);
	Place(Stats, DefenseText, FVector2D(160, 46), FVector2D(90, 38));

	UCanvasPanel* Health = Panel(TEXT("HealthPanel"), FAnchors(0.02f, 1.0f, 0.39f, 1.0f), FMargin(0, -64, 0, 64), FVector2D(0, 1));
	UHorizontalBox* HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	UCanvasPanelSlot* HealthRowSlot = Health->AddChildToCanvas(HealthRow);
	HealthRowSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	HealthRowSlot->SetOffsets(FMargin(0));
	UTextBlock* Heart = Text(TEXT("HealthIcon"), TEXT("♥"), 32, FLinearColor(0.9f, 0.035f, 0.04f));
	HealthRow->AddChildToHorizontalBox(Heart)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* HPLabelSlot = HealthRow->AddChildToHorizontalBox(Text(TEXT("HealthLabel"), TEXT("HP"), 22, FLinearColor::White));
	HPLabelSlot->SetPadding(FMargin(12, 0));
	HPLabelSlot->SetVerticalAlignment(VAlign_Center);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	FProgressBarStyle BarStyle;
	FSlateRoundedBoxBrush BackgroundBrush(FLinearColor(0.10f, 0.055f, 0.025f), 8.0f);
	FSlateRoundedBoxBrush FillBrush(FLinearColor::White, 8.0f);
	BarStyle.SetBackgroundImage(BackgroundBrush).SetFillImage(FillBrush);
	HealthBar->SetWidgetStyle(BarStyle);
	HealthBar->SetBarFillStyle(EProgressBarFillStyle::Scale);
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.82f, 0.015f, 0.025f));
	UHorizontalBoxSlot* BarSlot = HealthRow->AddChildToHorizontalBox(HealthBar);
	BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BarSlot->SetPadding(FMargin(0, 12, 12, 12));
	HealthText = Text(TEXT("HealthText"), TEXT("0 / 0"), 20, FLinearColor::White);
	HealthRow->AddChildToHorizontalBox(HealthText)->SetVerticalAlignment(VAlign_Center);

	UCanvasPanel* Ammo = Panel(TEXT("AmmoPanel"), FAnchors(0.76f, 1.0f, 0.98f, 1.0f), FMargin(0, -64, 0, 64), FVector2D(0, 1));
	UHorizontalBox* AmmoRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	UCanvasPanelSlot* AmmoRowSlot = Ammo->AddChildToCanvas(AmmoRow);
	AmmoRowSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	AmmoRowSlot->SetOffsets(FMargin(0));
	USizeBox* AmmoIconSize = WidgetTree->ConstructWidget<USizeBox>();
	AmmoIconSize->SetWidthOverride(34.0f);
	AmmoIconSize->SetHeightOverride(34.0f);
	AmmoIconSize->SetContent(Icon(TEXT("AmmoIcon"), AmmoIconTexture));
	AmmoRow->AddChildToHorizontalBox(AmmoIconSize)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* AmmoLabelSlot = AmmoRow->AddChildToHorizontalBox(Text(TEXT("AmmoLabel"), TEXT("AMMO"), 20, FLinearColor::White));
	AmmoLabelSlot->SetPadding(FMargin(12, 0));
	AmmoLabelSlot->SetVerticalAlignment(VAlign_Center);
	AmmoText = Text(TEXT("AmmoText"), TEXT("0 / 0"), 22, FLinearColor::White);
	UHorizontalBoxSlot* AmmoValueSlot = AmmoRow->AddChildToHorizontalBox(AmmoText);
	AmmoValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AmmoValueSlot->SetHorizontalAlignment(HAlign_Right);
	AmmoValueSlot->SetVerticalAlignment(VAlign_Center);

	if (bShowMinimap)
	{
		UCanvasPanel* Map = Panel(TEXT("MinimapPanel"), FAnchors(1, 0), FMargin(-28, 28, 224, 244), FVector2D(1, 0));
		MinimapImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MinimapImage"));
		// Reserved image area only; map content can be supplied later.
		MinimapImage->SetColorAndOpacity(FLinearColor(0.015f, 0.012f, 0.01f, 0.75f));
		UCanvasPanelSlot* MapSlot = Map->AddChildToCanvas(MinimapImage);
		MapSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		MapSlot->SetOffsets(FMargin(0, 22, 0, 0));
		UTextBlock* North = Text(TEXT("NorthLabel"), TEXT("N"), 18, Gold);
		North->SetJustification(ETextJustify::Center);
		UCanvasPanelSlot* NorthSlot = Map->AddChildToCanvas(North);
		NorthSlot->SetAnchors(FAnchors(0.5f, 0));
		NorthSlot->SetAlignment(FVector2D(0.5f, 0));
		NorthSlot->SetSize(FVector2D(32, 24));
	}
}

void USAPlayerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!StatusComponent)
	{
		if (ASABaseCharacter* Player = Cast<ASABaseCharacter>(GetOwningPlayerPawn()))
			SetStatusComponent(Player->GetStatusComponent());
	}
	RefreshStatus();
}

void USAPlayerWidget::SetStatusComponent(USAStatusComponent* InStatusComponent)
{
	StatusComponent = InStatusComponent;
	RefreshStatus();
}

void USAPlayerWidget::RefreshStatus()
{
	const USAStatusComponent* Status = IsValid(StatusComponent) ? StatusComponent.Get() : nullptr;
	float Attack = Status ? Status->DefaultDamage : 0.0f;
	if (Status)
	{
		if (const ASABaseCharacter* Character = Cast<ASABaseCharacter>(Status->GetOwner()))
		{
			if (const ASAWeaponActor* Weapon = Character->GetWeapon(); IsValid(Weapon)) Attack += Weapon->GetDamage();
		}
	}
	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetMaximumFractionalDigits(1);
	NumberFormat.SetUseGrouping(false);
	SetTextIfChanged(AttackText, FText::AsNumber(Attack, &NumberFormat));
	SetTextIfChanged(DefenseText, FText::AsNumber(Status ? Status->Defense : 0.0f, &NumberFormat));
	const float HP = Status ? FMath::Max(0.0f, Status->HP) : 0.0f;
	const float MaxHP = Status ? FMath::Max(0.0f, Status->MaxHP) : 0.0f;
	SetTextIfChanged(HealthText, FText::Format(NSLOCTEXT("SAHUD", "Health", "{0} / {1}"),
		FText::AsNumber(HP, &NumberFormat), FText::AsNumber(MaxHP, &NumberFormat)));
	if (HealthBar) HealthBar->SetPercent(MaxHP > 0.0f ? FMath::Clamp(HP / MaxHP, 0.0f, 1.0f) : 0.0f);
	SetTextIfChanged(AmmoText, FText::Format(NSLOCTEXT("SAHUD", "Ammo", "{0} / {1}"),
		FText::AsNumber(Status ? FMath::Max(0, Status->Ammo) : 0),
		FText::AsNumber(Status ? FMath::Max(0, Status->MaxAmmo) : 0)));
}

void USAPlayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Status fields are also edited directly by weapons, networking and Blueprints.
	RefreshStatus();
}
