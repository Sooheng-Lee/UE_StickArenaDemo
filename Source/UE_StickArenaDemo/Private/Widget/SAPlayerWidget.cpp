#include "Widget/SAPlayerWidget.h"
#include "Widget/SAMinimapData.h"

#include "Blueprint/WidgetTree.h"
#include "Characters/SABaseCharacter.h"
#include "Characters/SAEnemyCharacter.h"
#include "Components/SAStatusComponent.h"
#include "Components/SAUdpClientComponent.h"
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
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Items/SAWeaponActor.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor Gold(0.72f, 0.49f, 0.19f);
	const FLinearColor PanelColor(0.035f, 0.022f, 0.014f, 0.90f);
	constexpr float PanelCornerRadius = 10.0f;
	constexpr float PanelBorderThickness = 2.0f;

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
	static ConstructorHelpers::FObjectFinder<UTexture2D> Arrow(TEXT("/Game/UI/Icons/T_UI_PlayerArrow.T_UI_PlayerArrow"));
	PlayerArrowTexture = Arrow.Object;
	static ConstructorHelpers::FObjectFinder<USAMinimapData> MapData(TEXT("/Game/UI/Minimap/DA_Minimap_Main.DA_Minimap_Main"));
	MinimapData = MapData.Object;
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
		Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, PanelCornerRadius));
		Frame->SetPadding(FMargin(PanelBorderThickness));
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Frame);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrushColor(PanelColor);
		Background->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, PanelCornerRadius - PanelBorderThickness));
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
	UCanvasPanel* Timer = Panel(TEXT("MatchTimerPanel"), FAnchors(0.5f, 0), FMargin(0, 28, 160, 60), FVector2D(0.5f, 0));
	MatchTimeText = Text(TEXT("MatchTimeText"), TEXT("--:--"), 30, Gold);
	MatchTimeText->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* TimerSlot = Timer->AddChildToCanvas(MatchTimeText);
	TimerSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	TimerSlot->SetOffsets(FMargin(0));
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
		UCanvasPanel* Map = Panel(TEXT("MinimapPanel"), FAnchors(1, 0), FMargin(-28, 28, 280, 288), FVector2D(1, 0));
		MinimapImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MinimapImage"));
		// The background and marker canvas share a square coordinate space.
		MinimapImage->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, PanelCornerRadius - PanelBorderThickness));
		MinimapImage->SetColorAndOpacity(FLinearColor(0.015f, 0.012f, 0.01f, 0.75f));
		UCanvasPanelSlot* MapSlot = Map->AddChildToCanvas(MinimapImage);
		MapSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		MapSlot->SetOffsets(FMargin(24, 32, 24, 32));
		MinimapMarkers = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MinimapMarkers"));
		UCanvasPanelSlot* MarkersSlot = Map->AddChildToCanvas(MinimapMarkers);
		MarkersSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		MarkersSlot->SetOffsets(FMargin(24, 32, 24, 32));
		MinimapMarkers->SetClipping(EWidgetClipping::ClipToBounds);
		MinimapPlayerArrow = Icon(TEXT("MinimapPlayerArrow"), PlayerArrowTexture);
		MinimapPlayerArrow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UCanvasPanelSlot* ArrowSlot = Place(MinimapMarkers, MinimapPlayerArrow, FVector2D::ZeroVector, FVector2D(26, 26));
		ArrowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ArrowSlot->SetZOrder(10);
		const auto Direction = [&Text, Map](FName Name, const TCHAR* Label, FVector2D Anchor)
		{
			UTextBlock* LabelWidget = Text(Name, Label, 18, Gold);
			LabelWidget->SetJustification(ETextJustify::Center);
			UCanvasPanelSlot* LabelSlot = Map->AddChildToCanvas(LabelWidget);
			LabelSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			LabelSlot->SetAlignment(Anchor);
			LabelSlot->SetSize(FVector2D(24, 24));
		};
		Direction(TEXT("NorthLabel"), TEXT("N"), FVector2D(0.5f, 0));
		Direction(TEXT("WestLabel"), TEXT("W"), FVector2D(0, 0.5f));
		Direction(TEXT("SouthLabel"), TEXT("S"), FVector2D(0.5f, 1));
		Direction(TEXT("EastLabel"), TEXT("E"), FVector2D(1, 0.5f));
	}
}

void USAPlayerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!MatchClient && GetOwningPlayerPawn())
		MatchClient = GetOwningPlayerPawn()->FindComponentByClass<USAUdpClientComponent>();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!StatusComponent)
	{
		if (ASABaseCharacter* Player = Cast<ASABaseCharacter>(GetOwningPlayerPawn()))
			SetStatusComponent(Player->GetStatusComponent());
	}
	RefreshStatus();
	InitializeMinimapBounds();
	RefreshMinimap();
}

void USAPlayerWidget::SetStatusComponent(USAStatusComponent* InStatusComponent)
{
	if (StatusComponent != InStatusComponent) bHealthInitialized = false;
	StatusComponent = InStatusComponent;
	RefreshStatus();
}

void USAPlayerWidget::RefreshStatus()
{
	if (IsValid(MatchClient) && MatchClient->HasMatchTime())
	{
		const int32 Seconds = FMath::CeilToInt(MatchClient->GetMatchRemainingSeconds());
		SetTextIfChanged(MatchTimeText, FText::FromString(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60)));
	}
	else SetTextIfChanged(MatchTimeText, FText::FromString(TEXT("--:--")));
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
	TargetHealthPercent = MaxHP > 0.0f ? FMath::Clamp(HP / MaxHP, 0.0f, 1.0f) : 0.0f;
	if (!bHealthInitialized || !Status || MaxHP <= 0.0f)
	{
		DisplayedHealthPercent = TargetHealthPercent;
		bHealthInitialized = true;
	}
	if (HealthBar) HealthBar->SetPercent(DisplayedHealthPercent);
	SetTextIfChanged(AmmoText, FText::Format(NSLOCTEXT("SAHUD", "Ammo", "{0} / {1}"),
		FText::AsNumber(Status ? FMath::Max(0, Status->Ammo) : 0),
		FText::AsNumber(Status ? FMath::Max(0, Status->MaxAmmo) : 0)));
}

void USAPlayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Status fields are also edited directly by weapons, networking and Blueprints.
	RefreshStatus();
	UpdateHealthBar(InDeltaTime);
	MinimapScanRemaining -= InDeltaTime;
	RefreshMinimap();
}

void USAPlayerWidget::UpdateHealthBar(float DeltaSeconds)
{
	const float Alpha = 1.0f - FMath::Exp(-10.0f * FMath::Max(DeltaSeconds, 0.0f));
	DisplayedHealthPercent = FMath::Lerp(DisplayedHealthPercent, TargetHealthPercent, Alpha);
	if (FMath::IsNearlyEqual(DisplayedHealthPercent, TargetHealthPercent, 0.0001f))
		DisplayedHealthPercent = TargetHealthPercent;
	if (HealthBar) HealthBar->SetPercent(DisplayedHealthPercent);
}

void USAPlayerWidget::SetMatchClient(USAUdpClientComponent* InMatchClient)
{
	MatchClient = InMatchClient;
	RefreshStatus();
}

