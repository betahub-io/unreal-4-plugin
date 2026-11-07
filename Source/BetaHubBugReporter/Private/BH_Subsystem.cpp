#include "BH_Subsystem.h"
#include "BH_PluginSettings.h"
#include "BH_LogCapture.h"
#include "BH_ScreenshotCapture.h"
#include "SBH_ReportForm.h"
#include "BH_Log.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SWeakWidget.h"
#include "Framework/Application/SlateApplication.h"

struct FBH_SavedInputState
{
    TWeakObjectPtr<APlayerController> PlayerController;
    TWeakPtr<SWidget> FocusedWidget;
    bool bShowMouseCursor = false;
    bool bIgnoreInput = false;
    EMouseCaptureMode MouseCaptureMode = EMouseCaptureMode::CapturePermanently;
    EMouseLockMode MouseLockMode = EMouseLockMode::DoNotLock;
    bool bHideCursorDuringCapture = false;
};

namespace
{
    // UE 4.27 has no getter for the viewport's mouse lock mode, so rebuild it from the public queries.
    // LockInFullscreen comes back as LockAlways (fullscreen) or LockOnCapture (windowed),
    // which behave the same as LockInFullscreen in the current window mode.
    EMouseLockMode ReadMouseLockMode(UGameViewportClient& ViewportClient)
    {
        if (!ViewportClient.LockDuringCapture())
        {
            return EMouseLockMode::DoNotLock;
        }
        return ViewportClient.ShouldAlwaysLockMouse() ? EMouseLockMode::LockAlways : EMouseLockMode::LockOnCapture;
    }

    bool CapturesMousePermanently(EMouseCaptureMode Mode)
    {
        return Mode == EMouseCaptureMode::CapturePermanently
            || Mode == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;
    }
}

void UBH_Subsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (GEngine && GEngine->GetNetMode(GetWorld()) == ENetMode::NM_DedicatedServer)
    {
        return;
    }

    UE_LOG(LogBetaHub, Log, TEXT("BH_Subsystem initialized"));

    UBH_PluginSettings* Settings = GetMutableDefault<UBH_PluginSettings>();

    if (Settings->bSpawnServiceOnStartup)
    {
        UE_LOG(LogBetaHub, Log, TEXT("Starting bug reporter service automatically"));

        // Start log capture
        LogCapture = NewObject<UBH_LogCapture>(this);
        LogCapture->StartCapture();

        // Setup input binding for shortcut
        if (Settings->bEnableShortcut)
        {
            SetupInputBinding();
        }
    }
}

void UBH_Subsystem::Deinitialize()
{
    // Clean up actor spawned delegate
    if (PlayerControllerChangedHandle.IsValid())
    {
        if (UWorld* World = GetWorld())
        {
            World->RemoveOnActorSpawnedHandler(PlayerControllerChangedHandle);
        }
        PlayerControllerChangedHandle.Reset();
    }

    if (LogCapture)
    {
        LogCapture->StopCapture();
        LogCapture = nullptr;
    }

    // Shutting down: drop the form without restoring input or calling game code.
    RemoveReportFormWidget();
    SavedInputState.Reset();

    Super::Deinitialize();
}

void UBH_Subsystem::ShowReportForm()
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogBetaHub, Error, TEXT("Cannot show report form - no game viewport"));
        return;
    }

    // Take screenshot before showing the form
    FString ScreenshotPath = BH_ScreenshotCapture::CaptureScreenshot();
    LastScreenshotPath = ScreenshotPath;

    // Remember input state before the form changes it. Skipped when re-opening an open form,
    // which would otherwise save the form's own UI-only state.
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
    if (PC && ViewportClient && !SavedInputState.IsValid())
    {
        SaveInputState(PC, *ViewportClient);
    }

    RemoveReportFormWidget();

    // Create new Slate widget
    SAssignNew(ReportFormWidget, SBH_ReportForm)
        .Subsystem(this)
        .ScreenshotPath(ScreenshotPath);

    ReportFormViewportContent = SNew(SWeakWidget).PossiblyNullContent(ReportFormWidget);
    GEngine->GameViewport->AddViewportWidgetContent(
        ReportFormViewportContent.ToSharedRef(),
        GetDefault<UBH_PluginSettings>()->ReportFormZOrder
    );

    ReportFormWidget->Show();

    // Capture mouse for UI
    if (PC)
    {
        PC->bShowMouseCursor = true;
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(ReportFormWidget);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
    }

    UE_LOG(LogBetaHub, Log, TEXT("Bug report form shown"));
}

void UBH_Subsystem::HideReportForm()
{
    if (!ReportFormWidget.IsValid())
    {
        return;
    }

    RemoveReportFormWidget();
    RestoreInputState();

    UE_LOG(LogBetaHub, Log, TEXT("Bug report form hidden"));

    OnReportFormClosed.Broadcast();
}

