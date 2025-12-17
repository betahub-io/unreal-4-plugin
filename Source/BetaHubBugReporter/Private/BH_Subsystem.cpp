#include "BH_Subsystem.h"
#include "BH_PluginSettings.h"
#include "BH_LogCapture.h"
#include "BH_ScreenshotCapture.h"
#include "SBH_ReportForm.h"
#include "BH_Log.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SWeakWidget.h"
#include "Framework/Application/SlateApplication.h"

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

    HideReportForm();

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

    // Remove existing widget if any
    if (ReportFormWidget.IsValid())
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(ReportFormWidget.ToSharedRef());
        ReportFormWidget.Reset();
    }

    // Create new Slate widget
    SAssignNew(ReportFormWidget, SBH_ReportForm)
        .Subsystem(this)
        .ScreenshotPath(ScreenshotPath);

    GEngine->GameViewport->AddViewportWidgetContent(
        SNew(SWeakWidget).PossiblyNullContent(ReportFormWidget)
    );

    ReportFormWidget->Show();

    // Capture mouse for UI
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            PC->bShowMouseCursor = true;
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(ReportFormWidget);
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
        }
    }

    UE_LOG(LogBetaHub, Log, TEXT("Bug report form shown"));
}

void UBH_Subsystem::HideReportForm()
{
    if (ReportFormWidget.IsValid())
    {
        ReportFormWidget->Hide();

        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->RemoveViewportWidgetContent(ReportFormWidget.ToSharedRef());
        }

        ReportFormWidget.Reset();

        // Return to game input mode
        if (UWorld* World = GetWorld())
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                PC->bShowMouseCursor = false;
                FInputModeGameOnly InputMode;
                PC->SetInputMode(InputMode);
            }
        }

        UE_LOG(LogBetaHub, Log, TEXT("Bug report form hidden"));
    }
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
