#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BH_Subsystem.h"
#include "BH_PluginSettings.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/Children.h"
#include "Widgets/Layout/SBorder.h"

// These tests need a running game world: run them with -game, e.g.
// UE4Editor.exe <Project>.uproject -game -ExecCmds="Automation RunTests BetaHub;Quit"
// Packaged games start like -game: the game instance begins on a placeholder world and loads the map afterwards.

namespace
{
    constexpr uint32 GameTestFlags =EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

    constexpr double MapLoadTimeoutSeconds = 30.0;
    constexpr double InputTimeoutSeconds = 5.0;

    struct FBH_GameFixture
    {
        APlayerController* PlayerController = nullptr;
        UGameViewportClient* ViewportClient = nullptr;
        UBH_Subsystem* Subsystem = nullptr;
    };

    UWorld* FindGameWorld()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Game)
            {
                return Context.World();
            }
        }
        return nullptr;
    }

    bool GetGameFixture(FAutomationTestBase& Test, FBH_GameFixture& OutFixture)
    {
        UWorld* World = FindGameWorld();
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

    // UMG's AddToViewport(ZOrder) adds the widget to the viewport at ZOrder + 10, so a game menu
    // added with the default ZOrder 0 lands here.
    constexpr int32 UmgMenuViewportZOrder = 10;

    const FName ReportFormWidgetType(TEXT("SBH_ReportForm"));

    // Stands in for a game's full-screen menu. Its parent is the viewport overlay, which draws
    // later slots on top of earlier ones.
    TSharedRef<SWidget> AddMenuToViewport(UGameViewportClient& ViewportClient, int32 ZOrder)
    {
        TSharedRef<SWidget> Menu = SNew(SBorder);
        ViewportClient.AddViewportWidgetContent(Menu, ZOrder);
        return Menu;
    }

    int32 FindOverlaySlot(SWidget& Overlay, const TSharedRef<SWidget>& Widget)
    {
        FChildren* Slots = Overlay.GetChildren();
        for (int32 Index = 0; Index < Slots->Num(); ++Index)
        {
            if (Slots->GetChildAt(Index) == Widget)
            {
                return Index;
            }
        }
        return INDEX_NONE;
    }

    // The form is not added to the overlay directly; it sits inside a wrapper widget.
    int32 FindReportFormOverlaySlot(SWidget& Overlay)
    {
        FChildren* Slots = Overlay.GetChildren();
        for (int32 Index = 0; Index < Slots->Num(); ++Index)
        {
            FChildren* WrappedWidgets = Slots->GetChildAt(Index)->GetChildren();
            if (WrappedWidgets->Num() > 0 && WrappedWidgets->GetChildAt(0)->GetType() == ReportFormWidgetType)
            {
                return Index;
            }
        }
        return INDEX_NONE;
    }

    // Opens the form over a menu at MenuZOrder and returns whether the form is drawn above it.
    bool IsReportFormAboveMenu(FAutomationTestBase& Test, const FBH_GameFixture& Fixture, int32 MenuZOrder)
    {
        const TSharedRef<SWidget> Menu = AddMenuToViewport(*Fixture.ViewportClient, MenuZOrder);
        Fixture.Subsystem->ShowReportForm();

        bool bFormAbove = false;
        const TSharedPtr<SWidget> Overlay = Menu->GetParentWidget();
        if (Test.TestTrue(TEXT("Menu added to the viewport overlay"), Overlay.IsValid()))
        {
            const int32 FormSlot = FindReportFormOverlaySlot(*Overlay);
            Test.TestNotEqual(TEXT("Report form found in the viewport overlay"), FormSlot, static_cast<int32>(INDEX_NONE));
            bFormAbove = FormSlot > FindOverlaySlot(*Overlay, Menu);
        }

        Fixture.Subsystem->HideReportForm();
        Fixture.ViewportClient->RemoveViewportWidgetContent(Menu);
        return bFormAbove;
    }

    bool IsReportFormOpen()
    {
        UGameViewportClient* ViewportClient = GEngine->GameViewport;
        if (!ViewportClient)
        {
            return false;
        }

        // A throwaway widget gives access to the viewport overlay.
        const TSharedRef<SWidget> Probe = AddMenuToViewport(*ViewportClient, 0);
        const TSharedPtr<SWidget> Overlay = Probe->GetParentWidget();
        const bool bOpen = Overlay.IsValid() && FindReportFormOverlaySlot(*Overlay) != INDEX_NONE;
        ViewportClient->RemoveViewportWidgetContent(Probe);
        return bOpen;
    }

    APlayerController* GetGamePlayerController()
    {
        UWorld* World = FindGameWorld();
        return World ? World->GetFirstPlayerController() : nullptr;
    }

    int32 CountShortcutBindings(const APlayerController& PlayerController)
    {
        if (!PlayerController.InputComponent)
        {
            return 0;
        }
        const FKey ShortcutKey = GetDefault<UBH_PluginSettings>()->ShortcutKey;
        return PlayerController.InputComponent->KeyBindings.FilterByPredicate([&ShortcutKey](const FInputKeyBinding& Binding)
        {
            return Binding.Chord.Key == ShortcutKey;
        }).Num();
    }

    void PressShortcutKey(APlayerController& PlayerController)
    {
        const FKey ShortcutKey = GetDefault<UBH_PluginSettings>()->ShortcutKey;
        PlayerController.InputKey(ShortcutKey, IE_Pressed, 1.0f, false);
        PlayerController.InputKey(ShortcutKey, IE_Released, 0.0f, false);
    }

    // Latent step: waits until Condition holds; fails the test after Timeout.
    class FBH_WaitForCommand : public IAutomationLatentCommand
    {
    public:
        FBH_WaitForCommand(FAutomationTestBase& InTest, const TCHAR* InWhat, double InTimeoutSeconds, TFunction<bool()> InCondition)
            : Test(InTest), What(InWhat), TimeoutSeconds(InTimeoutSeconds), Condition(MoveTemp(InCondition))
        {
        }

        virtual bool Update() override
        {
            if (Condition())
            {
                return true;
            }
            if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
            {
                Test.AddError(FString::Printf(TEXT("Timed out waiting for %s"), *What));
                return true;
            }
            return false;
        }

    private:
        FAutomationTestBase& Test;
        FString What;
        double TimeoutSeconds;
        TFunction<bool()> Condition;
    };

    void WaitFor(FAutomationTestBase& Test, const TCHAR* What, double TimeoutSeconds, TFunction<bool()> Condition)
    {
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBH_WaitForCommand>(Test, What, TimeoutSeconds, MoveTemp(Condition)));
    }

    void Then(TFunction<void()> Step)
    {
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFunctionLatentCommand>([Step]()
        {
            Step();
            return true;
        }));
    }

    // Reloads the current map, the way a level change or joining a server replaces the world.
    void ReloadMapAndWait(FAutomationTestBase& Test)
    {
        UWorld* OldWorld = FindGameWorld();
        const TWeakObjectPtr<UWorld> WeakOldWorld = OldWorld;
        UGameplayStatics::OpenLevel(OldWorld, FName(*OldWorld->GetOutermost()->GetName()));

        WaitFor(Test, TEXT("the map to reload"), MapLoadTimeoutSeconds, [WeakOldWorld]()
        {
            UWorld* World = FindGameWorld();
            return World && World != WeakOldWorld.Get() && World->AreActorsInitialized() && World->GetFirstPlayerController();
        });
    }

    // Presses the shortcut once it is bound and expects the form to open.
    void ExpectShortcutOpensForm(FAutomationTestBase& Test)
    {
        WaitFor(Test, TEXT("the shortcut to be bound"), InputTimeoutSeconds, []()
        {
            APlayerController* PlayerController = GetGamePlayerController();
            return PlayerController && CountShortcutBindings(*PlayerController) > 0;
        });
        Then([&Test]()
        {
            APlayerController* PlayerController = GetGamePlayerController();
            if (Test.TestNotNull(TEXT("Player controller"), PlayerController))
            {
                Test.TestEqual(TEXT("Shortcut bindings on the player controller"), CountShortcutBindings(*PlayerController), 1);
                EnterGameplayState(PlayerController);
                PressShortcutKey(*PlayerController);
            }
        });
        WaitFor(Test, TEXT("the shortcut to open the report form"), InputTimeoutSeconds, []()
        {
            return IsReportFormOpen();
        });
        Then([]()
        {
            UWorld* World = FindGameWorld();
            if (UBH_Subsystem* Subsystem = World ? World->GetGameInstance()->GetSubsystem<UBH_Subsystem>() : nullptr)
            {
                Subsystem->HideReportForm();
            }
        });
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_FormAboveUmgMenuTest, "BetaHub.ReportForm.DrawnAboveUmgMenu", GameTestFlags)