void USAPlayerWidget::InitializeMinimapBounds()
{
	if (!bShowMinimap || !GetWorld()) return;
	FString MapPackage = GetWorld()->GetOutermost()->GetName();
	MapPackage.ReplaceInline(*GetWorld()->StreamingLevelsPrefix, TEXT(""));
	if (MinimapData && MinimapData->Texture && MinimapData->WorldSize > 0.0f
		&& MinimapData->SourceMap.ToSoftObjectPath().GetLongPackageName() == MapPackage)
	{
		MinimapWorldCenter = MinimapData->WorldCenter;
		MinimapWorldSize = MinimapData->WorldSize;
		if (MinimapImage)
		{
			MinimapImage->SetBrushFromTexture(MinimapData->Texture);
			MinimapImage->SetColorAndOpacity(FLinearColor::White);
		}
		return;
	}
	FBox Bounds(ForceInit);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components(*It);
		for (const UStaticMeshComponent* Component : Components)
		{
			if (!Component->GetStaticMesh()) continue;
			const bool bTagged = Component->ComponentHasTag(TEXT("SAPlayerSpawnGround")) || It->ActorHasTag(TEXT("SAPlayerSpawnGround"));
			if (!bTagged && Component->GetStaticMesh()->GetFName() != FName(TEXT("SM_Ground"))) continue;
			bool bRock = false;
			for (const UMaterialInterface* Material : Component->GetMaterials())
				bRock |= Material && Material->GetFName() == FName(TEXT("MI_Arena_Rock"));
			if (bTagged || !bRock) Bounds += Component->Bounds.GetBox();
		}
	}
	if (Bounds.IsValid)
	{
		MinimapWorldCenter = FVector2D(Bounds.GetCenter());
		MinimapWorldSize = FMath::Max(1.0f, static_cast<float>(FMath::Max(Bounds.GetSize().X, Bounds.GetSize().Y)) * 1.12f);
	}
}

void USAPlayerWidget::RefreshMinimap()
{
	if (!bShowMinimap || !MinimapMarkers || !MinimapPlayerArrow || !GetWorld()) return;
	const APawn* Player = GetOwningPlayerPawn();
	MinimapMarkers->SetVisibility(IsValid(Player) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (!IsValid(Player)) return;
	// Discover network proxies periodically; movement remains smooth every frame.
	if (MinimapScanRemaining <= 0.0f)
	{
		MinimapScanRemaining = 0.25f;
		for (TActorIterator<ASAEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (EnemyMinimapMarkers.Contains(*It)) continue;
			UImage* Dot = WidgetTree->ConstructWidget<UImage>();
			Dot->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 4.0f));
			Dot->SetColorAndOpacity(FLinearColor(1.0f, 0.025f, 0.035f));
			UCanvasPanelSlot* DotSlot = MinimapMarkers->AddChildToCanvas(Dot);
			DotSlot->SetSize(FVector2D(8, 8));
			DotSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			EnemyMinimapMarkers.Add(*It, Dot);
		}
	}
	const auto PositionMarker = [this](UImage* Marker, const FVector& Location)
	{
		const float WorldSize = FMath::Max(1.0f, MinimapWorldSize);
		const FVector2D UV(0.5 + (Location.Y - MinimapWorldCenter.Y) / WorldSize,
			0.5 - (Location.X - MinimapWorldCenter.X) / WorldSize);
		// Padding keeps the entire rotating arrow inside the map at its edges.
		const FVector2D Clamped(FMath::Clamp(UV.X, 0.09, 0.91), FMath::Clamp(UV.Y, 0.09, 0.91));
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Marker->Slot))
		{
			Slot->SetAnchors(FAnchors(Clamped.X, Clamped.Y));
			Slot->SetPosition(FVector2D::ZeroVector);
		}
	};
	PositionMarker(MinimapPlayerArrow, Player->GetActorLocation());
	MinimapPlayerArrow->SetRenderTransformAngle(Player->GetActorRotation().Yaw);
	for (auto It = EnemyMinimapMarkers.CreateIterator(); It; ++It)
	{
		const ASAEnemyCharacter* Enemy = It.Key().Get();
		if (!IsValid(Enemy))
		{
			It.Value()->RemoveFromParent();
			It.RemoveCurrent();
			continue;
		}
		const bool bVisible = Enemy->GetCharacterState() != ECharacterState::Dead && !Enemy->IsHidden();
		It.Value()->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (bVisible) PositionMarker(It.Value(), Enemy->GetActorLocation());
	}
}
