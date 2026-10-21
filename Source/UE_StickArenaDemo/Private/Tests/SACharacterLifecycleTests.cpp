#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Characters/SAPlayerCharacter.h"
#include "Characters/SAEnemyCharacter.h"
#include "AnimInstances/SABaseAnimInstance.h"
#include "Components/SAStatusComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "InputActionValue.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "GameFramework/SpringArmComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSACharacterLifecycleTest, "StickArena.Character.SpawnAndRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSACharacterLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	UClass* Class = LoadClass<ASAPlayerCharacter>(nullptr, TEXT("/Game/PlayerCharacters/TypeA/BP_TypeA.BP_TypeA_C"));
	ASAPlayerCharacter* Player = World->SpawnActor<ASAPlayerCharacter>(Class);
	World->SpawnActor<APlayerController>()->Possess(Player);
	ASABaseCharacter* Attacker = World->SpawnActor<ASABaseCharacter>();
	Attacker->SetCharacterState(ECharacterState::Idle);
	TestEqual(TEXT("Play starts in Spawning"), Player->GetCharacterState(), ECharacterState::Spawning);
	UMaterialInterface* Original = Class->GetDefaultObject<ASAPlayerCharacter>()->GetMesh()->GetMaterial(0);
	UMaterialInstanceDynamic* Effect = Cast<UMaterialInstanceDynamic>(Player->GetMesh()->GetMaterial(0));
	TestNotNull(TEXT("Spawn uses dynamic dissolve material"), Effect);
	TestEqual(TEXT("Spawn begins fully dissolved"), Player->GetLifecycleVisibility(), 0.0f);
	Player->TakeDamage(Attacker);
	TestEqual(TEXT("Spawn is immune to incoming damage"), Player->GetStatusComponent()->HP, 100.0f);
	Player->Attack(FInputActionValue(true));
	TestEqual(TEXT("Attack input cannot interrupt spawning"), Player->GetCharacterState(), ECharacterState::Spawning);
	TestNull(TEXT("No attack montage during spawning"), Player->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage());
	Player->Movement(FInputActionValue(FVector(1, 0, 0)));
	TestTrue(TEXT("Movement input remains allowed"), !Player->GetPendingMovementInputVector().IsNearlyZero());
	Player->Tick(1.0f);
	TestTrue(TEXT("Dissolve visibility advances halfway"), FMath::IsNearlyEqual(Player->GetLifecycleVisibility(), 0.5f));
	Player->Tick(1.0f);
	TestEqual(TEXT("Completed spawn enters Idle"), Player->GetCharacterState(), ECharacterState::Idle);
	TestTrue(TEXT("Original character material restored"), Player->GetMesh()->GetMaterial(0) == Original);
	Player->GetStatusComponent()->HP = 0;
	Player->SetCharacterState(ECharacterState::Dead, 20.0f);
	Player->Tick(0.1f);
	TestEqual(TEXT("Death animation plays before dissolve"), Player->GetLifecycleVisibility(), 1.0f);
	Player->RestoreFromDamaged();
	TestEqual(TEXT("Recovery notifies cannot cancel death"), Player->GetCharacterState(), ECharacterState::Dead);
	// Exercise the montage-end callback; the live test verifies natural animation completion.
	UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance();
	TestNotNull(TEXT("Death montage started"), Anim->GetCurrentActiveMontage());
	Player->HandleDeathMontageEnded(Anim->GetCurrentActiveMontage(), false);
	Player->Tick(0.75f);
	TestTrue(TEXT("Montage end starts reverse dissolve"), FMath::IsNearlyEqual(Player->GetLifecycleVisibility(), 0.5f, 0.01f));
	Player->Tick(0.75f);
	TestFalse(TEXT("Fully dissolved corpse is hidden"), Player->GetMesh()->IsVisible());
	TestEqual(TEXT("Corpse collision disabled after dissolve"), Player->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	{
		// TimerManager suppresses duplicate ticks within a single engine frame.
		TGuardValue<uint64> SimulatedFrame(GFrameCounter, GFrameCounter + 1);
		World->GetTimerManager().Tick(0.0f);
		++GFrameCounter;
		World->GetTimerManager().Tick(4.9f);
		TestEqual(TEXT("No respawn before five seconds"), Player->GetCharacterState(), ECharacterState::Dead);
		++GFrameCounter;
		World->GetTimerManager().Tick(0.2f);
	}
	TestEqual(TEXT("Respawn begins after five seconds"), Player->GetCharacterState(), ECharacterState::Spawning);
	TestEqual(TEXT("Respawn restores maximum health"), Player->GetStatusComponent()->HP, Player->GetStatusComponent()->MaxHP);
	TestTrue(TEXT("Respawn makes mesh visible again"), Player->GetMesh()->IsVisible());
	TestTrue(TEXT("Respawn restores collision"), Player->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
	Player->Tick(2.0f);
	TestEqual(TEXT("Respawn appearance completes in Idle"), Player->GetCharacterState(), ECharacterState::Idle);
	ASAEnemyCharacter* PlacedEnemy = World->SpawnActor<ASAEnemyCharacter>();
	TestTrue(TEXT("Placed test enemies can also respawn"), PlacedEnemy->CanRespawnLocally());
	FSAPlayerRegistration Registration;
	Registration.PlayerId = TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	PlacedEnemy->InitializeRemotePlayer(Registration);
	FSAPlayerNetworkState Snapshot;
	Snapshot.PlayerId = Registration.PlayerId;
	Snapshot.Sequence = 1;
	PlacedEnemy->ApplyNetworkState(Snapshot);
	TestFalse(TEXT("Network proxies wait for their owner's respawn"), PlacedEnemy->CanRespawnLocally());
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSACharacterLifecycleLiveTest, "StickArena.Character.LifecycleLive",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSACharacterLifecycleLiveTest::RunTest(const FString& Parameters)
{
	ASAPlayerCharacter* Player = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetFirstPlayerController())
			Player = Cast<ASAPlayerCharacter>(Context.World()->GetFirstPlayerController()->GetPawn());
	if (!TestNotNull(TEXT("Live player"), Player)) return false;
	const TWeakObjectPtr<ASAPlayerCharacter> WeakPlayer(Player);
	if (USpringArmComponent* Boom = Player->FindComponentByClass<USpringArmComponent>()) Boom->bDoCollisionTest = false;
	Player->BeginSpawning();
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakPlayer]
	{
		if (!WeakPlayer.IsValid()) { AddError(TEXT("Player disappeared")); return true; }
		TestEqual(TEXT("Live spawn state"), WeakPlayer->GetCharacterState(), ECharacterState::Spawning);
		UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(WeakPlayer->GetMesh()->GetMaterial(0));
		TestNotNull(TEXT("Live spawn material remains assigned"), Dynamic);
		if (Dynamic)
		{
			float Value = -1;
			Dynamic->GetScalarParameterValue(FMaterialParameterInfo(TEXT("DissolveVisibility")), Value);
			TestTrue(TEXT("Live shader receives visibility"), FMath::IsNearlyEqual(Value, WeakPlayer->GetLifecycleVisibility()));
			AddInfo(FString::Printf(TEXT("LIVE_MATERIAL %s visibility=%f parent=%s"), *Dynamic->GetPathName(), Value, *GetPathNameSafe(Dynamic->Parent)));
		}
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/Character_Spawning.png"), true, false);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakPlayer]
	{
		if (!WeakPlayer.IsValid()) return true;
		TestEqual(TEXT("Live spawn finishes"), WeakPlayer->GetCharacterState(), ECharacterState::Idle);
		WeakPlayer->GetStatusComponent()->HP = 0;
		WeakPlayer->SetCharacterState(ECharacterState::Dead, 20.0f);
		return true;
	}));
	// Wait for the actual montage end, with a bounded timeout, then sample its reverse effect.
	const TSharedRef<double> DeathWaitStarted = MakeShared<double>(0.0);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakPlayer, DeathWaitStarted]
	{
		if (!WeakPlayer.IsValid()) return true;
		if (*DeathWaitStarted == 0.0) *DeathWaitStarted = FPlatformTime::Seconds();
		if (WeakPlayer->GetLifecycleVisibility() < 0.8f)
		{
			TestEqual(TEXT("Dead state survives animation notifies"), WeakPlayer->GetCharacterState(), ECharacterState::Dead);
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/Character_DeathDissolve.png"), true, false);
			return true;
		}
		if (FPlatformTime::Seconds() - *DeathWaitStarted > 12.0) { AddError(TEXT("Natural death montage did not start dissolve")); return true; }
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakPlayer]
	{
		if (WeakPlayer.IsValid()) TestFalse(TEXT("Dead mesh disappears after dissolve"), WeakPlayer->GetMesh()->IsVisible());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakPlayer]
	{
		if (!WeakPlayer.IsValid()) return true;
		TestEqual(TEXT("Automatic respawn re-enters Spawning"), WeakPlayer->GetCharacterState(), ECharacterState::Spawning);
		TestEqual(TEXT("Automatic respawn restores HP"), WeakPlayer->GetStatusComponent()->HP, WeakPlayer->GetStatusComponent()->MaxHP);
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/Character_Respawning.png"), true, false);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakPlayer]
	{
		if (WeakPlayer.IsValid()) TestEqual(TEXT("Automatic respawn returns to Idle"), WeakPlayer->GetCharacterState(), ECharacterState::Idle);
		return true;
	}));
	return true;
}
#endif
