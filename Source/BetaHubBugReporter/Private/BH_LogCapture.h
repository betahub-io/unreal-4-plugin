#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"
#include "BH_LogCapture.generated.h"

UCLASS()
class UBH_LogCapture : public UObject, public FOutputDevice
{
    GENERATED_BODY()

public:
    UBH_LogCapture();
    virtual ~UBH_LogCapture();

    void StartCapture();
    void StopCapture();

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override;

    FString GetCapturedLogs() const;
    void ClearLogs();

private:
    FString CapturedLogs;
    FCriticalSection LogMutex;
    bool bIsCapturing;
};