bool FBH_FormAboveUmgMenuTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    TestTrue(TEXT("Form drawn above a menu added with UMG AddToViewport"), IsReportFormAboveMenu(*this, Fixture, UmgMenuViewportZOrder));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_FormUsesConfiguredZOrderTest, "BetaHub.ReportForm.UsesConfiguredZOrder", GameTestFlags)

bool FBH_FormUsesConfiguredZOrderTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    UBH_PluginSettings* Settings = GetMutableDefault<UBH_PluginSettings>();
    const int32 OriginalZOrder = Settings->ReportFormZOrder;
    Settings->ReportFormZOrder = UmgMenuViewportZOrder - 1;

    TestFalse(TEXT("Form drawn below a menu with a higher ZOrder"), IsReportFormAboveMenu(*this, Fixture, UmgMenuViewportZOrder));

    Settings->ReportFormZOrder = OriginalZOrder;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_CloseRemovesViewportSlotTest, "BetaHub.ReportForm.CloseRemovesViewportSlot", GameTestFlags)

bool FBH_CloseRemovesViewportSlotTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    // A menu only gives access to the viewport overlay, so its slots can be counted.
    const TSharedRef<SWidget> Menu = AddMenuToViewport(*Fixture.ViewportClient, UmgMenuViewportZOrder);
    const TSharedPtr<SWidget> Overlay = Menu->GetParentWidget();
    if (TestTrue(TEXT("Menu added to the viewport overlay"), Overlay.IsValid()))
    {
        const int32 SlotsBeforeOpen = Overlay->GetChildren()->Num();

        Fixture.Subsystem->ShowReportForm();
        Fixture.Subsystem->HideReportForm();

        TestEqual(TEXT("Viewport overlay slots after the form closes"), Overlay->GetChildren()->Num(), SlotsBeforeOpen);
    }

    Fixture.ViewportClient->RemoveViewportWidgetContent(Menu);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_ShortcutOpensFormTest, "BetaHub.Shortcut.OpensReportForm", GameTestFlags)

bool FBH_ShortcutOpensFormTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    ExpectShortcutOpensForm(*this);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_ShortcutAfterMapLoadTest, "BetaHub.Shortcut.OpensReportFormAfterMapLoad", GameTestFlags)

bool FBH_ShortcutAfterMapLoadTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    ReloadMapAndWait(*this);
    ExpectShortcutOpensForm(*this);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBH_FormClosesOnMapLoadTest, "BetaHub.ReportForm.ClosesOnMapLoad", GameTestFlags)

bool FBH_FormClosesOnMapLoadTest::RunTest(const FString& Parameters)
{
    FBH_GameFixture Fixture;
    if (!GetGameFixture(*this, Fixture))
    {
        return false;
    }

    UBH_Subsystem* Subsystem = Fixture.Subsystem;
    Subsystem->ShowReportForm();
    ReloadMapAndWait(*this);
    WaitFor(*this, TEXT("the report form to close after the map change"), InputTimeoutSeconds, []()
    {
        return !IsReportFormOpen();
    });
    Then([Subsystem]()
    {
        Subsystem->HideReportForm();
    });
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
