#include "BH_LogCapture.h"
#include "BH_Log.h"

UBH_LogCapture::UBH_LogCapture()
    : bIsCapturing(false)
{
}

UBH_LogCapture::~UBH_LogCapture()
{
    StopCapture();
}

void UBH_LogCapture::StartCapture()
{
    if (!bIsCapturing)
    {
        GLog->AddOutputDevice(this);
        bIsCapturing = true;
        UE_LOG(LogBetaHub, Log, TEXT("Log capture started"));
    }
}

void UBH_LogCapture::StopCapture()
{
    if (bIsCapturing)
    {
        GLog->RemoveOutputDevice(this);
        bIsCapturing = false;
    }
}

void UBH_LogCapture::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
    FScopeLock Lock(&LogMutex);
    CapturedLogs.Append(V);
    CapturedLogs.Append(TEXT("\n"));
}

FString UBH_LogCapture::GetCapturedLogs() const
{
    FScopeLock Lock(&const_cast<UBH_LogCapture*>(this)->LogMutex);
    return CapturedLogs;
}

void UBH_LogCapture::ClearLogs()
{
    FScopeLock Lock(&LogMutex);
    CapturedLogs.Empty();
}
