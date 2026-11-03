#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BH_Subsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

// These tests need a running game world: run them with -game, e.g.
// UE4Editor.exe <Project>.uproject -game -ExecCmds="Automation RunTests BetaHub;Quit"

namespace
{
    constexpr uint32 GameTestFlags =EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

    struct FBH_GameFixture
    {
        APlayerController* PlayerController = nullptr;
        UGameViewportClient* ViewportClient = nullptr;
        UBH_Subsystem* Subsystem = nullptr;
    };

    bool GetGameFixture(FAutomationTestBase& Test, FBH_GameFixture& OutFixture)
    {
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Game)
            {
                World = Context.World();
                break;
            }
        }
        if (!Test.TestNotNull(TEXT("Game world (run with -game)"), World))
        {
            return false;
        }

        UGameInstance* GameInstance = World->GetGameInstance();
        OutFixture.PlayerController = World->GetFirstPlayerController();
        OutFixture.ViewportClient = World->GetGameViewport();
        OutFixture.Subsystem = GameInstance ? GameInstance->GetSubsystem<UBH_Subsystem>() : nullptr;

        return Test.TestNotNull(TEXT("Player controller"), OutFixture.PlayerController)
            && Test.TestNotNull(TEXT("Game viewport"), OutFixture.ViewportClient)
            && Test.TestNotNull(TEXT("BetaHub subsystem"), OutFixture.Subsystem);
    }

    void EnterPauseMenuState(APlayerController* PlayerController)
    {
        PlayerController->bShowMouseCursor = true;
        PlayerController->SetInputMode(FInputModeUIOnly());
    }

    void EnterGameplayState(APlayerController* PlayerController)
    {
        PlayerController->bShowMouseCursor = false;
        PlayerController->SetInputMode(FInputModeGameOnly());
    }

    void TestIsPauseMenuState(FAutomationTestBase& Test, const FBH_GameFixture& Fixture)
    {
        Test.TestTrue(TEXT("Cursor visible"), Fixture.PlayerController->bShowMouseCursor);
        Test.TestTrue(TEXT("UI-only input (viewport ignores game input)"), Fixture.ViewportClient->IgnoreInput());
        Test.TestTrue(TEXT("Mouse not captured"), Fixture.ViewportClient->GetMouseCaptureMode() == EMouseCaptureMode::NoCapture);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_CloseRestoresPauseMenuInputTest, "BetaHub.ReportForm.CloseRestoresPauseMenuInput", GameTestFlags)

bool FBH_CloseRestoresPauseMenuInputTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    EnterPauseMenuState(Fixture.PlayerController);
    Fixture.Subsystem->ShowReportForm();
    Fixture.Subsystem->HideReportForm();

    TestIsPauseMenuState(*this, Fixture);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_CloseRestoresGameplayInputTest, "BetaHub.ReportForm.CloseRestoresGameplayInput", GameTestFlags)

bool FBH_CloseRestoresGameplayInputTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    EnterGameplayState(Fixture.PlayerController);
    Fixture.Subsystem->ShowReportForm();
    Fixture.Subsystem->HideReportForm();

    TestFalse(TEXT("Cursor hidden"), Fixture.PlayerController->bShowMouseCursor);
    TestFalse(TEXT("Game receives input"), Fixture.ViewportClient->IgnoreInput());
    TestTrue(TEXT("Mouse captured permanently"), Fixture.ViewportClient->GetMouseCaptureMode() == EMouseCaptureMode::CapturePermanently);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_ReopenKeepsOriginalInputTest, "BetaHub.ReportForm.ReopenWhileOpenKeepsOriginalInput", GameTestFlags)

bool FBH_ReopenKeepsOriginalInputTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    EnterPauseMenuState(Fixture.PlayerController);
    Fixture.Subsystem->ShowReportForm();
    Fixture.Subsystem->ShowReportForm();
    Fixture.Subsystem->HideReportForm();

    TestIsPauseMenuState(*this, Fixture);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_CloseBroadcastsEventTest, "BetaHub.ReportForm.CloseBroadcastsOnReportFormClosed", GameTestFlags)

bool FBH_CloseBroadcastsEventTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    // Dynamic delegates need a UFUNCTION target. Binding a throwaway actor's K2_DestroyActor
    // makes the broadcast observable without adding a test-only UCLASS to the plugin.
    AActor* Listener = Fixture.PlayerController->GetWorld()->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Listener actor"), Listener))
    {
        return false;
    }
    FScriptDelegate DestroyListener;
    DestroyListener.BindUFunction(Listener, GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor));
    Fixture.Subsystem->OnReportFormClosed.Add(DestroyListener);

    Fixture.Subsystem->ShowReportForm();
    Fixture.Subsystem->HideReportForm();

    TestTrue(TEXT("OnReportFormClosed fired"), Listener->IsPendingKill());

    Fixture.Subsystem->OnReportFormClosed.Remove(DestroyListener);
    if (!Listener->IsPendingKill())
    {
        Listener->Destroy();
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
