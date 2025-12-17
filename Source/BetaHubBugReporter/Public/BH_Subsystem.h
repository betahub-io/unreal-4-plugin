#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BH_Subsystem.generated.h"

class UBH_LogCapture;
class SBH_ReportForm;

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

private:
    void SetupInputBinding();
    void OnPlayerControllerChanged(APlayerController* NewPlayerController);
    void BindShortcutKey(APlayerController* PlayerController);

    UPROPERTY()
    UBH_LogCapture* LogCapture;

    TSharedPtr<SBH_ReportForm> ReportFormWidget;
    FString LastScreenshotPath;

    FDelegateHandle PlayerControllerChangedHandle;
};
