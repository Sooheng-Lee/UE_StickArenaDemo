#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/SAPlayerCharacter.h"
#include "Components/SAUdpClientComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Widget/SAPlayerWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSAMatchTimerTest, "StickArena.Match.Timer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSAMatchTimerTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ASAPlayerCharacter* Player = World->SpawnActor<ASAPlayerCharacter>();
	USAUdpClientComponent* Client = Player->FindComponentByClass<USAUdpClientComponent>();
	const auto Packet = [](const TCHAR* Epoch, int32 Seq, const TCHAR* Remaining)
	{
		return FString::Printf(TEXT("SA6|type=MATCH_STATE|epoch=%s|seq=%d|remaining_ms=%s"), Epoch, Seq, Remaining);
	};
	TestFalse(TEXT("No local match starts before receiving server time"), Client->HasMatchTime());
	Client->ReceiveMatchState(Packet(TEXT("111"), 1, TEXT("180000")));
	TestTrue(TEXT("Server starts three minute match"), Client->GetMatchRemainingSeconds() > 179.0f);
	Client->ReceiveMatchState(Packet(TEXT("111"), 2, TEXT("61000")));
	Client->ReceiveMatchState(Packet(TEXT("111"), 1, TEXT("180000")));
	TestTrue(TEXT("Old packet cannot extend timer"), Client->GetMatchRemainingSeconds() <= 61.0f);
	Client->ReceiveMatchState(Packet(TEXT("111"), 3, TEXT("NaN")));
	TestEqual(TEXT("Malformed time ignored"), Client->MatchSequence, int64(2));
	Client->ReceiveMatchState(Packet(TEXT("111"), 3, TEXT("180001")));
	TestEqual(TEXT("Out of range time ignored"), Client->MatchSequence, int64(2));

	USAPlayerWidget* Widget = NewObject<USAPlayerWidget>();
	Widget->Initialize();
	Widget->SetMatchClient(Client);
	TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	Widget->RefreshStatus();
	UTextBlock* Timer = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("MatchTimeText")));
	if (TestNotNull(TEXT("HUD timer exists"), Timer))
		TestEqual(TEXT("Time formatted as minutes and seconds"), Timer->GetText().ToString(), FString(TEXT("01:01")));

	Player->GetCharacterMovement()->Velocity = FVector(400, 0, 0);
	Player->AddMovementInput(FVector::ForwardVector, 1.0f);
	Client->ReceiveMatchState(Packet(TEXT("111"), 3, TEXT("0")));
	TestTrue(TEXT("Server zero ends match"), Client->IsMatchExpired());
	TestTrue(TEXT("Expiry immediately removes velocity"), Player->GetVelocity().IsNearlyZero());
	Player->Movement(FInputActionValue(FVector(1, 0, 0)));
	TestTrue(TEXT("Movement input rejected at zero"), Player->GetPendingMovementInputVector().IsNearlyZero());
	Widget->RefreshStatus();
	if (Timer) TestEqual(TEXT("Expired HUD stays at zero"), Timer->GetText().ToString(), FString(TEXT("00:00")));
	Client->ReceiveMatchState(Packet(TEXT("111"), 4, TEXT("1000")));
	TestTrue(TEXT("Same match cannot reopen"), Client->IsMatchExpired());
	Client->ReceiveMatchState(Packet(TEXT("222"), 1, TEXT("1000")));
	TestFalse(TEXT("Server restart starts new match"), Client->IsMatchExpired());
	Client->ReceiveMatchState(Packet(TEXT("111"), 99, TEXT("0")));
	TestFalse(TEXT("Retired server epoch ignored"), Client->IsMatchExpired());
	Client->MatchDeadline -= 2.0;
	TestTrue(TEXT("Time continues to expire if UDP updates are lost"), Client->IsMatchExpired());
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
