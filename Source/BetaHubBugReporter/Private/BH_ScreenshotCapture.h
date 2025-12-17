#pragma once

#include "CoreMinimal.h"

class BH_ScreenshotCapture
{
public:
    static FString CaptureScreenshot();

private:
    static FString GetScreenshotPath();
};
