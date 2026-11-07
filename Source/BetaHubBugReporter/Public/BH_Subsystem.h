#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BH_Subsystem.generated.h"

class UBH_LogCapture;
class SBH_ReportForm;
class SWidget;
class APlayerController;
class UGameViewportClient;
class UInputComponent;
class UWorld;
struct FBH_SavedInputState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBH_OnReportFormClosed);

UCLASS()
class BETAHUBBUGREPORTER_API UBH_Subsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "BetaHub")
    void ShowReportForm();

    UFUNCTION(BlueprintCallable, Category = "BetaHub")
    void HideReportForm();

    UFUNCTION(BlueprintCallable, Category = "BetaHub")
    FString GetCapturedLogs() const;

    UFUNCTION(BlueprintCallable, Category = "BetaHub")
    FString TakeScreenshot();

    /** Fires after the report form closes. Mouse cursor and input mode are already restored to their state from before the form opened.
     *  A map change also closes the form; if the player controller that opened it is gone, input is left to the new map. */
    UPROPERTY(BlueprintAssignable, Category = "BetaHub")
    FBH_OnReportFormClosed OnReportFormClosed;

private:
    /** Runs every frame: the game instance changes worlds on every map load, so the shortcut and the open form follow the current world. */
    bool TickWorldWatch(float DeltaTime);
    void BindShortcutKey(APlayerController* PlayerController);

    void SaveInputState(APlayerController* PlayerController, UGameViewportClient& ViewportClient);
    void RestoreInputState();
    void RemoveReportFormWidget();

    UPROPERTY()
    UBH_LogCapture* LogCapture;

    TSharedPtr<SBH_ReportForm> ReportFormWidget;

    /** The viewport holds the form through this weak wrapper; removing the form from the viewport needs the wrapper, not the form. */
    TSharedPtr<SWidget> ReportFormViewportContent;

    FString LastScreenshotPath;

    /** Input state from before the form opened; null when there is nothing to restore. */
    TSharedPtr<FBH_SavedInputState> SavedInputState;

    /** World the open form belongs to; the form closes when the map changes. */
    TWeakObjectPtr<UWorld> ReportFormWorld;

    /** Input component that holds the shortcut binding; a new player controller gets a new one. */
    TWeakObjectPtr<UInputComponent> ShortcutInputComponent;

    bool bBindShortcut = false;

    FDelegateHandle WorldWatchTickerHandle;
};
