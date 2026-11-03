#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BH_Subsystem.generated.h"

class UBH_LogCapture;
class SBH_ReportForm;
class APlayerController;
class UGameViewportClient;
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

    /** Fires after the report form closes. Mouse cursor and input mode are already restored to their state from before the form opened. */
    UPROPERTY(BlueprintAssignable, Category = "BetaHub")
    FBH_OnReportFormClosed OnReportFormClosed;

private:
    void SetupInputBinding();
    void OnPlayerControllerChanged(APlayerController* NewPlayerController);
    void BindShortcutKey(APlayerController* PlayerController);

    void SaveInputState(APlayerController* PlayerController, UGameViewportClient& ViewportClient);
    void RestoreInputState();
    void RemoveReportFormWidget();

    UPROPERTY()
    UBH_LogCapture* LogCapture;

    TSharedPtr<SBH_ReportForm> ReportFormWidget;
    FString LastScreenshotPath;

    /** Input state from before the form opened; null when there is nothing to restore. */
    TSharedPtr<FBH_SavedInputState> SavedInputState;

    FDelegateHandle PlayerControllerChangedHandle;
};
