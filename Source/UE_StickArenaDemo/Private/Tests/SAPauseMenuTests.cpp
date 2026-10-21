#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Characters/SAPlayerCharacter.h"
#include "Widget/SAPauseMenuWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "GamePlay/SAUdpClientGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	ASAPlayerCharacter* MenuTestPlayer()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetFirstPlayerController())
				return Cast<ASAPlayerCharacter>(Context.World()->GetFirstPlayerController()->GetPawn());
		return nullptr;
	}
	USAPauseMenuWidget* FindMenu(UWorld* World)
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, USAPauseMenuWidget::StaticClass(), true);
		return Widgets.IsEmpty() ? nullptr : Cast<USAPauseMenuWidget>(Widgets[0]);
	}
	void PressEscape()
	{
		FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
		FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAPauseMenuLiveTest, "StickArena.Menu.ControlsLive",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSAPauseMenuLiveTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Standalone player"), MenuTestPlayer())) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]
	{
		PressEscape();
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]
	{
		ASAPlayerCharacter* Player = MenuTestPlayer();
		if (!Player) { AddError(TEXT("Player disappeared")); return true; }
		TestTrue(TEXT("Esc input opens menu"), Player->IsPauseMenuOpen());
		TestFalse(TEXT("Multiplayer world stays unpaused"), Player->GetWorld()->IsPaused());
		Player->Movement(FInputActionValue(FVector(1, 0, 0)));
		TestTrue(TEXT("Menu consumes movement"), Player->GetPendingMovementInputVector().IsNearlyZero());
		Player->Attack(FInputActionValue(true));
		TestEqual(TEXT("Menu blocks attack"), Player->GetCharacterState(), ECharacterState::Idle);
		USAUdpClientGameInstance* Connection = Player->GetGameInstance<USAUdpClientGameInstance>();
		TestTrue(TEXT("Connection survives open menu"), Connection && Connection->IsRegistered());
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/PauseMenu.png"), true, false);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]
	{
		ASAPlayerCharacter* Player = MenuTestPlayer();
		if (!Player) return true;
		USAPauseMenuWidget* Menu = FindMenu(Player->GetWorld());
		if (TestNotNull(TEXT("Menu widget"), Menu))
		{
			UButton* Continue = Cast<UButton>(Menu->GetWidgetFromName(TEXT("ContinueButton")));
			if (TestNotNull(TEXT("Continue button"), Continue)) Continue->OnClicked.Broadcast();
		}
		TestFalse(TEXT("Continue closes menu"), Player->IsPauseMenuOpen());
		Player->Movement(FInputActionValue(FVector(1, 0, 0)));
		TestFalse(TEXT("Continue restores movement"), Player->ConsumeMovementInputVector().IsNearlyZero());
		Player->StopMovementInput(FInputActionValue());
		Player->TogglePauseMenu();
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]
	{
		PressEscape();
		if (ASAPlayerCharacter* Player = MenuTestPlayer())
		{
			TestFalse(TEXT("Esc from focused button closes menu once"), Player->IsPauseMenuOpen());
			TestNull(TEXT("Closed menu removed from viewport"), FindMenu(Player->GetWorld()));
		}
		return true;
	}));
	return true;
}

// Opt-in: this test intentionally exits its standalone process through the actual button.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAPauseMenuQuitTest, "StickArena.Menu.QuitLive",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSAPauseMenuQuitTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("SAMenuQuitTest")))
	{
		AddInfo(TEXT("Skipped process-exit test; requires -SAMenuQuitTest."));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]
	{
		ASAPlayerCharacter* Player = MenuTestPlayer();
		if (!TestNotNull(TEXT("Quit test player"), Player)) return true;
		USAUdpClientGameInstance* Connection = Player->GetGameInstance<USAUdpClientGameInstance>();
		if (!TestTrue(TEXT("Registered before quit"), Connection && Connection->IsRegistered())) return true;
		Player->TogglePauseMenu();
		USAPauseMenuWidget* Menu = FindMenu(Player->GetWorld());
		if (!TestNotNull(TEXT("Quit menu"), Menu)) return true;
		UButton* Quit = Cast<UButton>(Menu->GetWidgetFromName(TEXT("QuitButton")));
		if (TestNotNull(TEXT("Quit button"), Quit)) Quit->OnClicked.Broadcast();
		TestFalse(TEXT("Quit disconnects before engine exit"), Connection->IsConnected());
		return true;
	}));
	return true;
}
#endif
