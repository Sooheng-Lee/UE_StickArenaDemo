#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/SABaseCharacter.h"
#include "Characters/SAEnemyCharacter.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Characters/SAPlayerCharacter.h"
#include "Components/SAStatusComponent.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Items/SAWeaponActor.h"
#include "UObject/UnrealType.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Widget/SAPlayerWidget.h"

namespace
{
	struct FSAStatusTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FSAStatusTestWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		~FSAStatusTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSADefenseTest, "StickArena.Status.Defense",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSADefenseTest::RunTest(const FString& Parameters)
{
	FSAStatusTestWorld TestWorld;
	ASABaseCharacter* Attacker = TestWorld.World->SpawnActor<ASABaseCharacter>();
	ASABaseCharacter* Defender = TestWorld.World->SpawnActor<ASABaseCharacter>();
	USAStatusComponent* Status = Defender->GetStatusComponent();
	TestEqual(TEXT("Default defense is two"), Status->Defense, 2.0f);
	Defender->TakeDamage(Attacker);
	TestEqual(TEXT("Default attack minus defense"), Status->HP, 97.0f);
	Status->Defense = 1000.0f;
	Defender->TakeDamage(Attacker);
	TestEqual(TEXT("High defense still takes one damage"), Status->HP, 96.0f);
	Status->HP = 0.5f;
	Defender->TakeDamage(Attacker);
	TestEqual(TEXT("Health cannot become negative"), Status->HP, 0.0f);
	Status->HP = 100.0f;
	Defender->SetCharacterState(ECharacterState::Idle); // Reset the preceding lethal-hit scenario.
	Status->Defense = 2.0f;
	ASAWeaponActor* Weapon = TestWorld.World->SpawnActor<ASAWeaponActor>();
	FindFProperty<FFloatProperty>(ASAWeaponActor::StaticClass(), TEXT("Damage"))->SetPropertyValue_InContainer(Weapon, 10.0f);
	Attacker->SetWeapon(Weapon);
	Defender->TakeDamage(Attacker);
	TestEqual(TEXT("Defense is subtracted once after adding weapon damage"), Status->HP, 87.0f);
	Defender->TakeDamage(nullptr);
	Defender->TakeDamage(Defender);
	TestEqual(TEXT("Missing attacker and self do not subtract health"), Status->HP, 87.0f);
	Defender->SetCharacterState(ECharacterState::Dead);
	Defender->TakeDamage(Attacker);
	TestEqual(TEXT("Dead character ignores damage"), Status->HP, 87.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAPlayerWidgetTest, "StickArena.Status.PlayerWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSAPlayerWidgetTest::RunTest(const FString& Parameters)
{
	FSAStatusTestWorld TestWorld;
	ASABaseCharacter* Player = TestWorld.World->SpawnActor<ASABaseCharacter>();
	USAStatusComponent* Status = Player->GetStatusComponent();
	USAPlayerWidget* Widget = NewObject<USAPlayerWidget>();
	Widget->Initialize();
	Widget->SetStatusComponent(Status);
	TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	Widget->RefreshStatus();
	const auto Text = [Widget](FName Name) { return Cast<UTextBlock>(Widget->WidgetTree->FindWidget(Name)); };
	UTextBlock* Attack = Text(TEXT("AttackText"));
	UTextBlock* Defense = Text(TEXT("DefenseText"));
	UTextBlock* Health = Text(TEXT("HealthText"));
	UTextBlock* Ammo = Text(TEXT("AmmoText"));
	UProgressBar* Bar = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("HealthBar")));
	if (!TestNotNull(TEXT("Attack label constructed"), Attack) || !TestNotNull(TEXT("Defense label constructed"), Defense)
		|| !TestNotNull(TEXT("Health label constructed"), Health) || !TestNotNull(TEXT("Ammo label constructed"), Ammo)
		|| !TestNotNull(TEXT("Health bar constructed"), Bar)) return false;
	TestEqual(TEXT("Default attack shown"), Attack->GetText().ToString(), FString(TEXT("5")));
	TestEqual(TEXT("Default defense shown"), Defense->GetText().ToString(), FString(TEXT("2")));
	Status->HP = 85;
	Status->MaxHP = 200;
	Status->Ammo = 36;
	Status->MaxAmmo = 120;
	Status->Defense = 74;
	ASAWeaponActor* Weapon = TestWorld.World->SpawnActor<ASAWeaponActor>();
	FindFProperty<FFloatProperty>(ASAWeaponActor::StaticClass(), TEXT("Damage"))->SetPropertyValue_InContainer(Weapon, 10.0f);
	Player->SetWeapon(Weapon);
	Widget->RefreshStatus();
	TestEqual(TEXT("Attack includes equipped weapon"), Attack->GetText().ToString(), FString(TEXT("15")));
	TestEqual(TEXT("Defense changes reach HUD"), Defense->GetText().ToString(), FString(TEXT("74")));
	TestEqual(TEXT("Current and maximum health shown"), Health->GetText().ToString(), FString(TEXT("85 / 200")));
	TestEqual(TEXT("Current and maximum ammo shown"), Ammo->GetText().ToString(), FString(TEXT("36 / 120")));
	TestEqual(TEXT("Health bar ratio"), Bar->GetPercent(), 0.425f);
	Status->MaxHP = 0;
	Widget->RefreshStatus();
	TestEqual(TEXT("Zero maximum health is safe"), Bar->GetPercent(), 0.0f);
	Widget->SetStatusComponent(nullptr);
	TestEqual(TEXT("Unbinding status clears health"), Health->GetText().ToString(), FString(TEXT("0 / 0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAMinimapTest, "StickArena.Status.Minimap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSAMinimapTest::RunTest(const FString& Parameters)
{
	FSAStatusTestWorld TestWorld;
	TestWorld.World->InitializeActorsForPlay(FURL());
	TestWorld.World->BeginPlay();
	TestWorld.World->GetWorldSettings()->NotifyBeginPlay();
	APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	LocalPlayer->PlayerController = Controller;
	Controller->Player = LocalPlayer;
	ASABaseCharacter* Player = TestWorld.World->SpawnActor<ASABaseCharacter>();
	Controller->Possess(Player);
	ASAEnemyCharacter* Enemy = TestWorld.World->SpawnActor<ASAEnemyCharacter>();
	Enemy->SetActorLocation(FVector(1200, 2400, 0));
	USAPlayerWidget* Widget = NewObject<USAPlayerWidget>(TestWorld.World);
	Widget->Initialize();
	Widget->SetPlayerContext(FLocalPlayerContext(LocalPlayer, TestWorld.World));
	TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestEqual(TEXT("Minimap owner pawn"), Widget->GetOwningPlayerPawn(), static_cast<APawn*>(Player))) return false;
	if (!TestEqual(TEXT("Minimap world"), Widget->GetWorld(), TestWorld.World)) return false;
	Widget->RefreshMinimap();
	UCanvasPanel* Markers = Cast<UCanvasPanel>(Widget->GetWidgetFromName(TEXT("MinimapMarkers")));
	UImage* Arrow = Cast<UImage>(Widget->GetWidgetFromName(TEXT("MinimapPlayerArrow")));
	if (!TestNotNull(TEXT("Minimap marker canvas"), Markers) || !TestNotNull(TEXT("Player arrow"), Arrow)) return false;
	TestNotNull(TEXT("Generated arrow texture is assigned"), Arrow->GetBrush().GetResourceObject());
	if (!TestEqual(TEXT("One player and one enemy marker"), Markers->GetChildrenCount(), 2)) return false;
	UWidget* Dot = Markers->GetChildAt(1);
	UCanvasPanelSlot* DotSlot = CastChecked<UCanvasPanelSlot>(Dot->Slot);
	TestTrue(TEXT("World +Y projects east and +X projects north"), DotSlot->GetAnchors().Minimum.Equals(FVector2D(0.7, 0.4), 0.001));
	Player->SetActorLocation(FVector(-1200, -2400, 0));
	Player->SetActorRotation(FRotator(0, 90, 0));
	Widget->RefreshMinimap();
	TestTrue(TEXT("Player marker follows world position"), CastChecked<UCanvasPanelSlot>(Arrow->Slot)->GetAnchors().Minimum.Equals(FVector2D(0.3, 0.6), 0.001));
	TestEqual(TEXT("Arrow follows facing direction"), Arrow->GetRenderTransform().Angle, 90.0f);
	Enemy->SetActorLocation(FVector(100000, -100000, 0));
	Widget->RefreshMinimap();
	TestTrue(TEXT("Off-map markers stay inside the border"), DotSlot->GetAnchors().Minimum.Equals(FVector2D(0.09, 0.09), 0.001));
	Enemy->SetCharacterState(ECharacterState::Dead);
	Widget->RefreshMinimap();
	TestEqual(TEXT("Dead enemies are hidden"), Dot->GetVisibility(), ESlateVisibility::Hidden);
	Enemy->Destroy();
	Widget->RefreshMinimap();
	TestEqual(TEXT("Disconnected enemy marker removed"), Markers->GetChildrenCount(), 1);
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FSAHUDLiveCheck, FAutomationTestBase*, Test, TWeakObjectPtr<USAPlayerWidget>, Widget);

bool FSAHUDLiveCheck::Update()
{
	if (!Test->TestTrue(TEXT("Local HUD remains valid"), Widget.IsValid())) return true;
	UTextBlock* Health = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("HealthText")));
	if (Test->TestNotNull(TEXT("Live health label"), Health))
		Test->TestEqual(TEXT("Native tick observes direct status changes"), Health->GetText().ToString(), FString(TEXT("85 / 100")));
	Test->TestNull(TEXT("Minimap layout does not create a scene capture"), Widget->GetOwningPlayerPawn()->FindComponentByClass<USceneCaptureComponent2D>());
	UImage* Map = Cast<UImage>(Widget->GetWidgetFromName(TEXT("MinimapImage")));
	if (Test->TestNotNull(TEXT("Minimap background exists"), Map))
		Test->TestNotNull(TEXT("Baked level texture is assigned"), Map->GetBrush().GetResourceObject());
	UCanvasPanel* Markers = Cast<UCanvasPanel>(Widget->GetWidgetFromName(TEXT("MinimapMarkers")));
	if (Test->TestNotNull(TEXT("Live minimap markers"), Markers))
		Test->TestTrue(TEXT("Both test enemies appear on the minimap"), Markers->GetChildrenCount() >= 3);
	for (const FName Name : {FName(TEXT("AttackIcon")), FName(TEXT("DefenseIcon")), FName(TEXT("AmmoIcon")), FName(TEXT("MinimapPlayerArrow"))})
	{
		UImage* Icon = Cast<UImage>(Widget->GetWidgetFromName(Name));
		if (Test->TestNotNull(TEXT("HUD icon is an image"), Icon))
			Test->TestNotNull(TEXT("Generated HUD icon texture assigned"), Icon->GetBrush().GetResourceObject());
	}
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/SAPlayerHUD_Minimap.png"), true, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAPlayerHUDLiveTest, "StickArena.Status.LocalPlayerHUD",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSAPlayerHUDLiveTest::RunTest(const FString& Parameters)
{
	ASAPlayerCharacter* Player = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetFirstPlayerController())
			Player = Cast<ASAPlayerCharacter>(Context.World()->GetFirstPlayerController()->GetPawn());
	}
	if (!TestNotNull(TEXT("Game spawned local player"), Player)) return false;
	USAPlayerWidget* Widget = Player->GetPlayerWidget();
	if (!TestNotNull(TEXT("HUD created automatically"), Widget)) return false;
	TestTrue(TEXT("HUD added to local player screen"), Widget->IsInViewport());
	Player->GetStatusComponent()->HP = 85;
	Player->GetStatusComponent()->Ammo = 36;
	Player->GetStatusComponent()->MaxAmmo = 120;
	for (const FVector Offset : {FVector(900, 600, 0), FVector(-900, -1200, 0)})
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player->GetWorld()->SpawnActor<ASAEnemyCharacter>(Player->GetActorLocation() + Offset, FRotator::ZeroRotator, SpawnParameters);
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSAHUDLiveCheck(this, Widget));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	return true;
}

#endif