void UBH_Subsystem::RemoveReportFormWidget()
{
    if (!ReportFormWidget.IsValid())
    {
        return;
    }

    ReportFormWidget->Hide();

    if (GEngine && GEngine->GameViewport && ReportFormViewportContent.IsValid())
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(ReportFormViewportContent.ToSharedRef());
    }

    ReportFormViewportContent.Reset();
    ReportFormWidget.Reset();
}

void UBH_Subsystem::SaveInputState(APlayerController* PlayerController, UGameViewportClient& ViewportClient)
{
    SavedInputState = MakeShared<FBH_SavedInputState>();
    SavedInputState->PlayerController = PlayerController;
    SavedInputState->bShowMouseCursor = PlayerController->bShowMouseCursor;
    SavedInputState->bIgnoreInput = ViewportClient.IgnoreInput();
    SavedInputState->MouseCaptureMode = ViewportClient.GetMouseCaptureMode();
    SavedInputState->MouseLockMode = ReadMouseLockMode(ViewportClient);
    SavedInputState->bHideCursorDuringCapture = ViewportClient.HideCursorDuringCapture();

    if (FSlateApplication::IsInitialized())
    {
        SavedInputState->FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
    }
}

void UBH_Subsystem::RestoreInputState()
{
    if (!SavedInputState.IsValid())
    {
        // The form opened without a player controller, so input was never changed.
        return;
    }

    const FBH_SavedInputState State = *SavedInputState;
    SavedInputState.Reset();

    APlayerController* PC = State.PlayerController.Get();
    UGameViewportClient* ViewportClient = PC ? PC->GetWorld()->GetGameViewport() : nullptr;
    if (!ViewportClient)
    {
        UE_LOG(LogBetaHub, Warning, TEXT("Player controller or viewport gone while the report form was open - input not restored"));
        return;
    }

    PC->bShowMouseCursor = State.bShowMouseCursor;

    // Rebuild the matching standard input mode, so Slate focus and mouse capture are restored too.
    const TSharedPtr<SWidget> FocusedWidget = State.FocusedWidget.Pin();
    if (State.bIgnoreInput)
    {
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(FocusedWidget);
        InputMode.SetLockMouseToViewportBehavior(State.MouseLockMode);
        PC->SetInputMode(InputMode);
    }
    else if (CapturesMousePermanently(State.MouseCaptureMode))
    {
        FInputModeGameOnly InputMode;
        InputMode.SetConsumeCaptureMouseDown(State.MouseCaptureMode == EMouseCaptureMode::CapturePermanently);
        PC->SetInputMode(InputMode);
    }
    else
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(FocusedWidget);
        InputMode.SetLockMouseToViewportBehavior(State.MouseLockMode);
        InputMode.SetHideCursorDuringCapture(State.bHideCursorDuringCapture);
        PC->SetInputMode(InputMode);
    }

    // Input modes set fixed capture and lock modes; put back the exact ones the game had.
    ViewportClient->SetMouseCaptureMode(State.MouseCaptureMode);
    ViewportClient->SetMouseLockMode(State.MouseLockMode);
}

FString UBH_Subsystem::GetCapturedLogs() const
{
    if (LogCapture)
    {
        return LogCapture->GetCapturedLogs();
    }
    return FString();
}

FString UBH_Subsystem::TakeScreenshot()
{
    return BH_ScreenshotCapture::CaptureScreenshot();
}

void UBH_Subsystem::SetupInputBinding()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Try to bind immediately if player controller exists
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        BindShortcutKey(PC);
    }

    // Listen for new player controllers spawning
    PlayerControllerChangedHandle = World->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateLambda([this](AActor* SpawnedActor)
        {
            if (APlayerController* PC = Cast<APlayerController>(SpawnedActor))
            {
                // Defer binding to next frame to ensure InputComponent is ready
                if (UWorld* MyWorld = GetWorld())
                {
                    MyWorld->GetTimerManager().SetTimerForNextTick([this, PC]()
                    {
                        BindShortcutKey(PC);
                    });
                }
            }
        })
    );
}

void UBH_Subsystem::OnPlayerControllerChanged(APlayerController* NewPlayerController)
{
    BindShortcutKey(NewPlayerController);
}

void UBH_Subsystem::BindShortcutKey(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return;
    }

    // Wait for InputComponent to be ready
    if (!PlayerController->InputComponent)
    {
        return;
    }

    UBH_PluginSettings* Settings = GetMutableDefault<UBH_PluginSettings>();
    if (!Settings->bEnableShortcut)
    {
        return;
    }

    // Bind the shortcut key using a lambda
    FInputKeyBinding KeyBinding(FInputChord(Settings->ShortcutKey), EInputEvent::IE_Pressed);
    KeyBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this]()
    {
        ShowReportForm();
    });
    PlayerController->InputComponent->KeyBindings.Add(KeyBinding);

    UE_LOG(LogBetaHub, Log, TEXT("Shortcut key bound: %s"), *Settings->ShortcutKey.ToString());
}
